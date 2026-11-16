#pragma once

#include <filesystem>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Starts the control loop over the native input bus. Its bindings and flight settings
    // come from ControlSettings, which must already be loaded. A bound button presses its
    // action's key or mouse button with SendInput while the player is piloting and the game
    // window has focus, so the game's own bindings decide what it does. Settings applied in
    // the menu and keys rebound in the game take effect without a restart.
    void StartShipControlLoop() noexcept;
}