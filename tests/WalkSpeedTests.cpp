#include "runtime/ShipControlCore.h"
#include "runtime/WalkSpeed.h"

#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::cerr << __LINE__ << ": " << #__VA_ARGS__ << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;
    namespace D = DeviceDraft;

    constexpr std::uint64_t kMs = 1'000'000;

    bool Near(double left, double right)
    {
        return std::abs(left - right) < 0.0001;
    }

    bool TestAxisTokens()
    {
        const auto named = ParseAxisBinding("Walk", "S-TECS SPACE-L THROTTLE STANDARD STEM@0x32");
        CHECK(named.status == BindingStatus::Bound && !named.devicePhysicalId);
        CHECK(named.device == "S-TECS SPACE-L THROTTLE STANDARD STEM" && named.channel == "axis/2");
        CHECK(ParseAxisBinding("Walk", "Stick@0x30").channel == "axis/0");
        CHECK(ParseAxisBinding("Walk", "Stick@0x37").channel == "axis/7");
        CHECK(ParseAxisBinding("Walk", "Stick@50").channel == "axis/2");

        constexpr auto kGuid = "{01234567-89ab-cdef-0123-456789abcdef}";
        const auto capture = ParseAxisBinding("Walk", std::string(kGuid) + "@axis:0x31");
        CHECK(capture.status == BindingStatus::Bound && capture.devicePhysicalId && capture.channel == "axis/1");
        const auto selector = ParseAxisBinding("Walk", std::string("guid:") + kGuid + "@0x31");
        CHECK(selector.status == BindingStatus::Bound && selector.devicePhysicalId && selector.channel == "axis/1");

        CHECK(ParseAxisBinding("Walk", "").status == BindingStatus::Unbound);
        CHECK(ParseAxisBinding("Walk", "Stick@0x2F").status == BindingStatus::Invalid);
        CHECK(ParseAxisBinding("Walk", "Stick@0x38").status == BindingStatus::Invalid);
        CHECK(ParseAxisBinding("Walk", "Stick@1").status == BindingStatus::Invalid);
        CHECK(ParseAxisBinding("Walk", "#0@0x30").status == BindingStatus::Unsupported);
        // A button token can't bind an axis, and an axis token can't bind a button.
        CHECK(ParseAxisBinding("Walk", std::string(kGuid) + "@button:3").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("Fire", std::string(kGuid) + "@axis:0x30").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("Fire", "Stick@0x30").channel == "button/47");
        return true;
    }

    bool TestSettings()
    {
        IniValues empty;
        const auto defaults = ParseWalkSpeedSettings(empty);
        CHECK(!defaults.enabled && defaults.axis.empty() && !defaults.invertAxis);
        CHECK(defaults.wheelModifier == WheelModifier::Alt && defaults.speedKeys == SpeedKeys::Numpad);
        CHECK(Near(defaults.step, 0.05) && Near(defaults.speed, 1.0) && defaults.showGauge);

        WalkSpeedSettings settings;
        settings.enabled = true;
        settings.showGauge = false;
        settings.axis = "S-TECS SPACE-L THROTTLE STANDARD STEM@0x32";
        settings.invertAxis = true;
        settings.wheelModifier = WheelModifier::Shift;
        settings.speedKeys = SpeedKeys::PageKeys;
        settings.step = 0.1;
        settings.speed = 0.375;
        IniValues ini;
        ini.Merge(FormatWalkSpeedSettings(settings));
        const auto loaded = ParseWalkSpeedSettings(ini);
        CHECK(loaded.enabled && loaded.invertAxis && !loaded.showGauge && loaded.axis == settings.axis);
        CHECK(loaded.wheelModifier == WheelModifier::Shift && loaded.speedKeys == SpeedKeys::PageKeys);
        CHECK(Near(loaded.step, 0.1) && Near(loaded.speed, 0.375));

        IniValues bad;
        bad.Merge("[WalkSpeed]\nEnabled=maybe\nWheelModifier=Banana\nSpeedKeys=Banana\nStep=9\nSpeed=-3\n");
        const auto clamped = ParseWalkSpeedSettings(bad);
        CHECK(!clamped.enabled && clamped.wheelModifier == WheelModifier::Alt && clamped.speedKeys == SpeedKeys::Numpad);
        CHECK(Near(clamped.step, 0.25) && Near(clamped.speed, 0.0));
        return true;
    }

    bool TestSelector()
    {
        CHECK(Near(AxisToSpeed(-1, false), 0) && Near(AxisToSpeed(0, false), 0.5) && Near(AxisToSpeed(1, false), 1));
        CHECK(Near(AxisToSpeed(-1, true), 1) && Near(AxisToSpeed(1, true), 0));
        CHECK(Near(AxisToSpeed(5, false), 1) && Near(ClampSpeed(std::nan("")), 1));

        WalkSpeedSelector selector(0.4);
        CHECK(selector.Source() == WalkSpeedSource::Virtual && Near(selector.Speed(), 0.4));
        selector.ObserveAxis(std::nullopt);
        CHECK(Near(selector.Speed(), 0.4));

        // The axis takes charge when it appears, and follows from there.
        selector.ObserveAxis(0.8);
        CHECK(selector.Source() == WalkSpeedSource::Axis && Near(selector.Speed(), 0.8));
        selector.ObserveAxis(0.7);
        CHECK(Near(selector.Speed(), 0.7));

        // Scrolling starts from the current speed, not the old saved one.
        selector.Nudge(-0.1);
        CHECK(selector.Source() == WalkSpeedSource::Virtual && Near(selector.Speed(), 0.6));
        // A parked axis, even with jitter, doesn't take it back.
        selector.ObserveAxis(0.705);
        selector.ObserveAxis(0.695);
        CHECK(selector.Source() == WalkSpeedSource::Virtual && Near(selector.Speed(), 0.6));
        // Really moving it does.
        selector.ObserveAxis(0.74);
        CHECK(selector.Source() == WalkSpeedSource::Axis && Near(selector.Speed(), 0.74));

        // Losing the axis keeps the speed.
        selector.ObserveAxis(std::nullopt);
        CHECK(selector.Source() == WalkSpeedSource::Virtual && Near(selector.Speed(), 0.74));
        // Its return counts as new.
        selector.ObserveAxis(0.2);
        CHECK(selector.Source() == WalkSpeedSource::Axis && Near(selector.Speed(), 0.2));

        selector.Nudge(-5);
        CHECK(Near(selector.Speed(), 0));
        selector.Nudge(5);
        CHECK(Near(selector.Speed(), 1));
        selector.SetVirtual(0.3);
        CHECK(selector.Source() == WalkSpeedSource::Virtual && Near(selector.Virtual(), 0.3));
        return true;
    }

    bool TestStick()
    {
        const auto forward = WalkStick({ .forward = true }, 0.5);
        CHECK(Near(forward.x, 0) && Near(forward.y, 0.5));
        const auto back = WalkStick({ .back = true }, 0.25);
        CHECK(Near(back.y, -0.25));
        const auto left = WalkStick({ .left = true }, 1.0);
        CHECK(Near(left.x, -1) && Near(left.y, 0));
        const auto cancel = WalkStick({ .forward = true, .back = true, .right = true }, 1.0);
        CHECK(Near(cancel.x, 1) && Near(cancel.y, 0));
        const auto none = WalkStick({ .forward = true, .back = true, .left = true, .right = true }, 1.0);
        CHECK(Near(none.x, 0) && Near(none.y, 0));
        // A diagonal is no faster than a straight line.
        const auto diagonal = WalkStick({ .forward = true, .right = true }, 1.0);
        CHECK(Near(diagonal.x * diagonal.x + diagonal.y * diagonal.y, 1.0));
        CHECK(Near(WalkStick({ .forward = true }, 0.0).y, 0));
        return true;
    }

    bool TestKeyRepeat()
    {
        SpeedKeyRepeat keys;
        std::uint64_t now = 1000 * kMs;
        CHECK(Near(keys.Update(false, now, 0.05), 0));
        // A tap is exactly one step, whatever its length.
        now += 8 * kMs;
        CHECK(Near(keys.Update(true, now, 0.05), 0.05));
        now += 8 * kMs;
        CHECK(Near(keys.Update(true, now, 0.05), 0));
        now += 8 * kMs;
        CHECK(Near(keys.Update(false, now, 0.05), 0));

        // Holding sweeps at a fixed rate once past the delay.
        now += 8 * kMs;
        CHECK(Near(keys.Update(true, now, 0.05), 0.05));
        now += SpeedKeyRepeat::kHoldDelayNanoseconds - kMs;
        CHECK(Near(keys.Update(true, now, 0.05), 0));
        now += 100 * kMs;
        CHECK(Near(keys.Update(true, now, 0.05), 0.05));
        now += 100 * kMs;
        CHECK(Near(keys.Update(true, now, 0.05), 0.05));
        now += 8 * kMs;
        CHECK(Near(keys.Update(false, now, 0.05), 0));
        return true;
    }

    bool TestGauge()
    {
        SpeedGauge gauge;
        std::uint64_t now = 1000 * kMs;
        const auto advance = [&](double speed, std::uint64_t milliseconds, bool allowed = true) {
            now += milliseconds * kMs;
            gauge.Update(speed, allowed, now);
        };

        // Stays hidden until the speed changes, including when it first appears.
        advance(0.5, 8);
        CHECK(!gauge.Visible() && Near(gauge.Alpha(), 0));
        advance(0.5, 1000);
        CHECK(!gauge.Visible());
        // Axis wobble isn't a change.
        advance(0.505, 8);
        CHECK(!gauge.Visible() && Near(gauge.Alpha(), 0));

        // A change wakes it, and it fades in rather than popping.
        advance(0.6, 8);
        CHECK(gauge.Visible());
        advance(0.6, 67);
        CHECK(Near(gauge.Alpha(), 0.5));
        advance(0.6, 100);
        CHECK(Near(gauge.Alpha(), 1));

        // It holds, and another change restarts the hold.
        advance(0.6, 1500);
        CHECK(Near(gauge.Alpha(), 1));
        advance(0.7, 300);
        advance(0.7, 1500);
        CHECK(gauge.Visible() && Near(gauge.Alpha(), 1));

        // Then it fades out and reports hidden.
        advance(0.7, 450);
        advance(0.7, 250);
        CHECK(gauge.Visible() && Near(gauge.Alpha(), 0.5));
        // A change partway through the fade brings it back from where it was.
        advance(0.8, 15);
        CHECK(gauge.Visible() && Near(gauge.Alpha(), 0.6));
        advance(0.8, 200);
        CHECK(Near(gauge.Alpha(), 1));
        advance(0.8, 1790);
        advance(0.8, 20);
        CHECK(gauge.Visible() && Near(gauge.Alpha(), 0.96));
        advance(0.8, 480);
        CHECK(Near(gauge.Alpha(), 0) && !gauge.Visible());
        advance(0.8, 1000);
        CHECK(!gauge.Visible());

        // A menu hides it at once, and a change made there doesn't show on the way out.
        advance(0.2, 100);
        CHECK(gauge.Visible());
        advance(0.2, 8, false);
        CHECK(!gauge.Visible() && Near(gauge.Alpha(), 0));
        advance(0.9, 8, false);
        advance(0.9, 8);
        CHECK(!gauge.Visible());
        return true;
    }

    template <std::size_t N> void Set(char (&output)[N], std::string_view text)
    {
        std::memcpy(output, text.data(), text.size());
        output[text.size()] = '\0';
    }

    constexpr auto kProvider = "test.provider";
    constexpr auto kPhysical = "{00000000-0000-0000-0000-000000000001}";

    bool TestCoreReadsAxis()
    {
        DeviceStore store;
        store.Start();
        DeviceDescription description;
        auto& device = description.device;
        Set(device.provider, kProvider);
        Set(device.physical, kPhysical);
        Set(device.displayName, " Test Throttle ");
        device.providerEpoch = device.enumerationRevision = device.connectionEpoch = 1;
        device.validity = D::Validity::Valid;
        for (const auto* id : { "axis/0", "axis/2", "axis/3" }) {
            D::Channel channel;
            channel.shape = D::Shape::Scalar;
            Set(channel.identity.provider, kProvider);
            Set(channel.identity.physical, kPhysical);
            Set(channel.identity.channel, id);
            Set(channel.displayName, id);
            channel.rawUnits = D::Units::ProviderDefined;
            channel.valueUnits = D::Units::Unitless;
            channel.components = 1;
            description.channels.push_back(channel);
        }
        device.channelCount = static_cast<std::uint32_t>(description.channels.size());
        CHECK(store.ReplaceInventory(std::span(&description, 1)) == D::Result::Ok);

        std::uint64_t sequence{};
        const auto publish = [&](std::uint64_t now, double value, bool rangeKnown = true) {
            DeviceFrame output;
            auto& frame = output.frame;
            Set(frame.provider, kProvider);
            Set(frame.physical, kPhysical);
            frame.providerEpoch = frame.enumerationRevision = frame.connectionEpoch = 1;
            frame.sequence = ++sequence;
            frame.sampledAt = frame.receivedAt = now;
            frame.clock = D::Clock::CoreMonotonicNanoseconds;
            frame.validity = D::Validity::Valid;
            for (const auto& channel : description.channels) {
                D::Sample sample;
                Set(sample.channel, channel.identity.channel);
                sample.shape = D::Shape::Scalar;
                sample.validity = D::Validity::Valid;
                sample.components = 1;
                sample.raw[0] = sample.value[0] = !std::strcmp(channel.identity.channel, "axis/2") ? value : 0.25;
                if (rangeKnown) {
                    sample.flags = D::kCalibrationRangeKnown;
                    sample.calibrationMinimum[0] = -1;
                    sample.calibrationMaximum[0] = 1;
                }
                output.samples.push_back(sample);
            }
            frame.sampleCount = static_cast<std::uint32_t>(output.samples.size());
            return store.Publish(output) == D::Result::Ok;
        };

        std::vector<std::string> logs;
        ShipControlCore core({}, [](std::string_view, bool) {},
            [&](std::string_view event, std::string_view detail) { logs.push_back(std::format("{} {}", event, detail)); });
        const auto logged = [&](std::string_view text) {
            for (const auto& line : logs) if (line.find(text) != std::string::npos) return true;
            return false;
        };
        const PilotObservation foot{ false, false, true };

        std::uint64_t now = 1000 * kMs;
        CHECK(publish(now, 0.5));
        core.Tick(store, foot, now);
        CHECK(!core.AxisValue());

        core.SetAxisBinding(ParseAxisBinding("Walk", "Test Throttle@0x32"));
        now += 8 * kMs;
        CHECK(publish(now, 0.5));
        core.Tick(store, foot, now);
        CHECK(logged("walk_axis status=bound token=Test Throttle@0x32"));
        CHECK(core.AxisValue() && Near(*core.AxisValue(), 0.5));
        now += 8 * kMs;
        CHECK(publish(now, -0.75));
        core.Tick(store, foot, now);
        CHECK(core.AxisValue() && Near(*core.AxisValue(), -0.75));

        // A stale frame is no reading at all.
        now += ShipControlCore::kMaximumFrameAgeNanoseconds + kMs;
        core.Tick(store, foot, now);
        CHECK(!core.AxisValue());
        // Nor is an axis with no known range.
        now += 8 * kMs;
        CHECK(publish(now, 0.5, false));
        core.Tick(store, foot, now);
        CHECK(!core.AxisValue());

        core.SetAxisBinding(ParseAxisBinding("Walk", "Test Throttle@0x35"));
        now += 8 * kMs;
        CHECK(publish(now, 0.5));
        core.Tick(store, foot, now);
        CHECK(logged("walk_axis status=channel_missing") && !core.AxisValue());

        core.SetAxisBinding(ParseAxisBinding("Walk", "Other Device@0x32"));
        now += 8 * kMs;
        CHECK(publish(now, 0.5));
        core.Tick(store, foot, now);
        CHECK(logged("walk_axis status=device_missing") && !core.AxisValue());

        core.SetAxisBinding(ParseAxisBinding("Walk", ""));
        now += 8 * kMs;
        CHECK(publish(now, 0.5));
        core.Tick(store, foot, now);
        CHECK(!core.AxisValue());
        return true;
    }
}

int main()
{
    if (!TestAxisTokens() || !TestSettings() || !TestSelector() || !TestStick() ||
        !TestKeyRepeat() || !TestGauge() || !TestCoreReadsAxis()) return 1;
    std::cout << "walk speed tests passed\n";
    return 0;
}
