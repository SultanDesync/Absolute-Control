#pragma once

#include "runtime/LookAxes.h"

// Turns the cockpit camera by the look axes. Ported from Absolute Head Tracking's
// CameraHook.cpp: it hooks the first-person camera's rotation and turns the game's own
// rotation by the head's. Head Tracking also hooked the ship's flight output to know when
// the player was piloting; here the control loop already knows, and says so with Publish.
namespace AbsoluteControl::Runtime::CameraLook
{
    struct InstallResult
    {
        bool camera{};          // The first-person rotation hook.
        bool aimRestriction{};  // The weapon field-of-fire limit's hooks.
    };
    // Once per process. The camera only turns if both installed: turning the head without the
    // limit would let the guns follow it anywhere.
    InstallResult Install();
    [[nodiscard]] bool Ready() noexcept;

    // The rotation to apply this frame. Call every tick while piloting; the camera goes back to
    // the game's own once Publish stops for a quarter of a second, or on Clear.
    void Publish(const Quaternion& rotation);
    void Clear();
}
