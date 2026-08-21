#include "ui/PauseMenuIntegration.h"

#include "EvidenceLog.h"
#include "ui/MenuMessaging.h"

#include <RE/Starfield.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>

namespace AbsoluteControlPanelResearch::Ui::PauseMenuIntegration
{
    namespace
    {
        constexpr std::uint32_t kControlPanelPauseAction = 0x534C4F50;
        constexpr std::string_view kPauseEntryLabel = "MOD OPTIONS";

        class EntryMemberCopyVisitor final :
            public RE::Scaleform::GFx::Value::ObjectVisitor
        {
        public:
            explicit EntryMemberCopyVisitor(RE::Scaleform::GFx::Value& a_target) noexcept :
                target_(a_target)
            {}

            void Visit(
                const char* a_name, const RE::Scaleform::GFx::Value& a_value) override
            {
                if (a_name && *a_name && target_.SetMember(a_name, a_value)) {
                    ++copiedMemberCount_;
                } else {
                    ++rejectedMemberCount_;
                }
            }

            [[nodiscard]] std::uint32_t CopiedMemberCount() const noexcept
            {
                return copiedMemberCount_;
            }

            [[nodiscard]] std::uint32_t RejectedMemberCount() const noexcept
            {
                return rejectedMemberCount_;
            }

        private:
            RE::Scaleform::GFx::Value& target_;
            std::uint32_t copiedMemberCount_{};
            std::uint32_t rejectedMemberCount_{};
        };

        enum class InjectionResult : std::uint8_t
        {
            NotReady,
            Injected,
            AlreadyPresent,
            Failed
        };

        std::atomic_bool g_enablePauseEntry{};
        std::atomic<RE::Scaleform::GFx::Movie*> g_expectedMovie{};
        std::atomic<std::uint32_t> g_cycle{};
        std::atomic<std::uint32_t> g_completedCycle{};
        std::atomic<std::uint32_t> g_captureInstalledCycle{};
        std::atomic<std::uint32_t> g_advanceAttemptCount{};
        std::atomic<std::int32_t> g_lastAdvanceResult{ -1 };
        std::atomic<std::uint32_t> g_requestedCommand{};
        // -1 has no queued origin, 0 is direct/standalone-hotkey, and 1 is PauseMenu. The
        // first queued Show wins until the displayed session claims it, so a
        // duplicate overlapping Show cannot rewrite that session's back stack.
        std::atomic<std::int32_t> g_returnToPauseOnClose{ -1 };

        struct ActiveMenuArraySnapshot
        {
            std::uint32_t count{};
            std::int32_t index{ -1 };
        };

        [[nodiscard]] ActiveMenuArraySnapshot ReadActiveMenuArray(
            RE::UI* a_ui, RE::IMenu* a_menu) noexcept
        {
            ActiveMenuArraySnapshot result;
            if (!a_ui) {
                return result;
            }

            const auto uiBytes = reinterpret_cast<const std::byte*>(a_ui);
            result.count = *reinterpret_cast<const std::uint32_t*>(uiBytes + 0x430);
            const auto entries = *reinterpret_cast<RE::IMenu* const* const*>(
                uiBytes + 0x438);
            if (!entries || !a_menu) {
                return result;
            }
            for (std::uint32_t index = 0; index < result.count; ++index) {
                if (entries[index] == a_menu) {
                    result.index = static_cast<std::int32_t>(index);
                    break;
                }
            }
            return result;
        }

        class PauseEntryCaptureHandler final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                if (a_params.argCount != 1 || !a_params.args[0].IsObject()) {
                    EvidenceLog::Event(
                        "pause_entry_capture_rejected", "reason=invalid_event");
                    return;
                }

                RE::Scaleform::GFx::Value payload;
                RE::Scaleform::GFx::Value action;
                const bool resolved = a_params.args[0].GetMember("params", &payload) &&
                                      payload.IsObject() &&
                                      payload.GetMember("entryAction", &action);
                std::uint32_t actionValue{};
                if (resolved && action.IsUInt()) {
                    actionValue = action.GetUInt();
                } else if (resolved && action.IsInt() && action.GetInt() >= 0) {
                    actionValue = static_cast<std::uint32_t>(action.GetInt());
                } else if (resolved && action.IsNumber() && action.GetNumber() >= 0.0 &&
                           action.GetNumber() <=
                               static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
                    actionValue = static_cast<std::uint32_t>(action.GetNumber());
                } else {
                    return;
                }
                if (actionValue != kControlPanelPauseAction) {
                    return;
                }

