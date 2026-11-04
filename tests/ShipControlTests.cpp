#include "ShipActionCatalog.h"
#include "runtime/ButtonRouter.h"
#include "runtime/PilotSignal.h"
#include "runtime/ShipBindings.h"
#include "runtime/ShipControlCore.h"

#include <array>
#include <cstring>
#include <format>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    std::cerr << __LINE__ << ": " << #__VA_ARGS__ << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;
    namespace D = DeviceDraft;

    constexpr std::uint64_t kMs = 1'000'000;

    template <std::size_t N> void Set(char (&output)[N], std::string_view text)
    {
        std::memcpy(output, text.data(), text.size());
        output[text.size()] = '\0';
    }

    bool TestIni()
    {
        IniValues ini;
        ini.Merge("\xEF\xBB\xBF; comment\r\n[ShipButtons]\r\niCruiseButton = Stick@26\r\n# other\r\niRepairButton=Stick@6\r\n[Other]\r\niCruiseButton=Elsewhere@1\r\n");
        ini.Merge("[shipbuttons]\niRepairButton = Throttle@9\n");
        CHECK(ini.Get("ShipButtons", "iCruiseButton") == "Stick@26");
        CHECK(ini.Get("SHIPBUTTONS", "icruisebutton") == "Stick@26");
        CHECK(ini.Get("ShipButtons", "iRepairButton") == "Throttle@9");
        CHECK(ini.Get("Other", "iCruiseButton") == "Elsewhere@1");
        CHECK(!ini.Get("ShipButtons", "iMissing"));
        return true;
    }

    bool TestTokens()
    {
        // The form the HOTAS menu saves.
        auto named = ParseShipBinding("Cruise", " S-TECS SPACE-L THROTTLE STANDARD STEM@26 ");
        CHECK(named.status == BindingStatus::Bound && !named.devicePhysicalId);
        CHECK(named.device == "S-TECS SPACE-L THROTTLE STANDARD STEM" && named.channel == "button/25");
        CHECK(ParseShipBinding("A", "Stick@129").channel == "pov/0/direction/up");
        CHECK(ParseShipBinding("A", "Stick@144").channel == "pov/3/direction/left");
        CHECK(ParseShipBinding("A", "Stick@0x10").channel == "button/15");
        // A display name can't contain '@', but it can contain spaces and digits.
        CHECK(ParseShipBinding("A", "VKBSim Gunfighter 2 MCG@18").device == "VKBSim Gunfighter 2 MCG");

        constexpr auto kGuid = "{01234567-89ab-cdef-0123-456789abcdef}";
        constexpr auto kUpper = "{01234567-89AB-CDEF-0123-456789ABCDEF}";
        auto capture = ParseShipBinding("A", std::string(kGuid) + "@button:3");
        CHECK(capture.status == BindingStatus::Bound && capture.devicePhysicalId);
        CHECK(capture.device == kUpper && capture.channel == "button/2");
        auto selector = ParseShipBinding("A", std::string("guid:") + kGuid + "@3");
        CHECK(selector.status == BindingStatus::Bound && selector.devicePhysicalId);
        CHECK(selector.device == kUpper && selector.channel == "button/2");

        CHECK(ParseShipBinding("A", "").status == BindingStatus::Unbound);
        CHECK(ParseShipBinding("A", "-1").status == BindingStatus::Unbound);
        CHECK(ParseShipBinding("A", "42").status == BindingStatus::Unsupported);
        CHECK(ParseShipBinding("A", "#0@5").status == BindingStatus::Unsupported);
        CHECK(ParseShipBinding("A", "Stick@0").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("A", "Stick@145").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("A", "Stick@0x32x").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("A", "@5").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("A", std::string(kGuid) + "@axis:0x30").status == BindingStatus::Invalid);
        CHECK(ParseShipBinding("A", "guid:nonsense@5").status == BindingStatus::Invalid);
        return true;
    }

    bool TestCatalogAndLoad()
    {
        std::set<std::string_view> keys;
        for (const auto& action : kShipActionCatalog) {
            CHECK(keys.insert(action.sourceIniKey).second);
            CHECK(action.recommendedMethod != ShipControlMethod::Direct || action.allowedMethods == kDirectOnly);
        }
        CHECK(FindShipAction("ShipAction1")->displayLabel == "Targeting / Dock / Travel");
        CHECK(FindShipAction("ShipAction1")->sourceIniKey == "iShipAction1Button");

        IniValues ini;
        ini.Merge("[ShipButtons]\nbShipButtonsEnabled = true\niShipAction1Button = Stick@18\n");
        const auto loaded = LoadShipBindings(ini);
        CHECK(loaded.enabled && loaded.bindings.size() == kShipActionCatalog.size());
        std::size_t bound{};
        for (const auto& binding : loaded.bindings) {
            if (binding.status != BindingStatus::Bound) continue;
            ++bound;
            CHECK(binding.actionId == "ShipAction1" && binding.channel == "button/17");
        }
        CHECK(bound == 1);
        ini.Merge("[ShipButtons]\nbShipButtonsEnabled = FALSE\n");
        CHECK(!LoadShipBindings(ini).enabled);
        return true;
    }

    bool TestPilotSignal()
    {
        PilotSignal pilot;
        CHECK(!pilot.Piloting() && !pilot.ButtonsLive());
        pilot.Update({ true, false, false }, 10 * kMs);
        CHECK(pilot.Piloting() && !pilot.ButtonsLive());
        pilot.Update({ true, false, true }, 20 * kMs);
        CHECK(pilot.ButtonsLive());
        // Targeting Mode clears the flag but the player is still flying.
        pilot.Update({ false, true, true }, 30 * kMs);
        CHECK(pilot.ButtonsLive());
        // A brief gap on leaving Targeting Mode doesn't count as leaving the seat.
        pilot.Update({ false, false, true }, 40 * kMs);
        CHECK(pilot.ButtonsLive());
        pilot.Update({ false, false, true }, 129 * kMs);
        CHECK(pilot.ButtonsLive());
        pilot.Update({ true, false, true }, 130 * kMs);
        CHECK(pilot.ButtonsLive());
        // A clear that lasts is getting up.
        pilot.Update({ false, false, true }, 140 * kMs);
        CHECK(pilot.ButtonsLive());
        pilot.Update({ false, false, true }, 230 * kMs);
        CHECK(!pilot.Piloting() && !pilot.ButtonsLive());
        // A menu closes the gate without ending piloting.
        pilot.Update({ true, false, true }, 240 * kMs);
        pilot.Update({ true, false, false }, 250 * kMs);
        CHECK(pilot.Piloting() && !pilot.ButtonsLive());
        return true;
    }

    struct Intent
    {
        std::size_t index{};
        bool held{};
        bool operator==(const Intent&) const = default;
    };

    bool TestRouter()
    {
        ButtonRouter router(1);
        std::vector<Intent> intents;
        const auto update = [&](bool gate, ButtonSample sample) {
            router.Update(gate, std::span<const ButtonSample>(&sample, 1),
                [&](std::size_t index, bool held) { intents.push_back({ index, held }); });
        };
        // Down from the first sample: not a new press.
        update(true, { true, true, 5 });
        CHECK(intents.empty());
        update(true, { true, false, 5 });
        CHECK(intents.empty());
        update(true, { true, true, 6 });
        CHECK(intents == std::vector<Intent>{ { 0, true } });
        update(true, { true, false, 6 });
        CHECK((intents == std::vector<Intent>{ { 0, true }, { 0, false } }));
        intents.clear();

        // Pressed and released between samples still fires.
        update(true, { true, false, 7 });
        CHECK((intents == std::vector<Intent>{ { 0, true }, { 0, false } }));
        intents.clear();

        // The gate closing releases, and the same hold doesn't fire when it reopens.
        update(true, { true, true, 8 });
        update(false, { true, true, 8 });
        CHECK((intents == std::vector<Intent>{ { 0, true }, { 0, false } }));
        intents.clear();
        update(true, { true, true, 8 });
        CHECK(intents.empty());
        update(true, { true, false, 8 });
        update(true, { true, true, 9 });
        CHECK(intents == std::vector<Intent>{ { 0, true } });
        intents.clear();

        // Losing the device releases, and counters restarting on reconnect don't fire.
        update(true, { false, false, 9 });
        CHECK(intents == std::vector<Intent>{ { 0, false } });
        intents.clear();
        update(true, { true, false, 0 });
        update(true, { true, false, 0 });
        CHECK(intents.empty());

        // Nothing fires while the gate is closed.
        update(false, { true, true, 1 });
        update(false, { true, false, 1 });
        update(false, { true, false, 2 });
        CHECK(intents.empty());
        return true;
    }

    constexpr auto kProvider = "test.provider";
    constexpr auto kPhysical = "{00000000-0000-0000-0000-000000000001}";

    DeviceDescription Description(const char* name, const char* physical, std::uint64_t connection,
        D::Validity validity = D::Validity::Valid)
    {
        DeviceDescription result;
        auto& device = result.device;
        Set(device.provider, kProvider);
        Set(device.physical, physical);
        Set(device.displayName, name);
        device.providerEpoch = 1;
        device.enumerationRevision = 1;
        device.connectionEpoch = connection;
        device.validity = validity;
        for (int index = 0; index < 4; ++index) {
            D::Channel channel;
            channel.shape = D::Shape::Button;
            Set(channel.identity.provider, kProvider);
            Set(channel.identity.physical, physical);
            Set(channel.identity.channel, std::format("button/{}", index).c_str());
            Set(channel.displayName, std::format("Button {}", index + 1).c_str());
            channel.rawUnits = channel.valueUnits = D::Units::Boolean;
            channel.components = 1;
            result.channels.push_back(channel);
        }
        device.channelCount = static_cast<std::uint32_t>(result.channels.size());
        return result;
    }

    struct FakeBus
    {
        DeviceStore store;
        DeviceDescription description{ Description("Test Stick", kPhysical, 1) };
        std::uint64_t sequence{};
        std::uint64_t presses[4]{};
        bool down[4]{};

        FakeBus()
        {
            store.Start();
            Inventory();
        }
        void Inventory()
        {
            (void)store.ReplaceInventory(std::span(&description, 1));
        }
        [[nodiscard]] bool Publish(std::uint64_t now)
        {
            DeviceFrame output;
            auto& frame = output.frame;
            Set(frame.provider, kProvider);
            Set(frame.physical, description.device.physical);
            frame.providerEpoch = description.device.providerEpoch;
            frame.enumerationRevision = description.device.enumerationRevision;
            frame.connectionEpoch = description.device.connectionEpoch;
            frame.sequence = ++sequence;
            frame.sampledAt = frame.receivedAt = now;
            frame.clock = D::Clock::CoreMonotonicNanoseconds;
            frame.validity = description.device.validity;
            for (std::size_t index = 0; index < description.channels.size(); ++index) {
                D::Sample sample;
                Set(sample.channel, description.channels[index].identity.channel);
                sample.shape = D::Shape::Button;
                sample.validity = frame.validity;
                sample.components = 1;
                sample.counterBits = 64;
                sample.pressCount = presses[index];
                sample.raw[0] = sample.value[0] = down[index] ? 1 : 0;
                output.samples.push_back(sample);
            }
            frame.sampleCount = static_cast<std::uint32_t>(output.samples.size());
            return store.Publish(output) == D::Result::Ok;
        }
        void Press(int index)
        {
            down[index] = true;
            ++presses[index];
        }
    };

    ShipBindingConfig Bindings()
    {
        ShipBindingConfig config;
        config.bindings.push_back(ParseShipBinding("Primary", "test stick@1"));
        config.bindings.push_back(ParseShipBinding("Secondary", std::string(kPhysical) + "@button:2"));
        config.bindings.push_back(ParseShipBinding("Missing", "Other Device@1"));
        config.bindings.push_back(ParseShipBinding("NoChannel", "Test Stick@99"));
        config.bindings.push_back(ParseShipBinding("OldIndex", "#0@1"));
        config.bindings.push_back(ParseShipBinding("Unset", ""));
        return config;
    }

    struct Recorder
    {
        std::vector<std::string> intents;
        std::vector<std::string> logs;
        ActionSink Sink()
        {
            return [this](std::string_view action, bool held) {
                intents.push_back(std::format("{}={}", action, held ? 1 : 0));
            };
        }
        ControlLog Log()
        {
            return [this](std::string_view event, std::string_view detail) {
                logs.push_back(std::format("{} {}", event, detail));
            };
        }
        [[nodiscard]] bool Logged(std::string_view text) const
        {
            for (const auto& line : logs) if (line.find(text) != std::string::npos) return true;
            return false;
        }
    };

    bool TestCore()
    {
        FakeBus bus;
        Recorder record;
        ShipControlCore core(Bindings(), record.Sink(), record.Log());
        const PilotObservation seated{ true, false, true };
        const PilotObservation onFoot{ false, false, true };
        std::uint64_t now = 1000 * kMs;
        const auto step = [&](const PilotObservation& observation) {
            now += 8 * kMs;
            if (!bus.Publish(now)) return false;
            core.Tick(bus.store, observation, now);
            return true;
        };

        CHECK(step(onFoot));
        CHECK(!core.GateOpen());
        // Names match without regard to case, and each binding reports its own problem.
        CHECK(record.Logged("ship_binding action=Primary status=bound"));
        CHECK(record.Logged("ship_binding action=Secondary status=bound"));
        CHECK(record.Logged("ship_binding action=Missing status=device_missing"));
        CHECK(record.Logged("ship_binding action=NoChannel status=channel_missing"));
        CHECK(record.Logged("ship_binding action=OldIndex status=unsupported"));
        CHECK(!record.Logged("action=Unset"));
        CHECK(record.Logged("ship_bindings_resolved enabled=1 bound=2 problems=3 devices=1"));
        CHECK(record.Logged("input_device name=Test Stick"));
        CHECK(record.Logged("ship_gate open=0"));

        // On foot, nothing fires.
        bus.Press(0);
        CHECK(step(onFoot));
        bus.down[0] = false;
        CHECK(step(onFoot));
        CHECK(record.intents.empty());

        CHECK(step(seated));
        CHECK(core.GateOpen() && record.Logged("ship_gate open=1 seated=1 targeting=0 input=1"));
        bus.Press(0);
        CHECK(step(seated));
        CHECK(record.intents == std::vector<std::string>{ "Primary=1" });
        bus.down[0] = false;
        CHECK(step(seated));
        CHECK((record.intents == std::vector<std::string>{ "Primary=1", "Primary=0" }));
        record.intents.clear();

        // Getting up releases a held button after the debounce, and it can't fire on foot.
        bus.Press(1);
        CHECK(step(seated));
        CHECK(record.intents == std::vector<std::string>{ "Secondary=1" });
        for (int index = 0; index < 20; ++index) CHECK(step(onFoot));
        CHECK((record.intents == std::vector<std::string>{ "Secondary=1", "Secondary=0" }));
        CHECK(!core.GateOpen());
        record.intents.clear();
        // Still held when the player sits again: needs a fresh press.
        CHECK(step(seated));
        CHECK(step(seated));
        CHECK(record.intents.empty());
        bus.down[1] = false;
        CHECK(step(seated));
        bus.Press(1);
        CHECK(step(seated));
        CHECK(record.intents == std::vector<std::string>{ "Secondary=1" });
        record.intents.clear();

        // A frame older than the limit is a lost device.
        now += ShipControlCore::kMaximumFrameAgeNanoseconds + kMs;
        core.Tick(bus.store, seated, now);
        CHECK(record.intents == std::vector<std::string>{ "Secondary=0" });
        record.intents.clear();
        bus.down[1] = false;
        CHECK(step(seated));

        // A disconnect releases, and a reconnect under a new epoch is resolved again.
        bus.Press(0);
        CHECK(step(seated));
        CHECK(step(seated));
        CHECK(record.intents == std::vector<std::string>{ "Primary=1" });
        record.intents.clear();
        bus.description.device.validity = D::Validity::Disconnected;
        bus.Inventory();
        CHECK(step(seated));
        CHECK(record.intents == std::vector<std::string>{ "Primary=0" });
        record.intents.clear();
        bus.description.device.validity = D::Validity::Valid;
        bus.description.device.connectionEpoch = 2;
        bus.presses[0] = 0;
        bus.down[0] = false;
        bus.Inventory();
        CHECK(step(seated));
        CHECK(record.intents.empty());
        bus.Press(0);
        CHECK(step(seated));
        CHECK(record.intents == std::vector<std::string>{ "Primary=1" });
        return true;
    }

    bool TestCoreConfigProblems()
    {
        {
            // Names as real controllers reported them in game, with their padding.
            DeviceStore store;
            store.Start();
            std::array devices{ Description(" VKBSim Gunfighter MCG Ultimate ", kPhysical, 1),
                Description("S-TECS SPACE-L THROTTLE STANDARD STEM ", "{00000000-0000-0000-0000-000000000002}", 1),
                Description(" Twin ", "{00000000-0000-0000-0000-000000000003}", 1),
                Description("Twin", "{00000000-0000-0000-0000-000000000004}", 1) };
            CHECK(store.ReplaceInventory(devices) == D::Result::Ok);
            Recorder record;
            ShipBindingConfig config;
            config.bindings.push_back(ParseShipBinding("Primary", "VKBSim Gunfighter MCG Ultimate@1"));
            config.bindings.push_back(ParseShipBinding("Secondary", "S-TECS SPACE-L THROTTLE STANDARD STEM@2"));
            config.bindings.push_back(ParseShipBinding("Paired", "Twin@1"));
            ShipControlCore core(config, record.Sink(), record.Log());
            core.Tick(store, { true, false, true }, 1000 * kMs);
            CHECK(record.Logged("ship_binding action=Primary status=bound"));
            CHECK(record.Logged("ship_binding action=Secondary status=bound"));
            CHECK(record.Logged("ship_binding action=Paired status=device_ambiguous"));
        }
        {
            // Two devices with one name can't be told apart.
            DeviceStore store;
            store.Start();
            std::array devices{ Description("Twin", kPhysical, 1),
                Description("Twin", "{00000000-0000-0000-0000-000000000002}", 1) };
            CHECK(store.ReplaceInventory(devices) == D::Result::Ok);
            Recorder record;
            ShipBindingConfig config;
            config.bindings.push_back(ParseShipBinding("Primary", "Twin@1"));
            ShipControlCore core(config, record.Sink(), record.Log());
            core.Tick(store, { true, false, true }, 1000 * kMs);
            CHECK(record.Logged("ship_binding action=Primary status=device_ambiguous"));
        }
        {
            // bShipButtonsEnabled=false turns every binding off.
            DeviceStore store;
            store.Start();
            auto description = Description("Test Stick", kPhysical, 1);
            CHECK(store.ReplaceInventory(std::span(&description, 1)) == D::Result::Ok);
            Recorder record;
            ShipBindingConfig config;
            config.enabled = false;
            config.bindings.push_back(ParseShipBinding("Primary", "Test Stick@1"));
            ShipControlCore core(config, record.Sink(), record.Log());
            core.Tick(store, { true, false, true }, 1000 * kMs);
            CHECK(record.Logged("ship_bindings_resolved enabled=0 bound=0 problems=0 devices=1"));
        }
        {
            // A store that isn't running yet is an empty inventory, not a failure.
            DeviceStore store;
            Recorder record;
            ShipControlCore core(Bindings(), record.Sink(), record.Log());
            core.Tick(store, { true, false, true }, 1000 * kMs);
            CHECK(record.intents.empty());
            CHECK(record.Logged("ship_binding action=Primary status=device_missing"));
        }
        return true;
    }
}

int main()
{
    if (!TestIni() || !TestTokens() || !TestCatalogAndLoad() || !TestPilotSignal() ||
        !TestRouter() || !TestCore() || !TestCoreConfigProblems()) return 1;
    std::cout << "ship control tests passed\n";
    return 0;
}
