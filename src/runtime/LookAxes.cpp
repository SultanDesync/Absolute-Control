#include "runtime/LookAxes.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        constexpr std::array<std::string_view, kLookAxes> kSections{ "LookYaw", "LookPitch", "LookRoll" };
        constexpr std::array<std::string_view, kLookAxes> kIds{ "LookYaw", "LookPitch", "LookRoll" };
        constexpr std::array<std::string_view, kLookAxes> kTrackerAxes{ "Yaw", "Pitch", "Roll" };
        struct Limits { float minimum, maximum; };
        constexpr std::array<Limits, kLookAxes> kLimits{ { { -85.0F, 85.0F }, { -60.0F, 60.0F }, { -45.0F, 45.0F } } };

        // The game's camera turns left and tilts left for positive yaw and roll. A tracker
        // already reports that way except for pitch; a stick reports right and back as
        // positive. Settings' Invert is on top of these, so neither needs it by default.
        constexpr std::array<float, kLookAxes> kAngleDirection{ 1.0F, -1.0F, 1.0F };
        constexpr std::array<float, kLookAxes> kStickDirection{ -1.0F, -1.0F, -1.0F };

        std::string Lower(std::string_view text)
        {
            std::string result(text);
            for (char& value : result) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return result;
        }

        std::string_view Trim(std::string_view text)
        {
            const auto first = text.find_first_not_of(" \t\r");
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
        }

        bool Bool(const IniValues& ini, std::string_view section, std::string_view key, bool fallback)
        {
            const auto text = ini.Get(section, key);
            if (!text) return fallback;
            const auto value = Lower(Trim(*text));
            if (value == "true" || value == "1" || value == "yes" || value == "on") return true;
            if (value == "false" || value == "0" || value == "no" || value == "off") return false;
            return fallback;
        }

        float Float(const IniValues& ini, std::string_view section, std::string_view key, float fallback)
        {
            const auto text = ini.Get(section, key);
            if (!text) return fallback;
            const auto trimmed = Trim(*text);
            float value{};
            const auto [end, error] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
            return error == std::errc{} && end == trimmed.data() + trimmed.size() && std::isfinite(value) ? value : fallback;
        }

        int Int(const IniValues& ini, std::string_view section, std::string_view key, int fallback)
        {
            const auto text = ini.Get(section, key);
            if (!text) return fallback;
            auto digits = Trim(*text);
            int base = 10;
            if (digits.starts_with("0x") || digits.starts_with("0X")) {
                digits.remove_prefix(2);
                base = 16;
            }
            int value{};
            const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
            return error == std::errc{} && end == digits.data() + digits.size() ? value : fallback;
        }

        std::string Text(double value)
        {
            auto text = std::format("{:.3f}", value);
            text.erase(text.find_last_not_of('0') + 1);
            if (text.ends_with('.')) text.pop_back();
            return text;
        }

        std::string Text(bool value) { return value ? "true" : "false"; }

        std::string Token(const ConfiguredBinding& binding)
        {
            return binding.status == BindingStatus::Bound ? std::string(Trim(binding.token)) : std::string{};
        }

        void Order(LookAxisConfig& axis, std::size_t index)
        {
            axis.minimum = std::clamp(axis.minimum, -180.0F, 0.0F);
            axis.maximum = std::clamp(axis.maximum, 0.0F, 180.0F);
            axis.centre = std::clamp(axis.centre, axis.minimum, axis.maximum);
            if (axis.minimum == axis.maximum) {
                axis.minimum = kLimits[index].minimum;
                axis.maximum = kLimits[index].maximum;
            }
        }

        // A tracker angle: deadzone, then where it sits from the axis centre.
        float ShapeAngle(double degrees, float deadzone)
        {
            const float size = static_cast<float>(std::abs(degrees));
            return size <= deadzone ? 0.0F : static_cast<float>(std::copysign(size - deadzone, degrees));
        }

        // A stick's travel past its deadzone, scaled to reach the limit on that side.
        float StickOffset(double travel, float deadzone, const LookAxisConfig& axis)
        {
            const float value = std::clamp(static_cast<float>(travel), -1.0F, 1.0F);
            const float dead = std::clamp(deadzone, 0.0F, 0.95F);
            const float size = std::abs(value);
            if (size <= dead) return 0.0F;
            const float shaped = std::copysign((size - dead) / (1.0F - dead), value);
            return shaped < 0.0F ? shaped * (axis.centre - axis.minimum) : shaped * (axis.maximum - axis.centre);
        }
    }

    std::optional<KeyChord> ParseKeyChord(std::string_view text)
    {
        KeyChord chord;
        text = Trim(text);
        if (text.empty()) return {};
        while (!text.empty()) {
            const auto plus = text.find('+');
            const auto part = Lower(Trim(text.substr(0, plus)));
            text = plus == std::string_view::npos ? std::string_view{} : text.substr(plus + 1);
            if (part == "ctrl" || part == "control") chord.control = true;
            else if (part == "alt") chord.alt = true;
            else if (part == "shift") chord.shift = true;
            else if (chord.key) return {};
            else if (part.size() == 1 && ((part[0] >= 'a' && part[0] <= 'z') || (part[0] >= '0' && part[0] <= '9'))) {
                chord.key = part[0] >= 'a' ? part[0] - 'a' + 'A' : part[0];
            } else if (part.size() >= 2 && part.size() <= 3 && part[0] == 'f') {
                int number{};
                const auto [end, error] = std::from_chars(part.data() + 1, part.data() + part.size(), number);
                if (error != std::errc{} || end != part.data() + part.size() || number < 1 || number > 24) return {};
                chord.key = 0x70 + number - 1; // VK_F1 onwards.
            } else if (part.starts_with("0x")) {
                int key{};
                const auto [end, error] = std::from_chars(part.data() + 2, part.data() + part.size(), key, 16);
                if (error != std::errc{} || end != part.data() + part.size() || key <= 0 || key > 0xFE) return {};
                chord.key = key;
            } else {
                return {};
            }
        }
        if (!chord.key) return {};
        return chord;
    }

    LookConfig LoadLookConfig(const IniValues& ini)
    {
        LookConfig config;
        config.enabled = Bool(ini, "Look", "Enabled", true);
        config.trackerDeadzone = std::clamp(Float(ini, "Look", "TrackerDeadzone", 0.0F), 0.0F, 30.0F);
        config.smoothing = std::clamp(Float(ini, "Look", "Smoothing", 15.0F) / 100.0F, 0.0F, 0.99F);
        config.recentreButton = ParseShipBinding("LookRecentre", ini.Get("Look", "RecentreButton").value_or(""));
        config.toggleButton = ParseShipBinding("LookToggle", ini.Get("Look", "ToggleButton").value_or(""));
        config.recentreKey = ini.Get("Look", "RecentreKey").value_or(config.recentreKey);
        config.toggleKey = ini.Get("Look", "ToggleKey").value_or(config.toggleKey);
        config.firingCone = std::clamp(Float(ini, "Look", "FiringCone", 30.0F), 0.0F, 90.0F);
        for (std::size_t index = 0; index < kLookAxes; ++index) {
            const auto section = kSections[index];
            auto& axis = config.axes[index];
            axis.enabled = Bool(ini, section, "Enabled", true);
            const auto token = ini.Get(section, "Axis").value_or(std::format("OpenTrack@{}", kTrackerAxes[index]));
            const auto read = [&](LookInputConfig& input, std::string_view prefix, std::string_view value) {
                input.binding = ParseAxisBinding(kIds[index], value);
                input.invert = Bool(ini, section, std::string(prefix) + "Invert", false);
                input.sensitivity = Float(ini, section, std::string(prefix) + "Sensitivity", 100.0F) / 100.0F;
                input.deadzone = std::clamp(Float(ini, section, std::string(prefix) + "Deadzone", 10.0F) / 100.0F, 0.0F, 0.95F);
            };
            read(axis.input, "", token);
            read(axis.second, "SecondAxis", ini.Get(section, "SecondAxis").value_or(""));
            axis.minimum = Float(ini, section, "Minimum", kLimits[index].minimum);
            axis.centre = Float(ini, section, "Centre", 0.0F);
            axis.maximum = Float(ini, section, "Maximum", kLimits[index].maximum);
            Order(axis, index);
        }
        return config;
    }

    std::vector<SettingEntry> WriteLookConfig(const LookConfig& config)
    {
        std::vector<SettingEntry> entries;
        const auto add = [&](std::string_view section, std::string_view key, std::string value) {
            entries.push_back({ std::string(section), std::string(key), std::move(value) });
        };
        add("Look", "Enabled", Text(config.enabled));
        add("Look", "TrackerDeadzone", Text(static_cast<double>(config.trackerDeadzone)));
        add("Look", "Smoothing", Text(config.smoothing * 100.0));
        add("Look", "RecentreButton", Token(config.recentreButton));
        add("Look", "ToggleButton", Token(config.toggleButton));
        add("Look", "RecentreKey", config.recentreKey);
        add("Look", "ToggleKey", config.toggleKey);
        add("Look", "FiringCone", Text(static_cast<double>(config.firingCone)));
        for (std::size_t index = 0; index < kLookAxes; ++index) {
            const auto section = kSections[index];
            const auto& axis = config.axes[index];
            add(section, "Enabled", Text(axis.enabled));
            const auto input = [&](const LookInputConfig& value, std::string_view prefix) {
                add(section, prefix.empty() ? "Axis" : prefix, Token(value.binding));
                add(section, std::string(prefix) + "Invert", Text(value.invert));
                add(section, std::string(prefix) + "Sensitivity", Text(value.sensitivity * 100.0));
                add(section, std::string(prefix) + "Deadzone", Text(value.deadzone * 100.0));
            };
            input(axis.input, "");
            input(axis.second, "SecondAxis");
            add(section, "Minimum", Text(static_cast<double>(axis.minimum)));
            add(section, "Centre", Text(static_cast<double>(axis.centre)));
            add(section, "Maximum", Text(static_cast<double>(axis.maximum)));
        }
        return entries;
    }

    LookConfig LoadHeadTrackingLookConfig(const IniValues& ini)
    {
        LookConfig config;
        config.enabled = Bool(ini, "General", "bEnabled", true);
        config.trackerDeadzone = std::clamp(Float(ini, "Tracking", "fDeadzoneDegrees", 0.0F), 0.0F, 30.0F);
        config.smoothing = std::clamp(Float(ini, "Tracking", "fSmoothing", 0.15F), 0.0F, 0.99F);
        const bool limit = Bool(ini, "AimRestriction", "bLimitAimToShipForwardCone", true);
        config.firingCone = limit ? std::clamp(Float(ini, "AimRestriction", "fFiringConeHalfAngleDegrees", 30.0F), 0.0F, 90.0F) : 0.0F;
        const auto chord = [&](std::string_view name, std::string fallback) {
            const auto key = Int(ini, "Hotkeys", std::format("i{}Key", name), -1);
            if (key < 0) return fallback;
            if (key == 0) return std::string{};
            std::string text;
            if (Bool(ini, "Hotkeys", std::format("b{}Ctrl", name), false)) text += "Ctrl+";
            if (Bool(ini, "Hotkeys", std::format("b{}Alt", name), false)) text += "Alt+";
            if (Bool(ini, "Hotkeys", std::format("b{}Shift", name), false)) text += "Shift+";
            const bool letter = (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9');
            return text + (letter ? std::string(1, static_cast<char>(key)) : std::format("0x{:02X}", key));
        };
        config.recentreKey = chord("Recenter", config.recentreKey);
        config.toggleKey = chord("Toggle", config.toggleKey);
        // Its own controller bindings were written in the old HOTAS bus's form. Any that
        // read as one of ours carry over.
        config.recentreButton = ParseShipBinding("LookRecentre", ini.Get("Hotkeys", "sRecenterControllerBinding").value_or(""));
        config.toggleButton = ParseShipBinding("LookToggle", ini.Get("Hotkeys", "sToggleControllerBinding").value_or(""));
        const float stickDeadzone = std::clamp(Float(ini, "ManualLook", "fControllerAxisDeadzone", 0.08F), 0.0F, 0.95F);
        for (std::size_t index = 0; index < kLookAxes; ++index) {
            const auto name = kTrackerAxes[index];
            auto& axis = config.axes[index];
            axis.enabled = Bool(ini, "Tracking", std::format("b{}Enabled", name), true);
            axis.input.binding = ParseAxisBinding(kIds[index], std::format("OpenTrack@{}", name));
            // Head Tracking had pitch inverted by default; here that's built in.
            const bool trackerInvert = Bool(ini, "Tracking", std::format("bInvert{}", name), index == 1);
            axis.input.invert = index == 1 ? !trackerInvert : trackerInvert;
            axis.input.sensitivity = Float(ini, "Tracking", std::format("f{}Sensitivity", name), 1.0F);
            axis.second.binding = ParseAxisBinding(kIds[index],
                ini.Get("ManualLook", std::format("s{}ControllerAxisBinding", name)).value_or(""));
            // Its stick pitch was inverted by default too, against a stick base that wasn't.
            const bool stickInvert = Bool(ini, "ManualLook", std::format("bInvertController{}", name), index == 1);
            axis.second.invert = index == 1 ? !stickInvert : stickInvert;
            axis.second.deadzone = stickDeadzone;
            axis.minimum = Float(ini, "Tracking", std::format("f{}MinimumDegrees", name), kLimits[index].minimum);
            axis.centre = Float(ini, "Tracking", std::format("f{}CenterDegrees", name), 0.0F);
            axis.maximum = Float(ini, "Tracking", std::format("f{}MaximumDegrees", name), kLimits[index].maximum);
            Order(axis, index);
        }
        return config;
    }

    Quaternion LookRotation(const std::array<float, kLookAxes>& degrees) noexcept
    {
        constexpr float kRadians = 0.01745329251994329577F;
        const auto turn = [](float x, float y, float z, float radians) {
            const float half = radians * 0.5F;
            const float sine = std::sin(half);
            return Quaternion{ std::cos(half), x * sine, y * sine, z * sine };
        };
        const auto multiply = [](const Quaternion& a, const Quaternion& b) {
            return Quaternion{
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            };
        };
        const auto yaw = turn(0.0F, 0.0F, 1.0F, degrees[0] * kRadians);
        const auto pitch = turn(1.0F, 0.0F, 0.0F, degrees[1] * kRadians);
        const auto roll = turn(0.0F, 1.0F, 0.0F, degrees[2] * kRadians);
        return multiply(multiply(yaw, roll), pitch);
    }

    void LookMixer::Recentre() noexcept
    {
        recentre_ = true;
    }

    bool LookMixer::Centred() const noexcept
    {
        return std::ranges::any_of(centre_, [](const auto& inputs) {
            return std::ranges::any_of(inputs, [](const auto& centre) { return centre.has_value(); });
        });
    }

    LookFrame LookMixer::Tick(const LookConfig& config,
        const std::array<std::array<LookReading, 2>, kLookAxes>& inputs, float seconds, bool enabled)
    {
        LookFrame frame;
        const bool recentre = std::exchange(recentre_, false);
        if (recentre) smoothed_ = {};
        if (!config.enabled || !enabled) {
            smoothed_ = {};
            return frame;
        }
        const float dt = std::clamp(seconds, 0.0F, 0.1F);
        for (std::size_t index = 0; index < kLookAxes; ++index) {
            const auto& axis = config.axes[index];
            if (!axis.enabled) {
                smoothed_[index] = 0.0F;
                continue;
            }
            float offset{};
            bool reporting{};
            for (std::size_t slot = 0; slot < 2; ++slot) {
                const auto& reading = inputs[index][slot];
                const auto& input = slot == 0 ? axis.input : axis.second;
                auto& centre = centre_[index][slot];
                if (!reading.value) continue;
                reporting = true;
                const float invert = input.invert ? -1.0F : 1.0F;
                if (reading.angle) {
                    // An angle input is centred on where it was first seen, or recentred.
                    if (!centre || recentre) centre = *reading.value;
                    const double degrees = (*reading.value - *centre) * input.sensitivity;
                    const float shaped = ShapeAngle(degrees, config.trackerDeadzone) * invert * kAngleDirection[index];
                    frame.tracker[index] = static_cast<float>(degrees);
                    offset += shaped;
                } else {
                    offset += StickOffset(*reading.value * input.sensitivity * invert * kStickDirection[index],
                        input.deadzone, axis);
                }
            }
            frame.active = frame.active || reporting;
            const float target = std::clamp(axis.centre + offset, axis.minimum, axis.maximum);
            // Exponential smoothing, the same at any tick rate: retention per 1/60 of a second.
            const float retention = std::pow(std::clamp(config.smoothing, 0.0F, 0.99F), dt * 60.0F);
            smoothed_[index] = dt <= 0.0F ? smoothed_[index] : smoothed_[index] + (target - smoothed_[index]) * (1.0F - retention);
            frame.degrees[index] = smoothed_[index];
        }
        if (!frame.active) {
            smoothed_ = {};
            frame.degrees = {};
        }
        return frame;
    }
}
