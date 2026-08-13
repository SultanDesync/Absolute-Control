#include "NativeMenuProbe.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
#include "MenuInputRouter.h"
#include "MenuSession.h"
#include "ProbeConfig.h"
#include "ResearchModule.h"

namespace AbsoluteControlPanelResearch::NativeMenuProbe
{
    namespace
    {
        constexpr std::array<std::string_view, 7> kRootCandidates{
            "_root",
            "root",
            "root1",
            "AbsoluteControlPanelMenu",
            "root.AbsoluteControlPanelMenu",
            "root1.AbsoluteControlPanelMenu",
            "root1.AbsoluteControlPanelMenu_mc"
        };

        constexpr REL::Version kSupportedRuntime{ 1, 16, 244, 0 };
        // Starfield publishes vertical wheel notches as direction-specific mouse
        // ButtonEvents: 0x800/+120 for up and 0x900/-120 for down. These are
        // distinct from SFSE's normalized binding key-code range.
        constexpr std::int32_t kMouseWheelUpIdCode = 0x800;
        constexpr std::int32_t kMouseWheelDownIdCode = 0x900;

        struct VerifiedRelocation
        {
            std::string_view symbol;
            std::uint64_t id;
            std::uint64_t expectedOffset;
        };

        constexpr std::array kVerifiedRelocations{
            VerifiedRelocation{ "GameMenuBase::ctor", 130615, 0x025516B0 },
            VerifiedRelocation{ "GameMenuBase::Unk10", 93620, 0x01667080 },
            VerifiedRelocation{ "GameMenuBase::Unk11", 93621, 0x016670C0 },
            VerifiedRelocation{ "IMenu::dtor", 130617, 0x025518A0 },
            VerifiedRelocation{ "IMenu::ShouldHandleEvent", 91901, 0x02553390 },
            VerifiedRelocation{ "IMenu::OnThumbstickEvent", 130633, 0x02553670 },
            VerifiedRelocation{ "IMenu::OnButtonEvent", 130632, 0x025533D0 },
            VerifiedRelocation{ "IMenu::LoadMovie", 130618, 0x02551AB0 },
            VerifiedRelocation{ "IMenu::ProcessMessage", 130624, 0x02552070 },
            VerifiedRelocation{ "IMenu::Unk09", 42815, 0x00481670 },
            VerifiedRelocation{ "IMenu::Unk0E", 130622, 0x02551D70 },
            VerifiedRelocation{ "IMenu::Unk12", 42816, 0x00481680 },
            VerifiedRelocation{ "IMenu::Unk13", 39540, 0x003AE910 },
            VerifiedRelocation{ "IMenu::Unk19", 130634, 0x02553940 },
            VerifiedRelocation{ "UI::IsMenuOpen", 130475, 0x02544EC0 }
        };

        enum class NativeFunction : std::uintptr_t
        {
            Ready,
            Close,
            Dispatch,
            Focus,
            ModelApplied
        };

        enum class PointerPhase : std::uint8_t
        {
            Down,
            Move,
            Up
        };

        std::atomic<ProbePhase> g_phase{ ProbePhase::Cold };
        ProbeConfig g_config;

        void OnPauseMenuInserted(RE::IMenu* a_menu) noexcept;

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

