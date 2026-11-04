#include "runtime/ShipBindings.h"

#include "ShipActionCatalog.h"
#include "runtime/LegacyHotasToken.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <format>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        std::string_view Trim(std::string_view text)
        {
            constexpr std::string_view kSpace = " \t\r\n";
            const auto first = text.find_first_not_of(kSpace);
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(kSpace) - first + 1);
        }

        std::string Lower(std::string_view text)
        {
            std::string result(text);
            for (char& value : result) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return result;
        }

        std::string ValueKey(std::string_view section, std::string_view key)
        {
            return Lower(section) + '\n' + Lower(key);
        }

        std::optional<std::uint32_t> ParseNumber(std::string_view text)
        {
            int base = 10;
            if (text.starts_with("0x") || text.starts_with("0X")) {
                text.remove_prefix(2);
                base = 16;
            }
            std::uint32_t value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            if (text.empty() || error != std::errc{} || end != text.data() + text.size()) return {};
            return value;
        }

        // HOTAS button values: 1-128 are physical buttons, 129-144 are POV directions.
        std::optional<std::string> ChannelForValue(std::uint32_t value)
        {
            constexpr std::array<std::string_view, 4> kDirections{ "up", "right", "down", "left" };
            if (value >= 1 && value <= 128) return std::format("button/{}", value - 1);
            if (value >= 129 && value <= 144) {
                const auto offset = value - 129;
                return std::format("pov/{}/direction/{}", offset / 4, kDirections[offset % 4]);
            }
            return {};
        }

        std::optional<std::string> ButtonChannel(const LegacyHotas::Binding& binding)
        {
            if (binding.kind == LegacyHotas::Kind::Axis) return {};
            return LegacyHotas::ChannelId(binding);
        }
    }

    void IniValues::Merge(std::string_view text)
    {
        if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
        std::string section;
        while (!text.empty()) {
            const auto end = text.find('\n');
            const auto line = Trim(text.substr(0, end));
            text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
            if (line.empty() || line.front() == ';' || line.front() == '#') continue;
            if (line.front() == '[') {
                if (line.back() == ']') section = std::string(Trim(line.substr(1, line.size() - 2)));
                continue;
            }
            const auto separator = line.find('=');
            if (separator == std::string_view::npos) continue;
            values_[ValueKey(section, Trim(line.substr(0, separator)))] = std::string(Trim(line.substr(separator + 1)));
        }
    }

    std::optional<std::string> IniValues::Get(std::string_view section, std::string_view key) const
    {
        const auto found = values_.find(ValueKey(section, key));
        if (found == values_.end()) return {};
        return found->second;
    }

    std::string_view ToString(BindingStatus status) noexcept
    {
        switch (status) {
        case BindingStatus::Unbound: return "unbound";
        case BindingStatus::Bound: return "bound";
        case BindingStatus::DeviceMissing: return "device_missing";
        case BindingStatus::DeviceAmbiguous: return "device_ambiguous";
        case BindingStatus::ChannelMissing: return "channel_missing";
        case BindingStatus::Unsupported: return "unsupported";
        case BindingStatus::Invalid: return "invalid";
        }
        return "invalid";
    }

    ConfiguredBinding ParseShipBinding(std::string_view actionId, std::string_view token)
    {
        ConfiguredBinding result;
        result.actionId = std::string(actionId);
        result.token = std::string(token);
        const auto text = Trim(token);
        if (text.empty() || text == "-1") return result;
        const auto separator = text.rfind('@');
        if (separator == std::string_view::npos) {
            result.status = BindingStatus::Unsupported;
            return result;
        }
        const auto device = Trim(text.substr(0, separator));
        const auto control = Trim(text.substr(separator + 1));
        result.status = BindingStatus::Invalid;
        if (device.empty() || control.empty()) return result;
        if (device.front() == '#') {
            result.status = BindingStatus::Unsupported;
            return result;
        }
        if (device.front() == '{') {
            const auto decoded = LegacyHotas::Decode(text, LegacyHotas::Dialect::Unspecified);
            const auto channel = decoded.binding ? ButtonChannel(*decoded.binding) : std::nullopt;
            if (!channel) return result;
            result.status = BindingStatus::Bound;
            result.devicePhysicalId = true;
            result.device = decoded.binding->physicalId;
            result.channel = *channel;
            return result;
        }
        const auto value = ParseNumber(control);
        const auto channel = value ? ChannelForValue(*value) : std::nullopt;
        if (!channel) return result;
        if (device.starts_with("guid:")) {
            const auto id = std::string(Trim(device.substr(5)));
            const auto decoded = LegacyHotas::Decode(id + "@button:1", LegacyHotas::Dialect::HotasCapture);
            if (!decoded.binding) return result;
            result.devicePhysicalId = true;
            result.device = decoded.binding->physicalId;
        } else {
            result.device = std::string(device);
        }
        result.status = BindingStatus::Bound;
        result.channel = *channel;
        return result;
    }

    ShipBindingConfig LoadShipBindings(const IniValues& ini)
    {
        ShipBindingConfig config;
        const auto enabled = ini.Get("ShipButtons", "bShipButtonsEnabled");
        config.enabled = !enabled || Lower(*enabled) != "false";
        config.bindings.reserve(kShipActionCatalog.size());
        for (const auto& action : kShipActionCatalog) {
            const auto token = ini.Get("ShipButtons", action.sourceIniKey);
            config.bindings.push_back(ParseShipBinding(action.actionId, token ? *token : std::string{}));
        }
        return config;
    }
}
