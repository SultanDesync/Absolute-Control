#pragma once

#include "diagnostics/AsyncLineSink.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace AbsoluteControlPanelResearch::EvidenceLog
{
    enum class Level : std::uint8_t
    {
        Trace,
        Info,
        Warning,
        Error
    };

    struct Options
    {
        Level minimumLevel{ Level::Info };
        std::size_t queueCapacity{ 4096 };
        std::filesystem::path pathOverride;
    };

    void Initialize(std::string_view a_runId, Options a_options = {}) noexcept;
    void Event(std::string_view a_event, std::string_view a_detail = {},
        Level a_level = Level::Info) noexcept;
    void Trace(std::string_view a_event, std::string_view a_detail = {}) noexcept;
    [[nodiscard]] bool Flush(
        std::chrono::milliseconds a_timeout = std::chrono::seconds(5)) noexcept;
    [[nodiscard]] bool Shutdown(
        std::chrono::milliseconds a_timeout = std::chrono::seconds(5)) noexcept;
    [[nodiscard]] Diagnostics::AsyncLineSinkStatistics Statistics() noexcept;
    [[nodiscard]] std::filesystem::path Path() noexcept;
}