                if (isPauseMenu && g_config.enablePauseMenuEntry) {
                    OnPauseMenuInserted(menu);
                }
            }
        };

        bool InstallLifecycleTraceHook() noexcept
        {
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

        [[nodiscard]] std::uintptr_t ToImageRva(const void* a_address) noexcept
        {
            const auto caller = reinterpret_cast<std::uintptr_t>(a_address);
            const auto imageBase =
                REX::FModule::GetExecutingModule().GetBaseAddress();
            return caller >= imageBase ? caller - imageBase : 0;
        }

        bool ValidateMenuRelocations() noexcept
        {
            const auto runtime = REX::FModule::GetExecutingModule().GetFileVersion();
            if (runtime != kSupportedRuntime) {
                EvidenceLog::Event(
                    "relocation_validation_failed",
                    std::format(
                        "unsupported_runtime={} expected={}", runtime, kSupportedRuntime));
                return false;
            }

            const auto database = REL::IDDB::GetSingleton();
            for (const auto& mapping : kVerifiedRelocations) {
                const auto actualOffset = database->offset(mapping.id);
                if (actualOffset != mapping.expectedOffset) {
                    EvidenceLog::Event(
                        "relocation_validation_failed",
                        std::format(
                            "symbol={} id={} actual=0x{:08X} expected=0x{:08X}",
                            mapping.symbol, mapping.id, actualOffset, mapping.expectedOffset));
                    return false;
                }
            }

            EvidenceLog::Event(
                "relocation_validation_succeeded",
                std::format(
                    "runtime={} mapping_count={}", runtime, kVerifiedRelocations.size()));
            return true;
        }

        void Transition(ProbeEvent a_event) noexcept
        {
            auto current = g_phase.load(std::memory_order_acquire);
            while (!g_phase.compare_exchange_weak(
                current, Advance(current, a_event), std::memory_order_acq_rel)) {
            }
        }

        void QueueNamedMenuMessage(std::string_view a_menuName,
            RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept
        {
            const auto queue = RE::UIMessageQueue::GetSingleton();
            if (!queue) {
                EvidenceLog::Event("menu_message_rejected", "UIMessageQueue unavailable");
                Transition(ProbeEvent::RuntimeFault);
                return;
            }

            const auto result =
                queue->AddMessage(RE::BSFixedString(a_menuName.data()), a_type);
            EvidenceLog::Event(
                "named_menu_message_requested",
                std::format(
                    "menu={} type={} source={} result={}", a_menuName,
                    static_cast<std::uint32_t>(a_type), a_source, result));
        }

        void QueueMenuMessage(RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept
        {
            QueueNamedMenuMessage(kMenuName, a_type, a_source);
            EvidenceLog::Event(
                a_type == RE::UI_MESSAGE_TYPE::kShow ? "menu_show_requested" :
                                                       "menu_hide_requested",
                std::format("source={}", a_source));
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

        void ProbeForeignMenuRoot(std::string_view a_menuName,
            std::uint32_t a_commandId) noexcept
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

        class PauseEntryCaptureHandler final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                constexpr std::uint32_t kSlopPauseAction = 0x534C4F50;
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
                if (actionValue != kSlopPauseAction) {
                    return;
                }

                const bool stopped =
                    a_params.args[0].Invoke("stopImmediatePropagation");
                EvidenceLog::Event(
                    "pause_entry_activated",
                    std::format(
                        "action=0x{:08X} propagation_stopped={}",
                        actionValue, stopped));
                QueueNamedMenuMessage(
                    "PauseMenu", RE::UI_MESSAGE_TYPE::kHide, "slop-pause-entry");
                QueueMenuMessage(RE::UI_MESSAGE_TYPE::kShow, "pause-entry");
            }
        };

        [[nodiscard]] PauseEntryCaptureHandler* GetPauseEntryCaptureHandler()
        {
            static auto handler =
                RE::Scaleform::make_shared<PauseEntryCaptureHandler>();
            return handler.get();
        }

        enum class PauseEntryInjectionResult : std::uint8_t
        {
            NotReady,
            Injected,
            AlreadyPresent,
            Failed
        };

        std::atomic<RE::Scaleform::GFx::Movie*> g_pauseExpectedMovie{};
        std::atomic<std::uint32_t> g_pauseCycle{};
        std::atomic<std::uint32_t> g_pauseCompletedCycle{};
        std::atomic<std::uint32_t> g_pauseCaptureInstalledCycle{};
        std::atomic<std::uint32_t> g_pauseAdvanceAttemptCount{};
        std::atomic<std::int32_t> g_pauseLastAdvanceResult{ -1 };
        std::atomic<std::uint32_t> g_pauseRequestedCommand{};

        [[nodiscard]] PauseEntryInjectionResult InjectPauseMenuEntry(
            RE::Scaleform::GFx::Movie* a_movie, std::uint32_t a_commandId,
            std::uint32_t a_cycle) noexcept
        {
            constexpr std::uint32_t kSlopPauseAction = 0x534C4F50;  // "SLOP"
            constexpr std::int32_t kMaximumVanillaEntries = 32;
            try {
                if (!a_movie || !a_movie->asMovieRoot) {
                    return PauseEntryInjectionResult::NotReady;
                }

                auto* movieRoot = a_movie->asMovieRoot.get();
                RE::Scaleform::GFx::Value list;
                if (!movieRoot->GetVariable(
                        &list, "_root.Menu_mc.MainPanel_mc.MainList_mc") ||
                    !list.IsObject()) {
                    return PauseEntryInjectionResult::NotReady;
                }

                RE::Scaleform::GFx::Value entryCountValue;
                RE::Scaleform::GFx::Value selectedIndexValue;
                const bool gotEntryCount = list.GetMember("entryCount", &entryCountValue);
                const bool gotSelectedIndex =
                    list.GetMember("selectedIndex", &selectedIndexValue);
                if (!gotEntryCount || !entryCountValue.IsInt() || !gotSelectedIndex ||
                    !selectedIndexValue.IsInt()) {
                    return PauseEntryInjectionResult::NotReady;
                }

                const auto entryCount = entryCountValue.GetInt();
                const auto originalIndex = selectedIndexValue.GetInt();
                if (entryCount <= 0 || entryCount > kMaximumVanillaEntries) {
                    return PauseEntryInjectionResult::NotReady;
                }

                RE::Scaleform::GFx::Value entries;
                movieRoot->CreateArray(&entries);
                bool alreadyPresent = false;
                bool cloned = entries.IsArray();
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
                        action.GetUInt() == kSlopPauseAction) {
                        alreadyPresent = true;
                    }
                    cloned = entries.PushBack(entry);
                }
                list.SetMember(
                    "selectedIndex", RE::Scaleform::GFx::Value(originalIndex));
                if (!cloned) {
                    EvidenceLog::Event(
                        "pause_entry_injection_rejected",
                        std::format(
                            "cycle={} id={} reason=clone_failed", a_cycle, a_commandId));
                    return PauseEntryInjectionResult::Failed;
                }
                if (alreadyPresent) {
                    EvidenceLog::Event(
                        "pause_entry_injection_skipped",
                        std::format(
                            "cycle={} id={} reason=already_present", a_cycle, a_commandId));
                    return PauseEntryInjectionResult::AlreadyPresent;
                }

                if (g_pauseCaptureInstalledCycle.load(std::memory_order_acquire) !=
                    a_cycle) {
                    RE::Scaleform::GFx::Value menuRoot;
                    const bool gotMenuRoot = movieRoot->GetVariable(
                        &menuRoot, "_root.Menu_mc") && menuRoot.IsObject();
                    RE::Scaleform::GFx::Value installedMarker;
                    const bool listenerAlreadyInstalled = gotMenuRoot &&
                        menuRoot.GetMember(
                            "_absoluteControlPanelCaptureInstalled",
                            &installedMarker) &&
                        installedMarker.IsBoolean() && installedMarker.GetBoolean();
                    if (listenerAlreadyInstalled) {
                        g_pauseCaptureInstalledCycle.store(
                            a_cycle, std::memory_order_release);
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
                        movieRoot->CreateFunction(
                            &callback, GetPauseEntryCaptureHandler());
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
                        const bool listenerInstalled =
                            callbackUsable && listenerInvoked;
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
                            return PauseEntryInjectionResult::Failed;
                        }
                        g_pauseCaptureInstalledCycle.store(
                            a_cycle, std::memory_order_release);
                    }
                }

                RE::Scaleform::GFx::Value entry;
                RE::Scaleform::GFx::Value label;
                RE::Scaleform::GFx::Value confirmText;
                movieRoot->CreateObject(&entry);
                movieRoot->CreateString(&label, "ABSOLUTE CONTROL PANEL");
                movieRoot->CreateString(&confirmText, "");
                const bool populated = entry.IsObject() &&
                    entry.SetMember("sActionText", label) &&
                    entry.SetMember("sConfirmText", confirmText) &&
                    entry.SetMember("uActionType", RE::Scaleform::GFx::Value(kSlopPauseAction)) &&
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
                        "populated={} invoked={} selection_restored={}",
                        a_cycle, a_commandId, entryCount, entryCount + 1,
                        kSlopPauseAction, populated, invoked, restored));
                return invoked ? PauseEntryInjectionResult::Injected :
                                 PauseEntryInjectionResult::Failed;
            } catch (...) {
                EvidenceLog::Event(
                    "pause_entry_injection_error",
                    std::format("cycle={} id={}", a_cycle, a_commandId));
                return PauseEntryInjectionResult::Failed;
            }
        }

        class PauseEntryAdvanceHandler final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                constexpr std::uint32_t kMaximumAdvanceAttempts = 180;
                const auto cycle = g_pauseCycle.load(std::memory_order_acquire);
                const auto expectedMovie =
                    g_pauseExpectedMovie.load(std::memory_order_acquire);
                const auto requestedCommand =
                    g_pauseRequestedCommand.exchange(0, std::memory_order_acq_rel);
                if (!cycle || !a_params.movie || a_params.movie != expectedMovie) {
                    return;
                }
                if (!requestedCommand &&
                    g_pauseCompletedCycle.load(std::memory_order_acquire) == cycle) {
                    return;
                }
                if (requestedCommand) {
                    g_pauseCompletedCycle.store(0, std::memory_order_release);
                    g_pauseAdvanceAttemptCount.store(0, std::memory_order_release);
                    g_pauseLastAdvanceResult.store(-1, std::memory_order_release);
                }

                const auto attempt =
                    g_pauseAdvanceAttemptCount.fetch_add(1, std::memory_order_acq_rel) + 1;
                const auto commandId = requestedCommand ? requestedCommand : 1000000 + cycle;
                const auto result =
                    InjectPauseMenuEntry(a_params.movie, commandId, cycle);
                const auto resultValue = static_cast<std::int32_t>(result);
                const auto priorResult =
                    g_pauseLastAdvanceResult.exchange(resultValue, std::memory_order_acq_rel);
                if (attempt == 1 || priorResult != resultValue) {
                    EvidenceLog::Event(
                        "pause_entry_advance_tick",
                        std::format(
                            "cycle={} attempt={} result={} movie=0x{:X}", cycle,
                            attempt, resultValue,
                            reinterpret_cast<std::uintptr_t>(a_params.movie)));
                }

                if (result == PauseEntryInjectionResult::Injected ||
                    result == PauseEntryInjectionResult::AlreadyPresent) {
                    g_pauseCompletedCycle.store(cycle, std::memory_order_release);
                    EvidenceLog::Event(
                        "pause_entry_advance_completed",
                        std::format("cycle={} attempts={}", cycle, attempt));
                } else if (attempt >= kMaximumAdvanceAttempts) {
                    g_pauseCompletedCycle.store(cycle, std::memory_order_release);
                    EvidenceLog::Event(
                        "pause_entry_advance_timeout",
                        std::format(
                            "cycle={} attempts={} last_result={}", cycle, attempt,
                            resultValue));
                }
            }
        };

        [[nodiscard]] PauseEntryAdvanceHandler* GetPauseEntryAdvanceHandler()
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

            const auto cycle =
                g_pauseCycle.fetch_add(1, std::memory_order_acq_rel) + 1;
            g_pauseExpectedMovie.store(movie, std::memory_order_release);
            g_pauseCompletedCycle.store(0, std::memory_order_release);
            g_pauseCaptureInstalledCycle.store(0, std::memory_order_release);
            g_pauseAdvanceAttemptCount.store(0, std::memory_order_release);
            g_pauseLastAdvanceResult.store(-1, std::memory_order_release);
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
                (movieRoot->GetVariable(&eventTarget, "_root") &&
                    eventTarget.IsObject());
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
            movieRoot->CreateFunction(&callback, GetPauseEntryAdvanceHandler());
            const bool callbackUsable = callback.IsObject() ||
                callback.GetType() ==
                    RE::Scaleform::GFx::Value::ValueType::kClosure;
            std::array<RE::Scaleform::GFx::Value, 5> listenerArgs{
                eventName, callback, RE::Scaleform::GFx::Value(false),
                RE::Scaleform::GFx::Value(std::int32_t{ 1000 }),
                RE::Scaleform::GFx::Value(false)
            };
            const bool invoked = rootTarget && callbackUsable && eventTarget.Invoke(
                    "addEventListener", nullptr, listenerArgs.data(),
                    listenerArgs.size());
            const bool markerSet = invoked && eventTarget.SetMember(
                "_absoluteControlPanelAdvanceInstalled",
                RE::Scaleform::GFx::Value(true));
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
                g_pauseCompletedCycle.store(cycle, std::memory_order_release);
            }
        }

        void QueueScanCodePulse(
            const SFSE::TaskInterface* a_taskInterface, std::uint32_t a_commandId,
            std::string a_command, WORD a_scanCode, bool a_extended) noexcept
        {
            a_taskInterface->AddTask([
                                         a_taskInterface, a_commandId,
                                         command = std::move(a_command), a_scanCode,
                                         a_extended]() {
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
                EvidenceLog::Event(
                    "research_input_key_down",
                    std::format(
                        "id={} command={} scan_code=0x{:02X} extended={} sent={} error={} "
                        "active=0x{:X} foreground_before=0x{:X} foreground_after=0x{:X} "
                        "focused={}",
                        a_commandId, command, a_scanCode, a_extended, sent, error,
                        reinterpret_cast<std::uintptr_t>(activeWindow),
                        reinterpret_cast<std::uintptr_t>(foregroundBefore),
                        reinterpret_cast<std::uintptr_t>(::GetForegroundWindow()),
                        focused));

                std::thread([
                                a_taskInterface, a_commandId, command, a_scanCode,
                                a_extended]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    a_taskInterface->AddTask([
                                                 a_commandId, command, a_scanCode,
                                                 a_extended]() {
                        INPUT keyUp{};
                        keyUp.type = INPUT_KEYBOARD;
                        keyUp.ki.wScan = a_scanCode;
                        keyUp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP |
                                         (a_extended ? KEYEVENTF_EXTENDEDKEY : 0);
                        ::SetLastError(ERROR_SUCCESS);
                        const auto sent = ::SendInput(1, &keyUp, sizeof(INPUT));
                        const auto error = ::GetLastError();
                        EvidenceLog::Event(
                            "research_input_key_up",
                            std::format(
                                "id={} command={} scan_code=0x{:02X} extended={} sent={} "
                                "error={}",
                                a_commandId, command, a_scanCode, a_extended, sent,
                                error));
                    });
                    if (command == "pause") {
                        std::this_thread::sleep_for(std::chrono::milliseconds(750));
                        a_taskInterface->AddTask([a_commandId]() {
                            const auto ui = RE::UI::GetSingleton();
                            const bool open = ui && ui->IsMenuOpen(
                                                        RE::BSFixedString("PauseMenu"));
                            EvidenceLog::Event(
                                "research_pause_state",
                                std::format("id={} open={}", a_commandId, open));
                            if (open) {
                                ProbeForeignMenuRoot("PauseMenu", a_commandId);
                            }
                        });
                    }
                }).detach();
            });
        }

        void PollResearchInputMailbox(
            const SFSE::TaskInterface* a_taskInterface,
            const std::filesystem::path& a_inputPath, std::uint32_t& a_lastCommandId) noexcept
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
                    g_pauseRequestedCommand.store(commandId, std::memory_order_release);
                    return;
                }
                if (command == "probe_pause_root" || command == "probe_main_root") {
                    a_lastCommandId = commandId;
                    const std::string menuName = command == "probe_pause_root" ?
                        "PauseMenu" : "MainMenu";
                    EvidenceLog::Event(
                        "foreign_menu_probe_queued",
                        std::format(
                            "id={} command={} menu={}", commandId, command, menuName));
                    a_taskInterface->AddTask([commandId, menuName]() {
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
                                          RE::UI_MESSAGE_TYPE::kShow :
                                          RE::UI_MESSAGE_TYPE::kHide;
                    a_taskInterface->AddTask([commandId, command, type]() {
                        QueueMenuMessage(type, command);
                        EvidenceLog::Event(
                            "research_probe_command_dispatched",
                            std::format("id={} command={}", commandId, command));
                    });
                    return;
                }

                WORD scanCode{};
                bool extended = false;
                if (command == "menu_up") {
                    scanCode = 0x11;  // W
                } else if (command == "nav_down") {
                    scanCode = 0x1F;  // S
                } else if (command == "nav_left") {
                    scanCode = 0x1E;  // A
                } else if (command == "nav_right") {
                    scanCode = 0x20;  // D
                } else if (command == "accept") {
                    scanCode = 0x12;  // E
                } else if (command == "pause" || command == "probe_escape") {
                    scanCode = 0x01;
                } else {
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
                    a_taskInterface, commandId, command, scanCode, extended);
            } catch (...) {
                EvidenceLog::Event("research_input_mailbox_error");
            }
        }

        class AbsoluteControlPanelMenu final : public RE::GameMenuBase
        {
        public:
            SF_MENU_NAME("AbsoluteControlPanelMenu");

            static void* operator new(std::size_t a_count)
            {
                EvidenceLog::Event(
                    "menu_allocation_started",
                    std::format("bytes={} allocator=global", a_count));
                auto* memory = ::operator new(a_count);
                EvidenceLog::Event(
                    "menu_allocation_completed",
                    std::format(
                        "address=0x{:X} allocator=global",
                        reinterpret_cast<std::uintptr_t>(memory)));
                return memory;
            }

            static void operator delete(void* a_memory) noexcept
            {
                if (!a_memory) {
                    return;
                }
                EvidenceLog::Event(
                    "menu_deallocation_started",
                    std::format(
                        "address=0x{:X} allocator=global",
                        reinterpret_cast<std::uintptr_t>(a_memory)));
                ::operator delete(a_memory);
            }

            static void operator delete(void* a_memory, std::size_t) noexcept
            {
                operator delete(a_memory);
            }

            AbsoluteControlPanelMenu()
            {
                // Current shipped GameMenuBase-derived constructors explicitly clear
                // the tail byte at +0x130 after the base constructor.  CommonLibSF
                // exposes that storage but does not initialize it in C++.
                unk128 = 0;
                unk130 = 0;
                menuName = kMenuName.data();
                SetFlags(g_config.menuFlags);
                // Current UI insertion sorts by the byte at +0x110.  GameMenuBase starts at
                // priority 6.  The probe is now opened only after PauseMenu is closed, but it
                // still stays below CursorMenu (20) for mouse input.
                *(
                    reinterpret_cast<std::uint8_t*>(this) + 0x110) = 19;
                EvidenceLog::Event(
                    "menu_constructed",
                    std::format(
                        "flags=0x{:08X} priority={} ref_count={}", g_config.menuFlags,
                        *(reinterpret_cast<std::uint8_t*>(this) + 0x110),
                        RefCountForEvidence()));
            }

            ~AbsoluteControlPanelMenu() override
            {
                MenuApiHost::SetMenuOpen(false);
                EvidenceLog::Event(
                    "menu_destructor_entered",
                    std::format(
                        "ref_count={} caller_rva=0x{:08X}", RefCountForEvidence(),
                        ToImageRva(_ReturnAddress())));
            }

            const char* GetName() const override
            {
                EvidenceLog::Event("menu_get_name", kMenuName);
                return kMenuName.data();
            }
            const char* GetRootPath() const override
            {
                EvidenceLog::Event("menu_get_root_path", kRootPath);
                return kRootPath.data();
            }
            std::uint64_t GetUnk05() override
            {
                // Current LoadingMenu, ContainerMenu, and MessageBoxMenu all share
                // ID 36396 in this slot, whose complete body is `mov eax, 1; ret`.
                EvidenceLog::Event("menu_get_unk05", "result=1");
                return 1;
            }
            bool UseEventDispatcher() override
            {
                EvidenceLog::Event("menu_use_event_dispatcher", "result=false");
                return false;
            }

            bool ShouldHandleEvent(const RE::InputEvent* a_event) override
            {
                if (!a_event || (a_event->deviceType != RE::InputEvent::DeviceType::kKeyboard &&
                                    a_event->deviceType != RE::InputEvent::DeviceType::kMouse)) {
                    return false;
                }
                return RE::IMenu::ShouldHandleEvent(a_event);
            }

            // Address Library v22 contains no offsets for these inherited placeholders.
            // Keep the research menu fail-closed instead of ever resolving ID 0.
            bool Unk0A() override
            {
                // Current IMenu ID 130619 implements this slot as
                // `return uiMovie != nullptr`.  The UI show processor uses a false
                // result as the condition for immediate menu cleanup.
                return uiMovie != nullptr;
            }
            bool Unk15(void*) override { return false; }
            std::uint64_t Unk18(void*, std::uint64_t) override { return 0; }
            float Unk1A() override { return 0.0F; }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::UpdateSceneRectEvent&,
                RE::BSTEventSource<RE::UpdateSceneRectEvent>*) override
            {
                return RE::BSEventNotifyControl::kContinue;
            }

            bool Unk09(const RE::InputEvent* a_event) override
            {
                if (a_event && a_event->deviceType == RE::InputEvent::DeviceType::kGamepad) {
                    return false;
                }
                bool handled = false;
                if (a_event && inputEventHandlingEnabled) {
                    switch (a_event->eventType) {
                    case RE::InputEvent::EventType::kButton: {
                        const auto* button = static_cast<const RE::ButtonEvent*>(a_event);
                        handled = HandleButtonInput(*button);
                        EvidenceLog::Event(
                            "menu_button_input",
                            std::format(
                                "id_code={} value={:.2f} held={:.3f} handled={}",
                                button->idCode, button->value, button->heldDownSecs,
                                handled));
                        if (handled) {
                            const_cast<RE::ButtonEvent*>(button)->status =
                                RE::InputEvent::Status::kStop;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
                if (handled) {
                    EvidenceLog::Event(
                        "menu_input_handled",
                        std::format(
                            "event_type={} device_type={}",
                            a_event ? static_cast<std::uint32_t>(a_event->eventType) :
                                      std::numeric_limits<std::uint32_t>::max(),
                            a_event ? static_cast<std::uint32_t>(a_event->deviceType) :
                                      std::numeric_limits<std::uint32_t>::max()));
                }
                return handled;
            }

            RE::UI_MESSAGE_RESULT ProcessMessage(RE::UIMessageData& a_message) override
            {
                const auto typeBefore = a_message.type;
                EvidenceLog::Event(
                    "menu_message_entered",
                    std::format(
                        "type={} caller_rva=0x{:08X}",
                        static_cast<std::uint32_t>(typeBefore),
                        ToImageRva(_ReturnAddress())));
                const auto result = RE::IMenu::ProcessMessage(a_message);
                EvidenceLog::Event(
                    "menu_message_base_completed",
                    std::format(
                        "type_before={} type_after={} result={}",
                        static_cast<std::uint32_t>(typeBefore),
                        static_cast<std::uint32_t>(a_message.type),
                        static_cast<std::int64_t>(result)));
                if (typeBefore == RE::UI_MESSAGE_TYPE::kShow) {
                    MenuApiHost::SetMenuOpen(true);
                    Transition(ProbeEvent::MenuOpened);
                    EvidenceLog::Event(
                        "menu_message_show",
                        std::format("result={}", static_cast<std::int64_t>(result)));
                } else if (typeBefore == RE::UI_MESSAGE_TYPE::kHide) {
                    MenuApiHost::SetMenuOpen(false);
                    Transition(ProbeEvent::MenuClosed);
                    EvidenceLog::Event(
                        "menu_message_hide",
                        std::format("result={}", static_cast<std::int64_t>(result)));
                    const auto ui = RE::UI::GetSingleton();
                    const bool pauseOpen = ui && ui->IsMenuOpen(
                                                     RE::BSFixedString("PauseMenu"));
                    EvidenceLog::Event(
                        "research_pause_state_after_probe_close",
                        std::format("open={}", pauseOpen));
                }
                return result;
            }

            bool LoadMovie(bool, bool a_arg2) override
            {
                EvidenceLog::Event(
                    "movie_load_entered",
                    std::format(
                        "arg2={} ref_count={} caller_rva=0x{:08X}", a_arg2,
                        RefCountForEvidence(), ToImageRva(_ReturnAddress())));
                const bool loaded = RE::IMenu::LoadMovie(false, a_arg2);
                EvidenceLog::Event(
                    "movie_load_result", std::format("loaded={}", loaded));
                if (!loaded || !uiMovie || !uiMovie->asMovieRoot) {
                    Transition(ProbeEvent::RuntimeFault);
                    return loaded;
                }

                bool bridgeRootFound = false;
                for (const auto candidatePath : kRootCandidates) {
                    RE::Scaleform::GFx::Value candidate;
                    const bool resolved = uiMovie->asMovieRoot->GetVariable(
                        &candidate, candidatePath.data());
                    const bool object = resolved && candidate.IsObject();
                    const bool hasCodeObject = object && candidate.HasMember(CODE_OBJ_NAME);
                    EvidenceLog::Event(
                        "root_candidate",
                        std::format(
                            "path={} resolved={} object={} display={} has_code_object={}",
                            candidatePath, resolved, object,
                            resolved && candidate.IsDisplayObject(), hasCodeObject));
                    if (!hasCodeObject) {
                        continue;
                    }

                    menuObj = candidate;
                    bridgeRootFound = true;
                    EvidenceLog::Event("root_selected", candidatePath);
                    break;
                }

                if (bridgeRootFound) {
                    MapCodeObjectFunctions();
                    LogMovieState("before_on_code_obj_create");
                    const bool invoked = menuObj.Invoke(ON_CODE_OBJ_CREATED_FUNC);
                    EvidenceLog::Event(
                        "bridge_create_invoked", std::format("invoked={}", invoked));
                    LogMovieState("after_on_code_obj_create");
                } else {
                    EvidenceLog::Event(
                        "bridge_root_missing", "movie remains visible; watchdog close remains armed");
                }
                return loaded;
            }

            void MapCodeObjectFunctions() override
            {
                RE::Scaleform::GFx::Value codeObject;
                const bool resolved = menuObj.GetMember(CODE_OBJ_NAME, &codeObject);
                if (!resolved || !codeObject.IsObject()) {
                    const auto previousType = codeObject.GetType();
                    uiMovie->asMovieRoot->CreateObject(&codeObject);
                    const bool assigned =
                        codeObject.IsObject() && menuObj.SetMember(CODE_OBJ_NAME, codeObject);
                    EvidenceLog::Event(
                        "bridge_code_object_created",
                        std::format(
                            "previously_resolved={} previous_type={} created={} assigned={}",
                            resolved, static_cast<std::int32_t>(previousType),
                            codeObject.IsObject(), assigned));
                    if (!assigned) {
                        return;
                    }
                }
                RegisterNativeFunction(
                    "ready", static_cast<std::uint64_t>(NativeFunction::Ready));
                RegisterNativeFunction(
                    "close", static_cast<std::uint64_t>(NativeFunction::Close));
                RegisterNativeFunction(
                    "dispatch", static_cast<std::uint64_t>(NativeFunction::Dispatch));
                RegisterNativeFunction(
                    "focus", static_cast<std::uint64_t>(NativeFunction::Focus));
                RegisterNativeFunction(
                    "modelApplied", static_cast<std::uint64_t>(NativeFunction::ModelApplied));
                EvidenceLog::Event("bridge_functions_mapped", "version=1 count=5");
            }

            void LogMovieState(std::string_view a_phase)
            {
                RE::Scaleform::GFx::Value codeObject;
                RE::Scaleform::GFx::Value readyFunction;
                RE::Scaleform::GFx::Value constructed;
                RE::Scaleform::GFx::Value drawn;
                RE::Scaleform::GFx::Value childCount;
                const bool gotCodeObject = menuObj.GetMember(CODE_OBJ_NAME, &codeObject);
                const bool gotReady = gotCodeObject && codeObject.IsObject() &&
                                      codeObject.GetMember("ready", &readyFunction);
                const bool gotConstructed = menuObj.GetMember("ACPConstructed", &constructed);
                const bool gotDrawn = menuObj.GetMember("ACPDrawn", &drawn);
                const bool gotChildCount = menuObj.GetMember("numChildren", &childCount);
                EvidenceLog::Event(
                    "movie_root_state",
                    std::format(
                        "phase={} root_type={} display={} code_object={} code_type={} "
                        "ready={} ready_type={} constructed={} drawn={} child_count_type={} "
                        "child_count={}",
                        a_phase, static_cast<std::int32_t>(menuObj.GetType()),
                        menuObj.IsDisplayObject(), gotCodeObject && codeObject.IsObject(),
                        static_cast<std::int32_t>(codeObject.GetType()), gotReady,
                        static_cast<std::int32_t>(readyFunction.GetType()),
                        gotConstructed && constructed.IsBoolean() && constructed.GetBoolean(),
                        gotDrawn && drawn.IsBoolean() && drawn.GetBoolean(),
                        static_cast<std::int32_t>(childCount.GetType()),
                        gotChildCount && childCount.IsInt() ? childCount.GetInt() :
                            gotChildCount && childCount.IsUInt() ?
                                static_cast<std::int32_t>(childCount.GetUInt()) : -1));
            }

            void Call(const RE::Scaleform::GFx::FunctionHandler::Params& a_params) override
            {
                const auto function = static_cast<NativeFunction>(
                    reinterpret_cast<std::uintptr_t>(a_params.userData));
                switch (function) {
                case NativeFunction::Ready:
                    EvidenceLog::Event("bridge_ready", std::format("argument_count={}", a_params.argCount));
                    PublishModel(session.Snapshot());
                    break;
                case NativeFunction::Close:
                    CloseSession();
                    break;
                case NativeFunction::Dispatch:
                    DispatchFlat(a_params);
                    break;
                case NativeFunction::Focus: {
                    const auto region = a_params.argCount >= 1 ?
                        ReadUnsigned(a_params.args[0]) :
                        std::numeric_limits<std::uint32_t>::max();
                    const auto action = a_params.argCount >= 2 ?
                        ReadUnsigned(a_params.args[1]) :
                        std::numeric_limits<std::uint32_t>::max();
                    if (a_params.argCount != 2 || region > static_cast<std::uint32_t>(
                            MenuInputRouter::FocusRegion::Actions) || action > 2) {
                        EvidenceLog::Event("bridge_focus_rejected", "invalid focus payload");
                        break;
                    }
                    inputFocus.region = static_cast<MenuInputRouter::FocusRegion>(region);
                    inputFocus.actionIndex = action;
                    EvidenceLog::Event("bridge_focus_updated",
                        std::format("region={} action_index={}", region, action));
                    break;
                }
                case NativeFunction::ModelApplied: {
                    const auto generation = a_params.argCount >= 1 ?
                                                ReadUnsigned(a_params.args[0]) :
                                                std::numeric_limits<std::uint32_t>::max();
                    EvidenceLog::Event(
                        "bridge_model_applied", std::format("revision={}", generation));
                    break;
                }
                }
            }

            void DispatchFlat(const RE::Scaleform::GFx::FunctionHandler::Params& a_params)
            {
                MenuSession::Command command;
                if (a_params.argCount != 10 || !a_params.args[1].IsString() ||
                    !a_params.args[2].IsString() || !a_params.args[3].IsString() ||
                    !a_params.args[4].IsString() || !a_params.args[9].IsString() ||
                    !ReadBoolean(a_params.args[6], command.value.booleanValue) ||
                    !ReadInteger(a_params.args[7], command.value.integerValue) ||
                    !ReadFiniteNumber(a_params.args[8], command.value.floatValue) ||
                    strnlen_s(a_params.args[9].GetString(), SlopApi::kStringValueCapacity) >=
                        SlopApi::kStringValueCapacity) {
                    command.schemaVersion = 0;
                    EvidenceLog::Event("bridge_command_rejected", "invalid flat dispatch arguments");
                    PublishModel(session.Dispatch(command));
                    return;
                }
                command.schemaVersion = ReadUnsigned(a_params.args[0]);
                command.moduleId = a_params.args[2].GetString();
                command.pageId = a_params.args[3].GetString();
                command.controlId = a_params.args[4].GetString();
                command.value.kind = static_cast<SlopApi::ValueKind>(ReadUnsigned(a_params.args[5]));
                strcpy_s(command.value.stringValue, a_params.args[9].GetString());
                const std::string_view name = a_params.args[1].GetString();
                if (name == "selectPage") command.kind = MenuSession::CommandKind::SelectPage;
                else if (name == "selectControl") command.kind = MenuSession::CommandKind::SelectControl;
                else if (name == "write") command.kind = MenuSession::CommandKind::Write;
                else if (name == "invoke") command.kind = MenuSession::CommandKind::Invoke;
                else if (name == "beginBindingCapture") command.kind = MenuSession::CommandKind::BeginBindingCapture;
                else if (name == "apply") command.kind = MenuSession::CommandKind::Apply;
                else if (name == "cancel") command.kind = MenuSession::CommandKind::Cancel;
                else if (name == "close") command.kind = MenuSession::CommandKind::Close;
                else command.schemaVersion = 0;
                DispatchCommand(command, name, "scaleform");
            }

            void DispatchCommand(const MenuSession::Command& a_command,
                std::string_view a_name, std::string_view a_source)
            {
                EvidenceLog::Event("bridge_command",
                    std::format("command={} source={}", a_name, a_source));
                const auto model = session.Dispatch(a_command);
                if (a_command.kind == MenuSession::CommandKind::BeginBindingCapture &&
                    model.error.empty() && model.bindingCaptureActive) {
                    // A release barrier is only needed when the same keyboard event that
                    // activates the row could otherwise become the new binding. Pointer
                    // activation has no initiating keyboard key to release; arming it lazily
                    // would consume the user's first intended modifier instead.
                    captureAwaitingRelease = a_source == "native-keyboard";
                    MenuApiHost::SetInputCaptureActive(true);
                    EvidenceLog::Event(
                        "binding_capture_started",
                        std::format(
                            "module={} page={} control={} flags=0x{:08X} awaiting_release={}",
                            model.captureModuleId, model.capturePageId,
                            model.captureControlId, session.BindingCaptureFlags(),
                            captureAwaitingRelease));
                    if (!captureAwaitingRelease) {
                        EvidenceLog::Event(
                            "binding_capture_armed", "source=pointer-or-scaleform");
                    }
                }
                EvidenceLog::Event(
                    model.error.empty() ? "bridge_command_accepted" :
                                          "bridge_command_rejected",
                    std::format("command={} source={} error={}",
                        a_name, a_source, model.error));
                PublishModel(model);
                if (a_command.kind == MenuSession::CommandKind::Close &&
                    model.error.empty()) {
                    QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
                }
            }

            [[nodiscard]] static std::string_view CommandName(
                MenuSession::CommandKind a_kind) noexcept
            {
                switch (a_kind) {
                case MenuSession::CommandKind::SelectPage: return "selectPage";
                case MenuSession::CommandKind::SelectControl: return "selectControl";
                case MenuSession::CommandKind::Write: return "write";
                case MenuSession::CommandKind::Invoke: return "invoke";
                case MenuSession::CommandKind::BeginBindingCapture: return "beginBindingCapture";
                case MenuSession::CommandKind::Apply: return "apply";
                case MenuSession::CommandKind::Cancel: return "cancel";
                case MenuSession::CommandKind::Close: return "close";
                }
                return "unknown";
            }

            void CloseSession()
            {
                if (session.IsBindingCaptureActive()) {
                    (void)session.CancelBindingCapture();
                    MenuApiHost::SetInputCaptureActive(false);
                }
                const auto model = session.Dispatch(MenuSession::Command{ .kind = MenuSession::CommandKind::Close });
                PublishModel(model);
                if (model.error.empty()) QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
            }

            [[nodiscard]] bool HandleButtonInput(const RE::ButtonEvent& a_event)
            {
                if (a_event.deviceType == RE::InputEvent::DeviceType::kMouse) {
                    if (a_event.idCode != kMouseWheelUpIdCode &&
                        a_event.idCode != kMouseWheelDownIdCode) {
                        return false;
                    }
                    if (!std::isfinite(a_event.value) || a_event.value == 0.0F) {
                        return true;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    const bool duplicate = lastWheelIdCode == a_event.idCode &&
                        lastWheelTimeCode == a_event.timeCode &&
                        lastWheelValue == a_event.value &&
                        now - lastWheelHandledAt < std::chrono::milliseconds(5);
                    if (duplicate) {
                        EvidenceLog::Event(
                            "mouse_wheel_duplicate", "repeated native delivery suppressed");
                        return true;
                    }
                    lastWheelIdCode = a_event.idCode;
                    lastWheelTimeCode = a_event.timeCode;
                    lastWheelValue = a_event.value;
                    lastWheelHandledAt = now;
                    HandlePointerWheel(
                        a_event.idCode == kMouseWheelUpIdCode ? -1 : 1);
                    return true;
                }
                if (a_event.deviceType != RE::InputEvent::DeviceType::kKeyboard ||
                    a_event.idCode < 0 ||
                    a_event.idCode >= static_cast<std::int32_t>(keyDown.size())) {
                    return false;
                }

                const auto idCode = static_cast<std::size_t>(a_event.idCode);
                const bool down = a_event.value > 0.0F;
                const bool pressed = down && !keyDown[idCode];
                keyDown[idCode] = down;

                if (session.IsBindingCaptureActive()) {
                    if (captureAwaitingRelease) {
                        if (std::ranges::none_of(keyDown, [](bool a_down) { return a_down; })) {
                            captureAwaitingRelease = false;
                            EvidenceLog::Event("binding_capture_armed", "all initiating keys released");
                        }
                        return true;
                    }
                    if (!pressed) {
                        return true;
                    }
                    if (a_event.idCode == VK_ESCAPE) {
                        const auto model = session.CancelBindingCapture();
                        MenuApiHost::SetInputCaptureActive(false);
                        EvidenceLog::Event("binding_capture_cancelled", "source=keyboard");
                        PublishModel(model);
                        return true;
                    }
                    const auto captureFlags = session.BindingCaptureFlags();
                    if (a_event.idCode == VK_BACK &&
                        (captureFlags & SlopApi::kBindingClearable) != 0) {
                        const auto model = session.CompleteBindingCapture({});
                        MenuApiHost::SetInputCaptureActive(false);
                        EvidenceLog::Event("binding_capture_cleared", "source=keyboard");
                        PublishModel(model);
                        return true;
                    }
                    if (IsModifierVirtualKey(a_event.idCode)) {
                        return true;
                    }
                    if ((captureFlags & SlopApi::kBindingKeyboard) == 0) {
                        return true;
                    }
                    const bool modifiers =
                        (captureFlags & SlopApi::kBindingModifiers) != 0;
                    const auto binding = std::format(
                        "keyboard:0x{:02X};ctrl={};alt={};shift={}",
                        a_event.idCode,
                        modifiers && IsCapturedModifierDown(
                                         VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ?
                            1 :
                            0,
                        modifiers && IsCapturedModifierDown(VK_MENU, VK_LMENU, VK_RMENU) ?
                            1 :
                            0,
                        modifiers && IsCapturedModifierDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ?
                            1 :
                            0);
                    const auto model = session.CompleteBindingCapture(binding);
                    MenuApiHost::SetInputCaptureActive(false);
                    EvidenceLog::Event(
                        model.error.empty() ? "binding_capture_completed" :
                                              "binding_capture_rejected",
                        std::format("source=keyboard binding={} error={}", binding, model.error));
                    PublishModel(model);
                    return true;
                }

                if (!MenuInputRouter::IsMenuKey(a_event.idCode)) {
                    return false;
                }
                if (!pressed) {
                    return true;
                }

                const auto previousFocus = inputFocus;
                auto routed = MenuInputRouter::Route(
                    session.Snapshot(), a_event.idCode, inputFocus);
                inputFocus = routed.focus;
                if (!routed.command) {
                    EvidenceLog::Event("menu_key_no_action",
                        std::format(
                            "id_code={} focus_region={} action_index={} changed={}",
                            a_event.idCode,
                            static_cast<std::uint32_t>(inputFocus.region),
                            inputFocus.actionIndex, previousFocus != inputFocus));
                    if (previousFocus != inputFocus) {
                        PublishModel(session.Snapshot());
                    }
                    return true;
                }
                if (routed.command->kind == MenuSession::CommandKind::Close) {
                    EvidenceLog::Event("bridge_close", "native keyboard requested close");
                }
                DispatchCommand(
                    *routed.command, CommandName(routed.command->kind), "native-keyboard");
                return true;
            }

            void HandlePointerPhase(PointerPhase a_phase)
            {
                const auto method = a_phase == PointerPhase::Down ? "handlePointerDown" :
                    (a_phase == PointerPhase::Move ? "handlePointerMove" :
                                                     "handlePointerUp");
                const auto failureEvent = a_phase == PointerPhase::Down ?
                    "pointer_down_rejected" :
                    (a_phase == PointerPhase::Move ? "pointer_move_rejected" :
                                                    "pointer_up_rejected");
                POINT point{};
                double stageX{};
                double stageY{};
                if (!ResolvePointerStage(point, stageX, stageY, failureEvent)) return;

                if (!uiMovie || !uiMovie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event(failureEvent, "movie root unavailable");
                    return;
                }
                RE::Scaleform::GFx::Value args[2]{
                    RE::Scaleform::GFx::Value(stageX),
                    RE::Scaleform::GFx::Value(stageY)
                };
                RE::Scaleform::GFx::Value handled;
                const bool invoked =
                    menuObj.Invoke(method, &handled, args, std::size(args));
                const bool hit = invoked && handled.IsBoolean() && handled.GetBoolean();
                if (a_phase != PointerPhase::Move) {
                    EvidenceLog::Event(
                        hit ? (a_phase == PointerPhase::Down ? "pointer_down_hit" :
                                                               "pointer_up_hit") :
                              (a_phase == PointerPhase::Down ? "pointer_down_missed" :
                                                               "pointer_up_missed"),
                        std::format(
                            "client={},{} stage={:.1f},{:.1f} owner=scaleform "
                            "invoked={}",
                            point.x, point.y, stageX, stageY, invoked));
                }
            }

            void HandlePointerWheel(std::int32_t a_direction)
            {
                POINT point{};
                double stageX{};
                double stageY{};
                if (!ResolvePointerStage(point, stageX, stageY, "mouse_wheel_rejected")) return;
                if (!uiMovie || !uiMovie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event("mouse_wheel_rejected", "movie root unavailable");
                    return;
                }
                RE::Scaleform::GFx::Value args[3]{
                    RE::Scaleform::GFx::Value(stageX),
                    RE::Scaleform::GFx::Value(stageY),
                    RE::Scaleform::GFx::Value(a_direction)
                };
                RE::Scaleform::GFx::Value handled;
                const bool invoked = menuObj.Invoke(
                    "handlePointerWheel", &handled, args, std::size(args));
                const bool hit = invoked && handled.IsBoolean() && handled.GetBoolean();
                EvidenceLog::Event(
                    hit ? "mouse_wheel_hit" : "mouse_wheel_missed",
                    std::format(
                        "client={},{} stage={:.1f},{:.1f} direction={} invoked={}",
                        point.x, point.y, stageX, stageY, a_direction, invoked));
            }

            [[nodiscard]] bool SetString(RE::Scaleform::GFx::Value& a_object,
                const char* a_name, std::string_view a_value) const
            {
                RE::Scaleform::GFx::Value text;
                uiMovie->asMovieRoot->CreateString(&text, std::string(a_value).c_str());
                return a_object.SetMember(a_name, text);
            }

            [[nodiscard]] bool SerializeControl(RE::Scaleform::GFx::Value& a_target,
                const MenuSession::Control& a_control) const
            {
                const auto& descriptor = a_control.descriptor;
                return SetString(a_target, "controlId", descriptor.controlId) &&
                    a_target.SetMember("kind", RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(descriptor.kind))) &&
                    a_target.SetMember("flags", RE::Scaleform::GFx::Value(descriptor.flags)) &&
                    SetString(a_target, "label", descriptor.label) && SetString(a_target, "description", descriptor.description) &&
                    a_target.SetMember("minimum", RE::Scaleform::GFx::Value(descriptor.minimumValue)) &&
                    a_target.SetMember("maximum", RE::Scaleform::GFx::Value(descriptor.maximumValue)) &&
                    a_target.SetMember("step", RE::Scaleform::GFx::Value(descriptor.stepValue)) &&
                    a_target.SetMember("available", RE::Scaleform::GFx::Value(a_control.available)) &&
                    a_target.SetMember("valueKind", RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(a_control.value.kind))) &&
                    a_target.SetMember("booleanValue", RE::Scaleform::GFx::Value(a_control.value.booleanValue != 0)) &&
                    a_target.SetMember("integerValue", RE::Scaleform::GFx::Value(static_cast<double>(a_control.value.integerValue))) &&
                    a_target.SetMember("floatValue", RE::Scaleform::GFx::Value(a_control.value.floatValue)) &&
                    SetString(a_target, "stringValue", a_control.value.stringValue) && SetString(a_target, "error", a_control.error);
            }

            void PublishModel(const MenuSession::Model& a_model)
            {
                if (!uiMovie || !uiMovie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event("bridge_model_publish_failed", "movie root unavailable");
                    return;
                }
                auto* root = uiMovie->asMovieRoot.get();
                RE::Scaleform::GFx::Value model, pages;
                root->CreateObject(&model); root->CreateArray(&pages);
                std::uint32_t activePage{};
                std::uint32_t selectedControl{};
                for (std::uint32_t i = 0; i < a_model.pages.size(); ++i) {
                    if (a_model.pages[i].moduleId == a_model.activeModuleId &&
                        a_model.pages[i].pageId == a_model.activePageId) {
                        activePage = i;
                        for (std::uint32_t j = 0; j < a_model.pages[i].controls.size(); ++j) {
                            if (a_model.pages[i].controls[j].descriptor.controlId == a_model.selectedControlId) {
                                selectedControl = j; break;
                            }
                        }
                        break;
                    }
                }
                bool populated = model.IsObject() && pages.IsArray() &&
                    model.SetMember("schemaVersion", RE::Scaleform::GFx::Value(a_model.schemaVersion)) &&
                    model.SetMember("revision", RE::Scaleform::GFx::Value(static_cast<double>(a_model.revision))) &&
                    model.SetMember("activePage", RE::Scaleform::GFx::Value(activePage)) &&
                    model.SetMember("selectedControl", RE::Scaleform::GFx::Value(selectedControl)) &&
                    model.SetMember("focusRegion", RE::Scaleform::GFx::Value(
                        static_cast<std::uint32_t>(inputFocus.region))) &&
                    model.SetMember("focusedAction", RE::Scaleform::GFx::Value(
                        inputFocus.actionIndex)) &&
                    model.SetMember("dirty", RE::Scaleform::GFx::Value(a_model.dirty)) &&
                    model.SetMember("bindingCaptureActive", RE::Scaleform::GFx::Value(
                        a_model.bindingCaptureActive)) &&
                    SetString(model, "captureModuleId", a_model.captureModuleId) &&
                    SetString(model, "capturePageId", a_model.capturePageId) &&
                    SetString(model, "captureControlId", a_model.captureControlId) &&
                    SetString(model, "error", a_model.error);
                for (std::uint32_t pageIndex = 0; populated && pageIndex < a_model.pages.size(); ++pageIndex) {
                    const auto& source = a_model.pages[pageIndex];
                    RE::Scaleform::GFx::Value page, controls;
                    root->CreateObject(&page); root->CreateArray(&controls);
                    populated = page.IsObject() && controls.IsArray() &&
                        SetString(page, "moduleId", source.moduleId) &&
                        SetString(page, "moduleTitle", source.moduleTitle) &&
                        SetString(page, "pageId", source.pageId) &&
                        SetString(page, "title", source.title) && SetString(page, "description", source.description);
                    for (std::uint32_t controlIndex = 0; populated && controlIndex < source.controls.size(); ++controlIndex) {
                        RE::Scaleform::GFx::Value control;
                        root->CreateObject(&control);
                        populated = control.IsObject() && SerializeControl(control, source.controls[controlIndex]) &&
                            controls.PushBack(control);
                    }
                    populated = populated && page.SetMember("controls", controls) && pages.PushBack(page);
                }
                populated = populated && model.SetMember("pages", pages);
                const bool invoked = populated && menuObj.Invoke("applyModel", nullptr, &model, 1);
                EvidenceLog::Event("bridge_model_published", std::format("revision={} populated={} invoked={}", a_model.revision, populated, invoked));
            }

            [[nodiscard]] static std::uint32_t ReadUnsigned(
                const RE::Scaleform::GFx::Value& a_value) noexcept
            {
                if (a_value.IsUInt()) {
                    return a_value.GetUInt();
                }
                if (a_value.IsInt() && a_value.GetInt() >= 0) {
                    return static_cast<std::uint32_t>(a_value.GetInt());
                }
                if (a_value.IsNumber() && std::isfinite(a_value.GetNumber()) &&
                    std::trunc(a_value.GetNumber()) == a_value.GetNumber() &&
                    a_value.GetNumber() >= 0.0 &&
                    a_value.GetNumber() <= static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
                    return static_cast<std::uint32_t>(a_value.GetNumber());
                }
                return std::numeric_limits<std::uint32_t>::max();
            }

            [[nodiscard]] static bool ReadBoolean(const RE::Scaleform::GFx::Value& a_value,
                std::uint32_t& a_result) noexcept
            {
                if (!a_value.IsBoolean()) return false;
                a_result = a_value.GetBoolean() ? 1U : 0U;
                return true;
            }

            [[nodiscard]] static bool ReadInteger(const RE::Scaleform::GFx::Value& a_value,
                std::int64_t& a_result) noexcept
            {
                constexpr double kMaximumExactInteger = 9007199254740991.0;
                if (a_value.IsInt()) { a_result = a_value.GetInt(); return true; }
                if (a_value.IsUInt()) { a_result = a_value.GetUInt(); return true; }
                if (!a_value.IsNumber() || !std::isfinite(a_value.GetNumber()) ||
                    std::trunc(a_value.GetNumber()) != a_value.GetNumber() ||
                    a_value.GetNumber() < -kMaximumExactInteger ||
                    a_value.GetNumber() > kMaximumExactInteger) return false;
                a_result = static_cast<std::int64_t>(a_value.GetNumber());
                return true;
            }

            [[nodiscard]] static bool ReadFiniteNumber(const RE::Scaleform::GFx::Value& a_value,
                double& a_result) noexcept
            {
                if (!a_value.IsNumber() || !std::isfinite(a_value.GetNumber())) return false;
                a_result = a_value.GetNumber();
                return true;
            }

            [[nodiscard]] std::int32_t RefCountForEvidence() const noexcept
            {
                return refCount;
            }

        private:
            [[nodiscard]] static bool ResolvePointerStage(POINT& a_point,
                double& a_stageX, double& a_stageY, std::string_view a_failureEvent)
            {
                const auto window = ::GetForegroundWindow();
                RECT client{};
                if (!window || !::GetClientRect(window, &client) ||
                    !::GetCursorPos(&a_point) || !::ScreenToClient(window, &a_point)) {
                    EvidenceLog::Event(a_failureEvent, "window geometry unavailable");
                    return false;
                }
                const auto width = static_cast<double>(client.right - client.left);
                const auto height = static_cast<double>(client.bottom - client.top);
                const auto scale = (std::min)(width / 1920.0, height / 1080.0);
                if (!std::isfinite(scale) || scale <= 0.0) {
                    EvidenceLog::Event(a_failureEvent, "invalid viewport scale");
                    return false;
                }
                const auto offsetX = (width - 1920.0 * scale) * 0.5;
                const auto offsetY = (height - 1080.0 * scale) * 0.5;
                a_stageX = (static_cast<double>(a_point.x) - offsetX) / scale;
                a_stageY = (static_cast<double>(a_point.y) - offsetY) / scale;
                return true;
            }

            [[nodiscard]] bool IsCapturedModifierDown(
                std::int32_t a_generic, std::int32_t a_left,
                std::int32_t a_right) const noexcept
            {
                const auto eventDown = [this](std::int32_t a_virtualKey) {
                    return a_virtualKey >= 0 &&
                        a_virtualKey < static_cast<std::int32_t>(keyDown.size()) &&
                        keyDown[static_cast<std::size_t>(a_virtualKey)];
                };
                return eventDown(a_generic) || eventDown(a_left) || eventDown(a_right) ||
                    (::GetAsyncKeyState(a_generic) & 0x8000) != 0 ||
                    (::GetAsyncKeyState(a_left) & 0x8000) != 0 ||
                    (::GetAsyncKeyState(a_right) & 0x8000) != 0;
            }

            [[nodiscard]] static bool IsModifierVirtualKey(
                std::int32_t a_virtualKey) noexcept
            {
                return a_virtualKey == VK_SHIFT || a_virtualKey == VK_LSHIFT ||
                    a_virtualKey == VK_RSHIFT || a_virtualKey == VK_CONTROL ||
                    a_virtualKey == VK_LCONTROL || a_virtualKey == VK_RCONTROL ||
                    a_virtualKey == VK_MENU || a_virtualKey == VK_LMENU ||
                    a_virtualKey == VK_RMENU || a_virtualKey == VK_LWIN ||
                    a_virtualKey == VK_RWIN;
            }

            MenuSession::Session session;
            MenuInputRouter::FocusState inputFocus;
            std::array<bool, 256> keyDown{};
            bool captureAwaitingRelease{};
            std::int32_t lastWheelIdCode{};
            std::uint32_t lastWheelTimeCode{};
            float lastWheelValue{};
            std::chrono::steady_clock::time_point lastWheelHandledAt{};
        };

        RE::Scaleform::Ptr<RE::IMenu>* CreateMenu(RE::Scaleform::Ptr<RE::IMenu>* a_result)
        {
            EvidenceLog::Event(
                "menu_factory_entered",
                std::format(
                    "return_storage_bits=0x{:X} caller_rva=0x{:08X}",
                    reinterpret_cast<std::uintptr_t>(
                        *reinterpret_cast<RE::IMenu**>(a_result)),
                    ToImageRva(_ReturnAddress())));
            auto* menu = new AbsoluteControlPanelMenu();
            EvidenceLog::Event(
                "menu_factory_constructed",
                std::format(
                    "address=0x{:X} ref_count={}",
                    reinterpret_cast<std::uintptr_t>(menu), menu->RefCountForEvidence()));

            // Match current shipped factories exactly.  The engine passes return storage,
            // not an initialized Ptr: shipped code AddRefs the new object and overwrites
            // the pointer-sized slot without attempting to release its prior bits.
            menu->AddRef();
            *reinterpret_cast<RE::IMenu**>(a_result) = menu;
            EvidenceLog::Event(
                "menu_factory_completed",
                std::format(
                    "address=0x{:X} ref_count={}",
                    reinterpret_cast<std::uintptr_t>(menu),
                    menu->RefCountForEvidence()));
            return a_result;
        }

        void ScheduleOpenHotkey() noexcept
        {
            const auto virtualKey = g_config.openHotkey;
            if (virtualKey == 0) {
                EvidenceLog::Event("open_hotkey_disabled");
                return;
            }
            const auto taskInterface = SFSE::GetTaskInterface();
            if (!taskInterface) {
                EvidenceLog::Event("open_hotkey_failed", "SFSE task interface unavailable");
                return;
            }

            EvidenceLog::Event(
                "open_hotkey_registered", std::format("virtual_key=0x{:02X}", virtualKey));
            std::thread([taskInterface, virtualKey]() {
                bool wasDown = false;
                for (;;) {
                    DWORD foregroundProcess{};
                    const auto foreground = ::GetForegroundWindow();
                    if (foreground) {
                        ::GetWindowThreadProcessId(foreground, &foregroundProcess);
                    }
                    const bool focused = foregroundProcess == ::GetCurrentProcessId();
                    const auto keyState = focused ?
                        ::GetAsyncKeyState(static_cast<int>(virtualKey)) : 0;
                    const bool down = (keyState & 0x8000) != 0;
                    const bool pressed = focused &&
                        (((keyState & 0x0001) != 0) || (down && !wasDown));
                    if (pressed) {
                        taskInterface->AddTask([virtualKey]() {
                            const auto ui = RE::UI::GetSingleton();
                            if (!ui) {
                                EvidenceLog::Event(
                                    "open_hotkey_rejected", "UI singleton unavailable");
                                return;
                            }
                            const RE::BSFixedString slopMenu{ kMenuName.data() };
                            if (ui->IsMenuOpen(slopMenu)) {
                                EvidenceLog::Event(
                                    "open_hotkey_ignored", "Control Panel menu already open");
                                return;
                            }
                            if (ui->IsMenuOpen(RE::BSFixedString("PauseMenu")) ||
                                ui->IsMenuOpen(RE::BSFixedString("MainMenu"))) {
                                EvidenceLog::Event(
                                    "open_hotkey_rejected", "pause or main menu is active");
                                return;
                            }
                            EvidenceLog::Event("open_hotkey_requested",
                                std::format("virtual_key=0x{:02X}", virtualKey));
                            QueueMenuMessage(RE::UI_MESSAGE_TYPE::kShow, "hotkey");
                        });
                    }
                    wasDown = down;
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }).detach();
        }

        void SchedulePointerInput() noexcept
        {
            const auto taskInterface = SFSE::GetTaskInterface();
            if (!taskInterface) {
                EvidenceLog::Event(
                    "pointer_input_failed", "SFSE task interface unavailable");
                return;
            }

            EvidenceLog::Event("pointer_input_registered", "button=left");
            std::thread([taskInterface]() {
                bool wasDown = false;
                auto movePending = std::make_shared<std::atomic_bool>(false);
                const auto queuePointerPhase =
                    [taskInterface, movePending](PointerPhase a_phase) {
                        taskInterface->AddTask([a_phase, movePending]() {
                            const auto clearMovePending = [&]() {
                                if (a_phase == PointerPhase::Move) {
                                    movePending->store(
                                        false, std::memory_order_release);
                                }
                            };
                            const auto ui = RE::UI::GetSingleton();
                            if (!ui) {
                                clearMovePending();
                                return;
                            }
                            const RE::BSFixedString menuName{ kMenuName.data() };
                            if (!ui->IsMenuOpen(menuName)) {
                                clearMovePending();
                                return;
                            }
                            auto menu = ui->GetMenu(menuName);
                            if (menu) {
                                static_cast<AbsoluteControlPanelMenu*>(menu.get())
                                    ->HandlePointerPhase(a_phase);
                            }
                            clearMovePending();
                        });
                    };
                for (;;) {
                    DWORD foregroundProcess{};
                    const auto foreground = ::GetForegroundWindow();
                    if (foreground) {
                        ::GetWindowThreadProcessId(foreground, &foregroundProcess);
                    }
                    const bool focused = foregroundProcess == ::GetCurrentProcessId();
                    const auto buttonState = focused ? ::GetAsyncKeyState(VK_LBUTTON) : 0;
                    const bool down = (buttonState & 0x8000) != 0;
                    const bool pressed = focused && down && !wasDown;
                    const bool clickPulse = focused && !down &&
                        (buttonState & 0x0001) != 0;
                    if (pressed || clickPulse) {
                        queuePointerPhase(PointerPhase::Down);
                    } else if (focused && down &&
                               !movePending->exchange(
                                   true, std::memory_order_acq_rel)) {
                        queuePointerPhase(PointerPhase::Move);
                    }
                    if ((wasDown && !down) || clickPulse) {
                        queuePointerPhase(PointerPhase::Up);
                    }
                    wasDown = down;
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }
            }).detach();
        }

        void SchedulePauseMenuIntegration() noexcept
        {
            if (!g_config.enablePauseMenuEntry) {
                EvidenceLog::Event("pause_entry_integration_disabled");
                return;
            }
            EvidenceLog::Event(
                "pause_entry_integration_registered",
                "mode=active-menu-boundary+scaleform-advance fail_closed=true fallback=F2");
        }

        void ScheduleResearchInputMailbox() noexcept
        {
            const auto taskInterface = SFSE::GetTaskInterface();
            if (!taskInterface) {
                EvidenceLog::Event(
                    "research_input_mailbox_failed",
                    "SFSE task interface unavailable");
                return;
            }

            auto mailboxDirectory = EvidenceLog::Path().parent_path();
            if (const auto logDirectory = SFSE::log::log_directory()) {
                mailboxDirectory = *logDirectory;
            }
            const auto inputPath = mailboxDirectory / std::format(
                "AbsoluteControlPanelResearch.{}.input", g_config.runId);
            EvidenceLog::Event(
                "research_input_mailbox_registered", inputPath.string());
            std::thread([taskInterface, inputPath]() {
                std::uint32_t lastCommandId = 0;
                for (;;) {
                    PollResearchInputMailbox(
                        taskInterface, inputPath, lastCommandId);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }).detach();
        }

        void ScheduleExperiment() noexcept
        {
            if (!g_config.autoOpen) {
                EvidenceLog::Event("auto_open_disabled");
                return;
            }

            const auto taskInterface = SFSE::GetTaskInterface();
            if (!taskInterface) {
                EvidenceLog::Event("schedule_failed", "SFSE task interface unavailable");
                Transition(ProbeEvent::RuntimeFault);
                return;
            }

            const auto openDelay = g_config.openDelayMilliseconds;
            const auto visibleDuration = g_config.visibleMilliseconds;
            const auto requireArm = g_config.requireArm;
            const auto advanceTitleWithSendInput = g_config.advanceTitleWithSendInput;
            const auto armTimeout = g_config.armTimeoutMilliseconds;
            const auto runId = g_config.runId;
            const auto requestDirectory = EvidenceLog::Path().parent_path();
            const auto armPath = requestDirectory /
                                 std::format("AbsoluteControlPanelResearch.{}.arm", runId);
            const auto advancePath = requestDirectory /
                                     std::format("AbsoluteControlPanelResearch.{}.advance", runId);
            EvidenceLog::Event(
                "experiment_scheduled",
                std::format(
                    "require_arm={} advance_title={} arm_timeout_ms={} open_delay_ms={} "
                    "visible_ms={}",
                    requireArm, advanceTitleWithSendInput, armTimeout, openDelay,
                    visibleDuration));

            std::thread([
                            taskInterface, requireArm, advanceTitleWithSendInput, armTimeout,
                            openDelay, visibleDuration, armPath, advancePath]() {
                if (requireArm) {
                    const auto deadline = std::chrono::steady_clock::now() +
                                          std::chrono::milliseconds(armTimeout);
                    if (advanceTitleWithSendInput) {
                        EvidenceLog::Event(
                            "experiment_waiting_for_title_advance", advancePath.string());
                        while (!std::filesystem::exists(advancePath)) {
                            if (std::chrono::steady_clock::now() >= deadline) {
                                EvidenceLog::Event(
                                    "title_advance_timeout",
                                    std::format("timeout_ms={}", armTimeout));
                                Transition(ProbeEvent::RuntimeFault);
                                return;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }

                        taskInterface->AddTask([taskInterface]() {
                            ProbeForeignMenuRoot("MainMenu", 0);
                            const auto activeWindow = ::GetActiveWindow();
                            const auto foregroundBefore = ::GetForegroundWindow();
                            const auto focused = activeWindow != nullptr &&
                                                 ::SetForegroundWindow(activeWindow) != FALSE;

                            constexpr WORD kEnterScanCode = 0x1C;
                            INPUT keyDown{};
                            keyDown.type = INPUT_KEYBOARD;
                            keyDown.ki.wScan = kEnterScanCode;
                            keyDown.ki.dwFlags = KEYEVENTF_SCANCODE;
                            ::SetLastError(ERROR_SUCCESS);
                            const auto sentDown = ::SendInput(1, &keyDown, sizeof(INPUT));
                            const auto error = ::GetLastError();
                            EvidenceLog::Event(
                                "title_enter_key_down",
                                std::format(
                                    "scan_code=0x{:02X} sent={} error={} active=0x{:X} "
                                    "foreground_before=0x{:X} foreground_after=0x{:X} focused={}",
                                    kEnterScanCode, sentDown, error,
                                    reinterpret_cast<std::uintptr_t>(activeWindow),
                                    reinterpret_cast<std::uintptr_t>(foregroundBefore),
                                    reinterpret_cast<std::uintptr_t>(::GetForegroundWindow()),
                                    focused));

                            std::thread([taskInterface]() {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                taskInterface->AddTask([]() {
                                    constexpr WORD kEnterScanCode = 0x1C;
                                    INPUT keyUp{};
                                    keyUp.type = INPUT_KEYBOARD;
                                    keyUp.ki.wScan = kEnterScanCode;
                                    keyUp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
                                    ::SetLastError(ERROR_SUCCESS);
                                    const auto sentUp = ::SendInput(1, &keyUp, sizeof(INPUT));
                                    const auto error = ::GetLastError();
                                    EvidenceLog::Event(
                                        "title_enter_key_up",
                                        std::format(
                                            "scan_code=0x{:02X} sent={} error={} foreground=0x{:X}",
                                            kEnterScanCode, sentUp, error,
                                            reinterpret_cast<std::uintptr_t>(
                                                ::GetForegroundWindow())));
                                });
                            }).detach();
                        });
                        EvidenceLog::Event("title_advance_queued", advancePath.string());
                    }

                    EvidenceLog::Event("experiment_waiting_for_arm", armPath.string());
                    while (!std::filesystem::exists(armPath)) {
                        if (std::chrono::steady_clock::now() >= deadline) {
                            EvidenceLog::Event(
                                "experiment_arm_timeout",
                                std::format("timeout_ms={}", armTimeout));
                            Transition(ProbeEvent::RuntimeFault);
                            return;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    EvidenceLog::Event("experiment_armed", armPath.string());
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(openDelay));
                taskInterface->AddTask([]() {
                    QueueMenuMessage(RE::UI_MESSAGE_TYPE::kShow, "experiment");
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                taskInterface->AddTask([]() {
                    const auto ui = RE::UI::GetSingleton();
                    if (!ui) {
                        EvidenceLog::Event("menu_ui_state_unavailable");
                        return;
                    }
                    const RE::BSFixedString menuName{ kMenuName.data() };
                    const auto entry = ui->GetMenuEntry(menuName);
                    const auto menu = entry ? entry->menu.get() : nullptr;
                    EvidenceLog::Event(
                        "menu_ui_state",
                        std::format(
                            "entry={} menu=0x{:X} stack={} array={} advance={} "
                            "menus_visible={}",
                            entry != nullptr, reinterpret_cast<std::uintptr_t>(menu),
                            ui->menuStack.size(), ui->menuArray.size(),
                            ui->menusToAdvance.size(), ui->IsMenusVisible()));
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(visibleDuration));
                taskInterface->AddTask([]() {
                    EvidenceLog::Event("watchdog_fired", "forcing menu hide");
                    QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "watchdog");
                });
            }).detach();
        }
    }

    void OnDataReady() noexcept
    {
        Transition(ProbeEvent::PluginLoaded);
        Transition(ProbeEvent::DataReady);

        g_config = LoadProbeConfig(kConfigPath);
        EvidenceLog::Initialize(g_config.runId);
        EvidenceLog::Event(
                "config_loaded",
                std::format(
                    "path={} registration={} auto_open={} pause_entry={} require_arm={} hotkey=0x{:02X} flags=0x{:08X}",
                    kConfigPath.string(), g_config.enableRegistration, g_config.autoOpen,
                    g_config.enablePauseMenuEntry, g_config.requireArm,
                    g_config.openHotkey, g_config.menuFlags));

        if (!ResearchModule::Register()) {
            EvidenceLog::Event(
                "registration_failed", "synthetic API provider could not register");
            return;
        }

        if (!g_config.enableRegistration) {
            EvidenceLog::Event("registration_disabled", "configuration requested fail-closed");
            return;
        }

        if (!ValidateMenuRelocations()) {
            Transition(ProbeEvent::RegistrationFailed);
            EvidenceLog::Event(
                "registration_failed", "native menu compatibility gate rejected runtime");
            return;
        }

        if (!InstallLifecycleTraceHook()) {
            Transition(ProbeEvent::RegistrationFailed);
            EvidenceLog::Event(
                "registration_failed", "menu lifecycle trace callsite rejected runtime");
            return;
        }

        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            Transition(ProbeEvent::RegistrationFailed);
            EvidenceLog::Event("registration_failed", "Starfield UI singleton unavailable");
            return;
        }

        if (!ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()))) {
            ui->RegisterMenu(kMenuName.data(), &CreateMenu);
        }

        if (!ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()))) {
            Transition(ProbeEvent::RegistrationFailed);
            EvidenceLog::Event("registration_failed", "UI manager did not retain registration");
            return;
        }

        Transition(ProbeEvent::RegistrationEnabled);
        EvidenceLog::Event("registration_succeeded", kMenuName);
        ScheduleOpenHotkey();
        SchedulePointerInput();
        SchedulePauseMenuIntegration();
        ScheduleResearchInputMailbox();
        ScheduleExperiment();
    }

    ProbePhase Phase() noexcept { return g_phase.load(std::memory_order_acquire); }
    bool IsRegistrationEnabled() noexcept { return g_config.enableRegistration; }
}
