#include "research/ResearchSupport.h"

#include "EvidenceLog.h"
#include "NativeMenuProbe.h"
#include "runtime/ProbeRuntimeState.h"
#include "runtime/CooperativeService.h"
#include "ui/MenuMessaging.h"
#include "ui/PauseMenuIntegration.h"

#include <RE/Starfield.h>
#include <SFSE/SFSE.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AbsoluteControlPanelResearch::ResearchSupport
{
    namespace
    {
        [[nodiscard]] std::string_view Trim(std::string_view a_value) noexcept
        {
            const auto isSpace = [](unsigned char a_character) {
                return std::isspace(a_character) != 0;
            };
            while (!a_value.empty() &&
                   isSpace(static_cast<unsigned char>(a_value.front()))) {
                a_value.remove_prefix(1);
            }
            while (!a_value.empty() &&
                   isSpace(static_cast<unsigned char>(a_value.back()))) {
                a_value.remove_suffix(1);
            }
            return a_value;
        }

        [[nodiscard]] bool ParseBool(std::string_view a_value, bool a_fallback) noexcept
        {
            std::string normalized{ Trim(a_value) };
            std::ranges::transform(normalized, normalized.begin(), [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
            if (normalized == "1" || normalized == "true" || normalized == "yes" ||
                normalized == "on") {
                return true;
            }
            if (normalized == "0" || normalized == "false" || normalized == "no" ||
                normalized == "off") {
                return false;
            }
            return a_fallback;
        }

        [[nodiscard]] std::uint32_t ParseUnsigned(
            std::string_view a_value, std::uint32_t a_fallback) noexcept
        {
            a_value = Trim(a_value);
            int base = 10;
            if (a_value.starts_with("0x") || a_value.starts_with("0X")) {
                base = 16;
                a_value.remove_prefix(2);
            }
            std::uint32_t parsed{};
            const auto [end, error] = std::from_chars(
                a_value.data(), a_value.data() + a_value.size(), parsed, base);
            return error == std::errc{} && end == a_value.data() + a_value.size() ?
                parsed : a_fallback;
        }

        struct DelayedJob
        {
            std::chrono::steady_clock::time_point due;
            std::function<void()> callback;
        };

        struct ResearchServices
        {
            std::mutex lifecycleLock;
            std::mutex delayedLock;
            std::vector<DelayedJob> delayedJobs;
            std::mutex activeKeyLock;
            std::unordered_map<WORD, DWORD> activeKeys;
            std::shared_ptr<Runtime::CallbackGate> taskGate;
            Runtime::CooperativeService scheduler;
            Runtime::CooperativeService titleAdvance;
            Runtime::CooperativeService mailbox;
            Runtime::CooperativeService experiment;
        };

        ResearchServices& Services() noexcept
        {
            // Intentionally process-lived: SFSE has no plugin-unload notification,
            // and joining threads from DLL static destruction risks loader-lock
            // deadlock. Stop() remains available for controlled research teardown.
            static auto* services = new ResearchServices{};
            return *services;
        }

        template <class Callback>
        void QueueIfActive(const SFSE::TaskInterface* a_taskInterface,
            const std::shared_ptr<Runtime::CallbackGate>& a_gate,
            Callback&& a_callback)
        {
            a_taskInterface->AddTask([
                gate = a_gate, callback = std::forward<Callback>(a_callback)]() mutable {
                if (gate) {
                    (void)gate->TryInvoke(std::move(callback));
                }
            });
        }

        bool ScheduleDelayed(std::chrono::milliseconds a_delay,
            std::function<void()> a_callback) noexcept
        {
            try {
                auto& services = Services();
                const std::scoped_lock lock{ services.delayedLock };
                if (!services.scheduler.IsRunning()) {
                    return false;
                }
                services.delayedJobs.push_back(DelayedJob{
                    std::chrono::steady_clock::now() + a_delay,
                    std::move(a_callback) });
                return true;
            } catch (...) {
                return false;
            }
        }

        void StartScheduler() noexcept
        {
            auto& services = Services();
            {
                const std::scoped_lock lock{ services.delayedLock };
                services.delayedJobs.clear();
            }
            (void)services.scheduler.Start(
                [&services](std::stop_token a_stopToken,
                    const std::shared_ptr<Runtime::CallbackGate>&) {
                    while (!a_stopToken.stop_requested()) {
                        std::vector<std::function<void()>> due;
                        const auto now = std::chrono::steady_clock::now();
                        {
                            const std::scoped_lock lock{ services.delayedLock };
                            auto job = services.delayedJobs.begin();
                            while (job != services.delayedJobs.end()) {
                                if (job->due <= now) {
                                    due.push_back(std::move(job->callback));
                                    job = services.delayedJobs.erase(job);
                                } else {
                                    ++job;
                                }
                            }
                        }
                        for (auto& callback : due) {
                            if (a_stopToken.stop_requested()) {
                                break;
                            }
                            callback();
                        }
                        if (!Runtime::InterruptibleWait(
                                a_stopToken, std::chrono::milliseconds(5))) {
                            break;
                        }
                    }
                });
        }

        void TrackKeyDown(WORD a_scanCode, DWORD a_flags) noexcept
        {
            const std::scoped_lock lock{ Services().activeKeyLock };
            Services().activeKeys[a_scanCode] = a_flags;
        }

        void TrackKeyUp(WORD a_scanCode) noexcept
        {
            const std::scoped_lock lock{ Services().activeKeyLock };
            Services().activeKeys.erase(a_scanCode);
        }

        void ReleaseActiveKeys() noexcept
        {
            std::unordered_map<WORD, DWORD> active;
            {
                const std::scoped_lock lock{ Services().activeKeyLock };
                active.swap(Services().activeKeys);
            }
            for (const auto [scanCode, flags] : active) {
                INPUT keyUp{};
                keyUp.type = INPUT_KEYBOARD;
                keyUp.ki.wScan = scanCode;
                keyUp.ki.dwFlags = flags | KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
                (void)::SendInput(1, &keyUp, sizeof(INPUT));
            }
        }

        class ForeignMenuMemberVisitor final :
            public RE::Scaleform::GFx::Value::ObjectVisitor
        {
        public:
            struct Child
            {
                std::string name;
                RE::Scaleform::GFx::Value value;
            };

            ForeignMenuMemberVisitor(std::uint32_t a_commandId,
                std::string_view a_menuName, std::string_view a_path,
                bool a_includePublic, bool a_collectChildren) :
                commandId(a_commandId),
                menuName(a_menuName),
                path(a_path),
                includePublic(a_includePublic),
                collectChildren(a_collectChildren)
            {}

            bool IncludeAS3PublicMembers() const override { return includePublic; }

            void Visit(const char* a_name,
                const RE::Scaleform::GFx::Value& a_value) override
            {
                ++visited;
                if (recorded < kMaximumRecordedMembers) {
                    EvidenceLog::Event(
                        "foreign_menu_member",
                        std::format(
                            "id={} menu={} path={} name={} type={} object={} display={} array={}",
                            commandId, menuName, path, a_name ? a_name : "<null>",
                            static_cast<std::uint32_t>(a_value.GetType()),
                            a_value.IsObject(), a_value.IsDisplayObject(), a_value.IsArray()));
                    ++recorded;
                }
                if (collectChildren && children.size() < kMaximumChildObjects && a_name &&
                    a_value.IsObject() && !a_value.IsArray() &&
                    std::string_view(a_name) != "_root" &&
                    std::string_view(a_name) != "_parent") {
                    children.push_back(Child{ a_name, a_value });
                }
            }

            static constexpr std::size_t kMaximumRecordedMembers = 256;
            static constexpr std::size_t kMaximumChildObjects = 24;
            std::uint32_t commandId{};
            std::string menuName;
            std::string path;
            bool includePublic{};
            bool collectChildren{};
            std::size_t visited{};
            std::size_t recorded{};
            std::vector<Child> children;
        };

        void ProbeForeignMenuRoot(
            std::string_view a_menuName, std::uint32_t a_commandId) noexcept
        {
            try {
                const auto ui = RE::UI::GetSingleton();
                const RE::BSFixedString menuName{ a_menuName.data() };
                auto menu = ui ? ui->GetMenu(menuName) : nullptr;
                if (!menu || !menu->uiMovie || !menu->uiMovie->asMovieRoot) {
                    EvidenceLog::Event(
                        "foreign_menu_probe_unavailable",
                        std::format("id={} menu={}", a_commandId, a_menuName));
                    return;
                }

                const auto database = REL::IDDB::GetSingleton();
                constexpr std::uint64_t kVisitMembersId = 169753;
                const auto visitMembersOffset = database ? database->offset(kVisitMembersId) : 0;
                if (visitMembersOffset == 0) {
                    EvidenceLog::Event(
                        "foreign_menu_probe_rejected",
                        std::format(
                            "id={} menu={} visit_members_id={} offset=0",
                            a_commandId, a_menuName, kVisitMembersId));
                    return;
                }

                RE::Scaleform::GFx::Value root;
                const bool resolved = menu->uiMovie->asMovieRoot->GetVariable(&root, "_root");
                EvidenceLog::Event(
                    "foreign_menu_root",
                    std::format(
                        "id={} menu={} resolved={} type={} object={} display={} "
                        "visit_members_id={} offset=0x{:08X}",
                        a_commandId, a_menuName, resolved,
                        static_cast<std::uint32_t>(root.GetType()), root.IsObject(),
                        root.IsDisplayObject(), kVisitMembersId, visitMembersOffset));
                if (!resolved || !root.IsObject()) {
                    return;
                }

                ForeignMenuMemberVisitor rootVisitor{
                    a_commandId, a_menuName, "_root", true, true };
                root.VisitMembers(&rootVisitor);
                EvidenceLog::Event(
                    "foreign_menu_members_complete",
                    std::format(
                        "id={} menu={} path=_root visited={} recorded={} children={}",
                        a_commandId, a_menuName, rootVisitor.visited,
                        rootVisitor.recorded, rootVisitor.children.size()));

                for (auto& child : rootVisitor.children) {
                    if (!child.value.IsObject()) {
                        continue;
                    }
                    const auto childPath = std::format("_root.{}", child.name);
                    ForeignMenuMemberVisitor childVisitor{
                        a_commandId, a_menuName, childPath, false, false };
                    child.value.VisitMembers(&childVisitor);
                    EvidenceLog::Event(
                        "foreign_menu_members_complete",
                        std::format(
                            "id={} menu={} path={} visited={} recorded={} children=0",
                            a_commandId, a_menuName, childPath, childVisitor.visited,
                            childVisitor.recorded));
                }

                const auto probePath = [&](std::string_view a_path) {
                    RE::Scaleform::GFx::Value target;
                    const bool targetResolved =
                        menu->uiMovie->asMovieRoot->GetVariable(&target, a_path.data());
                    EvidenceLog::Event(
                        "foreign_menu_target",
                        std::format(
                            "id={} menu={} path={} resolved={} type={} object={} display={}",
                            a_commandId, a_menuName, a_path, targetResolved,
                            static_cast<std::uint32_t>(target.GetType()), target.IsObject(),
                            target.IsDisplayObject()));
                    if (!targetResolved || !target.IsObject()) {
                        return;
                    }
                    ForeignMenuMemberVisitor targetVisitor{
                        a_commandId, a_menuName, a_path, true, true };
                    target.VisitMembers(&targetVisitor);
                    EvidenceLog::Event(
                        "foreign_menu_members_complete",
                        std::format(
                            "id={} menu={} path={} visited={} recorded={} children={}",
                            a_commandId, a_menuName, a_path, targetVisitor.visited,
                            targetVisitor.recorded, targetVisitor.children.size()));
                    for (auto& targetChild : targetVisitor.children) {
                        const auto targetChildPath =
                            std::format("{}.{}", a_path, targetChild.name);
                        ForeignMenuMemberVisitor targetChildVisitor{
                            a_commandId, a_menuName, targetChildPath, false, false };
                        targetChild.value.VisitMembers(&targetChildVisitor);
                        EvidenceLog::Event(
                            "foreign_menu_members_complete",
                            std::format(
                                "id={} menu={} path={} visited={} recorded={} children=0",
                                a_commandId, a_menuName, targetChildPath,
                                targetChildVisitor.visited, targetChildVisitor.recorded));
                    }
                };

                if (a_menuName == "PauseMenu") {
                    probePath("_root.Menu_mc.MainPanel_mc");
                    probePath("_root.Menu_mc.MainPanel_mc.MainList_mc");
                    probePath("_root.Menu_mc.MainPanel_mc.MainList_mc.EntryHolder_mc");
                    probePath("_root.Menu_mc.MainPanel_mc.ButtonBar_mc");
                } else if (a_menuName == "MainMenu") {
                    probePath("_root.Menu_mc.MOTDHolder_mc");
                    probePath("_root.Menu_mc.MOTDHolder_mc.MOTD_mc");
                    probePath("_root.Menu_mc.AdBannerHolder_mc");
                    probePath("_root.Menu_mc.AdBannerHolder_mc.AdBanner_mc");
                    probePath("_root.Menu_mc.ButtonBar_mc");
                    probePath("_root.Menu_mc.MainPanel_mc");
                    probePath("_root.Menu_mc.MainPanel_mc.MainList_mc");
                    probePath("_root.Menu_mc.MainPanel_mc.MainList_mc.EntryHolder_mc");
                }
            } catch (...) {
                EvidenceLog::Event(
                    "foreign_menu_probe_error",
                    std::format("id={} menu={}", a_commandId, a_menuName));
            }
        }

        void QueueScanCodePulse(
            const SFSE::TaskInterface* a_taskInterface, std::uint32_t a_commandId,
            std::string a_command, WORD a_scanCode, bool a_extended,
            const std::shared_ptr<Runtime::CallbackGate>& a_gate) noexcept
        {
            QueueIfActive(a_taskInterface, a_gate, [
                a_taskInterface, gate = a_gate, a_commandId,
                command = std::move(a_command), a_scanCode, a_extended]() {
                const auto activeWindow = ::GetActiveWindow();
                const auto foregroundBefore = ::GetForegroundWindow();
                const bool focused = activeWindow != nullptr &&
                    ::SetForegroundWindow(activeWindow) != FALSE;
                if (activeWindow) {
                    ::SetActiveWindow(activeWindow);
                    ::SetFocus(activeWindow);
                }
                INPUT keyDown{};
                keyDown.type = INPUT_KEYBOARD;
                keyDown.ki.wScan = a_scanCode;
                keyDown.ki.dwFlags = KEYEVENTF_SCANCODE |
                    (a_extended ? KEYEVENTF_EXTENDEDKEY : 0);
                ::SetLastError(ERROR_SUCCESS);
                const auto sent = ::SendInput(1, &keyDown, sizeof(INPUT));
                const auto error = ::GetLastError();
                if (sent == 1) {
                    TrackKeyDown(a_scanCode,
                        a_extended ? KEYEVENTF_EXTENDEDKEY : 0);
                }
                EvidenceLog::Trace(
                    "research_input_key_down",
                    std::format(
                        "id={} command={} scan_code=0x{:02X} extended={} sent={} error={} "
                        "active=0x{:X} foreground_before=0x{:X} foreground_after=0x{:X} "
                        "focused={}",
                        a_commandId, command, a_scanCode, a_extended, sent, error,
                        reinterpret_cast<std::uintptr_t>(activeWindow),
                        reinterpret_cast<std::uintptr_t>(foregroundBefore),
                        reinterpret_cast<std::uintptr_t>(::GetForegroundWindow()), focused));

                const bool releaseScheduled = ScheduleDelayed(
                    std::chrono::milliseconds(100),
                    [a_taskInterface, gate, a_commandId, command, a_scanCode, a_extended]() {
                    QueueIfActive(a_taskInterface, gate,
                        [a_commandId, command, a_scanCode, a_extended]() {
                        INPUT keyUp{};
                        keyUp.type = INPUT_KEYBOARD;
                        keyUp.ki.wScan = a_scanCode;
                        keyUp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP |
                            (a_extended ? KEYEVENTF_EXTENDEDKEY : 0);
                        ::SetLastError(ERROR_SUCCESS);
                        const auto sent = ::SendInput(1, &keyUp, sizeof(INPUT));
                        const auto error = ::GetLastError();
                        TrackKeyUp(a_scanCode);
                        EvidenceLog::Trace(
                            "research_input_key_up",
                            std::format(
                                "id={} command={} scan_code=0x{:02X} extended={} sent={} error={}",
                                a_commandId, command, a_scanCode, a_extended, sent, error));
                    });
                });
                if (!releaseScheduled && sent == 1) {
                    ReleaseActiveKeys();
                    EvidenceLog::Trace(
                        "research_input_key_release_forced",
                        std::format("id={} command={}", a_commandId, command));
                }
                if (command == "pause") {
                    (void)ScheduleDelayed(
                        std::chrono::milliseconds(850),
                        [a_taskInterface, gate, a_commandId]() {
                        QueueIfActive(a_taskInterface, gate, [a_commandId]() {
                            const auto ui = RE::UI::GetSingleton();
                            const bool open = ui &&
                                ui->IsMenuOpen(RE::BSFixedString("PauseMenu"));
                            EvidenceLog::Trace(
                                "research_pause_state",
                                std::format("id={} open={}", a_commandId, open));
                            if (open) {
                                ProbeForeignMenuRoot("PauseMenu", a_commandId);
                            }
                        });
                    });
                }
            });
        }

        void PollMailbox(const SFSE::TaskInterface* a_taskInterface,
            const std::filesystem::path& a_inputPath,
            std::uint32_t& a_lastCommandId,
            const std::shared_ptr<Runtime::CallbackGate>& a_gate) noexcept
        {
            try {
                std::ifstream stream{ a_inputPath };
                std::string idLine;
                std::string commandLine;
                if (!stream || !std::getline(stream, idLine) ||
                    !std::getline(stream, commandLine) || !idLine.starts_with("id=") ||
                    !commandLine.starts_with("command=")) {
                    return;
                }
                std::uint32_t commandId{};
                const auto idText = std::string_view{ idLine }.substr(3);
                const auto [end, error] = std::from_chars(
                    idText.data(), idText.data() + idText.size(), commandId);
                if (error != std::errc{} || end != idText.data() + idText.size() ||
                    commandId <= a_lastCommandId) {
                    return;
                }

                const std::string command = commandLine.substr(8);
                if (command == "inject_pause_entry") {
                    a_lastCommandId = commandId;
                    EvidenceLog::Event(
                        "pause_entry_injection_queued",
                        std::format("id={} command={}", commandId, command));
                    Ui::PauseMenuIntegration::RequestInjection(commandId);
                    return;
                }
                if (command == "probe_pause_root" || command == "probe_main_root") {
                    a_lastCommandId = commandId;
                    const std::string menuName = command == "probe_pause_root" ?
                        "PauseMenu" : "MainMenu";
                    EvidenceLog::Event(
                        "foreign_menu_probe_queued",
                        std::format("id={} command={} menu={}", commandId, command, menuName));
                    QueueIfActive(a_taskInterface, a_gate, [commandId, menuName]() {
                        ProbeForeignMenuRoot(menuName, commandId);
                    });
                    return;
                }
                if (command == "show_probe" || command == "hide_probe") {
                    a_lastCommandId = commandId;
                    EvidenceLog::Event(
                        "research_probe_command_queued",
                        std::format("id={} command={}", commandId, command));
                    const auto type = command == "show_probe" ?
                        RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide;
                    QueueIfActive(a_taskInterface, a_gate,
                        [commandId, command, type]() {
                        Ui::QueueControlPanelMessage(type, command);
                        EvidenceLog::Event(
                            "research_probe_command_dispatched",
                            std::format("id={} command={}", commandId, command));
                    });
                    return;
                }

                WORD scanCode{};
                bool extended = false;
                if (command == "menu_up") scanCode = 0x11;
                else if (command == "nav_down") scanCode = 0x1F;
                else if (command == "nav_left") scanCode = 0x1E;
                else if (command == "nav_right") scanCode = 0x20;
                else if (command == "accept") scanCode = 0x12;
                else if (command == "pause" || command == "probe_escape") scanCode = 0x01;
                else {
                    EvidenceLog::Event(
                        "research_input_rejected",
                        std::format("id={} command={}", commandId, command));
                    a_lastCommandId = commandId;
                    return;
                }
                a_lastCommandId = commandId;
                EvidenceLog::Event(
                    "research_input_queued",
                    std::format("id={} command={}", commandId, command));
                QueueScanCodePulse(
                    a_taskInterface, commandId, command, scanCode, extended, a_gate);
            } catch (...) {
                EvidenceLog::Event("research_input_mailbox_error");
            }
        }

        void StartMailbox(const Config& a_config,
            const std::shared_ptr<Runtime::CallbackGate>& a_gate) noexcept
        {
            const auto taskInterface = SFSE::GetTaskInterface();
            if (!taskInterface) {
                EvidenceLog::Event(
                    "research_input_mailbox_failed", "SFSE task interface unavailable");
                return;
            }
            auto mailboxDirectory = EvidenceLog::Path().parent_path();
            if (const auto logDirectory = SFSE::log::log_directory()) {
                mailboxDirectory = *logDirectory;
            }
            const auto inputPath = mailboxDirectory /
                std::format("AbsoluteControlPanelResearch.{}.input", a_config.runId);
            EvidenceLog::Event("research_input_mailbox_registered", inputPath.string());
            const bool started = Services().mailbox.Start(
                [taskInterface, inputPath, gate = a_gate](std::stop_token a_stopToken,
                    const std::shared_ptr<Runtime::CallbackGate>&) {
                std::uint32_t lastCommandId = 0;
                while (!a_stopToken.stop_requested()) {
                    PollMailbox(taskInterface, inputPath, lastCommandId, gate);
                    if (!Runtime::InterruptibleWait(
                            a_stopToken, std::chrono::milliseconds(100))) {
                        break;
                    }
                }
            });
            if (!started) {
                EvidenceLog::Event(
                    "research_input_mailbox_failed", "worker could not start");
            }
        }

        void StartExperiment(const Config& a_config,
            const std::shared_ptr<Runtime::CallbackGate>& a_gate) noexcept
        {
            if (!a_config.autoOpen) {
                EvidenceLog::Event("auto_open_disabled");
                return;
            }
            const auto taskInterface = SFSE::GetTaskInterface();
            if (!taskInterface) {
                EvidenceLog::Event("schedule_failed", "SFSE task interface unavailable");
                Runtime::Transition(ProbeEvent::RuntimeFault);
                return;
            }
            const auto requestDirectory = EvidenceLog::Path().parent_path();
            const auto armPath = requestDirectory /
                std::format("AbsoluteControlPanelResearch.{}.arm", a_config.runId);
            EvidenceLog::Event(
                "experiment_scheduled",
                std::format(
                    "require_arm={} advance_title={} arm_timeout_ms={} open_delay_ms={} visible_ms={}",
                    a_config.requireArm, a_config.advanceTitleWithSendInput,
                    a_config.armTimeoutMilliseconds, a_config.openDelayMilliseconds,
                    a_config.visibleMilliseconds));

            const bool started = Services().experiment.Start(
                [taskInterface, gate = a_gate, config = a_config, armPath](
                    std::stop_token a_stopToken,
                    const std::shared_ptr<Runtime::CallbackGate>&) {
                if (config.requireArm) {
                    const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(config.armTimeoutMilliseconds);
                    EvidenceLog::Event("experiment_waiting_for_arm", armPath.string());
                    while (!std::filesystem::exists(armPath)) {
                        if (a_stopToken.stop_requested()) {
                            return;
                        }
                        if (std::chrono::steady_clock::now() >= deadline) {
                            EvidenceLog::Event(
                                "experiment_arm_timeout",
                                std::format("timeout_ms={}", config.armTimeoutMilliseconds));
                            Runtime::Transition(ProbeEvent::RuntimeFault);
                            return;
                        }
                        if (!Runtime::InterruptibleWait(
                                a_stopToken, std::chrono::milliseconds(100))) {
                            return;
                        }
                    }
                    EvidenceLog::Event("experiment_armed", armPath.string());
                }

                if (!Runtime::InterruptibleWait(a_stopToken,
                        std::chrono::milliseconds(config.openDelayMilliseconds))) {
                    return;
                }
                QueueIfActive(taskInterface, gate, []() {
                    Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kShow, "experiment");
                });
                if (!Runtime::InterruptibleWait(
                        a_stopToken, std::chrono::milliseconds(1000))) {
                    return;
                }
                QueueIfActive(taskInterface, gate, []() {
                    const auto ui = RE::UI::GetSingleton();
                    if (!ui) {
                        EvidenceLog::Event("menu_ui_state_unavailable");
                        return;
                    }
                    const RE::BSFixedString menuName{ NativeMenuProbe::kMenuName.data() };
                    const auto entry = ui->GetMenuEntry(menuName);
                    const auto menu = entry ? entry->menu.get() : nullptr;
                    EvidenceLog::Event(
                        "menu_ui_state",
                        std::format(
                            "entry={} menu=0x{:X} stack={} array={} advance={} menus_visible={}",
                            entry != nullptr, reinterpret_cast<std::uintptr_t>(menu),
                            ui->menuStack.size(), ui->menuArray.size(),
                            ui->menusToAdvance.size(), ui->IsMenusVisible()));
                });
                if (!Runtime::InterruptibleWait(a_stopToken,
                        std::chrono::milliseconds(config.visibleMilliseconds))) {
                    return;
                }
                QueueIfActive(taskInterface, gate, []() {
                    EvidenceLog::Event("watchdog_fired", "forcing menu hide");
                    Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kHide, "watchdog");
                });
            });
            if (!started) {
                EvidenceLog::Event("schedule_failed", "experiment worker could not start");
                Runtime::Transition(ProbeEvent::RuntimeFault);
            }
        }
    }

    Config LoadConfig(const std::filesystem::path& a_path) noexcept
    {
        Config config;
        std::ifstream stream{ a_path };
        std::string line;
        while (stream && std::getline(stream, line)) {
            const auto trimmed = Trim(line);
            if (trimmed.empty() || trimmed.starts_with('#') || trimmed.starts_with(';') ||
                trimmed.starts_with('[')) {
                continue;
            }
            const auto separator = trimmed.find('=');
            if (separator == std::string_view::npos) {
                continue;
            }
            const auto key = Trim(trimmed.substr(0, separator));
            const auto value = Trim(trimmed.substr(separator + 1));
            if (key == "RunId") config.runId.assign(value);
            else if (key == "AutoOpen") config.autoOpen = ParseBool(value, config.autoOpen);
            else if (key == "RequireArm") config.requireArm = ParseBool(value, config.requireArm);
            else if (key == "AdvanceTitleWithSendInput") {
                config.advanceTitleWithSendInput =
                    ParseBool(value, config.advanceTitleWithSendInput);
            } else if (key == "ArmTimeoutMilliseconds") {
                config.armTimeoutMilliseconds =
                    ParseUnsigned(value, config.armTimeoutMilliseconds);
            } else if (key == "OpenDelayMilliseconds") {
                config.openDelayMilliseconds =
                    ParseUnsigned(value, config.openDelayMilliseconds);
            } else if (key == "VisibleMilliseconds") {
                config.visibleMilliseconds =
                    ParseUnsigned(value, config.visibleMilliseconds);
            }
        }
        config.openDelayMilliseconds =
            std::clamp(config.openDelayMilliseconds, 1000u, 120000u);
        config.armTimeoutMilliseconds =
            std::clamp(config.armTimeoutMilliseconds, 1000u, 600000u);
        config.visibleMilliseconds =
            std::clamp(config.visibleMilliseconds, 2000u, 900000u);
        if (config.runId.empty()) config.runId = "manual";
        return config;
    }

    void StartTitleAdvance(const Config& a_config) noexcept
    {
        if (!a_config.advanceTitleWithSendInput) return;
        Stop();
        const auto taskInterface = SFSE::GetTaskInterface();
        if (!taskInterface) {
            EvidenceLog::Event(
                "title_advance_service_failed", "SFSE task interface unavailable");
            return;
        }

        auto requestDirectory = EvidenceLog::Path().parent_path();
        if (const auto logDirectory = SFSE::log::log_directory()) {
            requestDirectory = *logDirectory;
        }
        const auto advancePath = requestDirectory /
            std::format("AbsoluteControlPanelResearch.{}.advance", a_config.runId);

        auto& services = Services();
        const std::scoped_lock lifecycleLock{ services.lifecycleLock };
        auto gate = std::make_shared<Runtime::CallbackGate>();
        services.taskGate = gate;
        StartScheduler();
        EvidenceLog::Event(
            "title_advance_service_registered", advancePath.string());
        const bool started = services.titleAdvance.Start(
            [taskInterface, gate, config = a_config, advancePath](
                std::stop_token a_stopToken,
                const std::shared_ptr<Runtime::CallbackGate>&) {
                const auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(config.armTimeoutMilliseconds);
                while (!std::filesystem::exists(advancePath)) {
                    if (a_stopToken.stop_requested()) return;
                    if (std::chrono::steady_clock::now() >= deadline) {
                        EvidenceLog::Event(
                            "title_advance_timeout",
                            std::format("timeout_ms={}", config.armTimeoutMilliseconds));
                        return;
                    }
                    if (!Runtime::InterruptibleWait(
                            a_stopToken, std::chrono::milliseconds(100))) return;
                }
                QueueIfActive(taskInterface, gate, [taskInterface, gate]() {
                    ProbeForeignMenuRoot("MainMenu", 0);
                    const auto activeWindow = ::GetActiveWindow();
                    const auto foregroundBefore = ::GetForegroundWindow();
                    const bool focused = activeWindow != nullptr &&
                        ::SetForegroundWindow(activeWindow) != FALSE;
                    constexpr WORD kEnterScanCode = 0x1C;
                    INPUT keyDown{};
                    keyDown.type = INPUT_KEYBOARD;
                    keyDown.ki.wScan = kEnterScanCode;
                    keyDown.ki.dwFlags = KEYEVENTF_SCANCODE;
                    ::SetLastError(ERROR_SUCCESS);
                    const auto sentDown = ::SendInput(1, &keyDown, sizeof(INPUT));
                    const auto error = ::GetLastError();
                    if (sentDown == 1) TrackKeyDown(kEnterScanCode, 0);
                    EvidenceLog::Trace(
                        "title_enter_key_down",
                        std::format(
                            "scan_code=0x{:02X} sent={} error={} active=0x{:X} "
                            "foreground_before=0x{:X} foreground_after=0x{:X} focused={}",
                            kEnterScanCode, sentDown, error,
                            reinterpret_cast<std::uintptr_t>(activeWindow),
                            reinterpret_cast<std::uintptr_t>(foregroundBefore),
                            reinterpret_cast<std::uintptr_t>(::GetForegroundWindow()),
                            focused));
                    const bool releaseScheduled = ScheduleDelayed(
                        std::chrono::milliseconds(100), [taskInterface, gate]() {
                            QueueIfActive(taskInterface, gate, []() {
                                constexpr WORD kEnterScanCode = 0x1C;
                                INPUT keyUp{};
                                keyUp.type = INPUT_KEYBOARD;
                                keyUp.ki.wScan = kEnterScanCode;
                                keyUp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
                                ::SetLastError(ERROR_SUCCESS);
                                const auto sentUp = ::SendInput(1, &keyUp, sizeof(INPUT));
                                const auto error = ::GetLastError();
                                TrackKeyUp(kEnterScanCode);
                                EvidenceLog::Trace(
                                    "title_enter_key_up",
                                    std::format(
                                        "scan_code=0x{:02X} sent={} error={} foreground=0x{:X}",
                                        kEnterScanCode, sentUp, error,
                                        reinterpret_cast<std::uintptr_t>(
                                            ::GetForegroundWindow())));
                            });
                        });
                    if (!releaseScheduled && sentDown == 1) ReleaseActiveKeys();
                });
                EvidenceLog::Event("title_advance_queued", advancePath.string());
            });
        if (!started) {
            EvidenceLog::Event(
                "title_advance_service_failed", "worker could not start");
        }
    }

    void Start(const Config& a_config) noexcept
    {
        Stop();
        auto& services = Services();
        const std::scoped_lock lifecycleLock{ services.lifecycleLock };
        auto gate = std::make_shared<Runtime::CallbackGate>();
        services.taskGate = gate;
        StartScheduler();
        StartMailbox(a_config, gate);
        StartExperiment(a_config, gate);
    }

    void Stop() noexcept
    {
        auto& services = Services();
        const std::scoped_lock lifecycleLock{ services.lifecycleLock };
        const auto gate = std::exchange(services.taskGate, {});
        if (gate) {
            gate->Deactivate();
        }

        services.mailbox.Stop();
        services.experiment.Stop();
        services.titleAdvance.Stop();
        services.scheduler.Stop();
        {
            const std::scoped_lock delayedLock{ services.delayedLock };
            services.delayedJobs.clear();
        }
        if (gate) {
            gate->WaitForIdle();
        }
        // Any synthesized key that reached key-down but whose scheduled key-up
        // was invalidated is released synchronously before Stop returns.
        ReleaseActiveKeys();
    }
}
