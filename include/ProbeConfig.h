#pragma once

#include <cstdint>
#include <filesystem>

namespace AbsoluteControlPanelResearch
{
    struct ProbeConfig
    {
        bool enableRegistration{ true };
        bool enablePauseMenuEntry{ false };
        std::uint32_t openHotkey{ 0x71 };  // F2; zero disables the listener.
        std::uint32_t menuFlags{ 0x0800071B };
    };

    [[nodiscard]] ProbeConfig LoadProbeConfig(const std::filesystem::path& a_path) noexcept;
}
