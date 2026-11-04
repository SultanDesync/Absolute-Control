#include "runtime/ShipControlLoop.h"

#include "EvidenceLog.h"
#include "runtime/BackendRuntime.h"
#include "runtime/CooperativeService.h"
#include "runtime/FlightSignals.h"
#include "runtime/ShipControlCore.h"
#include "runtime/WalkSpeedRuntime.h"

#include <fstream>
#include <iterator>
#include <memory>
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

        ShipBindingConfig LoadBindings(const std::vector<std::filesystem::path>& files)
        {
            IniValues ini;
            std::string sources;
            for (const auto& file : files) {
                std::ifstream stream{ file, std::ios::binary };
                if (!stream) continue;
                ini.Merge(std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() });
                if (!sources.empty()) sources += ',';
                sources += file.filename().string();
            }
            Log::Event("ship_bindings_loaded", std::format("files={}", sources));
            return LoadShipBindings(ini);
        }
    }

    void StartShipControlLoop(const std::vector<std::filesystem::path>& bindingFiles) noexcept
    {
        try {
            // Process lifetime, like NativeBackend: never join a worker under the loader lock.
            static auto* service = new CooperativeService;
            auto core = std::make_shared<ShipControlCore>(LoadBindings(bindingFiles),
                [](std::string_view actionId, bool held) {
                    Log::Event("ship_action_intent", std::format("action={} held={}", actionId, held ? 1 : 0));
                },
                [](std::string_view event, std::string_view detail) { Log::Event(event, detail); });
            const bool started = service->Start(
                [core](std::stop_token stop, const std::shared_ptr<CallbackGate>&) {
                    const auto& devices = NativeBackend().Devices();
                    std::string axisToken;
                    while (!stop.stop_requested()) {
                        if (auto token = WalkSpeedRuntime::AxisToken(); token != axisToken) {
                            axisToken = std::move(token);
                            core->SetAxisBinding(ParseAxisBinding("WalkSpeed", axisToken));
                        }
                        const auto now = BackendRuntime::Now();
                        const auto signals = ReadFlightSignals();
                        PilotObservation observation;
                        if (signals) {
                            observation.seated = signals->flag != 0;
                            observation.targeting = signals->tm != 0;
                            observation.inputActive = signals->active != 0;
                        }
                        core->Tick(devices, observation, now);
                        WalkSpeedRuntime::Tick(signals, core->AxisValue(), now);
                        if (!InterruptibleWait(stop, kTickInterval)) return;
                    }
                });
            Log::Event(started ? "control_loop_started" : "control_loop_failed", "output=log_only");
        } catch (...) {
            Log::Event("control_loop_failed", "exception during start");
        }
    }
}
