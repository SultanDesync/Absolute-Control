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

    void ShipControlCore::SetAxisBinding(std::size_t slot, ConfiguredBinding binding)
    {
        if (slot >= axes_.size()) return;
        axes_[slot] = { std::move(binding), {}, {} };
        reconciled_ = false;
    }

    void ShipControlCore::SetButtonBinding(std::size_t slot, ConfiguredBinding binding)
    {
        if (slot >= buttons_.size()) return;
        buttons_[slot] = { std::move(binding), {}, {} };
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
                next.angle = channel->rawUnits == D::Units::Radians;
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
            // Which axes a device really reports settles any "channel missing" binding.
            std::string axes;
            for (const auto& channel : description.channels) {
                const std::string_view id{ channel.identity.channel };
                if (!id.starts_with("axis/")) continue;
                if (!axes.empty()) axes += ',';
                axes += id.substr(5);
            }
            log_("input_device", std::format("name={} id={} channels={} valid={} axes={}",
                description.device.displayName, description.device.physical,
                description.channels.size(), description.device.validity == D::Validity::Valid ? 1 : 0, axes));
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

        for (std::size_t slot = 0; slot < axes_.size(); ++slot) {
            auto& axis = axes_[slot];
            const auto next = Resolve(axis.binding, snapshot);
            if (next.status != axis.resolved.status) {
                if (slot == 0) {
                    log_("walk_axis", std::format("status={} token={}", ToString(next.status), axis.binding.token));
                } else {
                    log_("flight_axis", std::format("axis={} status={} token={}", axis.binding.actionId,
                        ToString(next.status), axis.binding.token));
                }
            }
            axis.resolved = next;
        }
        for (auto& button : buttons_) {
            const auto next = Resolve(button.binding, snapshot);
            if (next.status != button.resolved.status) {
                log_("control_button", std::format("button={} status={} token={}", button.binding.actionId,
                    ToString(next.status), button.binding.token));
            }
            button.resolved = next;
        }
    }

    void ShipControlCore::SampleButtons(const DeviceStore& devices, std::uint64_t now)
    {
        std::ranges::fill(samples_, ButtonSample{});
        for (auto& axis : axes_) axis.value.reset();
        for (auto& button : buttons_) button.down = false;
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
            for (auto& axis : axes_) {
                if (axis.resolved.status != BindingStatus::Bound || axis.resolved.deviceSlot != slot ||
                    axis.resolved.channelIndex >= required) continue;
                const auto& sample = buffer_[axis.resolved.channelIndex];
                if (sample.validity == D::Validity::Valid && sample.shape == D::Shape::Scalar &&
                    (sample.flags & D::kCalibrationRangeKnown) && axis.binding.channel == sample.channel) {
                    axis.value = sample.value[0];
                }
            }
            for (auto& button : buttons_) {
                if (button.resolved.status != BindingStatus::Bound || button.resolved.deviceSlot != slot ||
                    button.resolved.channelIndex >= required) continue;
                const auto& sample = buffer_[button.resolved.channelIndex];
                if (sample.validity == D::Validity::Valid && button.binding.channel == sample.channel) {
                    button.down = sample.value[0] != 0;
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
