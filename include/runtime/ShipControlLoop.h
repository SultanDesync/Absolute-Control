#pragma once

#include <filesystem>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Starts the ship control loop over the native input bus. Bindings come from these INI
    // files, later files overriding earlier ones, and are only ever read. A bound button
    // presses its action's key or mouse button with SendInput while the player is piloting
    // and the game window has focus, so the game's own bindings decide what it does.
    void StartShipControlLoop(const std::vector<std::filesystem::path>& bindingFiles) noexcept;
}
