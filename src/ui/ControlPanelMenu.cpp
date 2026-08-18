

#include "NativeMenuProbe.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
#include "MenuInputRouter.h"
#include "MenuSession.h"
#include "input/PlatformInputServices.h"
#include "runtime/ProbeRuntimeState.h"
#include "runtime/RuntimeCompatibility.h"
#include "scaleform/ScaleformMenuBridge.h"
#include "ui/ControlPanelMenu.h"
#include "ui/MenuAudioIntegration.h"
#include "ui/MenuMessaging.h"

namespace AbsoluteControlPanelResearch::Ui::ControlPanelMenu
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

        constexpr auto kMenuName = NativeMenuProbe::kMenuName;
        constexpr auto kMoviePath = NativeMenuProbe::kMoviePath;
        constexpr auto kRootPath = NativeMenuProbe::kRootPath;
        std::uint32_t g_menuFlags{};

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
                SetFlags(g_menuFlags);
                // Current UI insertion sorts by the byte at +0x110. GameMenuBase starts at
                // priority 6. PauseMenu remains resident at observed priority 11 when this
                // panel is opened from its injected row; priority 19 covers and owns input
                // above it while remaining below CursorMenu (20).
                *(
                    reinterpret_cast<std::uint8_t*>(this) + 0x110) = 19;
                EvidenceLog::Event(
                    "menu_constructed",
                    std::format(
                        "flags=0x{:08X} priority={} ref_count={}", g_menuFlags,
                        *(reinterpret_cast<std::uint8_t*>(this) + 0x110),
                        RefCountForEvidence()));
            }

            ~AbsoluteControlPanelMenu() override
            {
                audioLease.Release("menu-destructor");
                (void)bridge.OnHidden();
                MenuApiHost::SetMenuOpen(false);
                EvidenceLog::Event(
                    "menu_destructor_entered",
                    std::format(
                        "ref_count={} caller_rva=0x{:08X}", RefCountForEvidence(),
                        Runtime::ToImageRva(_ReturnAddress())));
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
                                    a_event->deviceType != RE::InputEvent::DeviceType::kMouse &&
                                    a_event->deviceType != RE::InputEvent::DeviceType::kGamepad)) {
                    return false;
                }
                return RE::IMenu::ShouldHandleEvent(a_event);
            }

            // CommonLibSF's generated declarations still carry ID 0 for several
            // inherited menu virtuals even though Address Library 1.16.244 exposes
            // their current mappings.  Delegate explicitly: calling the inherited
            // wrappers would resolve the executable base, while replacing viewport
            // behavior with a stub leaves an ultrawide game in the movie's 16:9 rect.
            bool Unk0A() override
            {
                // Current IMenu ID 130619 implements this slot as
                // `return uiMovie != nullptr`.  The UI show processor uses a false
                // result as the condition for immediate menu cleanup.
                return uiMovie != nullptr;
            }
            bool Unk15(void*) override { return false; }
            std::uint64_t Unk10() override
            {
                using func_t = std::uint64_t (*)(RE::IMenu*);
                static REL::Relocation<func_t> func{ REL::ID(93620) };
                return func(this);
            }
            std::uint64_t Unk11() override
            {
                using func_t = std::uint64_t (*)(RE::IMenu*);
                static REL::Relocation<func_t> func{ REL::ID(93621) };
                return func(this);
            }
            std::uint64_t Unk18(void* a_arg1, std::uint64_t a_arg2) override
            {
                using func_t = std::uint64_t (*)(RE::IMenu*, void*, std::uint64_t);
                static REL::Relocation<func_t> func{ REL::ID(130625) };
                return func(this, a_arg1, a_arg2);
            }
            std::uint64_t Unk19(
                void* a_arg1, std::int32_t a_arg2, std::int32_t a_arg3) override
            {
                using func_t = std::uint64_t (*)(
                    RE::IMenu*, void*, std::int32_t, std::int32_t);
                static REL::Relocation<func_t> func{ REL::ID(130634) };
                return func(this, a_arg1, a_arg2, a_arg3);
            }
            float Unk1A() override
            {
                using func_t = float (*)(RE::IMenu*);
                static REL::Relocation<func_t> func{ REL::ID(130630) };
                return func(this);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::UpdateSceneRectEvent& a_event,
                RE::BSTEventSource<RE::UpdateSceneRectEvent>* a_source) override
            {
                using sink_t = RE::BSTEventSink<RE::UpdateSceneRectEvent>;
                using func_t = RE::BSEventNotifyControl (*)(
                    sink_t*, const RE::UpdateSceneRectEvent&,
                    RE::BSTEventSource<RE::UpdateSceneRectEvent>*);
                static REL::Relocation<func_t> func{ REL::ID(130642) };
                return func(static_cast<sink_t*>(this), a_event, a_source);
            }

            bool Unk09(const RE::InputEvent* a_event) override
            {
                bool handled = false;
                if (a_event && inputEventHandlingEnabled) {
                    switch (a_event->eventType) {
                    case RE::InputEvent::EventType::kButton: {
                        const auto* button = static_cast<const RE::ButtonEvent*>(a_event);
                        handled = bridge.HandleButtonInput(*button);
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
                bool returnToPause{};
                EvidenceLog::Event(
                    "menu_message_entered",
                    std::format(
                        "type={} caller_rva=0x{:08X}",
                        static_cast<std::uint32_t>(typeBefore),
                        Runtime::ToImageRva(_ReturnAddress())));
                if (typeBefore == RE::UI_MESSAGE_TYPE::kHide) {
                    // Disarm before the base class starts tearing down the movie.
                    // Pointer tasks already queued by the polling thread must become
                    // harmless before any Scaleform object can be invalidated.
                    returnToPause = bridge.OnHidden();
                }
                const auto result = RE::IMenu::ProcessMessage(a_message);
                EvidenceLog::Event(
                    "menu_message_base_completed",
                    std::format(
                        "type_before={} type_after={} result={}",
                        static_cast<std::uint32_t>(typeBefore),
                        static_cast<std::uint32_t>(a_message.type),
                        static_cast<std::int64_t>(result)));
                if (typeBefore == RE::UI_MESSAGE_TYPE::kShow) {
                    // Custom menu names are not recognized by Starfield's
                    // hard-coded MenuAudioHandler. Acquire the same balanced,
                    // ref-counted mode lease used by PauseMenu.
                    audioLease.Acquire("menu-show");
                    bridge.OnShown();
                    MenuApiHost::SetMenuOpen(true);
                    Runtime::Transition(ProbeEvent::MenuOpened);
                    EvidenceLog::Event(
                        "menu_message_show",
                        std::format("result={}", static_cast<std::int64_t>(result)));
                } else if (typeBefore == RE::UI_MESSAGE_TYPE::kHide) {
                    audioLease.Release("menu-hide");
                    MenuApiHost::SetMenuOpen(false);
                    Runtime::Transition(ProbeEvent::MenuClosed);
                    EvidenceLog::Event(
                        "menu_message_hide",
                        std::format("result={}", static_cast<std::int64_t>(result)));
                    const auto ui = RE::UI::GetSingleton();
                    const bool pauseOpen = ui && ui->IsMenuOpen(
                                                     RE::BSFixedString("PauseMenu"));
                    EvidenceLog::Event(
                        "research_pause_state_after_probe_close",
                        std::format("open={}", pauseOpen));
                    if (returnToPause) {
                        if (pauseOpen) {
                            EvidenceLog::Event(
                                "control_panel_return_revealed",
                                "target=PauseMenu source=resident-underlay");
                        } else {
                            // A UI overhaul or another plugin may still remove the
                            // underlay. Recover the promised Pause-origin back target
                            // without issuing a duplicate Show in the normal path.
                            Ui::QueueNamedMenuMessage(
                                "PauseMenu", RE::UI_MESSAGE_TYPE::kShow,
                                "control-panel-return-recovery");
                            EvidenceLog::Event(
                                "control_panel_return_queued",
                                "target=PauseMenu source=missing-underlay-recovery");
                        }
                    }
                }
                return result;
            }

            bool LoadMovie(bool, bool a_arg2) override
            {
                EvidenceLog::Event(
                    "movie_load_entered",
                    std::format(
                        "arg2={} ref_count={} caller_rva=0x{:08X}", a_arg2,
                        RefCountForEvidence(), Runtime::ToImageRva(_ReturnAddress())));
                const bool loaded = RE::IMenu::LoadMovie(false, a_arg2);
                EvidenceLog::Event(
                    "movie_load_result", std::format("loaded={}", loaded));
                if (!loaded || !uiMovie || !uiMovie->asMovieRoot) {
                    Runtime::Transition(ProbeEvent::RuntimeFault);
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
                    bridge.Attach(uiMovie.get(), menuObj);
                    MapCodeObjectFunctions();
                    bridge.LogMovieState("before_on_code_obj_create");
                    const bool invoked = menuObj.Invoke(ON_CODE_OBJ_CREATED_FUNC);
                    EvidenceLog::Event(
                        "bridge_create_invoked", std::format("invoked={}", invoked));
                    bridge.LogMovieState("after_on_code_obj_create");
                } else {
                    EvidenceLog::Event(
                        "bridge_root_missing", "queueing fail-closed menu hide");
                    Runtime::Transition(ProbeEvent::RuntimeFault);
                    Ui::QueueControlPanelMessage(
                        RE::UI_MESSAGE_TYPE::kHide, "bridge_root_missing");
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
                    "ready", static_cast<std::uint64_t>(Scaleform::NativeFunction::Ready));
                RegisterNativeFunction(
                    "close", static_cast<std::uint64_t>(Scaleform::NativeFunction::Close));
                RegisterNativeFunction(
                    "dispatch", static_cast<std::uint64_t>(Scaleform::NativeFunction::Dispatch));
                RegisterNativeFunction(
                    "compound", static_cast<std::uint64_t>(Scaleform::NativeFunction::Compound));
                RegisterNativeFunction(
                    "focus", static_cast<std::uint64_t>(Scaleform::NativeFunction::Focus));
                RegisterNativeFunction(
                    "modelApplied", static_cast<std::uint64_t>(Scaleform::NativeFunction::ModelApplied));
                EvidenceLog::Event("bridge_functions_mapped", "version=1 count=6");
            }

            void Call(
                const RE::Scaleform::GFx::FunctionHandler::Params& a_params) override
            {
                bridge.Call(a_params);
            }

            void HandlePointerPhase(Input::PointerPhase a_phase)
            {
                bridge.HandlePointerPhase(a_phase);
            }

            [[nodiscard]] std::int32_t RefCountForEvidence() const noexcept
            {
                return refCount;
            }

        private:
            Ui::PauseMenuAudioLease audioLease;
            Scaleform::MenuBridge bridge;
        };

        RE::Scaleform::Ptr<RE::IMenu>* CreateMenu(RE::Scaleform::Ptr<RE::IMenu>* a_result)
        {
            EvidenceLog::Event(
                "menu_factory_entered",
                std::format(
                    "return_storage_bits=0x{:X} caller_rva=0x{:08X}",
                    reinterpret_cast<std::uintptr_t>(
                        *reinterpret_cast<RE::IMenu**>(a_result)),
                    Runtime::ToImageRva(_ReturnAddress())));
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

    }

    bool Register(std::uint32_t a_menuFlags) noexcept
    {
        g_menuFlags = a_menuFlags;
        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return false;
        }
        if (!ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()))) {
            ui->RegisterMenu(kMenuName.data(), &CreateMenu);
        }
        return ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()));
    }

    void RequestOpenFromHotkey() noexcept
    {
        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            EvidenceLog::Event("open_hotkey_rejected", "UI singleton unavailable");
            return;
        }
        const RE::BSFixedString menuName{ kMenuName.data() };
        if (ui->IsMenuOpen(menuName)) {
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
        EvidenceLog::Event(
            "open_hotkey_requested",
            std::format("virtual_key=0x{:02X}", Runtime::Config().openHotkey));
        Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kShow, "hotkey");
    }

    void DispatchPointerPhase(Input::PointerPhase a_phase) noexcept
    {
        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return;
        }
        const RE::BSFixedString menuName{ kMenuName.data() };
        if (!ui->IsMenuOpen(menuName)) {
            return;
        }
        auto menu = ui->GetMenu(menuName);
        if (menu) {
            static_cast<AbsoluteControlPanelMenu*>(menu.get())->HandlePointerPhase(a_phase);
        }
    }
}
