#include "runtime/ShipControlCore.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        namespace D = DeviceDraft;

        std::string_view TrimSpaces(std::string_view text)
        {
            const auto first = text.find_first_not_of(' ');
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(' ') - first + 1);
        }

        // Controllers can report names padded with spaces, which saved tokens don't keep.
        bool SameName(std::string_view left, std::string_view right)
        {
            return std::ranges::equal(TrimSpaces(left), TrimSpaces(right), [](char a, char b) {
                const auto lower = [](char value) {
                    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
                };
                return lower(a) == lower(b);
            });
        }
    }

    ShipControlCore::ShipControlCore(ShipBindingConfig config, ActionSink sink, ControlLog log) :
        config_(std::move(config)), sink_(std::move(sink)), log_(std::move(log)),
        router_(config_.bindings.size()), resolved_(config_.bindings.size()),
        samples_(config_.bindings.size()), buffer_(DeviceStore::kChannelLimit)
    {}

    void ShipControlCore::SetAxisBinding(ConfiguredBinding binding)
    {
        axisBinding_ = std::move(binding);
        axisResolved_ = {};
        axisValue_.reset();
        reconciled_ = false;
    }

    ShipControlCore::Resolved ShipControlCore::Resolve(const ConfiguredBinding& binding, const DeviceSnapshot& snapshot)
    {
        Resolved next{ binding.status };
        if (next.status != BindingStatus::Bound) return next;
        const DeviceDescription* match{};
        std::size_t matches{};
        for (const auto& description : snapshot.devices) {
            const bool same = binding.devicePhysicalId ?
                binding.device == description.device.physical :
                SameName(binding.device, description.device.displayName);
            if (same) {
                match = &description;
                ++matches;
            }
        }
        if (matches == 0) next.status = BindingStatus::DeviceMissing;
        else if (matches > 1) next.status = BindingStatus::DeviceAmbiguous;
        else {
            const auto channel = std::ranges::find_if(match->channels, [&](const D::Channel& candidate) {
                return binding.channel == candidate.identity.channel;
            });
            if (channel == match->channels.end()) next.status = BindingStatus::ChannelMissing;
            else {
                next.channelIndex = static_cast<std::size_t>(channel - match->channels.begin());
                const auto slot = std::ranges::find_if(slots_, [&](const D::Device& device) {
                    return !std::strcmp(device.physical, match->device.physical);
                });
                next.deviceSlot = static_cast<std::size_t>(slot - slots_.begin());
                if (slot == slots_.end()) slots_.push_back(match->device);
            }
        }
        return next;
    }

    void ShipControlCore::Reconcile(const DeviceStore& devices)
    {
        const auto snapshot = devices.Snapshot();
        cursor_ = snapshot.cursor;
        reconciled_ = true;
        slots_.clear();
        for (const auto& description : snapshot.devices) {
            log_("input_device", std::format("name={} id={} channels={} valid={}",
                description.device.displayName, description.device.physical,
                description.channels.size(), description.device.validity == D::Validity::Valid ? 1 : 0));
        }

        std::size_t bound{}, problems{};
        for (std::size_t index = 0; index < config_.bindings.size(); ++index) {
            const auto& binding = config_.bindings[index];
            const Resolved next = config_.enabled ? Resolve(binding, snapshot) : Resolved{};
            if (next.status == BindingStatus::Bound) ++bound;
            else if (next.status != BindingStatus::Unbound) ++problems;
            if (next.status != resolved_[index].status) {
                log_("ship_binding", std::format("action={} status={} token={}",
                    binding.actionId, ToString(next.status), binding.token));
            }
            resolved_[index] = next;
        }
        log_("ship_bindings_resolved", std::format("enabled={} bound={} problems={} devices={}",
            config_.enabled ? 1 : 0, bound, problems, snapshot.devices.size()));

        const auto axis = Resolve(axisBinding_, snapshot);
        if (axis.status != axisResolved_.status) {
            log_("walk_axis", std::format("status={} token={}", ToString(axis.status), axisBinding_.token));
        }
        axisResolved_ = axis;
    }

    void ShipControlCore::SampleButtons(const DeviceStore& devices, std::uint64_t now)
    {
        std::ranges::fill(samples_, ButtonSample{});
        axisValue_.reset();
        for (std::size_t slot = 0; slot < slots_.size(); ++slot) {
            D::Frame frame;
            std::uint32_t required{};
            const auto result = devices.CopyLatest(slots_[slot], frame, buffer_.data(),
                static_cast<std::uint32_t>(buffer_.size()), &required, now, kMaximumFrameAgeNanoseconds);
            if (result == D::Result::StaleGeneration || result == D::Result::NotFound) reconciled_ = false;
            if (result != D::Result::Ok || frame.validity != D::Validity::Valid) continue;
            for (std::size_t index = 0; index < resolved_.size(); ++index) {
                const auto& entry = resolved_[index];
                if (entry.status != BindingStatus::Bound || entry.deviceSlot != slot || entry.channelIndex >= required) continue;
                const auto& sample = buffer_[entry.channelIndex];
                if (sample.validity != D::Validity::Valid || config_.bindings[index].channel != sample.channel) continue;
                samples_[index] = { true, sample.value[0] != 0, sample.pressCount };
            }
            if (axisResolved_.status == BindingStatus::Bound && axisResolved_.deviceSlot == slot &&
                axisResolved_.channelIndex < required) {
                const auto& sample = buffer_[axisResolved_.channelIndex];
                if (sample.validity == D::Validity::Valid && sample.shape == D::Shape::Scalar &&
                    (sample.flags & D::kCalibrationRangeKnown) && axisBinding_.channel == sample.channel) {
                    axisValue_ = sample.value[0];
                }
            }
        }
    }

    void ShipControlCore::Tick(const DeviceStore& devices, const PilotObservation& observation, std::uint64_t now)
    {
        DeviceChanges changes;
        if (devices.CopyChanges(cursor_, changes) == D::Result::Ok &&
            (!changes.events.empty() || changes.dropped)) reconciled_ = false;
        if (!reconciled_) Reconcile(devices);
        SampleButtons(devices, now);

        pilot_.Update(observation, now);
        const bool gateOpen = pilot_.ButtonsLive();
        if (!gateLogged_ || gateOpen != gateOpen_) {
            log_("ship_gate", std::format("open={} seated={} targeting={} input={}", gateOpen ? 1 : 0,
                observation.seated ? 1 : 0, observation.targeting ? 1 : 0, observation.inputActive ? 1 : 0));
            gateLogged_ = true;
        }
        gateOpen_ = gateOpen;
        router_.Update(gateOpen_, std::span<const ButtonSample>(samples_), [this](std::size_t index, bool held) {
            sink_(config_.bindings[index].actionId, held);
        });
    }
}
