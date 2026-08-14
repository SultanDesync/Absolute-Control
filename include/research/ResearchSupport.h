#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace AbsoluteControlPanelResearch::ResearchSupport
{
    struct Config
    {
        std::string runId{ "manual" };
        bool autoOpen{};
        bool requireArm{};
        bool advanceTitleWithSendInput{};
        std::uint32_t armTimeoutMilliseconds{ 180000 };
        std::uint32_t openDelayMilliseconds{ 15000 };
        std::uint32_t visibleMilliseconds{ 12000 };
    };

    [[nodiscard]] Config LoadConfig(const std::filesystem::path& a_path) noexcept;
    void Start(const Config& a_config, bool a_providerReady) noexcept;
    void Stop() noexcept;
}
