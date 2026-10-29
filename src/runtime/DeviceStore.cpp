#include "runtime/DeviceStore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        using namespace DeviceDraft;
        template <std::size_t N> bool Text(const char (&value)[N], bool allowEmpty = false)
        {
            return (allowEmpty || value[0] != '\0') && std::memchr(value, '\0', N);
        }
        bool SameDevice(const Device& a, const Device& b)
        {
            return std::strcmp(a.provider, b.provider) == 0 && std::strcmp(a.physical, b.physical) == 0;
        }
        bool SameGeneration(const Device& a, const Device& b)
        {
            return a.providerEpoch == b.providerEpoch && a.connectionEpoch == b.connectionEpoch &&
                a.enumerationRevision == b.enumerationRevision;
        }
        bool Components(Shape shape, std::uint32_t count)
        {
            switch (shape) {
            case Shape::Button: case Shape::Scalar: case Shape::Pov: return count == 1;
            case Shape::Vector: return count >= 2 && count <= 4;
            case Shape::RelativeDelta: return count >= 1 && count <= 4;
            case Shape::Contact: return count == 4;
            case Shape::Pose: return count == 7;
            default: return false;
            }
        }
        bool Valid(const DeviceDescription& item)
        {
            const auto& d = item.device;
            if (d.structSize != sizeof(Device) || !Text(d.provider) || !Text(d.physical) ||
                !Text(d.displayName, true) || !d.providerEpoch || !d.enumerationRevision ||
                d.channelCount != item.channels.size() || item.channels.size() > DeviceStore::kChannelLimit ||
                (d.validity != Validity::Valid && d.validity != Validity::Disconnected) ||
                (d.validity == Validity::Valid && !d.connectionEpoch)) return false;
            for (std::size_t index = 0; index < item.channels.size(); ++index) {
                const auto& c = item.channels[index];
                if (c.structSize != sizeof(Channel) || !Text(c.identity.provider) ||
                    !Text(c.identity.physical) || !Text(c.identity.channel) || !Text(c.displayName, true) ||
                    std::strcmp(c.identity.provider, d.provider) || std::strcmp(c.identity.physical, d.physical) ||
                    !Components(c.shape, c.components)) return false;
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (!std::strcmp(c.identity.channel, item.channels[prior].identity.channel)) return false;
                }
            }
            return true;
        }
        bool SameDescription(const DeviceDescription& a, const DeviceDescription& b)
        {
            if (!SameGeneration(a.device, b.device) || a.device.validity != b.device.validity ||
                a.channels.size() != b.channels.size()) return false;
            for (std::size_t i = 0; i < a.channels.size(); ++i) {
                const auto& x = a.channels[i]; const auto& y = b.channels[i];
                if (std::strcmp(x.identity.channel, y.identity.channel) || x.shape != y.shape ||
                    x.components != y.components || x.rawUnits != y.rawUnits || x.valueUnits != y.valueUnits) return false;
            }
            return true;
        }
        bool SameInventoryDescription(const DeviceDescription& a, const DeviceDescription& b)
        {
            if (!SameDevice(a.device, b.device) || !SameDescription(a, b) ||
                std::strcmp(a.device.displayName, b.device.displayName)) return false;
            for (std::size_t i = 0; i < a.channels.size(); ++i) {
                if (std::strcmp(a.channels[i].displayName, b.channels[i].displayName)) return false;
            }
            return true;
        }
    }

    void DeviceStore::RecordChange(DeviceChangeKind kind)
    {
        ++cursor_;
        changes_[(cursor_ - 1) % kChangeLimit] = { cursor_, session_, kind };
        changeCount_ = std::min(changeCount_ + 1, kChangeLimit);
    }

    void DeviceStore::Start()
    {
        std::lock_guard lock(mutex_);
        if (ready_) return;
        entries_.clear();
        ++session_;
        ready_ = true;
        RecordChange(DeviceChangeKind::Started);
    }

    void DeviceStore::Stop()
    {
        std::lock_guard lock(mutex_);
        if (!ready_) return;
        ready_ = false;
        entries_.clear();
        RecordChange(DeviceChangeKind::Stopped);
    }

    DeviceDraft::Result DeviceStore::ReplaceInventory(std::span<const DeviceDescription> devices)
    {
        if (devices.size() > kDeviceLimit) return Result::BufferTooSmall;
        for (std::size_t i = 0; i < devices.size(); ++i) {
            if (!Valid(devices[i])) return Result::InvalidArgument;
            for (std::size_t j = 0; j < i; ++j) {
                if (SameDevice(devices[i].device, devices[j].device)) return Result::InvalidArgument;
            }
        }
        std::lock_guard lock(mutex_);
        if (!ready_) return Result::NotReady;
        const bool changed = devices.size() != entries_.size() ||
            !std::equal(devices.begin(), devices.end(), entries_.begin(), entries_.end(),
                [](const auto& device, const auto& entry) {
                    return SameInventoryDescription(device, entry.description);
                });
        std::vector<Entry> next;
        next.reserve(devices.size());
        for (const auto& device : devices) {
            Entry entry{ device, {} };
            const auto old = std::find_if(entries_.begin(), entries_.end(), [&](const auto& candidate) {
                return SameDevice(candidate.description.device, device.device);
            });
            if (old != entries_.end()) {
                const auto& prior = old->description.device;
                if (device.device.providerEpoch < prior.providerEpoch ||
                    (device.device.providerEpoch == prior.providerEpoch &&
                        (device.device.enumerationRevision < prior.enumerationRevision ||
                         device.device.connectionEpoch < prior.connectionEpoch))) return Result::StaleGeneration;
                if (SameDescription(old->description, device)) entry.history = old->history;
            }
            next.push_back(std::move(entry));
        }
        entries_.swap(next); // Failed validation/allocation never publishes a partial inventory.
        if (changed) RecordChange(DeviceChangeKind::InventoryChanged);
        return Result::Ok;
    }

    DeviceSnapshot DeviceStore::Snapshot() const
    {
        std::lock_guard lock(mutex_);
        DeviceSnapshot snapshot{ ready_, session_, cursor_, {} };
        snapshot.devices.reserve(entries_.size());
        for (const auto& entry : entries_) snapshot.devices.push_back(entry.description);
        return snapshot;
    }

    DeviceDraft::Result DeviceStore::CopyChanges(std::uint64_t after, DeviceChanges& changes) const
    {
        std::lock_guard lock(mutex_);
        if (after > cursor_) return Result::InvalidArgument;
        DeviceChanges copy{ cursor_, 0, {} };
        if (changeCount_) {
            const auto oldest = cursor_ - changeCount_ + 1;
            copy.dropped = after < oldest - 1 ? oldest - 1 - after : 0;
            // Compute a bounded count instead of incrementing a cursor at UINT64_MAX.
            const auto count = std::min(cursor_ - after, static_cast<std::uint64_t>(changeCount_));
            copy.events.reserve(static_cast<std::size_t>(count));
            for (std::uint64_t offset = 0; offset < count; ++offset) {
                const auto sequence = cursor_ - count + 1 + offset;
                copy.events.push_back(changes_[(sequence - 1) % kChangeLimit]);
            }
        }
        changes = std::move(copy);
        return Result::Ok;
    }

    DeviceDraft::Result DeviceStore::Find(const DeviceDraft::Device& expected, const Entry*& entry) const
    {
        if (expected.structSize != sizeof(Device) || !Text(expected.provider) || !Text(expected.physical))
            return Result::InvalidArgument;
        if (!ready_) return Result::NotReady;
        for (const auto& candidate : entries_) {
            if (!SameDevice(candidate.description.device, expected)) continue;
            if (!SameGeneration(candidate.description.device, expected)) return Result::StaleGeneration;
            entry = &candidate;
            return Result::Ok;
        }
        return Result::NotFound;
    }

    DeviceDraft::Result DeviceStore::Publish(const DeviceFrame& input)
    {
        const auto& f = input.frame;
        if (f.structSize != sizeof(Frame) || !Text(f.provider) || !Text(f.physical) ||
            !f.sequence || !f.sampledAt || f.sampledAt > f.receivedAt ||
            f.clock != Clock::CoreMonotonicNanoseconds || f.sampleCount != input.samples.size())
            return Result::InvalidArgument;
        Device expected;
        std::memcpy(expected.provider, f.provider, sizeof(expected.provider));
        std::memcpy(expected.physical, f.physical, sizeof(expected.physical));
        expected.providerEpoch = f.providerEpoch;
        expected.enumerationRevision = f.enumerationRevision;
        expected.connectionEpoch = f.connectionEpoch;
        std::lock_guard lock(mutex_);
        const Entry* found{};
        const auto result = Find(expected, found);
        if (result != Result::Ok) return result;
        if (f.validity != found->description.device.validity || input.samples.size() != found->description.channels.size())
            return Result::InvalidArgument;
        if (!found->history.empty()) {
            const auto& previous = found->history.back().frame;
            if (f.sequence <= previous.sequence || f.sampledAt < previous.sampledAt || f.receivedAt < previous.receivedAt)
                return Result::StaleGeneration;
        }
        for (std::size_t i = 0; i < input.samples.size(); ++i) {
            const auto& s = input.samples[i]; const auto& channel = found->description.channels[i];
            if (s.structSize != sizeof(Sample) || !Text(s.channel) ||
                std::strcmp(s.channel, channel.identity.channel) || s.shape != channel.shape ||
                s.components != channel.components || s.validity != f.validity ||
                s.origin != SampleOrigin::Physical ||
                (s.counterBits != 0 && s.counterBits != 32 && s.counterBits != 64) ||
                (s.counterBits == 0 && (s.pressCount || s.releaseCount)) ||
                (s.counterBits == 32 && ((s.pressCount >> 32) || (s.releaseCount >> 32))) ||
                (s.counterBits && s.shape != Shape::Button) ||
                (s.flags & ~kCalibrationRangeKnown) || s.reserved) return Result::InvalidArgument;
            for (std::size_t c = 0; c < s.components; ++c) {
                if (!std::isfinite(s.raw[c]) || !std::isfinite(s.value[c])) return Result::InvalidArgument;
                if ((s.flags & kCalibrationRangeKnown) &&
                    (!std::isfinite(s.calibrationMinimum[c]) || !std::isfinite(s.calibrationMaximum[c]) ||
                     s.calibrationMinimum[c] >= s.calibrationMaximum[c])) return Result::InvalidArgument;
            }
            if (s.shape == Shape::Button &&
                ((s.raw[0] != 0 && s.raw[0] != 1) || (s.value[0] != 0 && s.value[0] != 1))) return Result::InvalidArgument;
        }
        auto& entry = entries_[static_cast<std::size_t>(found - entries_.data())];
        entry.history.push_back(input);
        if (entry.history.size() > kHistoryLimit) entry.history.pop_front();
        return Result::Ok;
    }

    DeviceDraft::Result DeviceStore::CopyDevices(Device* records, std::uint32_t capacity, std::uint32_t* required) const
    {
        if (!required || (!records && capacity)) return Result::InvalidArgument;
        std::lock_guard lock(mutex_);
        if (!ready_) return Result::NotReady;
        *required = static_cast<std::uint32_t>(entries_.size());
        if (capacity < entries_.size()) return Result::BufferTooSmall;
        for (std::size_t i = 0; i < entries_.size(); ++i) records[i] = entries_[i].description.device;
        return Result::Ok;
    }

    DeviceDraft::Result DeviceStore::CopyChannels(const Device& expected, Channel* records,
        std::uint32_t capacity, std::uint32_t* required) const
    {
        if (!required || (!records && capacity)) return Result::InvalidArgument;
        std::lock_guard lock(mutex_);
        const Entry* entry{};
        const auto result = Find(expected, entry);
        if (result != Result::Ok) return result;
        *required = static_cast<std::uint32_t>(entry->description.channels.size());
        if (capacity < *required) return Result::BufferTooSmall;
        std::copy(entry->description.channels.begin(), entry->description.channels.end(), records);
        return Result::Ok;
    }

    DeviceDraft::Result DeviceStore::CopyLatest(const Device& expected, Frame& frame, Sample* records,
        std::uint32_t capacity, std::uint32_t* required, std::uint64_t now, std::uint64_t maximumAge) const
    {
        if (!required || (!records && capacity) || !maximumAge) return Result::InvalidArgument;
        std::lock_guard lock(mutex_);
        const Entry* entry{};
        const auto result = Find(expected, entry);
        if (result != Result::Ok) return result;
        if (entry->history.empty()) return Result::NotReady;
        const auto& latest = entry->history.back();
        *required = static_cast<std::uint32_t>(latest.samples.size());
        if (capacity < *required) return Result::BufferTooSmall;
        frame = latest.frame;
        std::copy(latest.samples.begin(), latest.samples.end(), records);
        // A caller can sample now immediately before a concurrent publication.
        // That newer frame has zero age at the read request, not unsigned underflow.
        const auto age = now > frame.sampledAt ? now - frame.sampledAt : 0;
        if (frame.validity == Validity::Valid && age > maximumAge) {
            frame.validity = Validity::Stale;
            for (std::size_t i = 0; i < latest.samples.size(); ++i) records[i].validity = Validity::Stale;
        }
        return Result::Ok;
    }

    std::vector<DeviceFrame> DeviceStore::History(const Device& expected) const
    {
        std::lock_guard lock(mutex_);
        const Entry* entry{};
        if (Find(expected, entry) != Result::Ok) return {};
        return { entry->history.begin(), entry->history.end() };
    }
}
