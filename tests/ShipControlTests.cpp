#include "ShipActionCatalog.h"
#include "runtime/ButtonCapture.h"
#include "runtime/ButtonRouter.h"
#include "runtime/ControlSettings.h"
#include "runtime/FlightAxes.h"
#include "runtime/SettingsImport.h"
#include "runtime/PilotSignal.h"
#include "runtime/ShipBindings.h"
#include "runtime/ShipControlCore.h"
#include "runtime/ShipOutputs.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <iterator>
#include <fstream>
#include <filesystem>
#include <initializer_list>
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
        const auto loaded = LoadHotasShipBindings(ini);
        CHECK(loaded.enabled && loaded.bindings.size() == kShipActionCatalog.size());
        std::size_t bound{};
        for (const auto& binding : loaded.bindings) {
            if (binding.status != BindingStatus::Bound) continue;
            ++bound;
            CHECK(binding.actionId == "ShipAction1" && binding.channel == "button/17");
        }
        CHECK(bound == 1);
        ini.Merge("[ShipButtons]\nbShipButtonsEnabled = FALSE\n");
        CHECK(!LoadHotasShipBindings(ini).enabled);
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

    // One section of ControlMap_Custom.txt: 0x03, a big-endian length, then records.
    std::vector<std::uint8_t> Section(std::initializer_list<ControlMap::Record> records)
    {
        std::vector<std::uint8_t> body;
        for (const auto& record : records) {
            body.insert(body.end(), record.context.begin(), record.context.end());
            body.push_back(0);
            body.insert(body.end(), record.action.begin(), record.action.end());
            body.push_back(0);
            for (int shift = 0; shift < 32; shift += 8) body.push_back(static_cast<std::uint8_t>(record.device >> shift));
            body.push_back(static_cast<std::uint8_t>(record.token));
            body.push_back(static_cast<std::uint8_t>(record.token >> 8));
            body.push_back(record.flag);
            body.push_back(0);
        }
        const auto length = body.size() + 3;
        std::vector<std::uint8_t> section{ 0x03, static_cast<std::uint8_t>(length >> 8), static_cast<std::uint8_t>(length) };
        section.insert(section.end(), body.begin(), body.end());
        return section;
    }

    std::size_t ActionIndex(std::string_view actionId)
    {
        return static_cast<std::size_t>(FindShipAction(actionId) - kShipActionCatalog.data());
    }

    bool TestControlMap()
    {
        CHECK(ControlMap::Parse({}).valid && ControlMap::Parse({}).records.empty());
        CHECK(!ControlMap::ReadFile("no-such-control-map.txt").valid);

        // Keyboard and mouse sit in separate sections; a gamepad record (device 2) is dropped.
        auto bytes = Section({ { "ShipHUD", "Cruise", 1, 0x004B, 0 }, { "ShipHUD", "Cruise", 2, 0x0001, 0 },
            { "ShipHUD", "RepairShip", 1, ControlMap::kUnboundToken, 0 } });
        const auto mouse = Section({ { "ShipHUD", "RepairShip", 1, 0x0002, 1 },
            { "ShipHUD", "Boosters", 1, ControlMap::kUnboundToken, 0 } });
        bytes.insert(bytes.end(), mouse.begin(), mouse.end());
        const auto file = ControlMap::Parse(bytes);
        CHECK(file.valid && file.records.size() == 4);

        const ControlMap::Output vanilla{ ShipActionOutputKind::Keyboard, 0x14, false };
        // Rebound to K: the scan code comes from this machine's keyboard layout.
        const auto cruise = ControlMap::ResolveBinding(file.records, "ShipHUD", "Cruise", vanilla);
        CHECK(cruise.kind == ShipActionOutputKind::Keyboard && cruise.code != 0x14 && !cruise.extended);
        CHECK(cruise.code == ControlMap::TokenToOutput(0x004B).code);
        // Primary unbound, alternate on the middle mouse button.
        const auto repair = ControlMap::ResolveBinding(file.records, "ShipHUD", "RepairShip", vanilla);
        CHECK(repair.kind == ShipActionOutputKind::Mouse && repair.code == 3);
        // Unbound with no alternate, and never rebound.
        CHECK(ControlMap::ResolveBinding(file.records, "ShipHUD", "Boosters", vanilla).kind == ShipActionOutputKind::None);
        CHECK(ControlMap::ResolveBinding(file.records, "ShipHUD", "TogglePOV", vanilla).code == 0x14);
        CHECK(ControlMap::HasPrimary(file.records, "ShipHUD", "Cruise"));
        CHECK(!ControlMap::HasPrimary(file.records, "ShipHUD", "TogglePOV"));

        const auto left = ControlMap::TokenToOutput(0x0025);
        CHECK(left.kind == ShipActionOutputKind::Keyboard && left.code == 0x4B && left.extended);
        CHECK(ControlMap::TokenToOutput(0x0000).kind == ShipActionOutputKind::Mouse);
        CHECK(ControlMap::TokenToOutput(ControlMap::kUnboundToken).kind == ShipActionOutputKind::None);

        // A file the game wrote on 1.16.244 after weapon 1 was moved from the mouse to "=":
        // the key sits in an alternate slot and the mouse primary is marked unbound, with an
        // empty third section.
        const std::vector<std::uint8_t> real{
            0x03, 0x00, 0x3D, 'M', 'a', 'i', 'n', 'G', 'a', 'm', 'e', 'p', 'l', 'a', 'y', 0, 'F', 'o', 'r', 'w', 'a', 'r', 'd', 0,
            0x01, 0, 0, 0, 0x57, 0, 0, 0, 'S', 'h', 'i', 'p', 'H', 'U', 'D', 0, 'W', 'e', 'a', 'p', 'o', 'n', 'G', 'r', 'o', 'u', 'p', '1', 0,
            0x01, 0, 0, 0, 0xBB, 0, 0x02, 0,
            0x03, 0x00, 0x3D, 'S', 'h', 'i', 'p', 'H', 'U', 'D', 0, 'W', 'e', 'a', 'p', 'o', 'n', 'G', 'r', 'o', 'u', 'p', '1', 0,
            0x01, 0, 0, 0, 0xFF, 0x02, 0, 0, 'S', 'h', 'i', 'p', 'H', 'U', 'D', 0, 'W', 'e', 'a', 'p', 'o', 'n', 'G', 'r', 'o', 'u', 'p', '1', 0,
            0x01, 0, 0, 0, 0xFF, 0x02, 0x02, 0,
            0x03, 0x00, 0x03 };
        const auto realFile = ControlMap::Parse(real);
        CHECK(realFile.valid && realFile.records.size() == 4);
        const auto weapon = ControlMap::ResolveBinding(realFile.records, "ShipHUD", "WeaponGroup1",
            { ShipActionOutputKind::Mouse, 1, false });
        CHECK(weapon.kind == ShipActionOutputKind::Keyboard && weapon.code == ControlMap::TokenToOutput(0x00BB).code && weapon.code != 0);
        CHECK(ResolveShipOutputs({}, realFile.records)[ActionIndex("FireWeapon0")].output.kind == ShipActionOutputKind::Keyboard);

        // Truncated or mislabelled bytes are invalid, keeping what parsed before them.
        auto truncated = bytes;
        truncated.resize(truncated.size() - 4);
        CHECK(!ControlMap::Parse(truncated).valid);
        auto mislabelled = bytes;
        mislabelled[0] = 0x04;
        CHECK(!ControlMap::Parse(mislabelled).valid);
        return true;
    }

    bool TestOutputs()
    {
        CHECK(ParseShipOutput(" KEY:0x13 ")->code == 0x13 && !ParseShipOutput("key:0x13")->extended);
        CHECK(ParseShipOutput("key:72")->extended);
        CHECK(ParseShipOutput("mouse:4")->kind == ShipActionOutputKind::Mouse);
        CHECK(ParseShipOutput("none")->kind == ShipActionOutputKind::None);
        CHECK(!ParseShipOutput("") && !ParseShipOutput("key:0") && !ParseShipOutput("mouse:5") &&
            !ParseShipOutput("key:0x1FFFF") && !ParseShipOutput("R"));

        const auto file = ControlMap::Parse(Section({ { "ShipHUD", "Cruise", 1, 0x004B, 0 },
            { "ShipHUD", "Up", 1, 0x004B, 0 }, { "ShipHUD", "RepairShip", 1, 0x004B, 0 } }));
        IniValues ini;
        ini.Merge("[ShipButtonKeys]\nRepair = key:0x21\nTogglePov = nonsense\n");
        auto outputs = ResolveShipOutputs(ini, file.records);
        CHECK(outputs.size() == kShipActionCatalog.size());
        // Untouched actions press the game's default.
        const auto& action1 = outputs[ActionIndex("ShipAction1")];
        CHECK(action1.output.code == 0x13 && action1.source == KeyboardResolutionSource::VanillaFallback);
        const auto& fire = outputs[ActionIndex("FireWeapon0")];
        CHECK(fire.output.kind == ShipActionOutputKind::Mouse && fire.output.code == 1);
        // The player's rebind wins over the default, and an explicit output over both.
        const auto& cruise = outputs[ActionIndex("Cruise")];
        CHECK(cruise.output.code == ControlMap::TokenToOutput(0x004B).code &&
            cruise.source == KeyboardResolutionSource::ControlMapCustom);
        const auto& repair = outputs[ActionIndex("Repair")];
        CHECK(repair.output.code == 0x21 && repair.source == KeyboardResolutionSource::LegacyManualOverride);
        // An output that doesn't parse changes nothing.
        CHECK(outputs[ActionIndex("TogglePov")].output.code == 0x10);
        // Context actions keep the game's fixed key whatever the rebinds say.
        const auto& power = outputs[ActionIndex("IncreaseSystemPower")];
        CHECK(power.output.code == 0x48 && power.output.extended && power.source == KeyboardResolutionSource::FixedContext);

        ini.Merge("[ShipButtons]\nFollowGameRebinds = false\n");
        outputs = ResolveShipOutputs(ini, file.records);
        CHECK(outputs[ActionIndex("Cruise")].output.code == 0x14);
        return true;
    }

    bool TestHeldOutputs()
    {
        std::vector<std::string> sent;
        HeldOutputs held([&](const ShipActionOutputSpec& output, bool down) {
            sent.push_back(std::format("{}:{:02X}:{}", output.kind == ShipActionOutputKind::Mouse ? "mouse" : "key",
                output.code, down ? 1 : 0));
        });
        const ShipActionOutputSpec space{ ShipActionOutputKind::Keyboard, 0x39, false };
        const ShipActionOutputSpec fire{ ShipActionOutputKind::Mouse, 1, false };

        // Two actions share Space: it goes down once and up when the last lets go.
        held.Set(space, 1, true);
        held.Set(space, 2, true);
        held.Set(space, 2, true);
        held.Set(space, 1, false);
        CHECK(sent == std::vector<std::string>{ "key:39:1" });
        held.Set(space, 2, false);
        CHECK(sent.back() == "key:39:0" && sent.size() == 2 && !held.Any());

        // Releasing what isn't held, or an action with no output, sends nothing.
        held.Set(space, 1, false);
        held.Set({}, 1, true);
        CHECK(sent.size() == 2);

        // The numpad and arrow keys share scan codes and differ only by the extended flag.
        held.Set({ ShipActionOutputKind::Keyboard, 0x48, true }, 1, true);
        held.Set({ ShipActionOutputKind::Keyboard, 0x48, false }, 2, true);
        CHECK(sent.size() == 4);

        held.Set(fire, 3, true);
        held.ReleaseAll();
        CHECK(sent.size() == 8 && !held.Any());
        CHECK(std::ranges::count(sent, std::string("mouse:01:0")) == 1);
        held.ReleaseAll();
        CHECK(sent.size() == 8);
        return true;
    }
    bool TestBindingNumbers()
    {
        CHECK(BindingNumberForChannel("button/0") == 1 && BindingNumberForChannel("button/127") == 128);
        CHECK(BindingNumberForChannel("button/128") == 0 && BindingNumberForChannel("button/") == 0);
        CHECK(BindingNumberForChannel("pov/0/direction/up") == 129 && BindingNumberForChannel("pov/0/direction/left") == 132);
        CHECK(BindingNumberForChannel("pov/3/direction/left") == 144 && BindingNumberForChannel("pov/4/direction/up") == 0);
        CHECK(BindingNumberForChannel("pov/0") == 0 && BindingNumberForChannel("axis/1") == 0);
        // What a recording saves reads back as the same channel.
        CHECK(ParseShipBinding("A", "Stick@132").channel == "pov/0/direction/left");
        return true;
    }

    bool TestImport()
    {
        IniValues hotas;
        hotas.Merge("[General]\nbSyncShipOutputsFromControlMap = false\n"
            "[Hardware]\niThrottleAxis = Lever@0x32\niPitchAxis = #0@0x31\n"
            "[ShipButtons]\niShipAction1Button = Stick@14\niCruiseButton = Lever@26\niRepairButton = 42\niGetUpButton = -1\n"
            "iFireWeapon0Button = Stick@1\niFireWeapon1Button = Stick@2\n"
            "[ShipButtonOutputs]\nsRepairOutput = key:0x21\nsCruiseOutput = nonsense\n"
            "[DualStick]\nbAccumulatorThrottle = true\n[MenuControls]\niMenuUpButton = Stick@5\n");
        const auto imported = ImportHotasSettings(hotas);
        IniValues own;
        for (const auto& entry : imported.entries) own.Set(entry.section, entry.key, entry.value);

        // Names say what they are, and "Ship Action 1" is saved as what it does.
        CHECK(own.Get("Throttle", "Axis") == "Lever@0x32" && own.Get("Pitch", "Axis") == "");
        CHECK(own.Get("ShipButtons", "TargetingDockTravel") == "Stick@14" && own.Get("ShipButtons", "Cruise") == "Lever@26");
        CHECK(own.Get("ShipButtons", "Repair") == "" && own.Get("ShipButtons", "GetUp") == "" && own.Get("ShipButtons", "Enabled") == "true");
        // Weapons are numbered as the game shows them, and each key still reaches its own action.
        CHECK(own.Get("ShipButtons", "WeaponGroup1") == "Stick@1" && own.Get("ShipButtons", "WeaponGroup2") == "Stick@2");
        CHECK(!own.Get("ShipButtons", "FireWeapon0") && LoadShipBindings(own).bindings[ActionIndex("FireWeapon1")].channel == "button/1");
        CHECK(own.Get("ShipButtons", "FollowGameRebinds") == "false");
        CHECK(own.Get("ShipButtonKeys", "Repair") == "key:0x21" && !own.Get("ShipButtonKeys", "Cruise"));
        // Every action has a line in the file, bound or not.
        for (std::size_t index = 0; index < kShipActionCatalog.size(); ++index) CHECK(own.Get("ShipButtons", ShipSettingKey(index)));

        // The new readers see what the HOTAS files meant.
        const auto buttons = LoadShipBindings(own);
        CHECK(buttons.enabled && buttons.bindings[ActionIndex("ShipAction1")].channel == "button/13");
        CHECK(buttons.bindings[ActionIndex("Cruise")].channel == "button/25" && buttons.bindings[ActionIndex("Repair")].status == BindingStatus::Unbound);
        CHECK(ResolveShipOutputs(own, {})[ActionIndex("Repair")].output.code == 0x21);

        // What couldn't come across is said in words: two cut features and two bindings in a
        // form that depends on the order devices happen to be listed in.
        const auto dropped = [&](std::string_view text) {
            return std::ranges::any_of(imported.dropped, [&](const std::string& line) { return line.find(text) != std::string::npos; });
        };
        CHECK(imported.dropped.size() == 4);
        CHECK(dropped("[DualStick] bAccumulatorThrottle") && dropped("[MenuControls] iMenuUpButton"));
        CHECK(dropped("Pitch axis \"#0@0x31\"") && dropped("Repair Ship button \"42\""));
        // An unbound button is not a loss.
        CHECK(!dropped("Get Up"));
        return true;
    }

    bool TestControlSettings()
    {
        namespace fs = std::filesystem;
        const auto folder = fs::temp_directory_path() / "absolute-control-settings-test";
        fs::remove_all(folder);
        fs::create_directories(folder);
        const auto base = folder / "AbsoluteHOTAS.ini", custom = folder / "AbsoluteHOTAS_Custom.ini", own = folder / "AbsoluteControl.ini";
        const std::string baseText = "[Hardware]\niPitchAxis = Stick@0x31\nfPitchDeadzone = 0.10\n[ShipButtons]\niCruiseButton = Stick@26\n"
            "[DualStick]\nbAccumulatorThrottle = true\n";
        const auto text = [](const fs::path& file) {
            std::ifstream stream{ file, std::ios::binary };
            return std::string(std::istreambuf_iterator<char>(stream), {});
        };
        auto& settings = ControlSettings::Instance();

        // A new install: nothing to import, defaults, and no file written until something changes.
        settings.Load(own, { base, custom });
        CHECK(settings.Origin() == "defaults" && !settings.Report().imported && !fs::exists(own));
        CHECK(LoadFlightConfig(settings.Merged()).pitch.status == BindingStatus::Unbound);

        // HOTAS files and no file of our own: imported once, the later file winning.
        std::ofstream(base, std::ios::binary) << baseText;
        std::ofstream(custom, std::ios::binary) << "[Hardware]\nfPitchDeadzone = 0.20\n";
        settings.Load(own, { base, custom, folder / "missing.ini" });
        CHECK(settings.Origin() == "imported" && fs::exists(own));
        CHECK(settings.Get("Pitch", "Axis") == "Stick@0x31" && settings.Get("pitch", "deadzone") == "20");
        CHECK(settings.Get("ShipButtons", "Cruise") == "Stick@26");
        auto report = settings.Report();
        CHECK(report.imported && report.from == "AbsoluteHOTAS.ini, AbsoluteHOTAS_Custom.ini" && report.date.size() == 10);
        CHECK(report.dropped.size() == 1 && report.dropped[0].find("bAccumulatorThrottle") != std::string::npos);
        // The file says what it is and how to get out of trouble, and keeps its sections together.
        const auto written = text(own);
        CHECK(written.find("only settings file") != std::string::npos && written.find("delete this file") != std::string::npos);
        CHECK(written.find("[Throttle]") < written.find("[Pitch]") && written.find("[Pitch]") < written.find("[ShipButtons]"));
        CHECK(written.find("[Import]") != std::string::npos && written.find("[Pitch]") == written.rfind("[Pitch]"));
        // The HOTAS files are never written.
        CHECK(text(base) == baseText);

        // The menu's changes go to our file only. An empty value unbinds.
        const auto before = settings.Generation();
        settings.Set("Pitch", "Deadzone", "5");
        settings.Set("ShipButtons", "Cruise", "");
        settings.Set("Roll", "Axis", "Pedals@0x33");
        CHECK(LoadShipBindings(settings.Merged()).bindings[ActionIndex("Cruise")].status == BindingStatus::Unbound);
        CHECK(settings.Save());
        settings.Publish();
        CHECK(settings.Generation() == before + 1 && text(base) == baseText);

        // From now on only our file is read: changing the HOTAS files does nothing.
        std::ofstream(custom, std::ios::binary) << "[Hardware]\nfPitchDeadzone = 0.40\niYawAxis = Stick@0x30\n";
        settings.Load(own, { base, custom });
        CHECK(settings.Origin() == "file");
        CHECK(settings.Get("Pitch", "Deadzone") == "5" && settings.Get("Roll", "Axis") == "Pedals@0x33");
        CHECK(settings.Get("ShipButtons", "Cruise") == "" && settings.Get("Yaw", "Axis") == "");
        // The import's record survives, so the menu can show it on any later launch.
        report = settings.Report();
        CHECK(report.imported && report.dropped.size() == 1);
        CHECK(text(own).find("[Pitch]") == text(own).rfind("[Pitch]"));

        // Starting fresh leaves an empty file behind, so nothing is imported a second time.
        CHECK(settings.Reset() && fs::exists(own));
        CHECK(!settings.Get("Pitch", "Axis") && !settings.Report().imported);
        settings.Load(own, { base, custom });
        CHECK(settings.Origin() == "file" && !settings.Get("Pitch", "Axis") && !settings.Report().imported);

        // Walk speed used to keep its own file. It is copied in once, and a reset leaves it alone.
        const auto walk = folder / "AbsoluteControlWalkSpeed.ini";
        std::ofstream(walk, std::ios::binary) << "; old file\n[WalkSpeed]\nEnabled=true\nSpeed=0.750\n[Other]\nKey=1\n";
        settings.Load(own, { base }, walk);
        CHECK(settings.Get("WalkSpeed", "Speed") == "0.750" && !settings.Get("Other", "Key"));
        settings.Set("WalkSpeed", "Speed", "0.500");
        settings.Set("Pitch", "Axis", "Stick@0x31");
        CHECK(settings.Save());
        std::ofstream(walk, std::ios::binary) << "[WalkSpeed]\nSpeed=0.100\n";
        settings.Load(own, { base }, walk);
        CHECK(settings.Get("WalkSpeed", "Speed") == "0.500");
        CHECK(settings.Reset() && settings.Get("WalkSpeed", "Speed") == "0.500" && !settings.Get("Pitch", "Axis"));

        // A test run imports every time, whatever was saved before.
        settings.Set("Pitch", "Axis", "Other@0x30");
        CHECK(settings.Save());
        settings.Load(own, { base }, {}, true);
        CHECK(settings.Origin() == "imported" && settings.Get("Pitch", "Axis") == "Stick@0x31");
        fs::remove_all(folder);
        return true;
    }
}
int main()
{
    if (!TestIni() || !TestTokens() || !TestCatalogAndLoad() || !TestPilotSignal() ||
        !TestRouter() || !TestCore() || !TestCoreConfigProblems() || !TestControlMap() ||
        !TestOutputs() || !TestHeldOutputs() || !TestBindingNumbers() || !TestImport() ||
        !TestControlSettings()) return 1;
    std::cout << "ship control tests passed\n";
    return 0;
}
