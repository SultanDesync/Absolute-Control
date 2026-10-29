#include "runtime/LegacyHotasToken.h"

#include <array>
#include <charconv>
#include <utility>

namespace AbsoluteControl::Runtime::LegacyHotas
{
    namespace
    {
        constexpr std::array<std::string_view, 4> kDirections{ "up", "right", "down", "left" };

        std::optional<std::string> Guid(std::string_view text)
        {
            if (text.size() != 38 || text.front() != '{' || text.back() != '}') return {};
            std::string result(text);
            for (std::size_t index = 1; index < 37; ++index) {
                const char value = text[index];
                if (index == 9 || index == 14 || index == 19 || index == 24) {
                    if (value != '-') return {};
                } else if (value >= 'a' && value <= 'f') {
                    result[index] = static_cast<char>(value - 'a' + 'A');
                } else if (!((value >= '0' && value <= '9') || (value >= 'A' && value <= 'F'))) {
                    return {};
                }
            }
            return result;
        }

        bool Number(std::string_view text, std::uint32_t& value, int base = 10)
        {
            if (text.empty()) return false;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            return error == std::errc{} && end == text.data() + text.size();
        }

        std::string Lower(std::string_view text)
        {
            std::string result(text);
            for (char& value : result) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return result;
        }

        std::optional<Binding> Control(std::string_view text, const std::string& physicalId,
            Dialect dialect)
        {
            // Zero and Power lower-case control text; producer/Head Tracking do not.
            const auto owned = dialect == Dialect::ZeroPower ? Lower(text) : std::string(text);
            text = owned;
            Binding result{ physicalId };
            if (text.starts_with("button:")) {
                std::uint32_t number{};
                if (!Number(text.substr(7), number) || number < 1 || number > 128) return {};
                result.kind = Kind::Button;
                result.index = number - 1;
            } else if (text.starts_with("axis:") && dialect == Dialect::HotasCapture) {
                const auto value = text.substr(5);
                std::uint32_t usage{};
                if (!value.starts_with("0x") || !Number(value.substr(2), usage, 16) ||
                    usage < 0x30 || usage > 0x37) return {};
                result.kind = Kind::Axis;
                result.index = usage - 0x30;
            } else if (text.starts_with("pov:")) {
                result.kind = Kind::PovDirection;
                const auto rest = text.substr(4);
                const auto separator = rest.find(':');
                std::uint32_t number{};
                if (separator == std::string_view::npos) {
                    if (dialect != Dialect::ZeroPower || !Number(rest, number)) return {};
                    if (number >= 129 && number <= 144) number -= 129;
                    else if (number >= 16) return {};
                    result.index = number / 4;
                    result.direction = number % 4;
                    return result;
                }
                if (!Number(rest.substr(0, separator), number)) return {};
                if (dialect == Dialect::ZeroPower && number >= 1 && number <= 4) --number;
                if (number > 3) return {};
                result.index = number;
                const auto direction = rest.substr(separator + 1);
                for (std::uint32_t index = 0; index < kDirections.size(); ++index) {
                    if (direction == kDirections[index]) {
                        result.direction = index;
                        return result;
                    }
                }
                if (dialect != Dialect::ZeroPower) return {};
                constexpr std::array<std::string_view, 4> compass{ "north", "east", "south", "west" };
                constexpr std::array<std::string_view, 4> letters{ "n", "e", "s", "w" };
                constexpr std::array<std::string_view, 4> digits{ "0", "1", "2", "3" };
                for (std::uint32_t index = 0; index < compass.size(); ++index) {
                    if (direction == compass[index] || direction == letters[index] || direction == digits[index]) {
                        result.direction = index;
                        return result;
                    }
                }
                return {};
            } else return {};
            return result;
        }

        bool Valid(const Binding& binding)
        {
            if (!Guid(binding.physicalId)) return false;
            switch (binding.kind) {
            case Kind::Button: return binding.index < 128 && binding.direction == 0;
            case Kind::Axis: return binding.index < 8 && binding.direction == 0;
            case Kind::PovDirection: return binding.index < 4 && binding.direction < 4;
            default: return false;
            }
        }
    }

    Decoded Decode(std::string_view token, Dialect dialect)
    {
        Decoded result{ Status::Invalid, std::string(token), {} };
        // BindingV1 has a 256-byte NUL-terminated buffer. Non-producer legacy
        // names/#indices stay opaque until a separate identity resolver exists.
        if (token.empty() || token.size() >= 256 || token.find('\0') != std::string_view::npos) return result;
        const auto separator = token.find('@');
        if (separator == std::string_view::npos || separator == 0 ||
            token.find('@', separator + 1) != std::string_view::npos) return result;
        const auto id = Guid(token.substr(0, separator));
        if (!id) {
            result.status = Status::UnsupportedIdentity;
            return result;
        }
        const auto text = token.substr(separator + 1);
        if (dialect == Dialect::Unspecified) {
            const auto producer = Control(text, *id, Dialect::HotasCapture);
            const auto consumer = Control(text, *id, Dialect::ZeroPower);
            if (producer && consumer && *producer != *consumer) {
                result.status = Status::AmbiguousDialect;
                return result;
            }
            result.binding = producer ? producer : consumer;
        } else if (dialect == Dialect::HotasCapture || dialect == Dialect::ZeroPower) {
            result.binding = Control(text, *id, dialect);
        }
        if (result.binding) result.status = Status::Ok;
        return result;
    }

    std::optional<std::string> EncodeCapture(const Binding& binding)
    {
        if (!Valid(binding)) return {};
        const auto id = *Guid(binding.physicalId);
        switch (binding.kind) {
        case Kind::Button: return id + "@button:" + std::to_string(binding.index + 1);
        case Kind::PovDirection: return id + "@pov:" + std::to_string(binding.index) + ":" +
            std::string(kDirections[binding.direction]);
        case Kind::Axis: return id + "@axis:0x3" + static_cast<char>('0' + binding.index);
        default: return {};
        }
    }

    std::optional<std::string> ChannelId(const Binding& binding)
    {
        if (!Valid(binding)) return {};
        switch (binding.kind) {
        case Kind::Button: return "button/" + std::to_string(binding.index);
        case Kind::Axis: return "axis/" + std::to_string(binding.index);
        case Kind::PovDirection: return "pov/" + std::to_string(binding.index) + "/direction/" +
            std::string(kDirections[binding.direction]);
        default: return {};
        }
    }
}
