#include "NativeMenuProbe.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
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
            ModelApplied
        };

        std::atomic<ProbePhase> g_phase{ ProbePhase::Cold };
        ProbeConfig g_config;

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

        void QueueMenuMessage(RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept
        {
            const auto queue = RE::UIMessageQueue::GetSingleton();
            if (!queue) {
                EvidenceLog::Event("menu_message_rejected", "UIMessageQueue unavailable");
                Transition(ProbeEvent::RuntimeFault);
                return;
            }

            const auto result =
                queue->AddMessage(RE::BSFixedString(kMenuName.data()), a_type);
            EvidenceLog::Event(
                a_type == RE::UI_MESSAGE_TYPE::kShow ? "menu_show_requested" :
                                                       "menu_hide_requested",
                std::format("source={} result={}", a_source, result));
        }

        void QueueScanCodePulse(
            const SFSE::TaskInterface* a_taskInterface, std::uint32_t a_commandId,
            std::string a_command, WORD a_scanCode, bool a_extended) noexcept
        {
            a_taskInterface->AddTask([
                                         a_taskInterface, a_commandId,
                                         command = std::move(a_command), a_scanCode,
                                         a_extended]() {
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
                        "id={} command={} scan_code=0x{:02X} extended={} sent={} error={}",
                        a_commandId, command, a_scanCode, a_extended, sent, error));

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
                EvidenceLog::Event(
                    "menu_input_dispatched",
                    std::format(
                        "event_type={} device_type={} handled={}",
                        a_event ? static_cast<std::uint32_t>(a_event->eventType) :
                                  std::numeric_limits<std::uint32_t>::max(),
                        a_event ? static_cast<std::uint32_t>(a_event->deviceType) :
                                  std::numeric_limits<std::uint32_t>::max(),
                        handled));
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
                    Transition(ProbeEvent::MenuOpened);
                    EvidenceLog::Event(
                        "menu_message_show",
                        std::format("result={}", static_cast<std::int64_t>(result)));
                } else if (typeBefore == RE::UI_MESSAGE_TYPE::kHide) {
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
                    "modelApplied", static_cast<std::uint64_t>(NativeFunction::ModelApplied));
                EvidenceLog::Event("bridge_functions_mapped", "version=1 count=4");
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
                else if (name == "apply") command.kind = MenuSession::CommandKind::Apply;
                else if (name == "cancel") command.kind = MenuSession::CommandKind::Cancel;
                else if (name == "close") command.kind = MenuSession::CommandKind::Close;
                else command.schemaVersion = 0;
                EvidenceLog::Event("bridge_command", std::format("command={}", name));
                const auto model = session.Dispatch(command);
                EvidenceLog::Event(model.error.empty() ? "bridge_command_accepted" : "bridge_command_rejected",
                    std::format("command={} error={}", name, model.error));
                PublishModel(model);
                if (name == "close" && model.error.empty()) {
                    QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
                }
            }

            void CloseSession()
            {
                const auto model = session.Dispatch(MenuSession::Command{ .kind = MenuSession::CommandKind::Close });
                PublishModel(model);
                if (model.error.empty()) QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
            }

            [[nodiscard]] bool HandleButtonInput(const RE::ButtonEvent& a_event)
            {
                if (a_event.deviceType != RE::InputEvent::DeviceType::kKeyboard ||
                    a_event.idCode < 0 ||
                    a_event.idCode >= static_cast<std::int32_t>(keyDown.size())) {
                    return false;
                }

                const auto idCode = static_cast<std::size_t>(a_event.idCode);
                const bool down = a_event.value > 0.0F;
                const bool pressed = down && !keyDown[idCode];
                keyDown[idCode] = down;
                if (!pressed) {
                    return true;
                }

                constexpr std::int32_t kEscape = VK_ESCAPE;
                if (a_event.idCode == kEscape) {
                    EvidenceLog::Event("bridge_close", "native keyboard requested close");
                    CloseSession();
                } else {
                    return false;
                }
                return true;
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
                    model.SetMember("dirty", RE::Scaleform::GFx::Value(a_model.dirty)) &&
                    SetString(model, "error", a_model.error);
                for (std::uint32_t pageIndex = 0; populated && pageIndex < a_model.pages.size(); ++pageIndex) {
                    const auto& source = a_model.pages[pageIndex];
                    RE::Scaleform::GFx::Value page, controls;
                    root->CreateObject(&page); root->CreateArray(&controls);
                    populated = page.IsObject() && controls.IsArray() &&
                        SetString(page, "moduleId", source.moduleId) && SetString(page, "pageId", source.pageId) &&
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
            MenuSession::Session session;
            std::array<bool, 256> keyDown{};
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
            const auto inputPath = requestDirectory /
                                   std::format("AbsoluteControlPanelResearch.{}.input", runId);
            EvidenceLog::Event(
                "experiment_scheduled",
                std::format(
                    "require_arm={} advance_title={} arm_timeout_ms={} open_delay_ms={} "
                    "visible_ms={}",
                    requireArm, advanceTitleWithSendInput, armTimeout, openDelay,
                    visibleDuration));

            std::thread([
                            taskInterface, requireArm, advanceTitleWithSendInput, armTimeout,
                            openDelay, visibleDuration, armPath, advancePath, inputPath]() {
                std::uint32_t lastCommandId = 0;
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
                        PollResearchInputMailbox(taskInterface, inputPath, lastCommandId);
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

                // Keep the fixed research-input mailbox alive after the one-shot experiment
                // is armed.  This supports repeated PauseMenu build/teardown observations in
                // one loaded save instead of replaying title and save-load automation.
                std::thread([taskInterface, inputPath, lastCommandId]() mutable {
                    for (;;) {
                        PollResearchInputMailbox(taskInterface, inputPath, lastCommandId);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }).detach();

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
                    "path={} registration={} auto_open={} require_arm={} flags=0x{:08X}",
                    kConfigPath.string(), g_config.enableRegistration, g_config.autoOpen,
                    g_config.requireArm, g_config.menuFlags));

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
        ScheduleExperiment();
    }

    ProbePhase Phase() noexcept { return g_phase.load(std::memory_order_acquire); }
    bool IsRegistrationEnabled() noexcept { return g_config.enableRegistration; }
}
