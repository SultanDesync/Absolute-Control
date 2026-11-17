#include "runtime/LookAxes.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::cerr << __LINE__ << ": " << #__VA_ARGS__ << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;
    using Inputs = std::array<std::array<LookReading, 2>, kLookAxes>;

    bool Near(float value, float expected, float tolerance = 0.01F)
    {
        return std::abs(value - expected) <= tolerance;
    }

    LookConfig Unsmoothed()
    {
        auto config = LoadLookConfig({});
        config.smoothing = 0.0F;
        return config;
    }

    bool DefaultsBindOpenTrack()
    {
        const auto config = LoadLookConfig({});
        CHECK(config.enabled);
        CHECK(config.axes[0].input.binding.status == BindingStatus::Bound);
        CHECK(config.axes[0].input.binding.device == "OpenTrack");
        CHECK(config.axes[0].input.binding.channel == "axis/yaw");
        CHECK(config.axes[1].input.binding.channel == "axis/pitch");
        CHECK(config.axes[2].input.binding.channel == "axis/roll");
        CHECK(config.axes[0].second.binding.status == BindingStatus::Unbound);
        CHECK(Near(config.axes[0].maximum, 85.0F) && Near(config.axes[1].minimum, -60.0F) && Near(config.axes[2].maximum, 45.0F));
        CHECK(Near(config.firingCone, 30.0F));
        return true;
    }

    bool TrackerTokens()
    {
        const auto binding = ParseAxisBinding("LookYaw", "OpenTrack@Yaw");
        CHECK(binding.status == BindingStatus::Bound && binding.channel == "axis/yaw");
        CHECK(ParseAxisBinding("LookYaw", "OpenTrack@pitch").channel == "axis/pitch");
        CHECK(ParseAxisBinding("LookYaw", "OpenTrack@Sideways").status == BindingStatus::Invalid);
        // A button can't be bound to a named axis.
        CHECK(ParseShipBinding("Fire", "OpenTrack@Yaw").status != BindingStatus::Bound);
        CHECK(AxisTokenControl("axis/yaw") == "Yaw");
        CHECK(AxisTokenControl("axis/2") == "0x32");
        CHECK(!AxisTokenControl("button/2"));
        CHECK(!AxisTokenControl("axis/9"));
        return true;
    }

    bool RoundTrip()
    {
        auto config = LoadLookConfig({});
        config.axes[1].second.binding = ParseAxisBinding("LookPitch", "T.16000M@0x33");
        config.axes[1].second.invert = true;
        config.axes[2].enabled = false;
        config.axes[0].minimum = -100.0F;
        config.smoothing = 0.3F;
        config.firingCone = 0.0F;
        config.recentreButton = ParseShipBinding("LookRecentre", "T.16000M@5");
        IniValues ini;
        for (const auto& entry : WriteLookConfig(config)) ini.Set(entry.section, entry.key, entry.value);
        const auto read = LoadLookConfig(ini);
        CHECK(read.axes[1].second.binding.channel == "axis/3");
        CHECK(read.axes[1].second.invert);
        CHECK(!read.axes[2].enabled);
        CHECK(Near(read.axes[0].minimum, -100.0F));
        CHECK(Near(read.smoothing, 0.3F));
        CHECK(Near(read.firingCone, 0.0F));
        CHECK(read.recentreButton.channel == "button/4");
        CHECK(read.recentreKey == "Ctrl+Alt+C");
        // Unbinding the tracker is kept: an empty Axis isn't replaced by the default.
        IniValues unbound;
        unbound.Set("LookYaw", "Axis", "");
        CHECK(LoadLookConfig(unbound).axes[0].input.binding.status == BindingStatus::Unbound);
        return true;
    }

    bool TrackerAngleCentresAndLimits()
    {
        const auto config = Unsmoothed();
        LookMixer mixer;
        Inputs inputs{};
        // The first reading is the centre, wherever the head is.
        inputs[0][0] = { 10.0, true };
        auto frame = mixer.Tick(config, inputs, 0.008F, true);
        CHECK(frame.active);
        CHECK(Near(frame.degrees[0], 0.0F));
        inputs[0][0] = { 40.0, true };
        frame = mixer.Tick(config, inputs, 0.008F, true);
        CHECK(Near(frame.degrees[0], 30.0F));
        CHECK(Near(frame.tracker[0], 30.0F));
        // Past the limit it stops at it.
        inputs[0][0] = { 200.0, true };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], 85.0F));
        // Recentred: the current angle is the new centre.
        mixer.Recentre();
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], 0.0F));
        return true;
    }

    bool TrackerPitchIsBuiltIn()
    {
        auto config = Unsmoothed();
        LookMixer mixer;
        Inputs inputs{};
        inputs[1][0] = { 0.0, true };
        mixer.Tick(config, inputs, 0.008F, true);
        inputs[1][0] = { 10.0, true };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[1], -10.0F));
        config.axes[1].input.invert = true;
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[1], 10.0F));
        return true;
    }

    bool StickReachesLimitsAndAdds()
    {
        auto config = Unsmoothed();
        config.axes[0].second.deadzone = 0.0F;
        config.axes[0].minimum = -60.0F;
        LookMixer mixer;
        Inputs inputs{};
        // A stick alone: full right looks fully right, which is negative yaw in the game.
        inputs[0][1] = { 1.0, false };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], -60.0F));
        inputs[0][1] = { -1.0, false };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], 85.0F));
        inputs[0][1] = { -0.5, false };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], 42.5F));
        // With the tracker: they add, then the limit applies.
        inputs[0][0] = { 0.0, true };
        mixer.Tick(config, inputs, 0.008F, true);
        inputs[0][0] = { 20.0, true };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], 62.5F));
        inputs[0][0] = { 60.0, true };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[0], 85.0F));
        return true;
    }

    bool StickDeadzone()
    {
        auto config = Unsmoothed();
        config.axes[2].input.binding = {};
        config.axes[2].second.deadzone = 0.2F;
        LookMixer mixer;
        Inputs inputs{};
        inputs[2][1] = { 0.15, false };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[2], 0.0F));
        inputs[2][1] = { 0.6, false };
        CHECK(Near(mixer.Tick(config, inputs, 0.008F, true).degrees[2], -22.5F));
        return true;
    }

    bool InactiveWithoutReadingsOrWhenOff()
    {
        auto config = Unsmoothed();
        LookMixer mixer;
        Inputs inputs{};
        CHECK(!mixer.Tick(config, inputs, 0.008F, true).active);
        inputs[0][0] = { 5.0, true };
        CHECK(mixer.Tick(config, inputs, 0.008F, true).active);
        CHECK(!mixer.Tick(config, inputs, 0.008F, false).active);
        config.enabled = false;
        CHECK(!mixer.Tick(config, inputs, 0.008F, true).active);
        // A disabled axis stays at zero while the others work.
        config.enabled = true;
        config.axes[0].enabled = false;
        inputs[0][0] = { 50.0, true };
        inputs[1][1] = { 1.0, false };
        const auto frame = mixer.Tick(config, inputs, 0.008F, true);
        CHECK(frame.active && Near(frame.degrees[0], 0.0F) && !Near(frame.degrees[1], 0.0F));
        return true;
    }

    bool SmoothingEases()
    {
        auto config = LoadLookConfig({});
        config.smoothing = 0.5F;
        config.axes[0].second.deadzone = 0.0F;
        LookMixer mixer;
        Inputs inputs{};
        inputs[0][1] = { -1.0, false };
        // One sixtieth of a second keeps half the gap.
        CHECK(Near(mixer.Tick(config, inputs, 1.0F / 60.0F, true).degrees[0], 42.5F));
        CHECK(Near(mixer.Tick(config, inputs, 1.0F / 60.0F, true).degrees[0], 63.75F));
        return true;
    }

    bool KeyChords()
    {
        const auto chord = ParseKeyChord("Ctrl+Alt+C");
        CHECK(chord && chord->key == 'C' && chord->control && chord->alt && !chord->shift);
        CHECK(ParseKeyChord("shift + f5")->key == 0x74);
        CHECK(ParseKeyChord("0x31")->key == 0x31);
        CHECK(!ParseKeyChord(""));
        CHECK(!ParseKeyChord("Ctrl+Alt"));
        CHECK(!ParseKeyChord("Ctrl+Banana"));
        CHECK(!ParseKeyChord("A+B"));
        return true;
    }

    bool HeadTrackingImport()
    {
        IniValues ini;
        ini.Merge("[Tracking]\nbInvertPitch = true\nbInvertYaw = true\nfYawSensitivity = 1.5\nfRollMaximumDegrees = 30\n"
            "bRollEnabled = false\nfSmoothing = 0.25\n"
            "[AimRestriction]\nbLimitAimToShipForwardCone = true\nfFiringConeHalfAngleDegrees = 40\n"
            "[Hotkeys]\niRecenterKey = 0x43\nbRecenterCtrl = true\nbRecenterAlt = false\nbRecenterShift = true\n"
            "iToggleKey = 0\n"
            "[ManualLook]\nbInvertControllerPitch = true\nsPitchControllerAxisBinding = T.16000M@0x31\n");
        const auto config = LoadHeadTrackingLookConfig(ini);
        // Head Tracking's default pitch inversion is built in here.
        CHECK(!config.axes[1].input.invert);
        CHECK(config.axes[0].input.invert);
        CHECK(Near(config.axes[0].input.sensitivity, 1.5F));
        CHECK(Near(config.axes[2].maximum, 30.0F));
        CHECK(!config.axes[2].enabled);
        CHECK(Near(config.smoothing, 0.25F));
        CHECK(Near(config.firingCone, 40.0F));
        CHECK(config.recentreKey == "Ctrl+Shift+C");
        CHECK(config.toggleKey.empty());
        CHECK(config.axes[1].second.binding.channel == "axis/1");
        CHECK(!config.axes[1].second.invert);
        IniValues off;
        off.Merge("[AimRestriction]\nbLimitAimToShipForwardCone = false\n");
        CHECK(Near(LoadHeadTrackingLookConfig(off).firingCone, 0.0F));
        return true;
    }

    bool RotationAxes()
    {
        // Yaw turns about Z: 90 degrees is a quarter turn.
        const auto yaw = LookRotation({ 90.0F, 0.0F, 0.0F });
        CHECK(Near(yaw.w, 0.7071F, 0.001F) && Near(yaw.z, 0.7071F, 0.001F) && Near(yaw.x, 0.0F));
        const auto pitch = LookRotation({ 0.0F, 90.0F, 0.0F });
        CHECK(Near(pitch.x, 0.7071F, 0.001F) && Near(pitch.z, 0.0F));
        const auto still = LookRotation({ 0.0F, 0.0F, 0.0F });
        CHECK(Near(still.w, 1.0F));
        return true;
    }
}

int main()
{
    const bool passed = DefaultsBindOpenTrack() && TrackerTokens() && RoundTrip() && TrackerAngleCentresAndLimits() &&
        TrackerPitchIsBuiltIn() && StickReachesLimitsAndAdds() && StickDeadzone() && InactiveWithoutReadingsOrWhenOff() &&
        SmoothingEases() && KeyChords() && HeadTrackingImport() && RotationAxes();
    std::cout << (passed ? "look axes tests passed\n" : "look axes tests FAILED\n");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
