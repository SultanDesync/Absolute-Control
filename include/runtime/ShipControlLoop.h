#pragma once

#include <filesystem>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Starts the control loop over the native input bus. Bindings and flight settings come
    // from bindingFiles (the AbsoluteHOTAS files, only ever read) and then settingsFile,
    // Absolute Control's own, which the menu writes and which wins. A bound button presses
    // its action's key or mouse button with SendInput while the player is piloting and the
    // game window has focus, so the game's own bindings decide what it does. Settings
    // applied in the menu and keys rebound in the game take effect without a restart.
    void StartShipControlLoop(const std::vector<std::filesystem::path>& bindingFiles,
        const std::filesystem::path& settingsFile) noexcept;
}