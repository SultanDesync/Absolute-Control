#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace AbsoluteControlPanelResearch::Ui::PauseViewportPolicy
{
    // Preserve text padding; refuse more than 25% field growth instead of stretching glyphs.
    [[nodiscard]] inline std::optional<double> WidthDelta(
        double fieldWidth, double naturalTextWidth) noexcept
    {
        if (!std::isfinite(fieldWidth) || fieldWidth < 80 || fieldWidth > 800 ||
            !std::isfinite(naturalTextWidth) || naturalTextWidth <= 0)
            return std::nullopt;
        const auto delta = std::max(0.0, std::ceil(naturalTextWidth + 4.0 - fieldWidth));
        return delta <= fieldWidth * 0.25 ? std::optional(delta) : std::nullopt;
    }

    // Heights are in list-local units. Never shrink a viewport another owner enlarged.
    [[nodiscard]] inline std::optional<double> Target(
        int count, double pitch, double current, double safeMaximum) noexcept
    {
        if (count < 1 || count > 128 || !std::isfinite(pitch) || pitch < 8 || pitch > 200 ||
            !std::isfinite(current) || current <= 0 || !std::isfinite(safeMaximum) || safeMaximum <= 0)
            return std::nullopt;
        const auto required = count * pitch + 1.0;
        return std::max(current, std::min(required, safeMaximum));
    }
}
