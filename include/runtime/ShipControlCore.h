#pragma once

#include "runtime/ButtonRouter.h"
#include "runtime/DeviceStore.h"
#include "runtime/PilotSignal.h"
#include "runtime/ShipBindings.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // The one interface ship actions leave through: this action is now held or released.
    using ActionSink = std::function<void(std::string_view actionId, bool held)>;
    using ControlLog = std::function<void(std::string_view event, std::string_view detail)>;

    // One control-loop step: read bound buttons off the bus, gate them on the pilot signal,
    // and report intents. It has no game or hardware dependency, so tests can drive it.
    class ShipControlCore
    {
    public:
        static constexpr std::uint64_t kMaximumFrameAgeNanoseconds = 250'000'000;

        ShipControlCore(ShipBindingConfig config, ActionSink sink, ControlLog log);

        void Tick(const DeviceStore& devices, const PilotObservation& observation, std::uint64_t now);
        [[nodiscard]] bool GateOpen() const noexcept { return gateOpen_; }

        // One extra axis read off the bus each tick. Nothing is read when it's unbound.
        void SetAxisBinding(ConfiguredBinding binding);
        // -1 to 1, or nothing if the axis is unbound, missing, stale or has no known range.
        [[nodiscard]] std::optional<double> AxisValue() const noexcept { return axisValue_; }

    private:
        struct Resolved
        {
            BindingStatus status{ BindingStatus::Unbound };
            std::size_t deviceSlot{};
            std::size_t channelIndex{};
        };
        [[nodiscard]] Resolved Resolve(const ConfiguredBinding& binding, const DeviceSnapshot& snapshot);
        void Reconcile(const DeviceStore& devices);
        void SampleButtons(const DeviceStore& devices, std::uint64_t now);

        ConfiguredBinding axisBinding_;
        Resolved axisResolved_;
        std::optional<double> axisValue_;
        ShipBindingConfig config_;
        ActionSink sink_;
        ControlLog log_;
        ButtonRouter router_;
        PilotSignal pilot_;
        std::vector<Resolved> resolved_;
        std::vector<DeviceDraft::Device> slots_;
        std::vector<ButtonSample> samples_;
        std::vector<DeviceDraft::Sample> buffer_;
        std::uint64_t cursor_{};
        bool reconciled_{};
        bool gateOpen_{};
        bool gateLogged_{};
    };
}
