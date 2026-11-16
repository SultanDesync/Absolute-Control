#include "runtime/ShipControlLoop.h"

#include "EvidenceLog.h"
#include "ShipActionCatalog.h"
#include "runtime/BackendRuntime.h"
#include "runtime/ControlMapReader.h"
#include "runtime/ControlSettings.h"
#include "runtime/CooperativeService.h"
#include "runtime/FlightAxes.h"
#include "runtime/FlightOutput.h"
#include "runtime/FlightTelemetry.h"
#include "runtime/FlightSignals.h"
#include "runtime/ShipControlCore.h"
#include "runtime/ShipOutputs.h"
#include "runtime/WalkSpeedRuntime.h"

#include <ShlObj.h>

#include <array>
#include <map>
#include <cmath>
#include <charconv>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        namespace Log = AbsoluteControlPanelResearch::EvidenceLog;
        using AbsoluteControlPanelResearch::Runtime::CallbackGate;
        using AbsoluteControlPanelResearch::Runtime::CooperativeService;
        using AbsoluteControlPanelResearch::Runtime::InterruptibleWait;

        constexpr auto kTickInterval = std::chrono::milliseconds(8);

        std::filesystem::path RebindPath()
        {
            PWSTR documents{};
            std::filesystem::path path;
            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents)) && documents) {
                path = std::filesystem::path(documents) / L"My Games" / L"Starfield" / L"ControlMap_Custom.txt";
            }
            if (documents) ::CoTaskMemFree(documents);
            return path;
        }

        ControlMap::File LoadRebinds()
        {
            const auto path = RebindPath();
            return path.empty() ? ControlMap::File{} : ControlMap::ReadFile(path);
        }

        // Changes when the game saves a rebind, so a key changed mid-session is picked up.
        std::filesystem::file_time_type RebindStamp(const std::filesystem::path& path)
        {
            std::error_code error;
            const auto stamp = std::filesystem::last_write_time(path, error);
            return error ? std::filesystem::file_time_type{} : stamp;
        }

        std::vector<ResolvedShipOutput> LoadOutputs(const IniValues& ini)
        {
            const auto rebinds = LoadRebinds();
            auto outputs = ResolveShipOutputs(ini, rebinds.records);
            std::size_t rebound{}, overridden{}, none{};
            for (const auto& entry : outputs) {
                if (entry.source == KeyboardResolutionSource::ControlMapCustom) ++rebound;
                if (entry.source == KeyboardResolutionSource::LegacyManualOverride) ++overridden;
                if (entry.output.kind == ShipActionOutputKind::None) ++none;
            }
            Log::Event("ship_outputs_resolved", std::format("actions={} control_map={} rebound={} overridden={} none={}",
                outputs.size(), rebinds.valid ? 1 : 0, rebound, overridden, none));
            return outputs;
        }

        // How a target's system is picked in Targeting Mode. The game uses its own pair of
        // keys for it there (A and D unless rebound), which are flight keys everywhere else,
        // so none of these press anything outside Targeting Mode.
        enum class TargetCycle
        {
            PowerButtons, // Previous and Next System press both their own key and the target's.
            Roll,         // A push of the axis is one step.
            Yaw,
            StrafeSideways,
            OwnButtons    // Previous and Next Target System.
        };

        TargetCycle LoadTargetCycle(const IniValues& ini)
        {
            std::string text = ini.Get("ShipButtons", "CycleTargetSystemWith").value_or("");
            std::erase_if(text, [](char c) { return c == ' ' || c == '\t' || c == '\r'; });
            for (char& c : text) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            if (text == "roll") return TargetCycle::Roll;
            if (text == "yaw") return TargetCycle::Yaw;
            if (text == "strafesideways") return TargetCycle::StrafeSideways;
            if (text == "ownbuttons") return TargetCycle::OwnButtons;
            return TargetCycle::PowerButtons;
        }

        std::size_t ActionIndex(std::string_view actionId)
        {
            for (std::size_t index = 0; index < kShipActionCatalog.size(); ++index) {
                if (kShipActionCatalog[index].actionId == actionId) return index;
            }
            return kShipActionCatalog.size();
        }

        // Keys go to whichever window has focus, so nothing is pressed unless it's the game's.
        bool GameHasFocus()
        {
            DWORD process{};
            ::GetWindowThreadProcessId(::GetForegroundWindow(), &process);
            return process == ::GetCurrentProcessId();
        }

        void SendOutput(const ShipActionOutputSpec& output, bool down)
        {
            INPUT input{};
            if (output.kind == ShipActionOutputKind::Keyboard) {
                input.type = INPUT_KEYBOARD;
                input.ki.wScan = output.code;
                input.ki.dwFlags = KEYEVENTF_SCANCODE | (output.extended ? KEYEVENTF_EXTENDEDKEY : 0) |
                    (down ? 0 : KEYEVENTF_KEYUP);
            } else if (output.kind == ShipActionOutputKind::Mouse) {
                input.type = INPUT_MOUSE;
                switch (output.code) {
                case 1: input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
                case 2: input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
                case 3: input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
                case 4:
                    input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
                    input.mi.mouseData = XBUTTON1;
                    break;
                default: return;
                }
            } else {
                return;
            }
            const auto sent = ::SendInput(1, &input, sizeof(INPUT));
            Log::Event("ship_output", std::format("kind={} code=0x{:02X} down={} sent={}",
                output.kind == ShipActionOutputKind::Keyboard ? "key" : "mouse", output.code, down ? 1 : 0, sent));
        }
    }
    namespace
    {
        // Names whichever axis or button was last moved, in the form a binding is written
        // in, so a control can be bound by hand without knowing its number.
        class InputIdentifier
        {
        public:
            void Tick(const DeviceStore& devices, std::uint64_t now)
            {
                namespace D = DeviceDraft;
                if (now < next_) return;
                next_ = now + 100'000'000;
                const auto snapshot = devices.Snapshot();
                for (const auto& description : snapshot.devices) {
                    D::Frame frame;
                    std::uint32_t required{};
                    if (devices.CopyLatest(description.device, frame, buffer_.data(), static_cast<std::uint32_t>(buffer_.size()),
                        &required, now, ShipControlCore::kMaximumFrameAgeNanoseconds) != D::Result::Ok ||
                        frame.validity != D::Validity::Valid) continue;
                    auto& states = states_[description.device.physical];
                    states.resize(description.channels.size());
                    std::string_view name{ description.device.displayName };
                    while (!name.empty() && name.front() == ' ') name.remove_prefix(1);
                    while (!name.empty() && name.back() == ' ') name.remove_suffix(1);
                    const auto count = std::min<std::size_t>(required, description.channels.size());
                    for (std::size_t index = 0; index < count; ++index) {
                        const auto& sample = buffer_[index];
                        if (sample.validity != D::Validity::Valid) continue;
                        const std::string_view id{ description.channels[index].identity.channel };
                        auto& state = states[index];
                        if (sample.shape == D::Shape::Scalar && id.starts_with("axis/")) {
                            const double value = sample.value[0];
                            if (!state.seen) {
                                state.seen = true;
                                state.value = value;
                            } else if (std::abs(value - state.value) >= 0.5) {
                                // Half the travel since it was last named: a deliberate move.
                                state.value = value;
                                const auto number = Number(id.substr(5));
                                Log::Event("input_moved", std::format("token={}@0x{:02X} value={:.2f}", name, 0x30 + number, value));
                            }
                        } else if (id.starts_with("button/")) {
                            const bool down = sample.value[0] != 0;
                            if (down && !state.down && state.seen) {
                                Log::Event("input_pressed", std::format("token={}@{}", name, Number(id.substr(7)) + 1));
                            }
                            state.seen = true;
                            state.down = down;
                        }
                    }
                }
            }

        private:
            struct State
            {
                bool seen{};
                bool down{};
                double value{};
            };

            static unsigned Number(std::string_view text)
            {
                unsigned value{};
                std::from_chars(text.data(), text.data() + text.size(), value);
                return value;
            }

            std::map<std::string, std::vector<State>> states_;
            std::vector<DeviceDraft::Sample> buffer_ = std::vector<DeviceDraft::Sample>(DeviceStore::kChannelLimit);
            std::uint64_t next_{};
        };
    }
    void StartShipControlLoop() noexcept
    {
        try {
            // Process lifetime, like NativeBackend: never join a worker under the loader lock.
            static auto* service = new CooperativeService;
            auto& settings = ControlSettings::Instance();
            {
                const auto report = settings.Report();
                Log::Event("settings_loaded", std::format("origin={} imported_from={} not_carried={}",
                    settings.Origin(), report.from, report.dropped.size()));
                if (settings.Origin() == "imported") {
                    for (const auto& line : report.dropped) Log::Event("settings_not_carried", line);
                }
            }

            // Only the loop's thread touches these once it is running.
            auto held = std::make_shared<HeldOutputs>(&SendOutput);
            auto outputs = std::make_shared<std::vector<ResolvedShipOutput>>();
            auto boostButton = std::make_shared<bool>(false);
            auto targeting = std::make_shared<bool>(false);
            auto targetCycle = std::make_shared<TargetCycle>(TargetCycle::PowerButtons);
            // AbsoluteZero steers pitch and yaw through the same aim point.
            const bool mouseSteeringOwned = ::GetModuleHandleW(L"AbsoluteZero.dll") != nullptr;

            struct Loaded
            {
                std::shared_ptr<ShipControlCore> core;
                std::shared_ptr<const FlightConfig> flight;
                bool flightOutput{};
            };
            // Everything that comes from the settings and the game's key bindings. Run again
            // whenever either changes, so the menu and the game's own rebind screen both take
            // effect without a restart.
            const auto load = [held, outputs, boostButton, targeting, targetCycle, mouseSteeringOwned]() {
                const auto ini = ControlSettings::Instance().Merged();
                *outputs = LoadOutputs(ini);
                *targetCycle = LoadTargetCycle(ini);
                Log::Event("target_cycle", std::format("mode={}", static_cast<int>(*targetCycle)));
                Loaded loaded;
                loaded.core = std::make_shared<ShipControlCore>(LoadShipBindings(ini),
                    [held, outputs, boostButton, targeting, targetCycle](std::string_view actionId, bool down) {
                        Log::Event("ship_action_intent", std::format("action={} held={}", actionId, down ? 1 : 0));
                        const bool previousTarget = actionId == "PreviousTargetSystem";
                        // The target's own buttons: only when chosen, and only in Targeting Mode.
                        // A release always goes through.
                        if (down && (previousTarget || actionId == "NextTargetSystem") &&
                            (!*targeting || *targetCycle != TargetCycle::OwnButtons)) return;
                        // The power buttons also step the target's system in Targeting Mode.
                        if (*targetCycle == TargetCycle::PowerButtons && (actionId == "PreviousSystem" || actionId == "NextSystem")) {
                            const auto target = ActionIndex(actionId == "PreviousSystem" ? "PreviousTargetSystem" : "NextTargetSystem");
                            if (target < outputs->size() && (!down || *targeting)) {
                                held->Set((*outputs)[target].output, static_cast<std::uint32_t>(2000 + target), down);
                            }
                        }
                        if (actionId == "FireBoosters") *boostButton = down;
                        // Bindings, outputs and the catalog share one order.
                        for (std::size_t index = 0; index < outputs->size(); ++index) {
                            if (kShipActionCatalog[index].actionId != actionId) continue;
                            held->Set((*outputs)[index].output, static_cast<std::uint32_t>(index), down);
                            return;
                        }
                    },
                    [](std::string_view event, std::string_view detail) { Log::Event(event, detail); });

                // Flight axes take slots 1 to 9; slot 0 stays the walk speed axis.
                const auto flight = std::make_shared<const FlightConfig>(LoadFlightConfig(ini));
                const std::array<const ConfiguredBinding*, 9> axes{ &flight->throttle, &flight->pitch, &flight->yaw,
                    &flight->roll, &flight->strafeLateral, &flight->strafeVertical, &flight->reverse, &flight->aimYaw, &flight->aimPitch };
                bool anyAxis{};
                for (std::size_t index = 0; index < axes.size(); ++index) {
                    loaded.core->SetAxisBinding(index + 1, *axes[index]);
                    anyAxis = anyAxis || axes[index]->status != BindingStatus::Unbound;
                }
                for (const auto& name : flight->ignored) Log::Event("flight_setting_ignored", std::format("setting={}", name));
                // The game's code is only patched for someone who has a flight axis bound.
                loaded.flightOutput = flight->enabled && anyAxis;
                const auto installed = loaded.flightOutput ? FlightOutput::Install() : FlightOutput::InstallResult{};
                Log::Event("flight_config", std::format("enabled={} axes={} output={} gates={} shared={} store_hooks={} split={} aim={} zero={} ignored={}",
                    flight->enabled ? 1 : 0, anyAxis ? 1 : 0, loaded.flightOutput ? 1 : 0, installed.steeringGates ? 1 : 0,
                    installed.sharedGate ? 1 : 0, installed.storeHooks, installed.splitHook ? 1 : 0,
                    ReticleModeName(flight->reticle), mouseSteeringOwned ? 1 : 0, flight->ignored.size()));
                loaded.flight = flight;
                return loaded;
            };

            const auto rebindPath = RebindPath();
            const auto first = std::make_shared<const Loaded>(load());
            const auto firstStamp = RebindStamp(rebindPath);
            const auto firstGeneration = settings.Generation();

            const bool started = service->Start(
                [load, first, firstStamp, firstGeneration, rebindPath, held, outputs, boostButton, targeting, targetCycle, mouseSteeringOwned](
                    std::stop_token stop, const std::shared_ptr<CallbackGate>&) {
                    constexpr std::uint32_t kStrafeOwner = 1000;
                    constexpr std::uint32_t kBoostZoneOwner = 1001;
                    constexpr std::uint32_t kTargetAxisOwner = 1002;
                    // An axis used to step the target's system: one short key press per push.
                    bool targetAxisArmed = true;
                    std::optional<ShipActionOutputSpec> targetPulse;
                    std::uint64_t targetPulseEnd{};
                    std::uint64_t nextThrottleLog{};
                    const auto outputFor = [&](std::string_view actionId) {
                        for (std::size_t index = 0; index < outputs->size(); ++index) {
                            if (kShipActionCatalog[index].actionId == actionId) return (*outputs)[index].output;
                        }
                        return ShipActionOutputSpec{};
                    };
                    auto current = *first;
                    auto strafeKey = outputFor("SwitchFlightModes");
                    auto boostKey = outputFor("FireBoosters");
                    auto stamp = firstStamp;
                    auto generation = firstGeneration;
                    std::uint64_t nextRebindCheck{};

                    const auto& devices = NativeBackend().Devices();
                    auto& telemetry = FlightTelemetry::Shared();
                    std::optional<std::string> axisToken;
                    bool flightLive{};
                    InputIdentifier identifier;
                    while (!stop.stop_requested()) {
                        const auto now = BackendRuntime::Now();
                        // The game saves a rebind to its file; the menu bumps the generation.
                        bool changed = ControlSettings::Instance().Generation() != generation;
                        const char* reason = "settings_applied";
                        if (!changed && now >= nextRebindCheck) {
                            nextRebindCheck = now + 1'000'000'000;
                            changed = RebindStamp(rebindPath) != stamp;
                            reason = "control_map_changed";
                        }
                        if (changed) {
                            generation = ControlSettings::Instance().Generation();
                            stamp = RebindStamp(rebindPath);
                            // Let go of everything first: a held key or a bound axis may be about to change.
                            held->ReleaseAll();
                            targetPulse.reset();
                            FlightOutput::Suspend();
                            *boostButton = false;
                            flightLive = false;
                            current = load();
                            strafeKey = outputFor("SwitchFlightModes");
                            boostKey = outputFor("FireBoosters");
                            axisToken.reset();
                            Log::Event("ship_outputs_reloaded", std::format("reason={}", reason));
                        }
                        const auto& core = current.core;
                        const auto& flight = current.flight;

                        if (auto token = WalkSpeedRuntime::AxisToken(); !axisToken || token != *axisToken) {
                            axisToken = std::move(token);
                            core->SetAxisBinding(ParseAxisBinding("WalkSpeed", *axisToken));
                        }
                        const auto signals = ReadFlightSignals();
                        PilotObservation observation;
                        if (signals) {
                            observation.seated = signals->flag != 0;
                            observation.targeting = signals->tm != 0;
                            observation.inputActive = signals->active != 0 && GameHasFocus();
                        }
                        *targeting = observation.targeting;
                        core->Tick(devices, observation, now);
                        identifier.Tick(devices, now);
                        WalkSpeedRuntime::Tick(signals, core->AxisValue(), now);

                        // Worked out every tick, piloting or not, so the menu can show an axis
                        // responding while it is being set up.
                        const auto axis = [&](std::size_t slot) -> std::optional<float> {
                            const auto value = core->AxisValue(slot);
                            return value ? std::optional<float>(static_cast<float>(*value)) : std::nullopt;
                        };
                        const FlightInputs inputs{ axis(1), axis(2), axis(3), axis(4), axis(5), axis(6), axis(7), axis(8), axis(9) };
                        const auto frame = ComputeFlightFrame(*flight, inputs);
                        const std::array<std::optional<float>, 9> positions{ inputs.throttle, inputs.pitch, inputs.yaw,
                            inputs.roll, inputs.strafeLateral, inputs.strafeVertical, inputs.reverse, inputs.aimYaw, inputs.aimPitch };
                        const std::array<float, 9> shaped{ frame.throttle, frame.pitch, frame.yaw, frame.splitRoll,
                            frame.splitStrafe, frame.vertical, frame.reverse ? 1.0F : 0.0F, frame.aimYaw, frame.aimPitch };
                        for (std::size_t index = 0; index < positions.size(); ++index) {
                            FlightTelemetry::SetPosition(index, positions[index]);
                            telemetry.output[index].store(shaped[index], std::memory_order_relaxed);
                        }
                        telemetry.reverse.store(frame.reverse, std::memory_order_relaxed);
                        telemetry.boost.store(frame.boost, std::memory_order_relaxed);

                        {
                            float push{};
                            switch (*targetCycle) {
                            case TargetCycle::Roll: push = frame.splitRoll; break;
                            case TargetCycle::Yaw: push = frame.yaw; break;
                            case TargetCycle::StrafeSideways: push = frame.splitStrafe; break;
                            default: break;
                            }
                            if (targetPulse && now >= targetPulseEnd) {
                                held->Set(*targetPulse, kTargetAxisOwner, false);
                                targetPulse.reset();
                            }
                            // It has to come back towards the centre before it can step again.
                            if (std::abs(push) < 0.25F) {
                                targetAxisArmed = true;
                            } else if (targetAxisArmed && !targetPulse && std::abs(push) >= 0.5F &&
                                observation.targeting && core->GateOpen()) {
                                targetAxisArmed = false;
                                targetPulse = outputFor(push < 0 ? "PreviousTargetSystem" : "NextTargetSystem");
                                targetPulseEnd = now + 80'000'000;
                                held->Set(*targetPulse, kTargetAxisOwner, true);
                                Log::Event("target_axis_step", std::format("push={:.2f}", push));
                            }
                        }

                        // Camera state 0xA is the take-off, docking and undocking cutscene.
                        const bool allowed = current.flightOutput && core->GateOpen() && signals &&
                            signals->clusterAddress && signals->cam != 0xA;
                        telemetry.piloting.store(allowed, std::memory_order_relaxed);
                        if (current.flightOutput && allowed != flightLive) {
                            flightLive = allowed;
                            Log::Event("flight_gate", std::format("open={}", allowed ? 1 : 0));
                        }
                        // Once a second while piloting with a lever bound: where the lever is,
                        // what was asked for, and what the game holds, read before this tick's
                        // write. It answers throttle questions from the log instead of from feel.
                        if (allowed && frame.throttleBound && now >= nextThrottleLog) {
                            nextThrottleLog = now + 1'000'000'000;
                            Log::Event("flight_throttle", std::format("lever={:.2f} asked={:.2f} game_target={:.2f} game_effective={:.2f} speed={:.1f} reverse={} boost={}",
                                inputs.throttle.value_or(0.0F), frame.throttle, signals->manual, signals->effective, signals->speed,
                                frame.reverse ? 1 : 0, frame.boost ? 1 : 0));
                        }
                        if (allowed) {
                            FlightOutput::Apply(*flight, frame, { signals->managerAddress, signals->clusterAddress },
                                frame.boost || *boostButton, mouseSteeringOwned, now);
                            // Strafing needs the game's flight mode key held, and the lever's boost
                            // zone presses the booster key.
                            held->Set(strafeKey, kStrafeOwner, frame.strafeModifier);
                            held->Set(boostKey, kBoostZoneOwner, frame.boost);
                        } else if (current.flightOutput) {
                            FlightOutput::Suspend();
                            held->Set(strafeKey, kStrafeOwner, false);
                            held->Set(boostKey, kBoostZoneOwner, false);
                        }
                        if (!InterruptibleWait(stop, kTickInterval)) break;
                    }
                    FlightOutput::Suspend();
                    held->ReleaseAll();
                });
            Log::Event(started ? "control_loop_started" : "control_loop_failed", "output=sendinput");
        } catch (...) {
            Log::Event("control_loop_failed", "exception during start");
        }
    }
}