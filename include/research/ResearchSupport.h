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
    // ResearchDev-only title handshake. It starts before PostDataLoad so the
    // runner can advance the title prompt that gates PostDataLoad itself.
    void StartTitleAdvance(const Config& a_config) noexcept;
    void Start(const Config& a_config) noexcept;
    void Stop() noexcept;
}
