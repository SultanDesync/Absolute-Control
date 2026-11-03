#include "LiveComponentsRegistry.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#define CHECK(expression)                                      \
    do {                                                       \
        if (!(expression)) {                                  \
            std::fprintf(stderr, "CHECK failed at line %d: %s\n", \
                __LINE__, #expression);                        \
            return 1;                                          \
        }                                                      \
    } while (false)

namespace
{
    using namespace AbsoluteControlPanelExperimental;

    struct Provider
    {
        LiveFrameV1 frame{};
        CompoundSnapshotV1 replacement{};
        std::uint32_t readCount{};
        std::uint32_t operationCount{};
    };

    struct LiveChannelDescriptorV1FlagsFixture
    {
        std::uint32_t structSize{};
        std::uint32_t abiVersion{};
        char moduleId[kIdentifierCapacity]{};
        char pageId[kIdentifierCapacity]{};
        char channelId[kIdentifierCapacity]{};
        char title[kLabelCapacity]{};
        ComponentKind kind{};
        RangeMeterDescriptorV1 rangeMeter{};
        TelemetryPlotDescriptorV1 telemetryPlot{};
        SegmentedGridDescriptorV1 segmentedGrid{};
        void* context{};
        ReadLiveFrameCallback readLiveFrame{};
        ApplyCompoundOperationCallback applyCompoundOperation{};
        std::uint32_t flags{};
    };

    Result __cdecl ReadFrame(void* context, LiveFrameV1* frame) noexcept
    {
        auto& provider = *static_cast<Provider*>(context);
        ++provider.readCount;
        *frame = provider.frame;
        return Result::Ok;
    }

    Result __cdecl ApplyOperation(
        void* context, const CompoundOperationV1*, CompoundSnapshotV1* replacement) noexcept
    {
        auto& provider = *static_cast<Provider*>(context);
        ++provider.operationCount;
        *replacement = provider.replacement;
        return Result::Ok;
    }

    void Copy(char* destination, std::size_t capacity, const char* source)
    {
        strcpy_s(destination, capacity, source);
    }

    LiveChannelDescriptorV1 Channel(ComponentKind kind, const char* channelId, Provider& provider)
    {
        LiveChannelDescriptorV1 descriptor;
        Copy(descriptor.moduleId, std::size(descriptor.moduleId), "test.module");
        Copy(descriptor.pageId, std::size(descriptor.pageId), "live");
        Copy(descriptor.channelId, std::size(descriptor.channelId), channelId);
        Copy(descriptor.title, std::size(descriptor.title), channelId);
        descriptor.kind = kind;
        descriptor.context = &provider;
        descriptor.readLiveFrame = &ReadFrame;
        return descriptor;
    }

    void InitializeRange(LiveChannelDescriptorV1& descriptor)
    {
        descriptor.rangeMeter.minimumValue = -1.0;
        descriptor.rangeMeter.maximumValue = 1.0;
        descriptor.rangeMeter.bandCount = 1;
        descriptor.rangeMeter.markerCount = 1;
        Copy(descriptor.rangeMeter.valueFormat, std::size(descriptor.rangeMeter.valueFormat), "%.2f");
        auto& band = descriptor.rangeMeter.bands[0];
        band.semantic = RangeBandSemantic::Active;
        band.minimumValue = -0.5;
        band.maximumValue = 0.5;
        Copy(band.label, std::size(band.label), "Active");
        auto& marker = descriptor.rangeMeter.markers[0];
        marker.semantic = RangeMarkerSemantic::Center;
        marker.value = 0.0;
        Copy(marker.markerId, std::size(marker.markerId), "center");
        Copy(marker.label, std::size(marker.label), "Center");
        Copy(marker.controlId, std::size(marker.controlId), "center.control");
    }

    void InitializePlot(LiveChannelDescriptorV1& descriptor)
    {
        descriptor.telemetryPlot.seriesCount = 2;
        descriptor.telemetryPlot.historyCapacity = 3;
        descriptor.telemetryPlot.minimumValue = -10.0;
        descriptor.telemetryPlot.maximumValue = 10.0;
        Copy(descriptor.telemetryPlot.series[0].seriesId,
            std::size(descriptor.telemetryPlot.series[0].seriesId), "input");
        Copy(descriptor.telemetryPlot.series[0].label,
            std::size(descriptor.telemetryPlot.series[0].label), "Input");
        Copy(descriptor.telemetryPlot.series[1].seriesId,
            std::size(descriptor.telemetryPlot.series[1].seriesId), "output");
        Copy(descriptor.telemetryPlot.series[1].label,
            std::size(descriptor.telemetryPlot.series[1].label), "Output");
    }

    void InitializeGrid(LiveChannelDescriptorV1& descriptor)
    {
        descriptor.applyCompoundOperation = &ApplyOperation;
        auto& grid = descriptor.segmentedGrid;
        Copy(grid.controlId, std::size(grid.controlId), "allocation");
        grid.columnCount = 2;
        grid.tierCount = 4;
        const char* columnIds[]{ "engines", "shields" };
        const char* tierIds[]{ "hollow", "green", "yellow", "red" };
        for (std::size_t index = 0; index < grid.columnCount; ++index) {
            Copy(grid.columns[index].columnId, std::size(grid.columns[index].columnId), columnIds[index]);
            Copy(grid.columns[index].label, std::size(grid.columns[index].label), columnIds[index]);
            grid.columns[index].maximumSegments = index == 0 ? 32 : 16;
        }
        for (std::size_t index = 0; index < grid.tierCount; ++index) {
            Copy(grid.tiers[index].tierId, std::size(grid.tiers[index].tierId), tierIds[index]);
            Copy(grid.tiers[index].label, std::size(grid.tiers[index].label), tierIds[index]);
        }
    }

    void InitializeRadial(LiveChannelDescriptorV1& descriptor)
    {
        auto& radial = descriptor.radialResponse;
        radial.maximumRadius = 200.0;
        Copy(radial.enabledControlId, std::size(radial.enabledControlId), "enabled");
        Copy(radial.radiusControlId, std::size(radial.radiusControlId), "radius");
        Copy(radial.idleMillisecondsControlId,
            std::size(radial.idleMillisecondsControlId), "idle-ms");
        Copy(radial.decayRateControlId,
            std::size(radial.decayRateControlId), "decay-rate");
        Copy(radial.pollRateControlId,
            std::size(radial.pollRateControlId), "poll-rate-hz");
    }

    void InitializeHeadPose(LiveChannelDescriptorV1& descriptor)
    {
        auto& pose = descriptor.headPose;
        pose.axisCount = 3;
        Copy(pose.recenterControlId, std::size(pose.recenterControlId),
            "recenter-now");
        Copy(pose.deadzoneControlId, std::size(pose.deadzoneControlId),
            "deadzone");
        const char* ids[]{ "yaw", "pitch", "roll" };
        const char* labels[]{ "Yaw / horizontal", "Pitch / vertical",
            "Roll / horizon" };
        const HeadPoseView views[]{ HeadPoseView::Top, HeadPoseView::Profile,
            HeadPoseView::ArtificialHorizon };
        for (std::size_t index = 0; index < pose.axisCount; ++index) {
            auto& axis = pose.axes[index];
            axis.view = views[index];
            Copy(axis.axisId, std::size(axis.axisId), ids[index]);
            Copy(axis.label, std::size(axis.label), labels[index]);
            char control[kIdentifierCapacity]{};
            sprintf_s(control, "%s-sensitivity", ids[index]);
            Copy(axis.sensitivityControlId,
                std::size(axis.sensitivityControlId), control);
            sprintf_s(control, "%s-minimum", ids[index]);
            Copy(axis.minimumControlId,
                std::size(axis.minimumControlId), control);
            sprintf_s(control, "%s-center", ids[index]);
            Copy(axis.centerControlId,
                std::size(axis.centerControlId), control);
            sprintf_s(control, "%s-maximum", ids[index]);
            Copy(axis.maximumControlId,
                std::size(axis.maximumControlId), control);
            sprintf_s(control, "%s-enabled", ids[index]);
            Copy(axis.enabledControlId,
                std::size(axis.enabledControlId), control);
            sprintf_s(control, "%s-inverted", ids[index]);
            Copy(axis.invertedControlId,
                std::size(axis.invertedControlId), control);
        }
    }

    LiveFrameV1 Frame(ComponentKind kind, std::uint64_t sequence, std::uint64_t timestamp)
    {
        LiveFrameV1 frame;
        frame.kind = kind;
        frame.sequence = sequence;
        frame.monotonicTimestampUs = timestamp;
        return frame;
    }

    CompoundOperationV1 Operation(CompoundOperationKind kind)
    {
        CompoundOperationV1 operation;
        operation.kind = kind;
        Copy(operation.moduleId, std::size(operation.moduleId), "test.module");
        Copy(operation.pageId, std::size(operation.pageId), "live");
        Copy(operation.channelId, std::size(operation.channelId), "power");
        Copy(operation.controlId, std::size(operation.controlId), "allocation");
        return operation;
    }
}

int main()
{
    using namespace AbsoluteControlPanelExperimental;
    using namespace AbsoluteControlPanelResearch::LiveComponents;

    static_assert(std::is_trivially_copyable_v<LiveFrameV1>);
    static_assert(kLiveFrameV1BaseSize == offsetof(LiveFrameV1, dynamicRange));
    static_assert(std::is_trivially_copyable_v<LiveChannelModelV1>);
    static_assert(sizeof(GridColumnDescriptorV1) == 168);
    static_assert(sizeof(SegmentedGridDescriptorV1) == 2092);
    static_assert(offsetof(LiveChannelDescriptorV1, flags) ==
        offsetof(LiveChannelDescriptorV1FlagsFixture, flags));
    static_assert(kLiveChannelDescriptorV1FlagsSize ==
        offsetof(LiveChannelDescriptorV1FlagsFixture, flags) +
            sizeof(std::uint32_t));
    static_assert(kLiveChannelDescriptorV1AssociationsSize ==
        offsetof(LiveChannelDescriptorV1, radialResponse));
    static_assert(kLiveChannelDescriptorV1RadialResponseSize ==
        offsetof(LiveChannelDescriptorV1, headPose));
    static_assert(kLiveFrameV1RadialResponseSize ==
        offsetof(LiveFrameV1, headPose));
    CHECK(AbsoluteControlPanel_QueryLiveComponentsExperimental(kAbiVersion + 1) == nullptr);
    const auto* api = AbsoluteControlPanel_QueryLiveComponentsExperimental(kAbiVersion);
    CHECK(api && api->structSize >= sizeof(ExperimentalApiV1) &&
        (api->capabilities & kLiveCapabilities) == kLiveCapabilities);

    static Registry registry;
    Provider rangeProvider;
    auto range = Channel(ComponentKind::RangeMeter, "axis", rangeProvider);
    InitializeRange(range);
    CHECK(registry.Register(range) == Result::Ok);
    CHECK(registry.Register(range) == Result::Duplicate);
    const auto initialDiagnostics = registry.Diagnostics();
    CHECK(initialDiagnostics.size() == 1 &&
        initialDiagnostics[0].moduleId == "test.module" &&
        initialDiagnostics[0].channelCount == 1);

    // Registration copies static descriptors; the renderer-facing model has no
    // provider context or callback fields to leak into ActionScript.
    Copy(range.rangeMeter.markers[0].label, std::size(range.rangeMeter.markers[0].label), "MUTATED");
    LiveChannelModelV1 rangeModel;
    CHECK(registry.Describe("test.module", "live", "axis", rangeModel) == Result::Ok);
    CHECK(std::strcmp(rangeModel.rangeMeter.markers[0].label, "Center") == 0);

    // Descriptor and capacity validation is complete before registration.
    auto invalidRange = range;
    invalidRange.rangeMeter.bandCount = static_cast<std::uint32_t>(kMaximumRangeBands + 1);
    CHECK(registry.Register(invalidRange) == Result::InvalidArgument);
    invalidRange = range;
    invalidRange.rangeMeter.bands[0].minimumValue = std::numeric_limits<double>::quiet_NaN();
    CHECK(registry.Register(invalidRange) == Result::InvalidArgument);
    invalidRange = range;
    invalidRange.rangeMeter.bands[0].visualRole = static_cast<VisualRole>(999);
    CHECK(registry.Register(invalidRange) == Result::InvalidArgument);

    Provider presentationProvider;
    auto presentation = Channel(
        ComponentKind::RangeMeter, "presentation", presentationProvider);
    InitializeRange(presentation);
    presentation.flags = kLivePresentationPinned |
        kLivePresentationSecondary | kLivePresentationCollapsedByDefault;
    static Registry presentationRegistry;
    CHECK(presentationRegistry.Register(presentation) == Result::Ok);
    LiveChannelModelV1 presentationModel;
    CHECK(presentationRegistry.Describe("test.module", "live",
        "presentation", presentationModel) == Result::Ok &&
        presentationModel.flags == presentation.flags);
    presentation.flags = kLivePresentationCollapsedByDefault;
    CHECK(presentationRegistry.Register(presentation) ==
        Result::InvalidArgument);

    Provider legacyProvider;
    auto legacy = Channel(ComponentKind::RangeMeter, "legacy", legacyProvider);
    InitializeRange(legacy);
    legacy.structSize = kLiveChannelDescriptorV1BaseSize;
    legacy.flags = kSegmentedGridCycleOnClick;
    static Registry legacyRegistry;
    CHECK(legacyRegistry.Register(legacy) == Result::Ok);
    LiveChannelModelV1 legacyModel;
    CHECK(legacyRegistry.Describe("test.module", "live", "legacy", legacyModel) ==
        Result::Ok && legacyModel.flags == kSegmentedGridNone);

    static Registry capacityRegistry;
    for (std::size_t index = 0; index < kMaximumChannels; ++index) {
        char channelId[kIdentifierCapacity]{};
        sprintf_s(channelId, "axis%zu", index);
        auto capacityChannel = Channel(ComponentKind::RangeMeter, channelId, rangeProvider);
        InitializeRange(capacityChannel);
        CHECK(capacityRegistry.Register(capacityChannel) == Result::Ok);
    }
    auto overflowChannel = Channel(ComponentKind::RangeMeter, "overflow", rangeProvider);
    InitializeRange(overflowChannel);
    CHECK(capacityRegistry.Register(overflowChannel) == Result::CapacityExceeded);
    const auto capacityDiagnostics = capacityRegistry.Diagnostics();
    CHECK(capacityDiagnostics.size() == 1 &&
        capacityDiagnostics[0].channelCount == kMaximumChannels);

    rangeProvider.frame = Frame(ComponentKind::RangeMeter, 1, 100);
    rangeProvider.frame.rangeMeter.available = 1;
    rangeProvider.frame.rangeMeter.liveValue = 0.25;
    auto publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Suspended && rangeProvider.readCount == 0);
    CHECK(registry.SetVisiblePage("test.module", "live") == Result::Ok);
    registry.SetMenuActive(true);
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Ok && publication.publish == 1 && rangeProvider.readCount == 1);

    // A non-advancing sequence or timestamp produces a visible stale frame.
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Stale && (publication.frame.flags & kFrameStale) != 0);
    rangeProvider.frame.sequence = 2;
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Stale);
    rangeProvider.frame.sequence = 2;
    rangeProvider.frame.monotonicTimestampUs = 200;
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Ok);

    // The additive frame tail replaces static range bands/markers from the
    // provider's current draft while the base v1 layout remains accepted.
    rangeProvider.frame.sequence = 3;
    rangeProvider.frame.monotonicTimestampUs = 300;
    // This is the pre-radial full frame size. Its dynamic-range tail remains
    // valid after the new radial frame was appended.
    rangeProvider.frame.structSize = kLiveFrameV1DynamicRangeSize;
    rangeProvider.frame.dynamicRange.present = 1;
    rangeProvider.frame.dynamicRange.bandCount = 1;
    rangeProvider.frame.dynamicRange.markerCount = 1;
    rangeProvider.frame.dynamicRange.bands[0] =
        range.rangeMeter.bands[0];
    rangeProvider.frame.dynamicRange.markers[0] =
        range.rangeMeter.markers[0];
    rangeProvider.frame.dynamicRange.markers[0].value = 0.25;
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Ok &&
        publication.frame.dynamicRange.present == 1 &&
        publication.frame.dynamicRange.markers[0].value == 0.25);
    rangeProvider.frame.sequence = 4;
    rangeProvider.frame.monotonicTimestampUs = 400;
    rangeProvider.frame.dynamicRange.bands[0].maximumValue = 2.0;
    CHECK(registry.Poll("test.module", "live", "axis").result ==
        Result::InvalidArgument);
    rangeProvider.frame.dynamicRange = {};
    rangeProvider.frame.structSize = sizeof(LiveFrameV1);

    rangeProvider.frame.sequence = 4;
    rangeProvider.frame.monotonicTimestampUs = 250;
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Stale);

    rangeProvider.frame.sequence = 4;
    rangeProvider.frame.monotonicTimestampUs = 400;
    rangeProvider.frame.rangeMeter.liveValue = std::numeric_limits<double>::infinity();
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::InvalidArgument && publication.publish == 0);
    rangeProvider.frame.rangeMeter.liveValue = 0.5;
    registry.SetMenuActive(false);
    const auto readsBeforeHiddenPoll = rangeProvider.readCount;
    publication = registry.Poll("test.module", "live", "axis");
    CHECK(publication.result == Result::Suspended && rangeProvider.readCount == readsBeforeHiddenPoll &&
          (publication.frame.flags & kFrameSuspended) != 0);
    registry.SetMenuActive(true);

    Provider plotProvider;
    auto plot = Channel(ComponentKind::TelemetryPlot, "plot", plotProvider);
    InitializePlot(plot);
    auto invalidPlot = plot;
    invalidPlot.telemetryPlot.seriesCount = static_cast<std::uint32_t>(kMaximumPlotSeries + 1);
    CHECK(registry.Register(invalidPlot) == Result::InvalidArgument);
    invalidPlot = plot;
    invalidPlot.telemetryPlot.historyCapacity = static_cast<std::uint32_t>(kMaximumPlotSamples + 1);
    CHECK(registry.Register(invalidPlot) == Result::InvalidArgument);
    CHECK(registry.Register(plot) == Result::Ok);
    for (std::uint64_t sequence = 1; sequence <= 5; ++sequence) {
        plotProvider.frame = Frame(ComponentKind::TelemetryPlot, sequence, sequence * 10);
        plotProvider.frame.telemetryPlot.seriesCount = 2;
        plotProvider.frame.telemetryPlot.availableMask = 3;
        plotProvider.frame.telemetryPlot.values[0] = static_cast<double>(sequence);
        plotProvider.frame.telemetryPlot.values[1] = -static_cast<double>(sequence);
        CHECK(registry.Poll("test.module", "live", "plot").result == Result::Ok);
    }
    TelemetryHistoryV1 history;
    CHECK(registry.TelemetryHistory("test.module", "live", "plot", history) == Result::Ok);
    CHECK(history.count == 3 && history.writeIndex == 2);
    // The fixed ring retains the newest bounded samples. writeIndex points to
    // the oldest record once capacity is reached: 3, 4, 5 in display order.
    CHECK(history.samples[history.writeIndex].values[0] == 3.0);
    CHECK(history.samples[(history.writeIndex + 1) % 3].values[0] == 4.0);
    CHECK(history.samples[(history.writeIndex + 2) % 3].values[0] == 5.0);
    plotProvider.frame.sequence = 6;
    plotProvider.frame.monotonicTimestampUs = 60;
    plotProvider.frame.telemetryPlot.values[1] = std::numeric_limits<double>::quiet_NaN();
    CHECK(registry.Poll("test.module", "live", "plot").result == Result::InvalidArgument);

    Provider radialProvider;
    auto radial = Channel(ComponentKind::RadialResponse, "centering", radialProvider);
    InitializeRadial(radial);
    radial.structSize = kLiveChannelDescriptorV1RadialResponseSize;
    radial.flags = kLivePresentationPinned;
    static Registry radialRegistry;
    CHECK(radialRegistry.Register(radial) == Result::Ok);
    LiveChannelModelV1 radialModel;
    CHECK(radialRegistry.Describe("test.module", "live", "centering",
        radialModel) == Result::Ok &&
        radialModel.radialResponse.maximumRadius == 200.0 &&
        std::strcmp(radialModel.radialResponse.radiusControlId, "radius") == 0);
    auto invalidRadial = radial;
    invalidRadial.structSize = kLiveChannelDescriptorV1AssociationsSize;
    CHECK(radialRegistry.Register(invalidRadial) == Result::InvalidArgument);
    invalidRadial = radial;
    invalidRadial.radialResponse.maximumRadius = 0.0;
    CHECK(radialRegistry.Register(invalidRadial) == Result::InvalidArgument);
    radialProvider.frame = Frame(ComponentKind::RadialResponse, 1, 100);
    radialProvider.frame.structSize = kLiveFrameV1RadialResponseSize;
    radialProvider.frame.radialResponse.available = 1;
    radialProvider.frame.radialResponse.liveX = 80.0;
    radialProvider.frame.radialResponse.liveY = -40.0;
    CHECK(radialRegistry.SetVisiblePage("test.module", "live") == Result::Ok);
    radialRegistry.SetMenuActive(true);
    CHECK(radialRegistry.Poll("test.module", "live", "centering").result ==
        Result::Ok);
    radialProvider.frame.sequence = 2;
    radialProvider.frame.monotonicTimestampUs = 200;
    radialProvider.frame.radialResponse.liveX = 201.0;
    radialProvider.frame.radialResponse.liveY = 0.0;
    CHECK(radialRegistry.Poll("test.module", "live", "centering").result ==
        Result::InvalidArgument);

    Provider poseProvider;
    auto pose = Channel(ComponentKind::HeadPose, "head-pose", poseProvider);
    InitializeHeadPose(pose);
    static Registry poseRegistry;
    CHECK(poseRegistry.Register(pose) == Result::Ok);
    LiveChannelModelV1 poseModel;
    CHECK(poseRegistry.Describe("test.module", "live", "head-pose",
        poseModel) == Result::Ok && poseModel.headPose.axisCount == 3 &&
        poseModel.headPose.axes[1].view == HeadPoseView::Profile &&
        std::strcmp(poseModel.headPose.axes[2].maximumControlId,
            "roll-maximum") == 0 &&
        std::strcmp(poseModel.headPose.recenterControlId,
            "recenter-now") == 0 &&
        std::strcmp(poseModel.headPose.deadzoneControlId,
            "deadzone") == 0);
    auto invalidPose = pose;
    invalidPose.structSize = kLiveChannelDescriptorV1RadialResponseSize;
    CHECK(poseRegistry.Register(invalidPose) == Result::InvalidArgument);
    invalidPose = pose;
    Copy(invalidPose.headPose.axes[1].axisId,
        std::size(invalidPose.headPose.axes[1].axisId), "yaw");
    CHECK(poseRegistry.Register(invalidPose) == Result::InvalidArgument);
    invalidPose = pose;
    invalidPose.headPose.recenterControlId[0] = ' ';
    CHECK(poseRegistry.Register(invalidPose) == Result::InvalidArgument);
    invalidPose = pose;
    invalidPose.headPose.deadzoneControlId[0] = ' ';
    CHECK(poseRegistry.Register(invalidPose) == Result::InvalidArgument);
    poseProvider.frame = Frame(ComponentKind::HeadPose, 1, 100);
    poseProvider.frame.headPose.axisCount = 3;
    for (std::size_t index = 0; index < 3; ++index) {
        poseProvider.frame.headPose.axes[index].available = 1;
        poseProvider.frame.headPose.axes[index].trackerDegrees =
            static_cast<double>(index * 10);
        poseProvider.frame.headPose.axes[index].outputDegrees =
            static_cast<double>(index * -5);
    }
    CHECK(poseRegistry.SetVisiblePage("test.module", "live") == Result::Ok);
    poseRegistry.SetMenuActive(true);
    CHECK(poseRegistry.Poll("test.module", "live", "head-pose").result ==
        Result::Ok);
    poseProvider.frame.sequence = 2;
    poseProvider.frame.monotonicTimestampUs = 200;
    poseProvider.frame.headPose.axes[2].outputDegrees =
        std::numeric_limits<double>::quiet_NaN();
    CHECK(poseRegistry.Poll("test.module", "live", "head-pose").result ==
        Result::InvalidArgument);

    Provider gridProvider;
    auto grid = Channel(ComponentKind::SegmentedAllocationGrid, "power", gridProvider);
    InitializeGrid(grid);
    grid.flags = kSegmentedGridCycleOnClick;
    auto invalidFlagOwner = range;
    invalidFlagOwner.flags = kSegmentedGridCycleOnClick;
    CHECK(registry.Register(invalidFlagOwner) == Result::InvalidArgument);
    auto invalidGrid = grid;
    invalidGrid.segmentedGrid.columnCount = static_cast<std::uint32_t>(kMaximumGridColumns + 1);
    CHECK(registry.Register(invalidGrid) == Result::InvalidArgument);
    invalidGrid = grid;
    invalidGrid.segmentedGrid.columns[0].maximumSegments = static_cast<std::uint32_t>(kMaximumGridSegments + 1);
    CHECK(registry.Register(invalidGrid) == Result::InvalidArgument);
    grid.associationCount = 2;
    Copy(grid.associations[0].columnId,
        std::size(grid.associations[0].columnId), "engines");
    Copy(grid.associations[0].controlId,
        std::size(grid.associations[0].controlId), "order-engines");
    Copy(grid.associations[1].columnId,
        std::size(grid.associations[1].columnId), "shields");
    Copy(grid.associations[1].controlId,
        std::size(grid.associations[1].controlId), "order-shields");
    // The complete pre-radial descriptor ended here; its grid associations
    // remain valid after the radial descriptor tail was appended.
    grid.structSize = kLiveChannelDescriptorV1AssociationsSize;
    invalidGrid = grid;
    Copy(invalidGrid.associations[1].columnId,
        std::size(invalidGrid.associations[1].columnId), "missing");
    CHECK(registry.Register(invalidGrid) == Result::InvalidArgument);
    invalidGrid = grid;
    Copy(invalidGrid.associations[1].controlId,
        std::size(invalidGrid.associations[1].controlId), "order-engines");
    CHECK(registry.Register(invalidGrid) == Result::InvalidArgument);
    CHECK(registry.Register(grid) == Result::Ok);
    LiveChannelModelV1 gridModel;
    CHECK(registry.Describe("test.module", "live", "power", gridModel) ==
        Result::Ok && gridModel.flags == kSegmentedGridCycleOnClick &&
        gridModel.associationCount == 2 &&
        std::strcmp(gridModel.associations[0].controlId,
            "order-engines") == 0);

    // The pre-association v1 descriptor ended after flags. Its original grid
    // column stride remains accepted, flags survive, and bytes beyond the
    // declared size cannot create associations from former tail padding.
    Provider flagsOnlyProvider;
    auto flagsOnly = Channel(ComponentKind::SegmentedAllocationGrid,
        "power-flags-only", flagsOnlyProvider);
    InitializeGrid(flagsOnly);
    flagsOnly.flags = kSegmentedGridCycleOnClick;
    flagsOnly.associationCount = 2;
    flagsOnly.structSize = sizeof(LiveChannelDescriptorV1FlagsFixture);
    static Registry flagsOnlyRegistry;
    CHECK(flagsOnlyRegistry.Register(flagsOnly) == Result::Ok);
    LiveChannelModelV1 flagsOnlyModel;
    CHECK(flagsOnlyRegistry.Describe("test.module", "live",
        "power-flags-only", flagsOnlyModel) == Result::Ok &&
        flagsOnlyModel.flags == kSegmentedGridCycleOnClick &&
        flagsOnlyModel.associationCount == 0);

    gridProvider.frame = Frame(ComponentKind::SegmentedAllocationGrid, 1, 100);
    gridProvider.frame.segmentedGrid.columnCount = 2;
    gridProvider.frame.segmentedGrid.columns[0].segmentCount = 32;
    gridProvider.frame.segmentedGrid.columns[0].currentCount = 10;
    gridProvider.frame.segmentedGrid.columns[0].maximumCount = 32;
    gridProvider.frame.segmentedGrid.columns[0].targetCount = 12;
    gridProvider.frame.segmentedGrid.columns[1].segmentCount = 16;
    gridProvider.frame.segmentedGrid.columns[1].maximumCount = 16;
    CHECK(registry.Poll("test.module", "live", "power").result == Result::Ok);
    gridProvider.frame.sequence = 2;
    gridProvider.frame.monotonicTimestampUs = 200;
    gridProvider.frame.segmentedGrid.columns[0].segmentCount = 33;
    CHECK(registry.Poll("test.module", "live", "power").result == Result::InvalidArgument);

    gridProvider.replacement.revision = 1;
    gridProvider.replacement.segmentedGrid = gridProvider.frame.segmentedGrid;
    gridProvider.replacement.segmentedGrid.columns[0].segmentCount = 32;
    auto operation = Operation(CompoundOperationKind::SetSegmentCount);
    Copy(operation.columnId, std::size(operation.columnId), "engines");
    Copy(operation.tierId, std::size(operation.tierId), "green");
    operation.count = 33;
    auto compound = registry.Apply(operation);
    CHECK(compound.result == Result::InvalidArgument && gridProvider.operationCount == 0);
    operation.count = 20;
    compound = registry.Apply(operation);
    CHECK(compound.result == Result::Ok && compound.publish == 1 && gridProvider.operationCount == 1);

    operation = Operation(CompoundOperationKind::SetSegmentTier);
    Copy(operation.columnId, std::size(operation.columnId), "engines");
    Copy(operation.tierId, std::size(operation.tierId), "yellow");
    operation.count = 32;
    CHECK(registry.Apply(operation).result == Result::InvalidArgument &&
        gridProvider.operationCount == 1);

    operation = Operation(CompoundOperationKind::TrimColumn);
    Copy(operation.columnId, std::size(operation.columnId), "missing");
    CHECK(registry.Apply(operation).result == Result::InvalidArgument && gridProvider.operationCount == 1);
    operation = Operation(CompoundOperationKind::SetTier);
    Copy(operation.tierId, std::size(operation.tierId), "red");
    CHECK(registry.Apply(operation).result == Result::InvalidArgument && gridProvider.operationCount == 2);
    // The provider returned a stale compound revision; it is not published.
    CHECK(gridProvider.operationCount == 2);

    registry.SetMenuActive(false);
    operation = Operation(CompoundOperationKind::SetTier);
    Copy(operation.tierId, std::size(operation.tierId), "yellow");
    CHECK(registry.Apply(operation).result == Result::Suspended && gridProvider.operationCount == 2);
    CHECK(registry.RequestImmediateRefresh("test.module", "live", "axis") == Result::Ok);
    std::uint32_t refreshRequested{};
    CHECK(registry.TakeImmediateRefreshRequest("test.module", "live", "axis", refreshRequested) == Result::Ok &&
          refreshRequested == 1);
    CHECK(registry.TakeImmediateRefreshRequest("test.module", "live", "axis", refreshRequested) == Result::Ok &&
          refreshRequested == 0);
    CHECK(registry.RequestImmediateRefresh("test.module", "live", "missing") == Result::NotFound);
    CHECK(registry.ChannelCount() == 3);
    CHECK(registry.UnregisterModule("test.module") == Result::Ok && registry.ChannelCount() == 0);
    CHECK(registry.UnregisterModule("test.module") == Result::NotFound);
    return 0;
}
