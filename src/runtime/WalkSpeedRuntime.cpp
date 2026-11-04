#include "runtime/WalkSpeedRuntime.h"

#include "EvidenceLog.h"
#include "ui/WalkGaugeMenu.h"

#include <atomic>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string_view>
#include <thread>

// The movement mechanism is adapted from Analog Walk Keys by Sator (GPL-3.0),
// https://www.nexusmods.com/starfield/mods/17709: let the game handle the movement key,
// then call its own thumbstick handler with the key's direction times the chosen speed.
// CommonLibSF only forward-declares this event. Its layout is IDEvent plus the stick axes.
namespace RE
{
    class ThumbstickEvent : public IDEvent
    {
    public:
        ~ThumbstickEvent() override = default;

        float xValue{};
        float yValue{};
    };
    static_assert(sizeof(ThumbstickEvent) == 0x40);
}

namespace AbsoluteControl::Runtime::WalkSpeedRuntime
{
    namespace
    {
        namespace Log = AbsoluteControlPanelResearch::EvidenceLog;
        using OnThumbstick = void (*)(void*, const RE::ThumbstickEvent*);
        using OnButton = void (*)(void*, const RE::ButtonEvent*);

        constexpr std::int32_t kLeftThumbstickId = 0x0B;
        constexpr auto kSaveDelayNanoseconds = 2'000'000'000ULL;

        OnThumbstick g_originalThumbstick{};
        OnButton g_originalButton{};
        OnButton g_originalFirstPerson{};
        OnButton g_originalThirdPerson{};
        std::atomic<void*> g_handler{};
        std::atomic_bool g_forward{}, g_back{}, g_left{}, g_right{};
        std::atomic_bool g_moveReady{}, g_wheelAllowed{}, g_injecting{}, g_refreshQueued{};
        std::atomic<double> g_speed{ 1.0 };
        std::atomic_bool g_sourceAxis{};
        std::atomic_int g_pendingWheel{}, g_wheelKey{};

        std::mutex g_mutex;
        WalkSpeedSettings g_settings;
        std::filesystem::path g_writePath;

        [[nodiscard]] bool IsOnFootCamera(std::uint32_t state) noexcept
        {
            using Camera = RE::CameraState;
            return state == Camera::kFirstPerson || state == Camera::kIronSights ||
                state == Camera::kPCTransition || state == Camera::kThirdPerson;
        }

        [[nodiscard]] bool IsGameForeground() noexcept
        {
            DWORD process{};
            ::GetWindowThreadProcessId(::GetForegroundWindow(), &process);
            return process == ::GetCurrentProcessId();
        }

