#include "runtime/FlightAxes.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        constexpr float kActivationFloor = 0.05F;
        constexpr long kAxisMaximum = 65535;

        std::string Lower(std::string_view text)
        {
            std::string result(text);
            for (char& value : result) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return result;
        }

        bool Bool(const IniValues& ini, std::string_view section, std::string_view key, bool fallback)
        {
            const auto text = ini.Get(section, key);
            if (!text) return fallback;
            const auto value = Lower(*text);
            if (value == "true" || value == "1" || value == "yes" || value == "on") return true;
            if (value == "false" || value == "0" || value == "no" || value == "off") return false;
            return fallback;
        }

        float Float(const IniValues& ini, std::string_view section, std::string_view key, float fallback)
        {
            const auto text = ini.Get(section, key);
            if (!text) return fallback;
            float value{};
            const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value);
            return error == std::errc{} && end == text->data() + text->size() && std::isfinite(value) ? value : fallback;
        }

        long Long(const IniValues& ini, std::string_view section, std::string_view key, long fallback)
        {
            const auto text = ini.Get(section, key);
            if (!text) return fallback;
            std::string_view digits{ *text };
            int base = 10;
            if (digits.starts_with("0x") || digits.starts_with("0X")) {
                digits.remove_prefix(2);
                base = 16;
            }
            long value{};
            const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
            return error == std::errc{} && end == digits.data() + digits.size() ? value : fallback;
        }

        ConfiguredBinding Axis(const IniValues& ini, std::string_view section, std::string_view key, std::string_view id)
        {
            const auto token = ini.Get(section, key);
            return ParseAxisBinding(id, token ? *token : std::string{});
        }

        AxisShape Shape(const IniValues& ini, std::string_view name, std::string_view saturation,
            std::string_view deadzone, std::string_view sensitivity, float deadzoneDefault, bool invertDefault)
        {
            AxisShape shape;
            shape.invert = Bool(ini, "Hardware", std::string("bInvert") + std::string(name), invertDefault);
            shape.sensitivity = Float(ini, "Hardware", sensitivity, 1.0F);
            shape.saturation = std::clamp(Float(ini, "Hardware", saturation, 1.0F), 0.05F, 1.0F);
            shape.deadzone = std::clamp(Float(ini, "Hardware", deadzone, deadzoneDefault), 0.0F, 0.95F);
            return shape;
        }

        bool Bound(const IniValues& ini, std::string_view section, std::string_view key)
        {
            const auto text = ini.Get(section, key);
            return text && !text->empty() && *text != "-1";
        }

        // Settings for features this build doesn't have, so a user can be told what was dropped.
        std::vector<std::string> IgnoredSettings(const IniValues& ini)
        {
            std::vector<std::string> ignored;
            const auto note = [&](bool present, std::string_view name) {
                if (present) ignored.emplace_back(name);
            };
            constexpr std::array<std::pair<std::string_view, std::string_view>, 26> kBindings{ {
                { "DigitalAxes", "iDigitalReverseButton" }, { "DigitalAxes", "iDigitalRollLeftButton" },
                { "DigitalAxes", "iDigitalRollRightButton" }, { "DigitalAxes", "iDigitalStrafeLeftButton" },
                { "DigitalAxes", "iDigitalStrafeRightButton" }, { "DigitalAxes", "iDigitalStrafeUpButton" },
                { "DigitalAxes", "iDigitalStrafeDownButton" },
                { "ControlExtensions", "iCruiseHoldButton" }, { "ControlExtensions", "iFullStopButton" },
                { "ControlExtensions", "iCruiseHalfButton" }, { "ControlExtensions", "iCruiseMaxButton" },
                { "Buttons", "iActivateButtonId" }, { "Buttons", "iStopButtonId" },
                { "MenuControls", "iMenuAcceptButton" }, { "MenuControls", "iMenuCancelButton" },
                { "MenuControls", "iMenuUpButton" }, { "MenuControls", "iMenuDownButton" },
                { "MenuControls", "iMenuLeftButton" }, { "MenuControls", "iMenuRightButton" },
                { "DualStick", "iTurnAssistButton" },
                { "Aim", "iAimYawAxis" }, { "Aim", "iAimPitchAxis" }, { "Aim", "iDigitalAimLeftButton" },
                { "Aim", "iDigitalAimRightButton" }, { "Aim", "iDigitalAimUpButton" }, { "Aim", "iDigitalAimDownButton" },
            } };
            for (const auto& [section, key] : kBindings) {
                note(Bound(ini, section, key), std::string(section) + "/" + std::string(key));
            }
            note(Bool(ini, "DualStick", "bAccumulatorThrottle", false), "DualStick/bAccumulatorThrottle");
            note(Bool(ini, "DualStick", "bAccumulatorTurnAssist", false), "DualStick/bAccumulatorTurnAssist");
            note(!Bool(ini, "Buttons", "bAlwaysOn", true), "Buttons/bAlwaysOn");
            note(Bool(ini, "Aim", "bHOSAMMode", false), "Aim/bHOSAMMode");
            note(Bool(ini, "Aim", "bAlignmentAssist", false), "Aim/bAlignmentAssist");
            if (const auto mode = ini.Get("Gate", "PilotGateMode")) note(Lower(*mode) != "injectiononly", "Gate/PilotGateMode");
            if (const auto signal = ini.Get("Gate", "PilotSignal")) note(Lower(*signal) != "auto", "Gate/PilotSignal");
            return ignored;
        }
    }

    float ShapeAxis(float position, const AxisShape& shape) noexcept
    {
        if (!std::isfinite(position)) return 0.0F;
        if (shape.invert) position = -position;
        const float dead = std::clamp(shape.deadzone, 0.0F, 0.95F);
        const float saturated = std::clamp(shape.saturation, 0.05F, 1.0F);
        const float size = std::abs(position);
        if (size <= dead) return 0.0F;
        const float ramp = std::clamp((size - dead) / std::max(saturated - dead, 1e-4F), 0.0F, 1.0F);
        return std::clamp(std::copysign(ramp * shape.sensitivity, position), -1.0F, 1.0F);
    }

    ThrottleSample EvaluateThrottle(const ThrottleConfig& config, float position) noexcept
    {
        if (!std::isfinite(position)) return {};
        long raw = std::lround((std::clamp(position, -1.0F, 1.0F) + 1.0F) * 0.5F * static_cast<float>(kAxisMaximum));
        if (config.invert) raw = kAxisMaximum - raw;
        const auto ratio = [](long value, long range) {
            return static_cast<float>(value) / static_cast<float>(range);
        };

        ThrottleSample sample;
        float normal{};
        if (config.unipolar && config.unipolarReverse) {
            const long forwardStart = config.reverseZoneCenter + config.reverseZoneDeadzone;
            const long detentLow = config.detentCenter - config.detentDeadzone;
            const long rampStart = config.detentCenter + config.detentDeadzone;
            sample.reverse = raw < config.reverseZoneCenter - config.reverseZoneDeadzone;
            if (raw <= forwardStart) {
                normal = 0.0F;
            } else if (raw < detentLow) {
                normal = detentLow - forwardStart <= 0 ? 0.0F : 0.5F * ratio(raw - forwardStart, detentLow - forwardStart);
            } else if (raw <= rampStart) {
                normal = 0.5F;
            } else if (config.boostZone) {
                const long boostStart = config.boostZoneCenter - config.boostZoneDeadzone;
                normal = raw >= boostStart || boostStart - rampStart <= 0 ? 1.0F :
                    0.5F + 0.5F * ratio(raw - rampStart, boostStart - rampStart);
            } else {
                normal = kAxisMaximum - rampStart <= 0 ? 1.0F : std::clamp(
                    (0.5F + 0.5F * ratio(raw - rampStart, kAxisMaximum - rampStart)) / config.saturation, 0.0F, 1.0F);
            }
        } else if (config.unipolar) {
            const float travel = ratio(raw, kAxisMaximum);
            normal = travel < config.idlePlateau ? 0.0F : std::clamp(
                (travel - config.idlePlateau) / (1.0F - config.idlePlateau) / config.saturation, 0.0F, 1.0F);
        } else {
            const long high = config.detentCenter + config.detentDeadzone;
            if (raw > high) normal = kAxisMaximum - high <= 0 ? 0.0F : ratio(raw - high, kAxisMaximum - high);
            // Below the detent is zero thrust: this lever layout has never reversed.
        }
        sample.boost = config.boostZone && config.unipolar && raw > config.boostZoneCenter + config.boostZoneDeadzone;
        sample.throttle = sample.reverse ? 0.0F : std::clamp(normal * config.sensitivity, 0.0F, 1.0F);
        return sample;
    }

    bool ReverseAxisHeld(const ReverseAxisConfig& config, float position) noexcept
    {
        if (!config.enabled || !std::isfinite(position)) return false;
        float value = (std::clamp(position, -1.0F, 1.0F) + 1.0F) * 0.5F;
        if (config.invert) value = 1.0F - value;
        value = std::clamp(value * config.sensitivity, 0.0F, 1.0F);
        const float dead = std::clamp(config.deadzone, 0.0F, 0.95F);
        value = value <= dead ? 0.0F : (value - dead) / (1.0F - dead);
        value = std::clamp(value / std::clamp(config.saturation, 0.05F, 1.0F), 0.0F, 1.0F);
        return value > config.activation;
    }

    namespace
    {
        constexpr double kTravelPerPercent = 655.35;

        // A percentage in the file, as the fraction the maths uses.
        float Fraction(const IniValues& ini, std::string_view section, std::string_view key, float fallback, float low, float high)
        {
            return std::clamp(Float(ini, section, key, fallback * 100.0F) / 100.0F, low, high);
        }

        // A percentage of the lever's travel in the file, as a position 0 to 65535.
        long Travel(const IniValues& ini, std::string_view section, std::string_view key, float fallbackPercent)
        {
            return std::lround(std::clamp<double>(Float(ini, section, key, fallbackPercent), 0, 100) * kTravelPerPercent);
        }

        AxisShape ReadShape(const IniValues& ini, std::string_view section, float deadzoneDefault)
        {
            AxisShape shape;
            shape.invert = Bool(ini, section, "Invert", false);
            shape.sensitivity = Float(ini, section, "Sensitivity", 100.0F) / 100.0F;
            shape.deadzone = Fraction(ini, section, "Deadzone", deadzoneDefault, 0.0F, 0.95F);
            shape.saturation = Fraction(ini, section, "FullOutputAt", 1.0F, 0.05F, 1.0F);
            return shape;
        }

        // Up to three decimals, without trailing zeros.
        std::string Text(double value)
        {
            auto text = std::format("{:.3f}", value);
            text.erase(text.find_last_not_of('0') + 1);
            if (text.ends_with('.')) text.pop_back();
            return text;
        }

        std::string Text(bool value) { return value ? "true" : "false"; }

        // Only a binding that can be read is carried; anything else is left unbound.
        std::string Token(const ConfiguredBinding& binding)
        {
            if (binding.status != BindingStatus::Bound) return {};
            const auto first = binding.token.find_first_not_of(" \t");
            return first == std::string::npos ? std::string{} :
                binding.token.substr(first, binding.token.find_last_not_of(" \t") - first + 1);
        }
    }

    FlightConfig LoadFlightConfig(const IniValues& ini)
    {
        FlightConfig config;
        config.enabled = Bool(ini, "General", "Enabled", true);
        config.throttle = Axis(ini, "Throttle", "Axis", "Throttle");
        config.pitch = Axis(ini, "Pitch", "Axis", "Pitch");
        config.yaw = Axis(ini, "Yaw", "Axis", "Yaw");
        config.roll = Axis(ini, "Roll", "Axis", "Roll");
        config.strafeLateral = Axis(ini, "StrafeSideways", "Axis", "StrafeLateral");
        config.strafeVertical = Axis(ini, "StrafeVertical", "Axis", "StrafeVertical");
        config.reverse = Axis(ini, "Throttle", "ReverseAxis", "Reverse");

        config.pitchShape = ReadShape(ini, "Pitch", 0.0F);
        config.yawShape = ReadShape(ini, "Yaw", 0.0F);
        config.rollShape = ReadShape(ini, "Roll", 0.0F);
        config.strafeLateralShape = ReadShape(ini, "StrafeSideways", 0.05F);
        config.strafeVerticalShape = ReadShape(ini, "StrafeVertical", 0.05F);

        auto& throttle = config.throttleConfig;
        throttle.invert = Bool(ini, "Throttle", "Invert", false);
        throttle.sensitivity = Float(ini, "Throttle", "Sensitivity", 100.0F) / 100.0F;
        throttle.saturation = Fraction(ini, "Throttle", "FullThrustAt", 1.0F, 0.05F, 1.0F);
        throttle.idlePlateau = Fraction(ini, "Throttle", "IdleBand", 0.05F, 0.0F, 0.95F);
        throttle.unipolar = !Bool(ini, "Throttle", "Centred", false);
        throttle.unipolarReverse = Bool(ini, "Throttle", "ReverseZone", false);
        throttle.reverseZoneCenter = Travel(ini, "Throttle", "ReverseZoneCentre", kDefaultReverseZoneCentre);
        throttle.reverseZoneDeadzone = Travel(ini, "Throttle", "ReverseZoneWidth", kDefaultReverseZoneWidth);
        throttle.boostZone = Bool(ini, "Throttle", "BoostZone", false);
        throttle.boostZoneCenter = Travel(ini, "Throttle", "BoostZoneCentre", kDefaultBoostZoneCentre);
        throttle.boostZoneDeadzone = Travel(ini, "Throttle", "BoostZoneWidth", kDefaultBoostZoneWidth);
        throttle.detentCenter = Travel(ini, "Throttle", "CentreDetent", 50.0F);
        throttle.detentDeadzone = Travel(ini, "Throttle", "CentreDetentWidth", 0.763F);

        config.reverseAxis.invert = Bool(ini, "Throttle", "ReverseAxisInvert", false);
        config.rollEnabled = Bool(ini, "Roll", "Enabled", true);
        config.holdForBoost = Bool(ini, "Throttle", "HoldForBoost", true);
        config.reverseGateVelocity = std::clamp(Float(ini, "Throttle", "ReverseBelowSpeed", 5.0F), 0.0F, 100.0F);
        config.steerThroughAim = Bool(ini, "Steering", "ThroughAimPoint", false);
        config.aimSensitivity = Float(ini, "Steering", "AimSensitivity", 100.0F) / 100.0F;
        return config;
    }

    std::vector<SettingEntry> WriteFlightConfig(const FlightConfig& config)
    {
        std::vector<SettingEntry> entries;
        const auto add = [&](std::string_view section, std::string_view key, std::string value) {
            entries.push_back({ std::string(section), std::string(key), std::move(value) });
        };
        const auto& throttle = config.throttleConfig;
        add("General", "Enabled", Text(config.enabled));

        add("Throttle", "Axis", Token(config.throttle));
        add("Throttle", "Invert", Text(throttle.invert));
        add("Throttle", "Sensitivity", Text(throttle.sensitivity * 100.0));
        add("Throttle", "FullThrustAt", Text(throttle.saturation * 100.0));
        add("Throttle", "IdleBand", Text(throttle.idlePlateau * 100.0));
        add("Throttle", "ReverseZone", Text(throttle.unipolarReverse));
        add("Throttle", "ReverseZoneCentre", Text(throttle.reverseZoneCenter / kTravelPerPercent));
        add("Throttle", "ReverseZoneWidth", Text(throttle.reverseZoneDeadzone / kTravelPerPercent));
        add("Throttle", "BoostZone", Text(throttle.boostZone));
        add("Throttle", "BoostZoneCentre", Text(throttle.boostZoneCenter / kTravelPerPercent));
        add("Throttle", "BoostZoneWidth", Text(throttle.boostZoneDeadzone / kTravelPerPercent));
        add("Throttle", "HoldForBoost", Text(config.holdForBoost));
        add("Throttle", "ReverseBelowSpeed", Text(static_cast<double>(config.reverseGateVelocity)));
        add("Throttle", "ReverseAxis", Token(config.reverse));
        add("Throttle", "ReverseAxisInvert", Text(config.reverseAxis.invert));
        add("Throttle", "Centred", Text(!throttle.unipolar));
        add("Throttle", "CentreDetent", Text(throttle.detentCenter / kTravelPerPercent));
        add("Throttle", "CentreDetentWidth", Text(throttle.detentDeadzone / kTravelPerPercent));

        const auto axis = [&](std::string_view section, const ConfiguredBinding& binding, const AxisShape& shape) {
            add(section, "Axis", Token(binding));
            add(section, "Invert", Text(shape.invert));
            add(section, "Sensitivity", Text(shape.sensitivity * 100.0));
            add(section, "Deadzone", Text(shape.deadzone * 100.0));
            add(section, "FullOutputAt", Text(shape.saturation * 100.0));
        };
        axis("Pitch", config.pitch, config.pitchShape);
        axis("Yaw", config.yaw, config.yawShape);
        axis("Roll", config.roll, config.rollShape);
        add("Roll", "Enabled", Text(config.rollEnabled));
        axis("StrafeSideways", config.strafeLateral, config.strafeLateralShape);
        axis("StrafeVertical", config.strafeVertical, config.strafeVerticalShape);

        add("Steering", "ThroughAimPoint", Text(config.steerThroughAim));
        add("Steering", "AimSensitivity", Text(config.aimSensitivity * 100.0));
        return entries;
    }

    FlightConfig LoadHotasFlightConfig(const IniValues& ini)
    {
        FlightConfig config;
        config.enabled = Bool(ini, "General", "bEnabled", true) && Bool(ini, "Injection", "bEnableInjection", true);
        config.throttle = Axis(ini, "Hardware", "iThrottleAxis", "Throttle");
        config.pitch = Axis(ini, "Hardware", "iPitchAxis", "Pitch");
        config.yaw = Axis(ini, "Hardware", "iYawAxis", "Yaw");
        config.roll = Axis(ini, "Hardware", "iRollAxis", "Roll");
        config.strafeLateral = Axis(ini, "Hardware", "iStrafeLatAxis", "StrafeLateral");
        config.strafeVertical = Axis(ini, "Hardware", "iStrafeVertAxis", "StrafeVertical");
        config.reverse = Axis(ini, "Hardware", "iReverseAxis", "Reverse");

        // A missing bInvertPitch has always meant inverted.
        config.pitchShape = Shape(ini, "Pitch", "fPitchSaturation", "fPitchDeadzone", "fPitchSensitivity", 0.0F, true);
        config.yawShape = Shape(ini, "Yaw", "fYawSaturation", "fYawDeadzone", "fYawSensitivity", 0.0F, false);
        config.rollShape = Shape(ini, "Roll", "fRollSaturation", "fRollDeadzone", "fRollSensitivity", 0.0F, false);
        config.strafeLateralShape = Shape(ini, "StrafeLat", "fStrafeSaturation", "fStrafeDeadzone",
            "fStrafeSensitivity", 0.05F, false);
        config.strafeVerticalShape = Shape(ini, "StrafeVert", "fStrafeVertSaturation", "fStrafeVertDeadzone",
            "fStrafeSensitivity", 0.05F, false);

        auto& throttle = config.throttleConfig;
        throttle.invert = Bool(ini, "Hardware", "bInvertThrottle", false);
        throttle.sensitivity = Float(ini, "Hardware", "fThrottleSensitivity", 1.0F);
        throttle.saturation = std::clamp(Float(ini, "Hardware", "fThrottleSaturation", 1.0F), 0.05F, 1.0F);
        throttle.unipolar = Bool(ini, "Normalization", "bUnipolarMode", true);
        throttle.idlePlateau = std::clamp(Float(ini, "Normalization", "fIdlePlateau", 0.05F), 0.0F, 0.95F);
        throttle.unipolarReverse = Bool(ini, "Normalization", "bUnipolarReverse", false);
        throttle.reverseZoneCenter = Long(ini, "Normalization", "iReverseZoneCenter", 3000);
        throttle.reverseZoneDeadzone = Long(ini, "Normalization", "iReverseZoneDeadzone", 3000);
        throttle.boostZone = Bool(ini, "Normalization", "bBoostZone", false);
        throttle.boostZoneCenter = Long(ini, "Normalization", "iBoostZoneCenter", 62000);
        throttle.boostZoneDeadzone = Long(ini, "Normalization", "iBoostZoneDeadzone", 2000);
        throttle.detentCenter = Long(ini, "Normalization", "iDetentCenter", 32768);
        throttle.detentDeadzone = Long(ini, "Normalization", "iDetentDeadzone", 500);

        auto& reverse = config.reverseAxis;
        reverse.enabled = Bool(ini, "Normalization", "bReverseAxisEnabled", true);
        reverse.invert = Bool(ini, "Hardware", "bInvertReverse", false);
        reverse.sensitivity = Float(ini, "Hardware", "fReverseSensitivity", 1.0F);
        reverse.saturation = std::clamp(Float(ini, "Hardware", "fReverseSaturation", 1.0F), 0.05F, 1.0F);
        reverse.deadzone = Float(ini, "Normalization", "fReverseDeadzone", 0.05F);
        reverse.activation = Float(ini, "Normalization", "fReverseActivationThreshold", 0.05F);

        config.rollEnabled = Bool(ini, "Injection", "bRollEnabled", true);
        config.holdForBoost = Bool(ini, "DualStick", "bHoldForBoost", Bool(ini, "Injection", "bHoldForBoost", true));
        config.throttleBurstMilliseconds = static_cast<int>(std::clamp(Long(ini, "Injection", "iThrottleBurstMs", 250), 0L, 5000L));
        config.reverseGateVelocity = std::clamp(Float(ini, "DualStick", "fReverseGateVelocity", 5.0F), 0.0F, 100.0F);
        // The stick steers through the aim point only when nothing else is bound to aim.
        // With separate aim axes or HOSAS mode it steers directly, as it always has.
        bool aimBound = Bool(ini, "Aim", "bHOSAMMode", false);
        for (const auto key : { "iAimYawAxis", "iAimPitchAxis", "iDigitalAimLeftButton", "iDigitalAimRightButton",
            "iDigitalAimUpButton", "iDigitalAimDownButton" }) {
            aimBound = aimBound || Bound(ini, "Aim", key);
        }
        config.steerThroughAim = Bool(ini, "Aim", "bSourceObjectAim", false) && !aimBound;
        config.aimSensitivity = Float(ini, "Aim", "fAimSensitivity", 1.0F);
        config.ignored = IgnoredSettings(ini);
        return config;
    }

    FlightFrame ComputeFlightFrame(const FlightConfig& config, const FlightInputs& inputs) noexcept
    {
        FlightFrame frame;
        if (!config.enabled) return frame;

        if (inputs.throttle) {
            const auto sample = EvaluateThrottle(config.throttleConfig, *inputs.throttle);
            frame.throttleBound = true;
            frame.throttle = sample.throttle;
            frame.reverse = sample.reverse;
            frame.boost = sample.boost;
        }
        if (inputs.reverse && ReverseAxisHeld(config.reverseAxis, *inputs.reverse)) frame.reverse = true;
        if (frame.reverse) frame.throttle = 0.0F;

        frame.pitchYawBound = inputs.pitch || inputs.yaw;
        frame.pitch = inputs.pitch ? ShapeAxis(*inputs.pitch, config.pitchShape) : 0.0F;
        frame.yaw = inputs.yaw ? ShapeAxis(*inputs.yaw, config.yawShape) : 0.0F;

        const float roll = inputs.roll && config.rollEnabled ? ShapeAxis(*inputs.roll, config.rollShape) : 0.0F;
        const float strafe = inputs.strafeLateral ? ShapeAxis(*inputs.strafeLateral, config.strafeLateralShape) : 0.0F;
        const float vertical = inputs.strafeVertical ? ShapeAxis(*inputs.strafeVertical, config.strafeVerticalShape) : 0.0F;
        // Strafe counts as deflected from its travel, before shaping can scale it down.
        const auto deflected = [](const std::optional<float>& position, const AxisShape& shape) {
            return position && std::isfinite(*position) && std::abs(*position) > std::max(kActivationFloor, shape.deadzone);
        };
        const bool rollActive = std::abs(roll) > kActivationFloor;
        const bool strafeActive = deflected(inputs.strafeLateral, config.strafeLateralShape);
        frame.verticalActive = deflected(inputs.strafeVertical, config.strafeVerticalShape);

        frame.lateral = strafeActive ? strafe : roll;
        frame.lateralActive = strafeActive || rollActive;
        frame.vertical = vertical;
        frame.splitRoll = roll;
        frame.splitStrafe = strafe;
        frame.splitActive = strafeActive || rollActive;
        frame.strafeModifier = strafeActive || frame.verticalActive;
        return frame;
    }
}
