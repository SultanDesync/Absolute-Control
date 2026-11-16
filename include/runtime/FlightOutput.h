#pragma once

#include "runtime/FlightAxes.h"

#include <cstdint>

// Writes flight axes into the game. The seams are AbsoluteHOTAS's, for Starfield 1.16.244:
// four gates on the block that stores roll, vertical strafe, yaw and pitch, direct writes
// of throttle, and hooks that hold back the game's own throttle stores so a lever position
// stays put. Everything is called from the control loop's thread.
namespace AbsoluteControl::Runtime::FlightOutput
{
    struct InstallResult
    {
        bool steeringGates{}; // False if the block wasn't found, as when another mod owns it.
        int storeHooks{};     // Game stores that can be held back.
        bool splitHook{};     // Roll and sideways strafe can be written separately.
    };

    // Patches the game's code once. Safe to call again: later calls return the first result.
    InstallResult Install();

    struct Target
    {
        std::uintptr_t manager{}; // PlayerControls::Manager.
        std::uintptr_t cluster{}; // The flight handler it points to.
    };

    // One tick with flight output allowed. boostHeld is the booster key, from a button or
    // the lever's boost zone. mouseSteeringOwned leaves pitch and yaw to another mod.
    void Apply(const FlightConfig& config, const FlightFrame& frame, const Target& target,
        bool boostHeld, bool mouseSteeringOwned, std::uint64_t nowNanoseconds);

    // Hands every lane back to the game. Nothing is zeroed: the game's next store wins.
    void Suspend();
}