        [[nodiscard]] bool WriteSettings(const WalkSpeedSettings& settings) noexcept
        {
            try {
                auto temporary = g_writePath;
                temporary += ".tmp";
                {
                    std::ofstream stream{ temporary, std::ios::out | std::ios::trunc };
                    stream << FormatWalkSpeedSettings(settings);
                    stream.flush();
                    if (!stream) return false;
                }
                if (::MoveFileExW(temporary.c_str(), g_writePath.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) return false;
                Log::Event("walk_settings_saved", std::format("speed={:.2f}", settings.speed));
                return true;
            } catch (...) {
                return false;
            }
        }

        void UpdateMoveKeys(const RE::ButtonEvent* event)
        {
            const auto* name = event->strUserEvent.c_str();
            if (!name) return;
            const std::string_view userEvent{ name };
            std::atomic_bool* key{};
            if (userEvent == "Forward") key = &g_forward;
            else if (userEvent == "Back") key = &g_back;
            else if (userEvent == "StrafeLeft" || userEvent == "StrafeLeft_MainGameplay" ||
                userEvent == "Strafe Left") key = &g_left;
            else if (userEvent == "StrafeRight" || userEvent == "StrafeRight_MainGameplay" ||
                userEvent == "StrafeRightMainGameplay" || userEvent == "Strafe Right") key = &g_right;
            if (!key) return;
            // Back and Strafe Left report negative values, so only magnitude means pressed.
            key->store(!event->disabled && std::fabs(event->value) > 0.0001F, std::memory_order_release);
        }

        void Emit(void* handler)
        {
            if (!handler || !g_originalThumbstick) return;
            const MoveKeys keys{ g_forward.load(std::memory_order_acquire), g_back.load(std::memory_order_acquire),
                g_left.load(std::memory_order_acquire), g_right.load(std::memory_order_acquire) };
            const auto speed = g_speed.load(std::memory_order_acquire);
            const bool moving = g_moveReady.load(std::memory_order_acquire) && speed < kFullSpeed &&
                (keys.forward != keys.back || keys.left != keys.right);
            // Never send a neutral stick unless this is what moved it, so normal keys keep working.
            if (!moving && !g_injecting.load(std::memory_order_acquire)) return;
            const auto stick = moving ? WalkStick(keys, speed) : StickVector{};
            RE::ThumbstickEvent event;
            event.deviceType = RE::InputEvent::DeviceType::kGamepad;
            event.eventType = RE::InputEvent::EventType::kThumbstick;
            event.idCode = kLeftThumbstickId;
            event.disabled = false;
            event.xValue = stick.x;
            event.yValue = stick.y;
            g_originalThumbstick(handler, &event);
            g_injecting.store(moving, std::memory_order_release);
        }

        void OnButtonShim(void* handler, const RE::ButtonEvent* event)
        {
            const bool keyboard = event && event->deviceType == RE::InputEvent::DeviceType::kKeyboard;
            if (keyboard) UpdateMoveKeys(event);
            if (g_originalButton) g_originalButton(handler, event);
            if (handler && keyboard) {
                // This has to follow the original, or its digital update overwrites the speed.
                g_handler.store(handler, std::memory_order_release);
                Emit(handler);
            }
        }

        void QueueRefresh()
        {
            if (g_refreshQueued.exchange(true, std::memory_order_acq_rel)) return;
            const auto* tasks = SFSE::GetTaskInterface();
            if (!tasks) {
                g_refreshQueued.store(false, std::memory_order_release);
                return;
            }
            tasks->AddTask([]() {
                g_refreshQueued.store(false, std::memory_order_release);
                Emit(g_handler.load(std::memory_order_acquire));
            });
        }

        [[nodiscard]] bool WheelModifierHeld() noexcept
        {
            if (!g_wheelAllowed.load(std::memory_order_acquire)) return false;
            const auto key = g_wheelKey.load(std::memory_order_acquire);
            return key && (::GetAsyncKeyState(key) & 0x8000);
        }

        // Only watches. Swallowing the scroll here was tested and doesn't reach the game's input.
        LRESULT CALLBACK WheelHook(int code, WPARAM message, LPARAM data)
        {
            if (code == HC_ACTION && message == WM_MOUSEWHEEL && WheelModifierHeld()) {
                const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
                g_pendingWheel.fetch_add(GET_WHEEL_DELTA_WPARAM(info->mouseData), std::memory_order_acq_rel);
            }
            return ::CallNextHookEx(nullptr, code, message, data);
        }

        // The camera states act on these user events directly, without checking the game's
        // WheelZoom flag, so this is where a scroll that sets the speed has to stop.
        [[nodiscard]] bool SkipZoom(const RE::ButtonEvent* event)
        {
            if (!event || !WheelModifierHeld()) return false;
            const auto* name = event->strUserEvent.c_str();
            if (!name) return false;
            const std::string_view userEvent{ name };
            if (userEvent != "ZoomIn" && userEvent != "ZoomOut") return false;
            Log::Event("walk_zoom_skipped", std::string{ userEvent });
            return true;
        }

        void OnFirstPersonButton(void* handler, const RE::ButtonEvent* event)
        {
            if (!SkipZoom(event) && g_originalFirstPerson) g_originalFirstPerson(handler, event);
        }

        void OnThirdPersonButton(void* handler, const RE::ButtonEvent* event)
        {
            if (!SkipZoom(event) && g_originalThirdPerson) g_originalThirdPerson(handler, event);
        }

        // Started the first time it's needed, and lives as long as the process.
        void StartWheelThread()
        {
            static std::once_flag once;
            std::call_once(once, []() {
                std::thread([]() {
                    const auto hook = ::SetWindowsHookExW(WH_MOUSE_LL, &WheelHook, ::GetModuleHandleW(nullptr), 0);
                    Log::Event(hook ? "walk_wheel_hook_installed" : "walk_wheel_hook_failed");
                    MSG message;
                    while (hook && ::GetMessageW(&message, nullptr, 0, 0) > 0) {}
                }).detach();
            });
        }

        [[nodiscard]] int WheelKey(WheelModifier modifier) noexcept
        {
            switch (modifier) {
            case WheelModifier::Alt: return VK_MENU;
            case WheelModifier::Shift: return VK_SHIFT;
            case WheelModifier::Ctrl: return VK_CONTROL;
            default: return 0;
            }
        }

        struct KeyPair
        {
            int up{};
            int down{};
        };

        [[nodiscard]] KeyPair SpeedKeyPair(SpeedKeys keys) noexcept
        {
            switch (keys) {
            case SpeedKeys::Numpad: return { VK_ADD, VK_SUBTRACT };
            case SpeedKeys::Brackets: return { VK_OEM_6, VK_OEM_4 };
            case SpeedKeys::PageKeys: return { VK_PRIOR, VK_NEXT };
            default: return {};
            }
        }

        struct InputFlags
        {
            bool move{};
            bool wheel{};
        };

        [[nodiscard]] InputFlags ReadInputFlags()
        {
            auto* manager = RE::BSInputEnableManager::GetSingleton();
            if (!manager) return {};
            std::uint32_t flags{};
            {
                RE::BSAutoLock lock{ manager->m_cacheLock };
                flags = static_cast<std::uint32_t>(manager->m_cachedUserEventFlags);
            }
            const auto required = static_cast<std::uint32_t>(RE::USER_EVENT_FLAG::Walking) |
                static_cast<std::uint32_t>(RE::USER_EVENT_FLAG::Looking);
            // The game turns WheelZoom off when the wheel belongs to a list, so this never
            // takes scrolling from a menu.
            const auto zoom = static_cast<std::uint32_t>(RE::USER_EVENT_FLAG::WheelZoom);
            return { (flags & required) == required, (flags & required) == required && (flags & zoom) == zoom };
        }
    }

