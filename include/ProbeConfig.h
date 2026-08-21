#pragma once

#include <cstdint>
#include <filesystem>

namespace AbsoluteControlPanelResearch
{
    enum class ModuleSortMode : std::uint32_t
    {
        Registration,
        Alphabetical
    };

    struct ProbeConfig
    {
        bool enableRegistration{ true };
        bool enablePauseMenuEntry{ false };
        // The PauseMenu row is the canonical entry point. A nonzero Win32
        // virtual-key code opts into a standalone recovery hotkey.
        std::uint32_t openHotkey{ 0 };
        std::uint32_t menuFlags{ 0x0800071B };
        ModuleSortMode moduleSort{ ModuleSortMode::Registration };
    };

    [[nodiscard]] ProbeConfig LoadProbeConfig(
        const std::filesystem::path& a_path,
        ProbeConfig a_fallback = {}) noexcept;
}
