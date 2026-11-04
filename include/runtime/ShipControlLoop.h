#pragma once

#include <filesystem>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Starts the ship control loop over the native input bus. Bindings come from these INI
    // files, later files overriding earlier ones, and are only ever read. For now the loop
    // only logs what each button would do: nothing reaches the game.
    void StartShipControlLoop(const std::vector<std::filesystem::path>& bindingFiles) noexcept;
}
