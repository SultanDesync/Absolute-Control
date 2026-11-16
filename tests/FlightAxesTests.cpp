#include "runtime/FlightAxes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::cerr << __LINE__ << ": " << #__VA_ARGS__ << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;

    bool Near(float value, float expected, float tolerance = 0.002F)
    {
        return std::abs(value - expected) <= tolerance;
    }

    // A lever position from the INI's 0 to 65535 units.
    float At(long raw)
    {
        return static_cast<float>(raw) / 65535.0F * 2.0F - 1.0F;
    }

    bool TestShape()
    {
        const AxisShape plain;
        CHECK(Near(ShapeAxis(0.5F, plain), 0.5F) && Near(ShapeAxis(-1.0F, plain), -1.0F));
        const AxisShape shaped{ false, 1.0F, 0.9F, 0.1F };
        CHECK(ShapeAxis(0.05F, shaped) == 0.0F && ShapeAxis(-0.1F, shaped) == 0.0F);
        CHECK(Near(ShapeAxis(0.5F, shaped), 0.5F) && Near(ShapeAxis(-0.5F, shaped), -0.5F));
        CHECK(Near(ShapeAxis(0.9F, shaped), 1.0F) && Near(ShapeAxis(1.0F, shaped), 1.0F));
        CHECK(Near(ShapeAxis(0.5F, { true, 1.0F, 0.9F, 0.1F }), -0.5F));
        // Sensitivity is a final gain, and the result still stops at full deflection.
        CHECK(Near(ShapeAxis(1.0F, { false, 0.5F, 1.0F, 0.0F }), 0.5F));
        CHECK(Near(ShapeAxis(0.75F, { false, 2.0F, 1.0F, 0.0F }), 1.0F));
        CHECK(ShapeAxis(std::nanf(""), plain) == 0.0F);
        return true;
    }

    bool TestThrottle()
    {
        ThrottleConfig plain;
        CHECK(EvaluateThrottle(plain, -1.0F).throttle == 0.0F);
        CHECK(EvaluateThrottle(plain, At(3000)).throttle == 0.0F); // Inside the idle plateau.
        CHECK(Near(EvaluateThrottle(plain, 0.0F).throttle, 0.4737F));
        CHECK(Near(EvaluateThrottle(plain, 1.0F).throttle, 1.0F));
        CHECK(!EvaluateThrottle(plain, -1.0F).reverse && !EvaluateThrottle(plain, 1.0F).boost);
        plain.invert = true;
        CHECK(Near(EvaluateThrottle(plain, -1.0F).throttle, 1.0F) && EvaluateThrottle(plain, 1.0F).throttle == 0.0F);
        plain.invert = false;
        plain.saturation = 0.5F;
        CHECK(Near(EvaluateThrottle(plain, 0.0F).throttle, 0.9474F) && Near(EvaluateThrottle(plain, 0.5F).throttle, 1.0F));
        plain.saturation = 1.0F;
        plain.sensitivity = 0.5F;
        CHECK(Near(EvaluateThrottle(plain, 1.0F).throttle, 0.5F));

        // Reverse zone at the bottom, a detent at half thrust, and a boost zone at the top.
        ThrottleConfig zones;
        zones.unipolarReverse = true;
        zones.reverseZoneCenter = 6000;
        zones.reverseZoneDeadzone = 2000;
        zones.boostZone = true;
        const auto reverse = EvaluateThrottle(zones, At(2000));
        CHECK(reverse.reverse && reverse.throttle == 0.0F && !reverse.boost);
        const auto idle = EvaluateThrottle(zones, At(6000));
        CHECK(!idle.reverse && idle.throttle == 0.0F);
        CHECK(Near(EvaluateThrottle(zones, At(20134)).throttle, 0.25F));
        CHECK(Near(EvaluateThrottle(zones, At(32768)).throttle, 0.5F));
        CHECK(Near(EvaluateThrottle(zones, At(46634)).throttle, 0.75F));
        // Full thrust is reached at the zone's lower edge; the boosters fire past its upper edge.
        const auto full = EvaluateThrottle(zones, At(61000));
        CHECK(Near(full.throttle, 1.0F) && !full.boost);
        CHECK(EvaluateThrottle(zones, At(65000)).boost);
        // The shipped reverse zone of 3000 +/- 3000 starts at zero, so it never triggers.
        ThrottleConfig shipped;
        shipped.unipolarReverse = true;
        CHECK(!EvaluateThrottle(shipped, -1.0F).reverse);

        // A boost zone without the reverse layout fires without reshaping the ramp.
        ThrottleConfig boostOnly;
        boostOnly.boostZone = true;
        CHECK(EvaluateThrottle(boostOnly, At(65000)).boost && Near(EvaluateThrottle(boostOnly, At(65000)).throttle, 0.991F));

        ThrottleConfig bipolar;
        bipolar.unipolar = false;
        CHECK(EvaluateThrottle(bipolar, 0.0F).throttle == 0.0F && Near(EvaluateThrottle(bipolar, 1.0F).throttle, 1.0F));
        const auto low = EvaluateThrottle(bipolar, -1.0F);
        CHECK(low.throttle == 0.0F && !low.reverse && !low.boost);
        return true;
    }

    bool TestReverseAxis()
    {
        ReverseAxisConfig config;
        CHECK(!ReverseAxisHeld(config, -1.0F) && !ReverseAxisHeld(config, At(4000)) && ReverseAxisHeld(config, 1.0F));
        config.invert = true;
        CHECK(ReverseAxisHeld(config, -1.0F) && !ReverseAxisHeld(config, 1.0F));
        config.enabled = false;
        CHECK(!ReverseAxisHeld(config, -1.0F));
        return true;
    }

    bool TestLoad()
    {
        const auto defaults = LoadHotasFlightConfig({});
        CHECK(defaults.enabled && defaults.throttle.status == BindingStatus::Unbound && defaults.ignored.empty());
        CHECK(defaults.pitchShape.invert && !defaults.yawShape.invert && defaults.rollEnabled && defaults.holdForBoost);
        CHECK(defaults.throttleConfig.unipolar && Near(defaults.throttleConfig.idlePlateau, 0.05F));
        CHECK(Near(defaults.strafeLateralShape.deadzone, 0.05F) && defaults.throttleBurstMilliseconds == 250);

        IniValues ini;
        ini.Merge("[Hardware]\niThrottleAxis = Throttle@0x32\niPitchAxis = Stick@0x31\niRollAxis = -1\n"
            "bInvertPitch = false\nfPitchSensitivity = 0.8\nfPitchSaturation = 5\nfYawDeadzone = 0.1\n"
            "fStrafeSensitivity = 0.6\nbInvertThrottle = true\n"
            "[Normalization]\nbUnipolarReverse = true\niReverseZoneCenter = 6000\nfIdlePlateau = 0.1\nbBoostZone = TRUE\n"
            "[Injection]\nbRollEnabled = false\niThrottleBurstMs = 100\n"
            "[Aim]\nbSourceObjectAim = true\nfAimSensitivity = 1.5\niAimYawAxis = Stick@0x35\nbHOSAMMode = true\n"
            "[DualStick]\nbAccumulatorThrottle = true\nbHoldForBoost = false\nfReverseGateVelocity = 12\n"
            "[DigitalAxes]\niDigitalRollLeftButton = Stick@4\niDigitalRollRightButton = -1\n"
            "[Buttons]\nbAlwaysOn = false\n[Gate]\nPilotGateMode = Full\nPilotSignal = Auto\n");
        const auto config = LoadHotasFlightConfig(ini);
        CHECK(config.throttle.status == BindingStatus::Bound && config.throttle.device == "Throttle" &&
            config.throttle.channel == "axis/2");
        CHECK(config.pitch.channel == "axis/1" && config.roll.status == BindingStatus::Unbound);
        CHECK(!config.pitchShape.invert && Near(config.pitchShape.sensitivity, 0.8F) && Near(config.pitchShape.saturation, 1.0F));
        CHECK(Near(config.yawShape.deadzone, 0.1F) && Near(config.strafeVerticalShape.sensitivity, 0.6F));
        CHECK(config.throttleConfig.invert && config.throttleConfig.unipolarReverse && config.throttleConfig.boostZone);
        CHECK(config.throttleConfig.reverseZoneCenter == 6000 && Near(config.throttleConfig.idlePlateau, 0.1F));
        CHECK(!config.rollEnabled && !config.holdForBoost && config.throttleBurstMilliseconds == 100);
        // Separate aim axes are bound here, so the stick steers directly.
        CHECK(!(config.reticle == ReticleMode::FlightStick) && Near(config.aimSensitivity, 1.5F) && Near(config.reverseGateVelocity, 12.0F));
        IniValues aimOnly;
        aimOnly.Merge("[Aim]\nbSourceObjectAim = true\niAimYawAxis = \n");
        CHECK((LoadHotasFlightConfig(aimOnly).reticle == ReticleMode::FlightStick));
        aimOnly.Merge("[Aim]\nbHOSAMMode = true\n");
        CHECK(!(LoadHotasFlightConfig(aimOnly).reticle == ReticleMode::FlightStick));
        const auto ignored = [&](std::string_view name) { return std::ranges::find(config.ignored, name) != config.ignored.end(); };
        CHECK(!ignored("Aim/iAimYawAxis") && ignored("Aim/bHOSAMMode") && ignored("DualStick/bAccumulatorThrottle"));
        // The aim axis is carried; HOSAS mode had left the reticle to the mouse.
        CHECK(config.aimYaw.channel == "axis/5" && config.reticle == ReticleMode::Mouse);
        IniValues miniStick;
        miniStick.Merge("[Aim]\nbSourceObjectAim = true\niAimYawAxis = Stick@0x33\niAimPitchAxis = Stick@0x34\nfAimPitchSensitivity = 3\nbInvertAimPitch = true\n");
        const auto mini = LoadHotasFlightConfig(miniStick);
        CHECK(mini.reticle == ReticleMode::OwnAxes && mini.aimPitchShape.invert && Near(mini.aimPitchShape.sensitivity, 3.0F));
        const auto miniRead = LoadFlightConfig([&] { IniValues own; for (const auto& entry : WriteFlightConfig(mini)) own.Set(entry.section, entry.key, entry.value); return own; }());
        CHECK(miniRead.reticle == ReticleMode::OwnAxes && miniRead.aimYaw.channel == "axis/3" && Near(miniRead.aimPitchShape.sensitivity, 3.0F));
        CHECK(Near(ComputeFlightFrame(mini, { {}, {}, {}, {}, {}, {}, {}, 0.5F, 0.5F }).aimPitch, -1.0F));
        CHECK(ignored("DigitalAxes/iDigitalRollLeftButton") && !ignored("DigitalAxes/iDigitalRollRightButton"));
        CHECK(ignored("Buttons/bAlwaysOn") && ignored("Gate/PilotGateMode") && !ignored("Gate/PilotSignal"));
        CHECK(config.ignored.size() == 5);

        ini.Merge("[Injection]\nbEnableInjection = false\n");
        CHECK(!LoadHotasFlightConfig(ini).enabled);
        return true;
    }

    bool TestFrame()
    {
        FlightConfig config;
        config.pitchShape.invert = false;
        config.strafeLateralShape.deadzone = 0.05F;

        FlightInputs inputs;
        auto frame = ComputeFlightFrame(config, inputs);
        CHECK(!frame.throttleBound && !frame.pitchYawBound && !frame.lateralActive && !frame.splitActive && !frame.strafeModifier);

        inputs.throttle = 1.0F;
        inputs.pitch = 0.5F;
        inputs.roll = 0.5F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(frame.throttleBound && Near(frame.throttle, 1.0F) && frame.pitchYawBound && Near(frame.pitch, 0.5F) && frame.yaw == 0.0F);
        // Roll alone has the shared lane, with no flight mode key.
        CHECK(frame.lateralActive && Near(frame.lateral, 0.5F) && frame.splitActive && Near(frame.splitRoll, 0.5F));
        CHECK(frame.splitStrafe == 0.0F && !frame.strafeModifier && !frame.verticalActive);

        // Strafe takes the shared lane while deflected; the split keeps both.
        inputs.strafeLateral = -0.4F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(Near(frame.lateral, -0.368F) && Near(frame.splitStrafe, -0.368F) && Near(frame.splitRoll, 0.5F));
        CHECK(frame.lateralActive && frame.strafeModifier);
        inputs.strafeLateral = 0.04F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(Near(frame.lateral, 0.5F) && !frame.strafeModifier);

        inputs.strafeVertical = 0.6F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(frame.verticalActive && frame.vertical > 0.5F && frame.strafeModifier);

        // A small roll leaves the lane to the game.
        inputs = {};
        inputs.roll = 0.03F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(!frame.lateralActive && !frame.splitActive);
        config.rollEnabled = false;
        inputs.roll = 0.8F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(!frame.lateralActive && frame.splitRoll == 0.0F);

        // Either reverse source zeroes the throttle.
        inputs = {};
        inputs.throttle = 1.0F;
        inputs.reverse = 1.0F;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(frame.reverse && frame.throttle == 0.0F);

        config.enabled = false;
        frame = ComputeFlightFrame(config, inputs);
        CHECK(!frame.throttleBound && !frame.reverse);
        return true;
    }
    // Absolute Control's own file: its defaults, and that what the import writes reads back
    // as the configuration the HOTAS files described.
    bool TestOwnFormat()
    {
        const auto fresh = LoadFlightConfig({});
        CHECK(fresh.enabled && fresh.throttle.status == BindingStatus::Unbound && fresh.ignored.empty());
        CHECK(!fresh.pitchShape.invert && Near(fresh.strafeLateralShape.deadzone, 0.05F) && fresh.rollEnabled && fresh.holdForBoost);
        CHECK(fresh.throttleConfig.unipolar && Near(fresh.throttleConfig.idlePlateau, 0.05F) && !(fresh.reticle == ReticleMode::FlightStick));
        // A new setup's zones work as soon as they are switched on.
        CHECK(fresh.throttleConfig.reverseZoneCenter - fresh.throttleConfig.reverseZoneDeadzone > 0);
        CHECK(fresh.throttleConfig.boostZoneCenter + fresh.throttleConfig.boostZoneDeadzone < 65535);

        IniValues own;
        own.Merge("[Throttle]\nAxis = Lever@0x32\nIdleBand = 10\nReverseZone = true\nReverseZoneCentre = 7.5\nBoostZoneWidth = 4\n"
            "[Pitch]\nAxis = Stick@0x31\nInvert = true\nSensitivity = 80\nDeadzone = 12\nFullOutputAt = 90\n[Roll]\nEnabled = false\n");
        const auto read = LoadFlightConfig(own);
        CHECK(read.throttle.channel == "axis/2" && Near(read.throttleConfig.idlePlateau, 0.10F) && read.throttleConfig.unipolarReverse);
        CHECK(read.throttleConfig.reverseZoneCenter == 4915 && read.throttleConfig.boostZoneDeadzone == 2621);
        CHECK(read.pitch.channel == "axis/1" && read.pitchShape.invert && Near(read.pitchShape.sensitivity, 0.8F));
        CHECK(Near(read.pitchShape.deadzone, 0.12F) && Near(read.pitchShape.saturation, 0.9F) && !read.rollEnabled);

        IniValues hotas;
        hotas.Merge("[Hardware]\niThrottleAxis = S-TECS@0x32\niPitchAxis = Stick@0x31\niRollAxis = Pedals@0x33\niStrafeLatAxis = #0@0x30\n"
            "bInvertPitch = true\nbInvertRoll = true\nfPitchSensitivity = 0.8\nfYawDeadzone = 0.1\nfRollSaturation = 0.85\n"
            "fStrafeSensitivity = 0.6\nfStrafeVertDeadzone = 0.08\nbInvertThrottle = true\nfThrottleSaturation = 0.9\n"
            "[Normalization]\nbUnipolarReverse = true\niReverseZoneCenter = 4832\niReverseZoneDeadzone = 3538\nfIdlePlateau = 0.07\n"
            "bBoostZone = true\niBoostZoneCenter = 61083\niBoostZoneDeadzone = 3407\niDetentCenter = 32768\niDetentDeadzone = 500\n"
            "[Injection]\nbRollEnabled = false\n[DualStick]\nbHoldForBoost = false\nfReverseGateVelocity = 12\n"
            "[Aim]\nbSourceObjectAim = true\nfAimSensitivity = 1.5\n");
        const auto before = LoadHotasFlightConfig(hotas);
        IniValues written;
        for (const auto& entry : WriteFlightConfig(before)) written.Set(entry.section, entry.key, entry.value);
        const auto after = LoadFlightConfig(written);

        CHECK(after.enabled == before.enabled && after.rollEnabled == before.rollEnabled && after.holdForBoost == before.holdForBoost);
        CHECK((after.reticle == ReticleMode::FlightStick) == (before.reticle == ReticleMode::FlightStick) && Near(after.aimSensitivity, before.aimSensitivity));
        CHECK(Near(after.reverseGateVelocity, before.reverseGateVelocity));
        CHECK(after.throttle.channel == before.throttle.channel && after.throttle.device == before.throttle.device);
        CHECK(after.pitch.channel == "axis/1" && after.roll.channel == "axis/3" && after.yaw.status == BindingStatus::Unbound);
        // A binding in a form that can't be read is left unbound, not copied across broken.
        CHECK(before.strafeLateral.status == BindingStatus::Unsupported && after.strafeLateral.status == BindingStatus::Unbound);
        const auto sameShape = [](const AxisShape& a, const AxisShape& b) {
            return a.invert == b.invert && Near(a.sensitivity, b.sensitivity) && Near(a.deadzone, b.deadzone) && Near(a.saturation, b.saturation);
        };
        CHECK(sameShape(after.pitchShape, before.pitchShape) && sameShape(after.yawShape, before.yawShape));
        CHECK(sameShape(after.rollShape, before.rollShape) && sameShape(after.strafeLateralShape, before.strafeLateralShape));
        CHECK(sameShape(after.strafeVerticalShape, before.strafeVerticalShape));
        const auto& a = after.throttleConfig;
        const auto& b = before.throttleConfig;
        CHECK(a.invert == b.invert && a.unipolar == b.unipolar && a.unipolarReverse == b.unipolarReverse && a.boostZone == b.boostZone);
        CHECK(Near(a.sensitivity, b.sensitivity) && Near(a.saturation, b.saturation) && Near(a.idlePlateau, b.idlePlateau));
        // Lever positions are kept as percentages to three decimals: within one unit of 65535.
        const auto close = [](long left, long right) { return std::abs(left - right) <= 1; };
        CHECK(close(a.reverseZoneCenter, b.reverseZoneCenter) && close(a.reverseZoneDeadzone, b.reverseZoneDeadzone));
        CHECK(close(a.boostZoneCenter, b.boostZoneCenter) && close(a.boostZoneDeadzone, b.boostZoneDeadzone));
        CHECK(close(a.detentCenter, b.detentCenter) && close(a.detentDeadzone, b.detentDeadzone));
        // The lever behaves the same before and after.
        for (const long raw : { 0L, 1000L, 5000L, 9000L, 20000L, 32768L, 50000L, 58000L, 63000L, 65535L }) {
            const auto x = EvaluateThrottle(b, At(raw)), y = EvaluateThrottle(a, At(raw));
            CHECK(Near(x.throttle, y.throttle, 0.001F) && x.reverse == y.reverse && x.boost == y.boost);
        }
        return true;
    }
}

int main()
{
    if (!TestShape() || !TestThrottle() || !TestReverseAxis() || !TestLoad() || !TestFrame() || !TestOwnFormat()) return 1;
    std::cout << "flight axes tests passed\n";
    return 0;
}
