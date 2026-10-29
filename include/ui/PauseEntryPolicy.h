#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace AbsoluteControlPanelResearch::Ui::PauseEntryPolicy
{
    inline constexpr std::uint32_t kAction = 0x534C4F50;
    inline constexpr const char* kLabel = "ABSOLUTE CONTROL";
    inline constexpr const char* kOwnerMarker = "_absoluteControlPanelEntry";

    // AS3 may expose uint fields as int or Number. Never truncate a foreign
    // fractional action into our action ID, or convert NaN/out-of-range values.
    [[nodiscard]] inline std::optional<std::uint32_t> ParseAction(double value) noexcept
    {
        if (!std::isfinite(value) || value < 0.0 ||
            value > std::numeric_limits<std::uint32_t>::max() ||
            std::floor(value) != value) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(value);
    }

    struct Identity
    {
        std::optional<std::uint32_t> action;
        bool owned{};
    };

    enum class Admission { Append, AlreadyPresent, Conflict };

    [[nodiscard]] inline Admission Inspect(std::span<const Identity> entries) noexcept
    {
        bool found = false;
        for (const auto& entry : entries) {
            if (entry.owned) {
                if (entry.action != kAction || found) return Admission::Conflict;
                found = true;
            } else if (entry.action == kAction) {
                // Matching an action number alone does not establish ownership.
                return Admission::Conflict;
            }
        }
        return found ? Admission::AlreadyPresent : Admission::Append;
    }

    [[nodiscard]] inline bool Handles(std::optional<std::uint32_t> action,
        Identity selectedEntry, bool enabled) noexcept
    {
        return enabled && action == kAction && selectedEntry.owned &&
            selectedEntry.action == kAction;
    }
}
