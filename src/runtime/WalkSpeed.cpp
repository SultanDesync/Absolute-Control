#include "runtime/WalkSpeed.h"

#include <charconv>
#include <format>
#include <string_view>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        constexpr auto kSection = "WalkSpeed";

        std::string Lower(std::string text)
        {
            for (char& value : text) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return text;
        }

        bool ParseBool(const std::optional<std::string>& text, bool fallback)
        {
            if (!text) return fallback;
            const auto value = Lower(*text);
            if (value == "true" || value == "1") return true;
            if (value == "false" || value == "0") return false;
            return fallback;
        }

        double ParseDouble(const std::optional<std::string>& text, double fallback)
        {
            if (!text) return fallback;
            double value{};
            const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value);
            return error == std::errc{} && end == text->data() + text->size() ? value : fallback;
        }

        WheelModifier ParseWheel(const std::optional<std::string>& text)
        {
            const auto value = text ? Lower(*text) : std::string{ "alt" };
            if (value == "off") return WheelModifier::Off;
            if (value == "shift") return WheelModifier::Shift;
            if (value == "ctrl") return WheelModifier::Ctrl;
            return WheelModifier::Alt;
        }

        SpeedKeys ParseKeys(const std::optional<std::string>& text)
        {
            const auto value = text ? Lower(*text) : std::string{ "numpad" };
            if (value == "off") return SpeedKeys::Off;
            if (value == "brackets") return SpeedKeys::Brackets;
            if (value == "pagekeys") return SpeedKeys::PageKeys;
            return SpeedKeys::Numpad;
        }

        std::string_view ToString(WheelModifier value)
        {
            switch (value) {
            case WheelModifier::Off: return "Off";
            case WheelModifier::Shift: return "Shift";
            case WheelModifier::Ctrl: return "Ctrl";
            default: return "Alt";
            }
        }

        std::string_view ToString(SpeedKeys value)
        {
            switch (value) {
            case SpeedKeys::Off: return "Off";
            case SpeedKeys::Brackets: return "Brackets";
            case SpeedKeys::PageKeys: return "PageKeys";
            default: return "Numpad";
            }
        }
    }

    WalkSpeedSettings ParseWalkSpeedSettings(const IniValues& ini)
    {
        WalkSpeedSettings settings;
        settings.enabled = ParseBool(ini.Get(kSection, "Enabled"), settings.enabled);
        settings.axis = ini.Get(kSection, "Axis").value_or(std::string{});
        settings.invertAxis = ParseBool(ini.Get(kSection, "InvertAxis"), settings.invertAxis);
        settings.showGauge = ParseBool(ini.Get(kSection, "ShowGauge"), settings.showGauge);
        settings.wheelModifier = ParseWheel(ini.Get(kSection, "WheelModifier"));
        settings.speedKeys = ParseKeys(ini.Get(kSection, "SpeedKeys"));
        settings.step = std::clamp(ParseDouble(ini.Get(kSection, "Step"), settings.step), 0.01, 0.25);
        settings.speed = ClampSpeed(ParseDouble(ini.Get(kSection, "Speed"), settings.speed));
        return settings;
    }

    std::string FormatWalkSpeedSettings(const WalkSpeedSettings& settings)
    {
        return std::format(
            "; User-owned Absolute Control walk speed settings.\n"
            "[WalkSpeed]\n"
            "Enabled={}\nAxis={}\nInvertAxis={}\nShowGauge={}\nWheelModifier={}\nSpeedKeys={}\nStep={:.2f}\nSpeed={:.3f}\n",
            settings.enabled, settings.axis, settings.invertAxis, settings.showGauge,
            ToString(settings.wheelModifier), ToString(settings.speedKeys), settings.step, settings.speed);
    }
}
