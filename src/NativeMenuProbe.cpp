#include "NativeMenuProbe.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
#include "ProbeConfig.h"
#include "ResearchInputCapture.h"
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
            DispatchCommand,
            SnapshotApplied,
            PollInputCapture
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
                    "dispatchCommand",
                    static_cast<std::uint64_t>(NativeFunction::DispatchCommand));
                RegisterNativeFunction(
                    "snapshotApplied",
                    static_cast<std::uint64_t>(NativeFunction::SnapshotApplied));
                RegisterNativeFunction(
                    "pollInputCapture",
                    static_cast<std::uint64_t>(NativeFunction::PollInputCapture));
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
                    ResearchInputCapture::Initialize();
                    enumeratedDeviceCount = ResearchInputCapture::DeviceCount();
                    RefreshProviderState();
                    EvidenceLog::Event(
                        "bridge_ready",
                        std::format(
                            "argument_count={} input_devices={}", a_params.argCount,
                            enumeratedDeviceCount));
                    PublishSnapshot("ready");
                    break;
                case NativeFunction::Close:
                    EvidenceLog::Event("bridge_close", "ActionScript requested close");
                    QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
                    break;
                case NativeFunction::DispatchCommand: {
                    if (a_params.argCount < 2 || !a_params.args[1].IsString()) {
                        EvidenceLog::Event(
                            "bridge_command_rejected", "missing version or command");
                        break;
                    }

                    const auto version = ReadUnsigned(a_params.args[0]);
                    const std::string_view command = a_params.args[1].GetString();
                    EvidenceLog::Event(
                        "bridge_command",
                        std::format("version={} command={}", version, command));
                    if (version != 1) {
                        EvidenceLog::Event(
                            "bridge_command_rejected",
                            std::format("version={} command={}", version, command));
                        break;
                    }

                    if (!HandleUiCommand(command)) {
                        EvidenceLog::Event(
                            "bridge_command_rejected",
                            std::format("version={} command={}", version, command));
                    }
                    break;
                }
                case NativeFunction::SnapshotApplied: {
                    const auto generation = a_params.argCount >= 1 ?
                                                ReadUnsigned(a_params.args[0]) :
                                                std::numeric_limits<std::uint32_t>::max();
                    EvidenceLog::Event(
                        "bridge_snapshot_applied",
                        std::format(
                            "generation={} expected={} matches={}", generation,
                            snapshotGeneration, generation == snapshotGeneration));
                    break;
                }
                case NativeFunction::PollInputCapture: {
                    if (!captureActive) {
                        break;
                    }
                    const auto result = ResearchInputCapture::Poll();
                    if (result.state == ResearchInputCapture::PollResult::State::Captured) {
                        captureActive = false;
                        SlopApi::ValueV1 value;
                        value.kind = SlopApi::ValueKind::String;
                        std::memcpy(
                            value.stringValue, result.binding.data(),
                            std::min(
                                result.binding.size(),
                                SlopApi::kStringValueCapacity - 1));
                        const bool accepted = WriteProviderValue(
                            ResearchModule::kBindingId, value, "binding-captured");
                        RefreshProviderState();
                        ++snapshotGeneration;
                        EvidenceLog::Event(
                            "binding_capture_completed",
                            std::format(
                                "generation={} binding={} provider_accepted={}",
                                snapshotGeneration, result.binding, accepted));
                        PublishSnapshot("binding-captured");
                    } else if (
                        result.state == ResearchInputCapture::PollResult::State::TimedOut ||
                        result.state == ResearchInputCapture::PollResult::State::Fault) {
                        captureActive = false;
                        ++snapshotGeneration;
                        EvidenceLog::Event(
                            "binding_capture_ended",
                            result.state ==
                                    ResearchInputCapture::PollResult::State::TimedOut ?
                                "result=timeout" :
                                "result=fault");
                        PublishSnapshot("capture-ended");
                    }
                    break;
                }
                }
            }

            [[nodiscard]] bool HandleUiCommand(std::string_view a_command)
            {
                RefreshProviderState();
                SlopApi::ValueV1 value;
                std::string_view controlId;
                if (a_command == "toggleFeature") {
                    controlId = ResearchModule::kToggleId;
                    value.kind = SlopApi::ValueKind::Boolean;
                    value.booleanValue = featureEnabled ? 0U : 1U;
                } else if (a_command == "incrementLevel") {
                    controlId = ResearchModule::kSensitivityId;
                    value.kind = SlopApi::ValueKind::Integer;
                    value.integerValue = std::min(responseLevel + 5, 100);
                } else if (a_command == "decrementLevel") {
                    controlId = ResearchModule::kSensitivityId;
                    value.kind = SlopApi::ValueKind::Integer;
                    value.integerValue = std::max(responseLevel - 5, 0);
                } else if (a_command == "beginBindingCapture") {
                    captureActive = ResearchInputCapture::BeginButtonCapture();
                    if (!captureActive) {
                        EvidenceLog::Event(
                            "bridge_command_rejected",
                            "command=beginBindingCapture reason=capture-unavailable");
                        PublishSnapshot("capture-unavailable");
                        return true;
                    }
                } else {
                    return false;
                }

                if (!controlId.empty() && !WriteProviderValue(controlId, value, a_command)) {
                    EvidenceLog::Event(
                        "bridge_command_rejected",
                        std::format("command={} reason=provider-rejected", a_command));
                    return true;
                }
                RefreshProviderState();

                ++snapshotGeneration;
                EvidenceLog::Event(
                    "bridge_command_accepted",
                    std::format(
                        "command={} generation={} enabled={} level={}", a_command,
                        snapshotGeneration, featureEnabled, responseLevel));
                PublishSnapshot(a_command);
                return true;
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
                constexpr std::int32_t kAccept = 'E';
                constexpr std::int32_t kUp = 'W';
                constexpr std::int32_t kDown = 'S';
                constexpr std::int32_t kLeft = 'A';
                constexpr std::int32_t kRight = 'D';

                if (a_event.idCode == kEscape) {
                    EvidenceLog::Event("bridge_close", "native keyboard requested close");
                    QueueMenuMessage(RE::UI_MESSAGE_TYPE::kHide, "native-keyboard");
                } else if (a_event.idCode == kUp || a_event.idCode == kDown) {
                    selectedControl = a_event.idCode == kUp ?
                                          (selectedControl + 2) % 3 :
                                          (selectedControl + 1) % 3;
                    PublishFocus();
                } else if (a_event.idCode == kAccept) {
                    constexpr std::array<std::string_view, 3> commands{
                        "toggleFeature", "incrementLevel", "beginBindingCapture"
                    };
                    (void)HandleUiCommand(commands[selectedControl]);
                } else if (a_event.idCode == kLeft && selectedControl == 1) {
                    (void)HandleUiCommand("decrementLevel");
                } else if (a_event.idCode == kRight && selectedControl == 1) {
                    (void)HandleUiCommand("incrementLevel");
                } else {
                    return false;
                }
                return true;
            }

            void PublishFocus()
            {
                const RE::Scaleform::GFx::Value focus(selectedControl);
                const bool invoked = menuObj.IsObject() &&
                                     menuObj.Invoke("applyFocus", nullptr, &focus, 1);
                EvidenceLog::Event(
                    "menu_focus_published",
                    std::format("index={} invoked={}", selectedControl, invoked));
            }

            void PublishSnapshot(std::string_view a_source)
            {
                RefreshProviderState();
                if (!uiMovie || !uiMovie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event(
                        "bridge_snapshot_publish_failed", "movie root unavailable");
                    return;
                }

                RE::Scaleform::GFx::Value snapshot;
                RE::Scaleform::GFx::Value binding;
                uiMovie->asMovieRoot->CreateObject(&snapshot);
                uiMovie->asMovieRoot->CreateString(&binding, capturedBinding.c_str());
                const bool populated = snapshot.IsObject() &&
                                       snapshot.SetMember(
                                           "generation",
                                           RE::Scaleform::GFx::Value(snapshotGeneration)) &&
                                       snapshot.SetMember(
                                           "enabled",
                                           RE::Scaleform::GFx::Value(featureEnabled)) &&
                                       snapshot.SetMember(
                                           "level",
                                           RE::Scaleform::GFx::Value(responseLevel)) &&
                                       snapshot.SetMember(
                                           "deviceCount",
                                           RE::Scaleform::GFx::Value(enumeratedDeviceCount)) &&
                                       snapshot.SetMember(
                                           "captureActive",
                                           RE::Scaleform::GFx::Value(captureActive)) &&
                                       snapshot.SetMember("binding", binding);
                const bool invoked = populated &&
                                     menuObj.Invoke("applySnapshot", nullptr, &snapshot, 1);
                EvidenceLog::Event(
                    "bridge_snapshot_published",
                    std::format(
                        "source={} generation={} enabled={} level={} devices={} "
                        "capture_active={} binding={} populated={} invoked={}",
                        a_source, snapshotGeneration, featureEnabled, responseLevel,
                        enumeratedDeviceCount, captureActive, capturedBinding, populated,
                        invoked));
            }

            [[nodiscard]] bool EnsureProvider()
            {
                if (!providerPage) {
                    providerPage = MenuApiHost::FindPage(
                        ResearchModule::kModuleId, ResearchModule::kPageId);
                    EvidenceLog::Event(
                        providerPage ? "api_page_selected" : "api_page_selection_failed",
                        std::format(
                            "module={} page={} revision={}", ResearchModule::kModuleId,
                            ResearchModule::kPageId, MenuApiHost::Revision()));
                }
                return providerPage.has_value();
            }

            void RefreshProviderState()
            {
                if (!EnsureProvider() || !providerPage->readValue) {
                    return;
                }

                SlopApi::ValueV1 value;
                auto result = providerPage->readValue(
                    providerPage->context, ResearchModule::kToggleId.data(), &value);
                if (result == SlopApi::Result::Ok &&
                    value.kind == SlopApi::ValueKind::Boolean) {
                    featureEnabled = value.booleanValue != 0;
                }

                value = {};
                result = providerPage->readValue(
                    providerPage->context, ResearchModule::kSensitivityId.data(), &value);
                if (result == SlopApi::Result::Ok &&
                    value.kind == SlopApi::ValueKind::Integer) {
                    responseLevel = static_cast<std::int32_t>(value.integerValue);
                }

                value = {};
                result = providerPage->readValue(
                    providerPage->context, ResearchModule::kBindingId.data(), &value);
                if (result == SlopApi::Result::Ok &&
                    value.kind == SlopApi::ValueKind::String &&
                    std::memchr(
                        value.stringValue, '\0',
                        SlopApi::kStringValueCapacity)) {
                    capturedBinding = value.stringValue;
                }
            }

            [[nodiscard]] bool WriteProviderValue(
                std::string_view a_controlId,
                const SlopApi::ValueV1& a_value,
                std::string_view a_source)
            {
                if (!EnsureProvider() || !providerPage->writeDraft) {
                    return false;
                }
                const std::string controlId{ a_controlId };
                auto result = providerPage->writeDraft(
                    providerPage->context, controlId.c_str(), &a_value);
                EvidenceLog::Event(
                    "api_draft_write",
                    std::format(
                        "module={} page={} control={} source={} result={}",
                        providerPage->moduleId, providerPage->pageId, controlId, a_source,
                        static_cast<std::uint32_t>(result)));
                if (result != SlopApi::Result::Ok) {
                    return false;
                }
                if (providerPage->apply) {
                    result = providerPage->apply(providerPage->context);
                    EvidenceLog::Event(
                        "api_page_apply",
                        std::format(
                            "module={} page={} source={} result={}",
                            providerPage->moduleId, providerPage->pageId, a_source,
                            static_cast<std::uint32_t>(result)));
                }
                const auto* api = SLOP_QueryApi(
                    SlopApi::kAbiVersion);
                if (result == SlopApi::Result::Ok && api &&
                    api->requestRefresh) {
                    (void)api->requestRefresh(
                        providerPage->moduleId.c_str(), providerPage->pageId.c_str());
                }
                return result == SlopApi::Result::Ok;
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
                if (a_value.IsNumber() && a_value.GetNumber() >= 0.0) {
                    return static_cast<std::uint32_t>(a_value.GetNumber());
                }
                return std::numeric_limits<std::uint32_t>::max();
            }

            [[nodiscard]] std::int32_t RefCountForEvidence() const noexcept
            {
                return refCount;
            }

        private:
            bool featureEnabled{ false };
            std::int32_t responseLevel{ 50 };
            std::uint32_t snapshotGeneration{ 0 };
            std::uint32_t enumeratedDeviceCount{ 0 };
            bool captureActive{ false };
            std::string capturedBinding{ "(unbound)" };
            std::int32_t selectedControl{ 0 };
            std::array<bool, 256> keyDown{};
            std::optional<MenuApiHost::Page> providerPage;
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