    void Install(const std::filesystem::path& readFile, const std::filesystem::path& writeFile) noexcept
    {
        try {
            {
                const std::scoped_lock lock{ g_mutex };
                g_writePath = writeFile;
                IniValues ini;
                std::ifstream stream{ readFile, std::ios::binary };
                if (stream) ini.Merge(std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() });
                g_settings = ParseWalkSpeedSettings(ini);
                g_speed.store(g_settings.speed, std::memory_order_release);
                Log::Event("walk_settings", std::format("enabled={} axis={} wheel={} keys={} step={:.2f} speed={:.3f}",
                    g_settings.enabled ? 1 : 0, g_settings.axis, static_cast<std::uint32_t>(g_settings.wheelModifier),
                    static_cast<std::uint32_t>(g_settings.speedKeys), g_settings.step, g_settings.speed));
            }
            if (::GetModuleHandleW(L"AnalogWalkKeys.dll")) {
                Log::Event("walk_hook_skipped", "AnalogWalkKeys.dll owns MovementHandler");
                return;
            }
            REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PlayerControls__MovementHandler[0] };
            const auto* entries = reinterpret_cast<const std::uintptr_t*>(vtable.address());
            g_originalThumbstick = reinterpret_cast<OnThumbstick>(entries[4]);
            g_originalButton = reinterpret_cast<OnButton>(vtable.write_vfunc(8, &OnButtonShim));
            REL::Relocation<std::uintptr_t> firstPerson{ RE::VTABLE::FirstPersonState[0] };
            g_originalFirstPerson = reinterpret_cast<OnButton>(firstPerson.write_vfunc(8, &OnFirstPersonButton));
            REL::Relocation<std::uintptr_t> thirdPerson{ RE::VTABLE::ThirdPersonState[0] };
            g_originalThirdPerson = reinterpret_cast<OnButton>(thirdPerson.write_vfunc(8, &OnThirdPersonButton));
            Log::Event("walk_hook_installed", "MovementHandler, FirstPersonState and ThirdPersonState slot 8");
        } catch (...) {
            Log::Event("walk_hook_failed", "exception during install");
        }
    }

    WalkSpeedSettings Settings()
    {
        const std::scoped_lock lock{ g_mutex };
        return g_settings;
    }

    bool ApplySettings(const WalkSpeedSettings& settings)
    {
        const std::scoped_lock lock{ g_mutex };
        auto next = settings;
        next.speed = g_settings.speed;
        if (!WriteSettings(next)) return false;
        g_settings = next;
        return true;
    }

    Status CurrentStatus() noexcept
    {
        return { g_speed.load(std::memory_order_acquire),
            g_sourceAxis.load(std::memory_order_acquire) ? WalkSpeedSource::Axis : WalkSpeedSource::Virtual,
            g_moveReady.load(std::memory_order_acquire) };
    }

    std::string AxisToken()
    {
        const std::scoped_lock lock{ g_mutex };
        return g_settings.axis;
    }

    void Tick(const std::optional<FlightSignals>& signals, std::optional<double> axisValue, std::uint64_t now)
    {
        // Only this thread touches these.
        static WalkSpeedSelector selector{ g_speed.load(std::memory_order_acquire) };
        static SpeedKeyRepeat keyUp, keyDown;
        static double loggedSpeed = -1;
        static bool loggedAxis{}, loggedReady{}, readyKnown{};
        static double savedSpeed = selector.Virtual();
        static double pendingSpeed = savedSpeed;
        static std::uint64_t virtualChangedAt{};
        static SpeedGauge gauge;
        static bool gaugeShown{};

        WalkSpeedSettings settings;
        {
            const std::scoped_lock lock{ g_mutex };
            settings = g_settings;
        }

        const bool onFoot = signals && signals->flag == 0 && signals->sit == 0 && signals->tm == 0 &&
            signals->active != 0 && IsOnFootCamera(signals->cam) && IsGameForeground();
        const auto input = onFoot ? ReadInputFlags() : InputFlags{};
        const bool ready = settings.enabled && onFoot && input.move;

        if (settings.enabled && settings.wheelModifier != WheelModifier::Off) StartWheelThread();
        g_wheelKey.store(WheelKey(settings.wheelModifier), std::memory_order_release);
        g_wheelAllowed.store(ready && input.wheel, std::memory_order_release);

        selector.ObserveAxis(axisValue ? std::optional{ AxisToSpeed(*axisValue, settings.invertAxis) } : std::nullopt);
        if (const auto wheel = g_pendingWheel.exchange(0, std::memory_order_acq_rel)) {
            selector.Nudge(wheel / static_cast<double>(WHEEL_DELTA) * settings.step);
        }
        const auto keys = SpeedKeyPair(settings.speedKeys);
        const auto pressed = [&](int key) { return ready && key && (::GetAsyncKeyState(key) & 0x8000) != 0; };
        const auto delta = keyUp.Update(pressed(keys.up), now, settings.step) -
            keyDown.Update(pressed(keys.down), now, settings.step);
        if (delta != 0.0) selector.Nudge(delta);

        const auto speed = selector.Speed();
        const bool fromAxis = selector.Source() == WalkSpeedSource::Axis;
        g_speed.store(speed, std::memory_order_release);
        g_sourceAxis.store(fromAxis, std::memory_order_release);
        if (std::abs(speed - loggedSpeed) >= 0.005 || fromAxis != loggedAxis) {
            Log::Event("walk_speed", std::format("speed={:.2f} source={}", speed, fromAxis ? "axis" : "virtual"));
            loggedSpeed = speed;
            loggedAxis = fromAxis;
        }

        if (!readyKnown || ready != loggedReady) {
            Log::Event("walk_ready", std::format("ready={}", ready ? 1 : 0));
            loggedReady = ready;
            readyKnown = true;
        }
        gauge.Update(speed, ready && settings.showGauge, now);
        AbsoluteControlPanelResearch::Ui::WalkGaugeMenu::Publish(speed, gauge.Alpha());
        if (gauge.Visible() != gaugeShown) {
            gaugeShown = gauge.Visible();
            if (gaugeShown) AbsoluteControlPanelResearch::Ui::WalkGaugeMenu::EnsureOpen();
            Log::Event("walk_gauge", std::format("shown={}", gaugeShown ? 1 : 0));
        }

        const bool wasReady = g_moveReady.exchange(ready, std::memory_order_acq_rel);
        if (!ready) {
            // The next hold event sets these again, so a key released during a menu can't stick.
            g_forward.store(false, std::memory_order_release);
            g_back.store(false, std::memory_order_release);
            g_left.store(false, std::memory_order_release);
            g_right.store(false, std::memory_order_release);
        }
        const bool anyKey = g_forward.load(std::memory_order_acquire) || g_back.load(std::memory_order_acquire) ||
            g_left.load(std::memory_order_acquire) || g_right.load(std::memory_order_acquire);
        // Also lets the speed change during a held key, and returns the stick to neutral.
        if ((ready && anyKey) || wasReady != ready || g_injecting.load(std::memory_order_acquire)) QueueRefresh();

        // Save the virtual axis once it has settled.
        if (selector.Virtual() != pendingSpeed) {
            pendingSpeed = selector.Virtual();
            virtualChangedAt = now;
        }
        if (pendingSpeed != savedSpeed && now - virtualChangedAt >= kSaveDelayNanoseconds) {
            savedSpeed = pendingSpeed;
            const std::scoped_lock lock{ g_mutex };
            g_settings.speed = savedSpeed;
            if (!WriteSettings(g_settings)) Log::Event("walk_settings_write_failed");
        }
    }
}
