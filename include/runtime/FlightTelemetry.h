#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

// What the control loop last read and wrote, for the menu to show while a setting is tuned.
namespace AbsoluteControl::Runtime::FlightTelemetry
{
    // The order the loop reads flight axes in.
    enum Axis : std::size_t { Throttle, Pitch, Yaw, Roll, StrafeLateral, StrafeVertical, Reverse, AimYaw, AimPitch, Count };

    struct State
    {
        std::array<std::atomic<float>, Count> position;
        std::array<std::atomic<float>, Count> output;
        std::atomic<bool> piloting{};
        std::atomic<bool> reverse{};
        std::atomic<bool> boost{};

        State()
        {
            for (auto& value : position) value.store(std::numeric_limits<float>::quiet_NaN());
            for (auto& value : output) value.store(0.0F);
        }
    };

    inline State& Shared()
    {
        static State state;
        return state;
    }

    // Nothing means the axis is unbound, missing or stale.
    inline void SetPosition(std::size_t axis, std::optional<float> value)
    {
        Shared().position[axis].store(value ? *value : std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);
    }

    // Looking around: yaw, pitch and roll in degrees.
    struct LookState
    {
        std::array<std::atomic<float>, 3> tracker{}; // The angle inputs alone, from their centre.
        std::array<std::atomic<float>, 3> output{};  // Where the camera is turned.
        std::atomic<bool> reporting{};  // Some bound look input is reporting.
        std::atomic<bool> on{ true };   // Not switched off with the toggle.
        std::atomic<bool> turning{};    // The camera is being turned right now.
        std::atomic<bool> ready{};      // The camera and field-of-fire hooks are in.
    };

    inline LookState& Look()
    {
        static LookState state;
        return state;
    }

    inline std::optional<float> Position(std::size_t axis)
    {
        const float value = Shared().position[axis].load(std::memory_order_relaxed);
        return std::isnan(value) ? std::nullopt : std::optional<float>(value);
    }
}
