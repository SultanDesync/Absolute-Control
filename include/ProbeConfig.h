#pragma once

#include <cstdint>
#include <filesystem>

namespace AbsoluteControlPanelResearch
{
    struct ProbeConfig
    {
        bool enableRegistration{ true };
        bool enablePauseMenuEntry{ false };
        // The PauseMenu row is the canonical entry point. A nonzero Win32
        // virtual-key code opts into a standalone recovery hotkey.
        std::uint32_t openHotkey{ 0 };
        std::uint32_t menuFlags{ 0x0800071B };
    };

    [[nodiscard]] ProbeConfig LoadProbeConfig(const std::filesystem::path& a_path) noexcept;
}
