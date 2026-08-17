#pragma once

#include "LiveComponentsExperimentalAPI.h"

#include <array>
#include <cstddef>
#include <vector>

namespace AbsoluteControlPanelResearch::LiveComponents
{
    using namespace AbsoluteControlPanelExperimental;

    struct PollPublication
    {
        Result result{ Result::NotReady };
        std::uint32_t publish{};
        LiveChannelModelV1 channel{};
        LiveFrameV1 frame{};
    };

    struct CompoundPublication
    {
        Result result{ Result::NotReady };
        std::uint32_t publish{};
        LiveChannelModelV1 channel{};
        CompoundSnapshotV1 snapshot{};
    };

    struct TelemetryHistoryV1
    {
        std::uint32_t structSize{ sizeof(TelemetryHistoryV1) };
        std::uint32_t count{};
        std::uint32_t writeIndex{};
        TelemetrySampleV1 samples[kMaximumPlotSamples]{};
    };

    // Registry ownership is confined to the game/UI thread. Registration may
    // copy descriptors, while Poll uses only fixed storage and never locks or
    // allocates around a provider live callback.
    class Registry
    {
    public:
        [[nodiscard]] Result Register(const LiveChannelDescriptorV1& descriptor) noexcept;
        [[nodiscard]] Result UnregisterModule(const char* moduleId) noexcept;
        [[nodiscard]] Result RequestImmediateRefresh(
            const char* moduleId, const char* pageId, const char* channelId) noexcept;
        [[nodiscard]] Result TakeImmediateRefreshRequest(
            const char* moduleId, const char* pageId, const char* channelId,
            std::uint32_t& requested) noexcept;

        void SetMenuActive(bool active) noexcept;
        [[nodiscard]] Result SetVisiblePage(const char* moduleId, const char* pageId) noexcept;
        [[nodiscard]] PollPublication Poll(
            const char* moduleId, const char* pageId, const char* channelId) noexcept;
        [[nodiscard]] CompoundPublication Apply(const CompoundOperationV1& operation) noexcept;
        [[nodiscard]] Result Describe(
            const char* moduleId, const char* pageId, const char* channelId,
            LiveChannelModelV1& model) const noexcept;
        [[nodiscard]] Result TelemetryHistory(
            const char* moduleId, const char* pageId, const char* channelId,
            TelemetryHistoryV1& history) const noexcept;
        [[nodiscard]] std::size_t ChannelCount() const noexcept;

        // UI-thread integration lane. PollVisiblePage invokes only channels on
        // the active route and reports whether any valid sequence advanced.
        // SnapshotPage copies the latest bounded frames without provider calls.
        [[nodiscard]] bool PollVisiblePage() noexcept;
        [[nodiscard]] std::vector<PollPublication> SnapshotPage(
            const char* moduleId, const char* pageId) const;
        [[nodiscard]] bool HasPageChannels(
            const char* moduleId, const char* pageId) const noexcept;

    private:
        struct Slot
        {
            bool occupied{};
            bool hasFrame{};
            bool refreshRequested{};
            LiveChannelDescriptorV1 descriptor{};
            LiveChannelModelV1 model{};
            LiveFrameV1 latestFrame{};
            std::uint64_t lastSequence{};
            std::uint64_t lastTimestampUs{};
            std::uint64_t compoundRevision{};
            TelemetryHistoryV1 telemetryHistory{};
        };

        std::array<Slot, kMaximumChannels> slots_{};
        bool menuActive_{};
        char visibleModuleId_[kIdentifierCapacity]{};
        char visiblePageId_[kIdentifierCapacity]{};
    };

    [[nodiscard]] Registry& HostRegistry() noexcept;
}
