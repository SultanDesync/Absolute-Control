#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace AbsoluteControl::Runtime::LegacyHotas
{
    // These are source dialects, not versions of a new saved-token grammar.
    enum class Dialect { Unspecified, HotasCapture, ZeroPower };
    enum class Kind { Button, PovDirection, Axis };
    enum class Status { Ok, Invalid, UnsupportedIdentity, AmbiguousDialect };

    struct Binding
    {
        std::string physicalId; // Uppercase brace GUID; never product/display name.
        Kind kind{};
        std::uint32_t index{}; // Zero-based button, POV, or scalar channel ordinal.
        std::uint32_t direction{}; // POV only: up/right/down/left = 0/1/2/3.
        bool operator==(const Binding&) const = default;
    };

    struct Decoded
    {
        Status status{ Status::Invalid };
        std::string original; // Always preserve saved text, including rejected input.
        std::optional<Binding> binding;
    };

    // No device lookup, configuration write or implicit subscriber migration.
    [[nodiscard]] Decoded Decode(std::string_view token, Dialect dialect);
    // For new producer-format captures only. Saved tokens use Decoded::original.
    [[nodiscard]] std::optional<std::string> EncodeCapture(const Binding& binding);
    // Provider-local stable channel identity, without display labels or DI usage IDs.
    [[nodiscard]] std::optional<std::string> ChannelId(const Binding& binding);
}
