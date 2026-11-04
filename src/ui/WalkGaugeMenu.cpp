#include "ui/WalkGaugeMenu.h"

#include "EvidenceLog.h"
#include "ui/MenuMessaging.h"

#include <atomic>
#include <new>

namespace AbsoluteControlPanelResearch::Ui::WalkGaugeMenu
{
    namespace
    {
        constexpr auto kMenuName = "AbsoluteWalkGauge";
        constexpr auto kRootPath = "_root";
        // What HUDMessagesMenu's constructor at 0x15E68C0 sets: an overlay above the HUD. It
        // draws over the game, and it stays out of input and doesn't pause anything.
        constexpr std::uint32_t kMenuFlags = 0x14080C00;
        constexpr std::uint8_t kMenuPriority = 12;

        enum class NativeFunction : std::uint64_t { Speed, Alpha };

        std::atomic<double> g_speed{ 1.0 };
        std::atomic<double> g_alpha{};

        class Menu final : public RE::GameMenuBase
        {
        public:
            SF_MENU_NAME("AbsoluteWalkGauge");

            static void* operator new(std::size_t size) { return ::operator new(size); }
            static void operator delete(void* memory) noexcept { ::operator delete(memory); }
            static void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }

            Menu()
            {
                // See the same steps in ControlPanelMenu.cpp, which explains each of them.
                unk128 = 0;
                unk130 = 0;
                menuName = kMenuName;
                SetFlags(kMenuFlags);
                *(reinterpret_cast<std::uint8_t*>(this) + 0x110) = kMenuPriority;
                inputEventHandlingEnabled = false;
            }

            const char* GetName() const override { return kMenuName; }
            const char* GetRootPath() const override { return kRootPath; }
            std::uint64_t GetUnk05() override { return 1; }
            bool UseEventDispatcher() override { return false; }
            bool ShouldHandleEvent(const RE::InputEvent*) override { return false; }
            bool Unk09(const RE::InputEvent*) override { return false; }

            // CommonLibSF has no current IDs for these. ControlPanelMenu.cpp has the notes.
            bool Unk0A() override { return uiMovie != nullptr; }
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
            std::uint64_t Unk19(void* a_arg1, std::int32_t a_arg2, std::int32_t a_arg3) override
            {
                using func_t = std::uint64_t (*)(RE::IMenu*, void*, std::int32_t, std::int32_t);
                static REL::Relocation<func_t> func{ REL::ID(130634) };
                return func(this, a_arg1, a_arg2, a_arg3);
            }
            float Unk1A() override
            {
                using func_t = float (*)(RE::IMenu*);
                static REL::Relocation<func_t> func{ REL::ID(130630) };
                return func(this);
            }
            RE::BSEventNotifyControl ProcessEvent(const RE::UpdateSceneRectEvent& a_event,
                RE::BSTEventSource<RE::UpdateSceneRectEvent>* a_source) override
            {
                using sink_t = RE::BSTEventSink<RE::UpdateSceneRectEvent>;
                using func_t = RE::BSEventNotifyControl (*)(sink_t*, const RE::UpdateSceneRectEvent&,
                    RE::BSTEventSource<RE::UpdateSceneRectEvent>*);
                static REL::Relocation<func_t> func{ REL::ID(130642) };
                return func(static_cast<sink_t*>(this), a_event, a_source);
            }

            bool LoadMovie(bool, bool a_arg2) override
            {
                const bool loaded = RE::IMenu::LoadMovie(false, a_arg2);
                if (!loaded || !uiMovie || !uiMovie->asMovieRoot) {
                    EvidenceLog::Event("walk_gauge_failed", "movie did not load");
                    return false;
                }
                uiMovie->asMovieRoot->GetVariable(&menuObj, kRootPath);
                if (!menuObj.IsObject() || !menuObj.HasMember(CODE_OBJ_NAME)) {
                    EvidenceLog::Event("walk_gauge_failed", "movie root missing");
                    return false;
                }
                MapCodeObjectFunctions();
                EvidenceLog::Event("walk_gauge_loaded");
                return true;
            }

            void MapCodeObjectFunctions() override
            {
                RegisterNativeFunction("speed", static_cast<std::uint64_t>(NativeFunction::Speed));
                RegisterNativeFunction("alpha", static_cast<std::uint64_t>(NativeFunction::Alpha));
            }

            void Call(const RE::Scaleform::GFx::FunctionHandler::Params& a_params) override
            {
                if (!a_params.ret) return;
                const auto function = static_cast<NativeFunction>(reinterpret_cast<std::uintptr_t>(a_params.userData));
                *a_params.ret = (function == NativeFunction::Speed ? g_speed : g_alpha).load(std::memory_order_acquire);
            }
        };

        RE::Scaleform::Ptr<RE::IMenu>* CreateMenu(RE::Scaleform::Ptr<RE::IMenu>* a_result)
        {
            // The engine passes empty return storage, as ControlPanelMenu.cpp describes.
            auto* menu = new Menu();
            menu->AddRef();
            *reinterpret_cast<RE::IMenu**>(a_result) = menu;
            return a_result;
        }
    }

    void Register() noexcept
    {
        try {
            const auto ui = RE::UI::GetSingleton();
            if (!ui) {
                EvidenceLog::Event("walk_gauge_failed", "UI singleton unavailable");
                return;
            }
            ui->RegisterMenu(kMenuName, &CreateMenu);
            const auto* entry = ui->GetMenuEntry(RE::BSFixedString{ kMenuName });
            EvidenceLog::Event(entry && entry->initFunc == &CreateMenu ? "walk_gauge_registered" : "walk_gauge_failed");
        } catch (...) {
            EvidenceLog::Event("walk_gauge_failed", "exception during registration");
        }
    }

    void Publish(double speed, double alpha) noexcept
    {
        g_speed.store(speed, std::memory_order_release);
        g_alpha.store(alpha, std::memory_order_release);
    }

    void EnsureOpen() noexcept
    {
        const auto* tasks = SFSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([]() {
            const auto ui = RE::UI::GetSingleton();
            if (ui && ui->IsMenuOpen(RE::BSFixedString{ kMenuName })) return;
            QueueNamedMenuMessage(kMenuName, RE::UI_MESSAGE_TYPE::kShow, "walk-speed");
        });
    }
}
