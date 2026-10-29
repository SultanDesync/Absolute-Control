#pragma once

#include "runtime/DeviceServiceDraft.h"

#include <array>
#include <deque>
#include <mutex>
#include <span>
#include <vector>

namespace AbsoluteControl::Runtime
{
    struct DeviceDescription
    {
        DeviceDraft::Device device;
        std::vector<DeviceDraft::Channel> channels;
    };
    struct DeviceFrame
    {
        DeviceDraft::Frame frame;
        std::vector<DeviceDraft::Sample> samples;
    };

    // Internal native notification contract, not an exported subscriber ABI.
    // Notifications invalidate an inventory view; they never carry input samples.
    enum class DeviceChangeKind { Started, InventoryChanged, Stopped };
    struct DeviceChange
    {
        std::uint64_t sequence{};
        std::uint64_t session{};
        DeviceChangeKind kind{};
    };
    struct DeviceChanges
    {
        std::uint64_t cursor{};
        std::uint64_t dropped{}; // Events after the requested cursor no longer retained.
        std::vector<DeviceChange> events;
    };
    struct DeviceSnapshot
    {
        bool ready{};
        std::uint64_t session{};
        std::uint64_t cursor{};
        std::vector<DeviceDescription> devices;
    };

    // Native data plane. The provider owns publication; readers receive copies.
    // No provider callbacks, borrowed pointers, hardware handles or menu state.
    class DeviceStore final
    {
    public:
        static constexpr std::size_t kDeviceLimit = 16;
        static constexpr std::size_t kChannelLimit = 256;
        static constexpr std::size_t kHistoryLimit = 8;
        static constexpr std::size_t kChangeLimit = 256;

        void Start();
        void Stop();
        [[nodiscard]] DeviceDraft::Result ReplaceInventory(std::span<const DeviceDescription> devices);
        [[nodiscard]] DeviceDraft::Result Publish(const DeviceFrame& frame);
        [[nodiscard]] DeviceDraft::Result CopyDevices(DeviceDraft::Device* records,
            std::uint32_t capacity, std::uint32_t* required) const;
        [[nodiscard]] DeviceDraft::Result CopyChannels(const DeviceDraft::Device& expected,
            DeviceDraft::Channel* records, std::uint32_t capacity, std::uint32_t* required) const;
        [[nodiscard]] DeviceDraft::Result CopyLatest(const DeviceDraft::Device& expected,
            DeviceDraft::Frame& frame, DeviceDraft::Sample* records,
            std::uint32_t capacity, std::uint32_t* required,
            std::uint64_t now, std::uint64_t maximumAge) const;
        [[nodiscard]] std::vector<DeviceFrame> History(const DeviceDraft::Device& expected) const;
        // Snapshot and its cursor are copied under one lock. Reconcile from a
        // snapshot at startup or after dropped events, then read after its cursor.
        // Both reads work while stopped and never acquire hardware/call providers.
        [[nodiscard]] DeviceSnapshot Snapshot() const;
        [[nodiscard]] DeviceDraft::Result CopyChanges(std::uint64_t after,
            DeviceChanges& changes) const;

    private:
        struct Entry { DeviceDescription description; std::deque<DeviceFrame> history; };
        [[nodiscard]] DeviceDraft::Result Find(const DeviceDraft::Device& expected,
            const Entry*& entry) const;
        void RecordChange(DeviceChangeKind kind); // mutex_ held; no allocation.
        mutable std::mutex mutex_;
        bool ready_{};
        std::vector<Entry> entries_;
        std::uint64_t session_{};
        std::uint64_t cursor_{};
        std::array<DeviceChange, kChangeLimit> changes_{};
        std::size_t changeCount_{};
    };
}
