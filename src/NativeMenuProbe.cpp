#include "NativeMenuProbe.h"

namespace AbsoluteControlPanelResearch::NativeMenuProbe
{
    namespace
    {
        // Fail closed until Gate R1 supplies a reproducibly built SWF and validates the root path
        // in game. Turning this on before that evidence would register a menu that cannot load.
        constexpr bool kEnableRegistration = false;

        std::atomic<ProbePhase> g_phase{ ProbePhase::Cold };

        class AbsoluteControlPanelMenu final : public RE::GameMenuBase
        {
        public:
            SF_MENU_NAME("AbsoluteControlPanelMenu");

            AbsoluteControlPanelMenu()
            {
                menuName = kMenuName.data();
                SetFlags(RE::IMenu::ShowCursor);
            }

            const char* GetName() const override { return kMenuName.data(); }
            const char* GetRootPath() const override { return kRootPath.data(); }
            std::uint64_t GetUnk05() override { return 0; }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::UpdateSceneRectEvent&,
                RE::BSTEventSource<RE::UpdateSceneRectEvent>*) override
            {
                // R1 must verify whether forwarding to a vanilla implementation is necessary for
                // resize/UI-scale behavior. Returning continue is safe while registration is off.
                return RE::BSEventNotifyControl::kContinue;
            }

            bool LoadMovie(bool a_addEventDispatcher, bool a_arg2) override
            {
                const bool loaded = RE::GameMenuBase::LoadMovie(a_addEventDispatcher, a_arg2);
                REX::INFO("Native menu movie load result: {}", loaded);
                return loaded;
            }

            void MapCodeObjectFunctions() override
            {
                // Gate R2 will add the smallest possible versioned bridge: ready, close,
                // dispatch-command, request-snapshot, and report-focus. No module policy belongs
                // in this class.
            }
        };

        RE::Scaleform::Ptr<RE::IMenu>* CreateMenu(RE::Scaleform::Ptr<RE::IMenu>* a_result)
        {
            *a_result = RE::Scaleform::make_shared<AbsoluteControlPanelMenu>();
            return a_result;
        }

        void Transition(ProbeEvent a_event) noexcept
        {
            auto current = g_phase.load(std::memory_order_acquire);
            while (!g_phase.compare_exchange_weak(
                current, Advance(current, a_event), std::memory_order_acq_rel)) {
            }
        }
    }

    void OnDataReady() noexcept
    {
        Transition(ProbeEvent::PluginLoaded);
        Transition(ProbeEvent::DataReady);

        if constexpr (!kEnableRegistration) {
            REX::INFO(
                "Native menu registration is intentionally disabled until research Gate R1.");
            return;
        }

        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            Transition(ProbeEvent::RegistrationFailed);
            REX::ERROR("Starfield UI singleton is unavailable; registration failed closed.");
            return;
        }

        if (!ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()))) {
            ui->RegisterMenu(kMenuName.data(), &CreateMenu);
        }

        if (ui->IsMenuRegistered(RE::BSFixedString(kMenuName.data()))) {
            Transition(ProbeEvent::RegistrationEnabled);
            REX::INFO("Registered {} with Starfield's native UI manager.", kMenuName);
        } else {
            Transition(ProbeEvent::RegistrationFailed);
            REX::ERROR("Starfield did not retain the native-menu registration.");
        }
    }

    ProbePhase Phase() noexcept { return g_phase.load(std::memory_order_acquire); }
    bool IsRegistrationEnabled() noexcept { return kEnableRegistration; }
}