                const bool stopped =
                    a_params.args[0].Invoke("stopImmediatePropagation");
                EvidenceLog::Event(
                    "pause_entry_activated",
                    std::format(
                        "action=0x{:08X} propagation_stopped={}",
                        actionValue, stopped));
                // Keep the native PauseMenu resident as the gameplay-pause owner while
                // the higher-priority Control Panel covers it. Hiding it before the
                // panel's Show exposed gameplay for a frame. The custom panel takes
                // its own balanced native PauseMenu audio-mode lease on Show because
                // Starfield's MenuAudioHandler recognizes only hard-coded menu names.
                EvidenceLog::Event(
                    "pause_entry_underlay_retained",
                    "menu=PauseMenu target=AbsoluteControlPanelMenu");
                Ui::QueueControlPanelMessage(
                    RE::UI_MESSAGE_TYPE::kShow, "pause-entry");
            }
        };

        [[nodiscard]] PauseEntryCaptureHandler* GetCaptureHandler()
        {
            static auto handler =
                RE::Scaleform::make_shared<PauseEntryCaptureHandler>();
            return handler.get();
        }

        [[nodiscard]] InjectionResult InjectPauseMenuEntry(
            RE::Scaleform::GFx::Movie* a_movie, std::uint32_t a_commandId,
            std::uint32_t a_cycle) noexcept
        {
            constexpr std::int32_t kMaximumVanillaEntries = 32;
            try {
                if (!a_movie || !a_movie->asMovieRoot) {
                    return InjectionResult::NotReady;
                }

                auto* movieRoot = a_movie->asMovieRoot.get();
                RE::Scaleform::GFx::Value list;
                if (!movieRoot->GetVariable(
                        &list, "_root.Menu_mc.MainPanel_mc.MainList_mc") ||
                    !list.IsObject()) {
                    return InjectionResult::NotReady;
                }

                RE::Scaleform::GFx::Value entryCountValue;
                RE::Scaleform::GFx::Value selectedIndexValue;
                const bool gotEntryCount = list.GetMember("entryCount", &entryCountValue);
                const bool gotSelectedIndex =
                    list.GetMember("selectedIndex", &selectedIndexValue);
                if (!gotEntryCount || !entryCountValue.IsInt() || !gotSelectedIndex ||
                    !selectedIndexValue.IsInt()) {
                    return InjectionResult::NotReady;
                }

                const auto entryCount = entryCountValue.GetInt();
                const auto originalIndex = selectedIndexValue.GetInt();
                if (entryCount <= 0 || entryCount > kMaximumVanillaEntries) {
                    return InjectionResult::NotReady;
                }

                RE::Scaleform::GFx::Value entries;
                RE::Scaleform::GFx::Value nativeEntryTemplate;
                movieRoot->CreateArray(&entries);
                bool alreadyPresent = false;
                bool cloned = entries.IsArray();
                bool gotNativeEntryTemplate = false;
                for (std::int32_t index = 0; cloned && index < entryCount; ++index) {
                    cloned = list.SetMember(
                        "selectedIndex", RE::Scaleform::GFx::Value(index));
                    RE::Scaleform::GFx::Value entry;
                    cloned = cloned && list.GetMember("selectedEntry", &entry) &&
                             entry.IsObject();
                    if (!cloned) {
                        break;
                    }

                    RE::Scaleform::GFx::Value action;
                    if (entry.GetMember("uActionType", &action) && action.IsUInt() &&
                        action.GetUInt() == kControlPanelPauseAction) {
                        alreadyPresent = true;
                    }
                    // Retain the last ordinary row (normally Quit) as the
                    // closest safe style/data template for our appended row.
                    // The shorter label then uses PauseMenu's native text
                    // format and scale instead of forcing a squeezed fallback.
                    nativeEntryTemplate = entry;
                    gotNativeEntryTemplate = true;
                    cloned = entries.PushBack(entry);
                }
                list.SetMember("selectedIndex", RE::Scaleform::GFx::Value(originalIndex));
                if (!cloned) {
                    EvidenceLog::Event(
                        "pause_entry_injection_rejected",
                        std::format(
                            "cycle={} id={} reason=clone_failed", a_cycle, a_commandId));
                    return InjectionResult::Failed;
                }
                if (alreadyPresent) {
                    EvidenceLog::Event(
                        "pause_entry_injection_skipped",
                        std::format(
                            "cycle={} id={} reason=already_present", a_cycle, a_commandId));
                    return InjectionResult::AlreadyPresent;
                }

                if (g_captureInstalledCycle.load(std::memory_order_acquire) != a_cycle) {
                    RE::Scaleform::GFx::Value menuRoot;
                    const bool gotMenuRoot = movieRoot->GetVariable(
                        &menuRoot, "_root.Menu_mc") && menuRoot.IsObject();
                    RE::Scaleform::GFx::Value installedMarker;
                    const bool listenerAlreadyInstalled = gotMenuRoot &&
                        menuRoot.GetMember(
                            "_absoluteControlPanelCaptureInstalled", &installedMarker) &&
                        installedMarker.IsBoolean() && installedMarker.GetBoolean();
                    if (listenerAlreadyInstalled) {
                        g_captureInstalledCycle.store(a_cycle, std::memory_order_release);
                        EvidenceLog::Event(
                            "pause_entry_capture_reused",
                            std::format("cycle={} id={}", a_cycle, a_commandId));
                    }
                    auto callbackType =
                        RE::Scaleform::GFx::Value::ValueType::kUndefined;
                    bool callbackUsable = false;
                    bool listenerInvoked = false;
                    if (gotMenuRoot && !listenerAlreadyInstalled) {
                        RE::Scaleform::GFx::Value eventName;
                        RE::Scaleform::GFx::Value callback;
                        movieRoot->CreateString(&eventName, "MainPanel_EntryPress");
                        movieRoot->CreateFunction(&callback, GetCaptureHandler());
                        callbackType = callback.GetType();
                        std::array<RE::Scaleform::GFx::Value, 5> listenerArgs{
                            eventName, callback, RE::Scaleform::GFx::Value(true),
                            RE::Scaleform::GFx::Value(std::int32_t{ 1000 }),
                            RE::Scaleform::GFx::Value(false)
                        };
                        callbackUsable = callback.IsObject() ||
                            callback.GetType() ==
                                RE::Scaleform::GFx::Value::ValueType::kClosure;
                        listenerInvoked = callbackUsable && menuRoot.Invoke(
                            "addEventListener", nullptr, listenerArgs.data(),
                            listenerArgs.size());
                    }
                    if (!listenerAlreadyInstalled) {
                        const bool listenerInstalled = callbackUsable && listenerInvoked;
                        const bool markerSet = listenerInstalled && menuRoot.SetMember(
                            "_absoluteControlPanelCaptureInstalled",
                            RE::Scaleform::GFx::Value(true));
                        EvidenceLog::Event(
                            "pause_entry_capture_installed",
                            std::format(
                                "cycle={} id={} menu_root={} callback_type={} "
                                "callback_usable={} listener_invoked={} installed={} "
                                "marker_set={}",
                                a_cycle, a_commandId, gotMenuRoot,
                                static_cast<std::uint32_t>(callbackType), callbackUsable,
                                listenerInvoked, listenerInstalled, markerSet));
                        if (!listenerInstalled) {
                            return InjectionResult::Failed;
                        }
                        g_captureInstalledCycle.store(a_cycle, std::memory_order_release);
                    }
                }

                RE::Scaleform::GFx::Value entry;
                RE::Scaleform::GFx::Value label;
                RE::Scaleform::GFx::Value confirmText;
                movieRoot->CreateObject(&entry);
                EntryMemberCopyVisitor templateCopy(entry);
                if (gotNativeEntryTemplate) {
                    nativeEntryTemplate.VisitMembers(&templateCopy);
                }
                movieRoot->CreateString(&label, kPauseEntryLabel.data());
                movieRoot->CreateString(&confirmText, "");
                const bool populated = entry.IsObject() &&
                    entry.SetMember("sActionText", label) &&
                    entry.SetMember("sConfirmText", confirmText) &&
                    entry.SetMember("uActionType", RE::Scaleform::GFx::Value(kControlPanelPauseAction)) &&
                    entry.SetMember("bDisabled", RE::Scaleform::GFx::Value(false)) &&
                    entry.SetMember("bHasDisabledAction", RE::Scaleform::GFx::Value(false)) &&
                    entry.SetMember("bHasNotification", RE::Scaleform::GFx::Value(false)) &&
                    entry.SetMember("bShowSpinner", RE::Scaleform::GFx::Value(false)) &&
                    entries.PushBack(entry);
                const bool invoked = populated &&
                    list.Invoke("InitializeEntries", nullptr, &entries, 1);
                const bool restored = list.SetMember(
                    "selectedIndex", RE::Scaleform::GFx::Value(originalIndex));
                EvidenceLog::Event(
                    invoked ? "pause_entry_injected" : "pause_entry_injection_rejected",
                    std::format(
                        "cycle={} id={} vanilla_count={} requested_count={} action=0x{:08X} "
                        "template={} members_copied={} members_rejected={} label={} "
                        "populated={} invoked={} selection_restored={}",
                        a_cycle, a_commandId, entryCount, entryCount + 1,
                        kControlPanelPauseAction, gotNativeEntryTemplate,
                        templateCopy.CopiedMemberCount(),
                        templateCopy.RejectedMemberCount(), kPauseEntryLabel,
                        populated, invoked, restored));
                return invoked ? InjectionResult::Injected : InjectionResult::Failed;
            } catch (...) {
                EvidenceLog::Event(
                    "pause_entry_injection_error",
                    std::format("cycle={} id={}", a_cycle, a_commandId));
                return InjectionResult::Failed;
            }
        }

        class PauseEntryAdvanceHandler final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                constexpr std::uint32_t kMaximumAdvanceAttempts = 180;
                const auto cycle = g_cycle.load(std::memory_order_acquire);
                const auto expectedMovie = g_expectedMovie.load(std::memory_order_acquire);
                const auto requestedCommand =
                    g_requestedCommand.exchange(0, std::memory_order_acq_rel);
                if (!cycle || !a_params.movie || a_params.movie != expectedMovie) {
                    return;
                }
                if (!requestedCommand &&
                    g_completedCycle.load(std::memory_order_acquire) == cycle) {
                    return;
                }
                if (requestedCommand) {
                    g_completedCycle.store(0, std::memory_order_release);
                    g_advanceAttemptCount.store(0, std::memory_order_release);
                    g_lastAdvanceResult.store(-1, std::memory_order_release);
                }

                const auto attempt =
                    g_advanceAttemptCount.fetch_add(1, std::memory_order_acq_rel) + 1;
                const auto commandId = requestedCommand ? requestedCommand : 1000000 + cycle;
                const auto result = InjectPauseMenuEntry(a_params.movie, commandId, cycle);
                const auto resultValue = static_cast<std::int32_t>(result);
                const auto priorResult =
                    g_lastAdvanceResult.exchange(resultValue, std::memory_order_acq_rel);
                if (attempt == 1 || priorResult != resultValue) {
                    EvidenceLog::Event(
                        "pause_entry_advance_tick",
                        std::format(
                            "cycle={} attempt={} result={} movie=0x{:X}", cycle,
                            attempt, resultValue,
                            reinterpret_cast<std::uintptr_t>(a_params.movie)));
                }

                if (result == InjectionResult::Injected ||
                    result == InjectionResult::AlreadyPresent) {
                    g_completedCycle.store(cycle, std::memory_order_release);
                    EvidenceLog::Event(
                        "pause_entry_advance_completed",
                        std::format("cycle={} attempts={}", cycle, attempt));
                } else if (attempt >= kMaximumAdvanceAttempts) {
                    g_completedCycle.store(cycle, std::memory_order_release);
                    EvidenceLog::Event(
                        "pause_entry_advance_timeout",
                        std::format(
                            "cycle={} attempts={} last_result={}", cycle, attempt,
                            resultValue));
                }
            }
        };

        [[nodiscard]] PauseEntryAdvanceHandler* GetAdvanceHandler()
        {
            static auto handler =
                RE::Scaleform::make_shared<PauseEntryAdvanceHandler>();
            return handler.get();
        }

        void OnPauseMenuInserted(RE::IMenu* a_menu) noexcept
        {
            auto* movie = a_menu && a_menu->uiMovie ? a_menu->uiMovie.get() : nullptr;
            if (!movie || !movie->asMovieRoot) {
                EvidenceLog::Event(
                    "pause_entry_boundary_rejected", "reason=movie_unavailable");
                return;
            }

            const auto cycle = g_cycle.fetch_add(1, std::memory_order_acq_rel) + 1;
            g_expectedMovie.store(movie, std::memory_order_release);
            g_completedCycle.store(0, std::memory_order_release);
            g_captureInstalledCycle.store(0, std::memory_order_release);
            g_advanceAttemptCount.store(0, std::memory_order_release);
            g_lastAdvanceResult.store(-1, std::memory_order_release);
            EvidenceLog::Event(
                "pause_entry_boundary_reached",
                std::format(
                    "cycle={} menu=0x{:X} movie=0x{:X}", cycle,
                    reinterpret_cast<std::uintptr_t>(a_menu),
                    reinterpret_cast<std::uintptr_t>(movie)));

            auto* movieRoot = movie->asMovieRoot.get();
            RE::Scaleform::GFx::Value eventTarget;
            const bool menuTarget = movieRoot->GetVariable(
                &eventTarget, "_root.Menu_mc") && eventTarget.IsObject();
            const bool rootTarget = menuTarget ||
                (movieRoot->GetVariable(&eventTarget, "_root") && eventTarget.IsObject());
            RE::Scaleform::GFx::Value installedMarker;
            const bool listenerAlreadyInstalled = rootTarget && eventTarget.GetMember(
                "_absoluteControlPanelAdvanceInstalled", &installedMarker) &&
                installedMarker.IsBoolean() && installedMarker.GetBoolean();
            if (listenerAlreadyInstalled) {
                EvidenceLog::Event(
                    "pause_entry_advance_listener_reused",
                    std::format(
                        "cycle={} target={} movie=0x{:X}", cycle,
                        menuTarget ? "menu" : "root",
                        reinterpret_cast<std::uintptr_t>(movie)));
                return;
            }
            RE::Scaleform::GFx::Value eventName;
            RE::Scaleform::GFx::Value callback;
            movieRoot->CreateString(&eventName, "enterFrame");
            movieRoot->CreateFunction(&callback, GetAdvanceHandler());
            const bool callbackUsable = callback.IsObject() ||
                callback.GetType() == RE::Scaleform::GFx::Value::ValueType::kClosure;
            std::array<RE::Scaleform::GFx::Value, 5> listenerArgs{
                eventName, callback, RE::Scaleform::GFx::Value(false),
                RE::Scaleform::GFx::Value(std::int32_t{ 1000 }),
                RE::Scaleform::GFx::Value(false)
            };
            const bool invoked = rootTarget && callbackUsable && eventTarget.Invoke(
                "addEventListener", nullptr, listenerArgs.data(), listenerArgs.size());
            const bool markerSet = invoked && eventTarget.SetMember(
                "_absoluteControlPanelAdvanceInstalled", RE::Scaleform::GFx::Value(true));
            EvidenceLog::Event(
                invoked ? "pause_entry_advance_listener_installed" :
                          "pause_entry_advance_listener_rejected",
                std::format(
                    "cycle={} target={} callback_type={} callback_usable={} invoked={} "
                    "marker_set={}",
                    cycle, menuTarget ? "menu" : (rootTarget ? "root" : "none"),
                    static_cast<std::uint32_t>(callback.GetType()), callbackUsable,
                    invoked, markerSet));
            if (!invoked) {
                g_completedCycle.store(cycle, std::memory_order_release);
            }
        }

        struct ActiveMenuInsertHook
        {
            using func_t = void (*)(RE::UI*, RE::IMenu**);
            static inline func_t original{};

            static void Thunk(RE::UI* a_ui, RE::IMenu** a_menuSlot) noexcept
            {
                auto* menu = a_menuSlot ? *a_menuSlot : nullptr;
                const auto before = ReadActiveMenuArray(a_ui, menu);
                const auto name = menu ? menu->GetName() : "<null>";
                const bool isPauseMenu =
                    std::string_view{ name ? name : "" } == "PauseMenu";
                const auto flags = menu ? menu->flags : 0;
                const auto priority = menu ? *(
                    reinterpret_cast<const std::uint8_t*>(menu) + 0x110) : 0;
                EvidenceLog::Event(
                    "active_menu_insert_entered",
                    std::format(
                        "name={} menu=0x{:X} count={} index={} priority={} flags=0x{:08X}",
                        name ? name : "<null>", reinterpret_cast<std::uintptr_t>(menu),
                        before.count, before.index, priority, flags));

                original(a_ui, a_menuSlot);

                menu = a_menuSlot ? *a_menuSlot : menu;
                const auto after = ReadActiveMenuArray(a_ui, menu);
                EvidenceLog::Event(
                    "active_menu_insert_completed",
                    std::format(
                        "name={} menu=0x{:X} count={} index={}", name ? name : "<null>",
                        reinterpret_cast<std::uintptr_t>(menu), after.count, after.index));

                if (isPauseMenu &&
                    g_enablePauseEntry.load(std::memory_order_acquire)) {
                    OnPauseMenuInserted(menu);
                }
            }
        };
    }

    bool InstallLifecycleHook(bool a_enablePauseEntry) noexcept
    {
        g_enablePauseEntry.store(a_enablePauseEntry, std::memory_order_release);
        constexpr std::uintptr_t kCallsiteRva = 0x0254181C;
        constexpr std::uintptr_t kExpectedTargetRva = 0x0253EF10;
        const auto imageBase = REX::FModule::GetExecutingModule().GetBaseAddress();
        const auto callsite = imageBase + kCallsiteRva;
        if (*reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8) {
            EvidenceLog::Event(
                "lifecycle_trace_rejected", "successful-show callsite is not rel32 call");
            return false;
        }
        const auto displacement = *reinterpret_cast<const std::int32_t*>(callsite + 1);
        const auto target = callsite + 5 + displacement;
        if (target != imageBase + kExpectedTargetRva) {
            EvidenceLog::Event(
                "lifecycle_trace_rejected",
                std::format(
                    "actual_target_rva=0x{:08X} expected_target_rva=0x{:08X}",
                    target - imageBase, kExpectedTargetRva));
            return false;
        }

        ActiveMenuInsertHook::original = reinterpret_cast<ActiveMenuInsertHook::func_t>(
            REL::GetTrampoline().write_call<5>(callsite, &ActiveMenuInsertHook::Thunk));
        EvidenceLog::Event(
            "lifecycle_trace_installed",
            std::format(
                "callsite_rva=0x{:08X} target_rva=0x{:08X}", kCallsiteRva,
                kExpectedTargetRva));
        return true;
    }

    void SetEntryEnabled(bool a_enabled) noexcept
    {
        g_enablePauseEntry.store(a_enabled, std::memory_order_release);
        EvidenceLog::Event("pause_entry_setting_changed",
            std::format("enabled={}", a_enabled));
    }

    bool IsEntryEnabled() noexcept
    {
        return g_enablePauseEntry.load(std::memory_order_acquire);
    }

    void LogRegistration(bool a_enablePauseEntry) noexcept
    {
        if (!a_enablePauseEntry) {
            EvidenceLog::Event("pause_entry_integration_disabled");
            return;
        }
        EvidenceLog::Event(
            "pause_entry_integration_registered",
            "mode=active-menu-boundary+scaleform-advance fail_closed=true recovery_hotkey=opt-in");
    }

    void RequestInjection(std::uint32_t a_commandId) noexcept
    {
        g_requestedCommand.store(a_commandId, std::memory_order_release);
    }

    bool SetReturnToPauseOnClose(bool a_enabled) noexcept
    {
        std::int32_t expected = -1;
        return g_returnToPauseOnClose.compare_exchange_strong(
            expected, a_enabled ? 1 : 0, std::memory_order_acq_rel);
    }

    bool ConsumeReturnToPauseOnClose() noexcept
    {
        return g_returnToPauseOnClose.exchange(-1, std::memory_order_acq_rel) == 1;
    }
}
