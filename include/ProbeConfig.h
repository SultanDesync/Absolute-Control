#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace AbsoluteControlPanelResearch
{
    struct ProbeConfig
    {
        std::string runId{ "manual" };
        bool enableRegistration{ true };
        bool autoOpen{ false };
        bool enablePauseMenuEntry{ false };
        bool requireArm{ false };
        bool advanceTitleWithSendInput{ false };
        std::uint32_t armTimeoutMilliseconds{ 180000 };
        std::uint32_t openDelayMilliseconds{ 15000 };
        std::uint32_t visibleMilliseconds{ 12000 };
        std::uint32_t openHotkey{ 0x71 };  // F2; zero disables the listener.
        std::uint32_t menuFlags{ 0x0800071B };
    };

    [[nodiscard]] ProbeConfig LoadProbeConfig(const std::filesystem::path& a_path) noexcept;
}
