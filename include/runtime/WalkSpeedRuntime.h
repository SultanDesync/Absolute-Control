#pragma once

#include "runtime/FlightSignals.h"
#include "runtime/WalkSpeed.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace AbsoluteControl::Runtime::WalkSpeedRuntime
{
    struct Status
    {
        double speed{ 1.0 };
        WalkSpeedSource source{ WalkSpeedSource::Virtual };
        bool ready{}; // Enabled, on foot, and the game is taking movement input.
    };

    // Loads settings and hooks on-foot movement. Normally both files are the same one.
    void Install(const std::filesystem::path& readFile, const std::filesystem::path& writeFile) noexcept;

    [[nodiscard]] WalkSpeedSettings Settings();
    // Stores everything except the saved speed, which belongs to the wheel and keys.
    [[nodiscard]] bool ApplySettings(const WalkSpeedSettings& settings);
    [[nodiscard]] Status CurrentStatus() noexcept;
    [[nodiscard]] std::string AxisToken();

    // Called by the control loop. axisValue is the bound axis, -1 to 1, if it can be read.
    void Tick(const std::optional<FlightSignals>& signals, std::optional<double> axisValue, std::uint64_t now);
}
