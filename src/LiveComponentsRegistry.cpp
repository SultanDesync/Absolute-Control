#include "LiveComponentsRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ranges>
#include <span>
#include <string_view>

namespace AbsoluteControlPanelResearch::LiveComponents
{
    namespace
    {
        template <std::size_t N>
        [[nodiscard]] bool IsTerminated(const char (&value)[N]) noexcept
        {
            return std::memchr(value, '\0', N) != nullptr;
        }

        template <std::size_t N>
        [[nodiscard]] std::string_view View(const char (&value)[N]) noexcept
        {
            const auto* end = static_cast<const char*>(std::memchr(value, '\0', N));
            return end ? std::string_view{ value, static_cast<std::size_t>(end - value) } : std::string_view{};
        }

        [[nodiscard]] bool IsIdentifier(std::string_view value, bool allowEmpty = false) noexcept
        {
            if (value.empty()) return allowEmpty;
            return std::ranges::all_of(value, [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '.' ||
                       character == '_' || character == '-';
            });
        }

        template <std::size_t N>
        [[nodiscard]] bool IsIdentifier(const char (&value)[N], bool allowEmpty = false) noexcept
        {
            return IsTerminated(value) && IsIdentifier(View(value), allowEmpty);
        }

        template <std::size_t N>
        [[nodiscard]] bool IsLabel(const char (&value)[N], bool allowEmpty = false) noexcept
        {
            return IsTerminated(value) && (allowEmpty || !View(value).empty());
        }

        [[nodiscard]] bool IsComponentKind(ComponentKind kind) noexcept
        {
            return kind >= ComponentKind::RangeMeter &&
                   kind <= ComponentKind::HeadPose;
        }

        [[nodiscard]] bool IsHeadPoseView(HeadPoseView view) noexcept
        {
            return view >= HeadPoseView::Top &&
                   view <= HeadPoseView::ArtificialHorizon;
        }

        [[nodiscard]] bool IsBandSemantic(RangeBandSemantic semantic) noexcept
        {
            return semantic >= RangeBandSemantic::Custom && semantic <= RangeBandSemantic::Boost;
        }

        [[nodiscard]] bool IsMarkerSemantic(RangeMarkerSemantic semantic) noexcept
        {
            return semantic >= RangeMarkerSemantic::Custom && semantic <= RangeMarkerSemantic::Detent;
        }

        [[nodiscard]] bool IsVisualRole(VisualRole role) noexcept
        {
            return role >= VisualRole::Neutral && role <= VisualRole::Tier3;
        }

        template <class Value, class Id>
        [[nodiscard]] bool HasDuplicateId(const Value* values, std::size_t count, Id id) noexcept
        {
            for (std::size_t left = 0; left < count; ++left) {
                for (std::size_t right = left + 1; right < count; ++right) {
                    if (View(values[left].*id) == View(values[right].*id)) return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool ValidateBandsAndMarkers(
            const RangeBandV1* bands, std::size_t bandCount,
            const RangeMarkerV1* markers, std::size_t markerCount,
            double minimumValue, double maximumValue, bool enforceDomain) noexcept
        {
            for (std::size_t index = 0; index < bandCount; ++index) {
                const auto& band = bands[index];
                if (band.structSize < sizeof(RangeBandV1) || !IsBandSemantic(band.semantic) ||
                    !IsVisualRole(band.visualRole) ||
                    !std::isfinite(band.minimumValue) || !std::isfinite(band.maximumValue) ||
                    band.minimumValue > band.maximumValue || !IsLabel(band.label, true) ||
                    (enforceDomain && (band.minimumValue < minimumValue || band.maximumValue > maximumValue))) {
                    return false;
                }
            }
            for (std::size_t index = 0; index < markerCount; ++index) {
                const auto& marker = markers[index];
                if (marker.structSize < sizeof(RangeMarkerV1) || !IsMarkerSemantic(marker.semantic) ||
                    !IsVisualRole(marker.visualRole) ||
                    !std::isfinite(marker.value) || !IsIdentifier(marker.markerId) ||
                    !IsLabel(marker.label, true) || !IsIdentifier(marker.controlId, true) ||
                    (enforceDomain && (marker.value < minimumValue || marker.value > maximumValue))) {
                    return false;
                }
            }
            return !HasDuplicateId(markers, markerCount, &RangeMarkerV1::markerId);
        }

        [[nodiscard]] bool ValidateRange(const RangeMeterDescriptorV1& descriptor) noexcept
        {
            return descriptor.structSize >= sizeof(RangeMeterDescriptorV1) &&
                   std::isfinite(descriptor.minimumValue) && std::isfinite(descriptor.maximumValue) &&
                   descriptor.minimumValue < descriptor.maximumValue &&
                   descriptor.bandCount <= kMaximumRangeBands &&
                   descriptor.markerCount <= kMaximumRangeMarkers &&
                   IsTerminated(descriptor.valueFormat) &&
                   ValidateBandsAndMarkers(descriptor.bands, descriptor.bandCount,
                       descriptor.markers, descriptor.markerCount,
                       descriptor.minimumValue, descriptor.maximumValue, true);
        }

        [[nodiscard]] bool ValidatePlot(const TelemetryPlotDescriptorV1& descriptor) noexcept
        {
            if (descriptor.structSize < sizeof(TelemetryPlotDescriptorV1) ||
                descriptor.seriesCount == 0 || descriptor.seriesCount > kMaximumPlotSeries ||
                descriptor.historyCapacity == 0 || descriptor.historyCapacity > kMaximumPlotSamples ||
                descriptor.autoRange > 1 || !std::isfinite(descriptor.minimumValue) ||
                !std::isfinite(descriptor.maximumValue) || descriptor.minimumValue >= descriptor.maximumValue ||
                descriptor.bandCount > kMaximumRangeBands || descriptor.markerCount > kMaximumRangeMarkers) {
                return false;
            }
            for (std::size_t index = 0; index < descriptor.seriesCount; ++index) {
                const auto& series = descriptor.series[index];
                if (series.structSize < sizeof(PlotSeriesDescriptorV1) ||
                    !IsVisualRole(series.visualRole) ||
                    !IsIdentifier(series.seriesId) || !IsLabel(series.label)) return false;
            }
            return !HasDuplicateId(descriptor.series, descriptor.seriesCount, &PlotSeriesDescriptorV1::seriesId) &&
                   ValidateBandsAndMarkers(descriptor.bands, descriptor.bandCount,
                       descriptor.markers, descriptor.markerCount,
                       descriptor.minimumValue, descriptor.maximumValue, descriptor.autoRange == 0);
        }

        [[nodiscard]] bool ValidateGrid(const SegmentedGridDescriptorV1& descriptor) noexcept
        {
            if (descriptor.structSize < sizeof(SegmentedGridDescriptorV1) ||
                !IsIdentifier(descriptor.controlId) || descriptor.columnCount == 0 ||
                descriptor.columnCount > kMaximumGridColumns || descriptor.tierCount == 0 ||
                descriptor.tierCount > kMaximumGridTiers) return false;
            for (std::size_t index = 0; index < descriptor.columnCount; ++index) {
                const auto& column = descriptor.columns[index];
                if (column.structSize < sizeof(GridColumnDescriptorV1) ||
                    !IsIdentifier(column.columnId) || !IsLabel(column.label) ||
                    column.maximumSegments == 0 ||
                    column.maximumSegments > kMaximumGridSegments) return false;
            }
            for (std::size_t index = 0; index < descriptor.tierCount; ++index) {
                const auto& tier = descriptor.tiers[index];
                if (tier.structSize < sizeof(GridTierDescriptorV1) ||
                    !IsVisualRole(tier.visualRole) ||
                    !IsIdentifier(tier.tierId) || !IsLabel(tier.label)) return false;
            }
            return !HasDuplicateId(descriptor.columns, descriptor.columnCount, &GridColumnDescriptorV1::columnId) &&
                   !HasDuplicateId(descriptor.tiers, descriptor.tierCount, &GridTierDescriptorV1::tierId);
        }

        [[nodiscard]] bool ValidateRadial(
            const RadialResponseDescriptorV1& descriptor) noexcept
        {
            return descriptor.structSize >= sizeof(RadialResponseDescriptorV1) &&
                   std::isfinite(descriptor.maximumRadius) &&
                   descriptor.maximumRadius > 0.0 &&
                   IsIdentifier(descriptor.enabledControlId) &&
                   IsIdentifier(descriptor.radiusControlId) &&
                   IsIdentifier(descriptor.idleMillisecondsControlId) &&
                   IsIdentifier(descriptor.decayRateControlId) &&
                   IsIdentifier(descriptor.pollRateControlId);
        }

        [[nodiscard]] bool ValidateHeadPose(
            const HeadPoseDescriptorV1& descriptor) noexcept
        {
            if (descriptor.structSize < sizeof(HeadPoseDescriptorV1) ||
                descriptor.axisCount == 0 ||
                descriptor.axisCount > kMaximumHeadPoseAxes) return false;
            if (descriptor.recenterControlId[0] != '\0' &&
                !IsIdentifier(descriptor.recenterControlId)) return false;
            if (descriptor.deadzoneControlId[0] != '\0' &&
                !IsIdentifier(descriptor.deadzoneControlId)) return false;
            for (std::size_t index = 0; index < descriptor.axisCount; ++index) {
                const auto& axis = descriptor.axes[index];
                if (axis.structSize < sizeof(HeadPoseAxisDescriptorV1) ||
                    !IsHeadPoseView(axis.view) || !IsIdentifier(axis.axisId) ||
                    !IsLabel(axis.label) ||
                    !IsIdentifier(axis.sensitivityControlId) ||
                    !IsIdentifier(axis.minimumControlId) ||
                    !IsIdentifier(axis.centerControlId) ||
                    !IsIdentifier(axis.maximumControlId) ||
                    !IsIdentifier(axis.enabledControlId) ||
                    !IsIdentifier(axis.invertedControlId)) return false;
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (View(descriptor.axes[prior].axisId) ==
                        View(axis.axisId)) return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool ValidateAssociations(
            const LiveChannelDescriptorV1& descriptor) noexcept
        {
            if (descriptor.structSize <
                kLiveChannelDescriptorV1AssociationsSize) return true;
            if (descriptor.associationCount > kMaximumGridControlAssociations ||
                (descriptor.kind != ComponentKind::SegmentedAllocationGrid &&
                    descriptor.associationCount != 0)) return false;
            for (std::size_t index = 0; index < descriptor.associationCount; ++index) {
                const auto& association = descriptor.associations[index];
                if (association.structSize < sizeof(GridControlAssociationV1) ||
                    !IsIdentifier(association.columnId) ||
                    !IsIdentifier(association.controlId)) return false;
                const auto columnExists = std::ranges::any_of(
                    std::span{ descriptor.segmentedGrid.columns,
                        descriptor.segmentedGrid.columnCount },
                    [&](const GridColumnDescriptorV1& candidate) {
                        return View(candidate.columnId) ==
                            View(association.columnId);
                    });
                if (!columnExists) return false;
                for (std::size_t prior = 0; prior < index; ++prior) {
                    if (View(descriptor.associations[prior].columnId) ==
                            View(association.columnId) ||
                        View(descriptor.associations[prior].controlId) ==
                            View(association.controlId)) return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool ValidateDescriptor(const LiveChannelDescriptorV1& descriptor) noexcept
        {
            if (descriptor.structSize < kLiveChannelDescriptorV1BaseSize ||
                descriptor.abiVersion != kAbiVersion || !IsComponentKind(descriptor.kind) ||
                !IsIdentifier(descriptor.moduleId) || !IsIdentifier(descriptor.pageId) ||
                !IsIdentifier(descriptor.channelId) || !IsLabel(descriptor.title) ||
                !descriptor.readLiveFrame) return false;
            const auto flags = descriptor.structSize >=
                    kLiveChannelDescriptorV1FlagsSize ?
                descriptor.flags : kSegmentedGridNone;
            constexpr std::uint32_t presentationFlags =
                kLivePresentationPinned | kLivePresentationSecondary |
                kLivePresentationCollapsedByDefault;
            constexpr std::uint32_t knownFlags =
                kSegmentedGridCycleOnClick | presentationFlags;
            if ((flags & ~knownFlags) != 0 ||
                (descriptor.kind != ComponentKind::SegmentedAllocationGrid &&
                    (flags & kSegmentedGridCycleOnClick) != 0) ||
                ((flags & kLivePresentationCollapsedByDefault) != 0 &&
                    (flags & kLivePresentationSecondary) == 0)) return false;
            bool validComponent{};
            switch (descriptor.kind) {
            case ComponentKind::RangeMeter:
                validComponent = ValidateRange(descriptor.rangeMeter); break;
            case ComponentKind::TelemetryPlot:
                validComponent = ValidatePlot(descriptor.telemetryPlot); break;
            case ComponentKind::SegmentedAllocationGrid:
                validComponent = ValidateGrid(descriptor.segmentedGrid); break;
            case ComponentKind::RadialResponse:
                validComponent = descriptor.structSize >=
                        kLiveChannelDescriptorV1RadialResponseSize &&
                    ValidateRadial(descriptor.radialResponse); break;
            case ComponentKind::HeadPose:
                validComponent = descriptor.structSize >=
                        kLiveChannelDescriptorV1HeadPoseSize &&
                    ValidateHeadPose(descriptor.headPose); break;
            }
            return validComponent && ValidateAssociations(descriptor);
        }

        [[nodiscard]] LiveChannelModelV1 CopyModel(const LiveChannelDescriptorV1& descriptor) noexcept
        {
            LiveChannelModelV1 model;
            std::memcpy(model.moduleId, descriptor.moduleId, sizeof(model.moduleId));
            std::memcpy(model.pageId, descriptor.pageId, sizeof(model.pageId));
            std::memcpy(model.channelId, descriptor.channelId, sizeof(model.channelId));
            std::memcpy(model.title, descriptor.title, sizeof(model.title));
            model.kind = descriptor.kind;
            model.flags = descriptor.structSize >=
                    kLiveChannelDescriptorV1FlagsSize ?
                descriptor.flags : kSegmentedGridNone;
            if (descriptor.structSize >=
                kLiveChannelDescriptorV1AssociationsSize) {
                model.associationCount = descriptor.associationCount;
                std::copy_n(descriptor.associations,
                    descriptor.associationCount, model.associations);
            }
            switch (descriptor.kind) {
            case ComponentKind::RangeMeter: model.rangeMeter = descriptor.rangeMeter; break;
            case ComponentKind::TelemetryPlot: model.telemetryPlot = descriptor.telemetryPlot; break;
            case ComponentKind::SegmentedAllocationGrid: model.segmentedGrid = descriptor.segmentedGrid; break;
            case ComponentKind::RadialResponse: model.radialResponse = descriptor.radialResponse; break;
            case ComponentKind::HeadPose: model.headPose = descriptor.headPose; break;
            }
            return model;
        }

        [[nodiscard]] bool SameKey(const LiveChannelDescriptorV1& descriptor,
            std::string_view moduleId, std::string_view pageId, std::string_view channelId) noexcept
        {
            return View(descriptor.moduleId) == moduleId && View(descriptor.pageId) == pageId &&
                   View(descriptor.channelId) == channelId;
        }

        [[nodiscard]] bool ValidateGridFrame(const SegmentedGridFrameV1& frame,
            const SegmentedGridDescriptorV1& descriptor) noexcept
        {
            if (frame.structSize < sizeof(SegmentedGridFrameV1) || frame.columnCount != descriptor.columnCount) return false;
            for (std::size_t columnIndex = 0; columnIndex < frame.columnCount; ++columnIndex) {
                const auto& column = frame.columns[columnIndex];
                const auto maximum = descriptor.columns[columnIndex].maximumSegments;
                if (column.structSize < sizeof(GridColumnFrameV1) || column.segmentCount > maximum ||
                    column.currentCount > maximum || column.maximumCount > maximum || column.targetCount > maximum) return false;
                for (std::size_t segmentIndex = 0; segmentIndex < column.segmentCount; ++segmentIndex) {
                    const auto& segment = column.segments[segmentIndex];
                    if (segment.tierIndex >= descriptor.tierCount || segment.live > 1 ||
                        segment.preview > 1 || segment.interactive > 1) return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool ValidateFrame(const LiveFrameV1& frame,
            const LiveChannelDescriptorV1& descriptor) noexcept
        {
            if (frame.structSize < kLiveFrameV1BaseSize || frame.abiVersion != kAbiVersion ||
                frame.kind != descriptor.kind || frame.sequence == 0 || frame.monotonicTimestampUs == 0 ||
                (frame.flags & ~(kFrameStale | kFrameUnavailable | kFrameSuspended)) != 0) return false;
            switch (frame.kind) {
            case ComponentKind::RangeMeter: {
                const bool validBase =
                    frame.rangeMeter.structSize >= sizeof(RangeMeterFrameV1) &&
                    frame.rangeMeter.available <= 1 &&
                       std::isfinite(frame.rangeMeter.liveValue) &&
                       (!frame.rangeMeter.available || (frame.rangeMeter.liveValue >= descriptor.rangeMeter.minimumValue &&
                           frame.rangeMeter.liveValue <= descriptor.rangeMeter.maximumValue));
                if (!validBase || frame.structSize <
                    kLiveFrameV1DynamicRangeSize) {
                    return validBase;
                }
                const auto& dynamic = frame.dynamicRange;
                return dynamic.structSize >= sizeof(LiveFrameV1::DynamicRangeV1) &&
                    dynamic.present <= 1 &&
                    (!dynamic.present ||
                        (dynamic.bandCount <= kMaximumRangeBands &&
                         dynamic.markerCount <= kMaximumRangeMarkers &&
                         ValidateBandsAndMarkers(dynamic.bands,
                             dynamic.bandCount, dynamic.markers,
                             dynamic.markerCount,
                             descriptor.rangeMeter.minimumValue,
                             descriptor.rangeMeter.maximumValue, true)));
            }
            case ComponentKind::TelemetryPlot:
                if (frame.telemetryPlot.structSize < sizeof(TelemetrySampleV1) ||
                    frame.telemetryPlot.seriesCount != descriptor.telemetryPlot.seriesCount ||
                    (frame.telemetryPlot.availableMask >> frame.telemetryPlot.seriesCount) != 0) return false;
                for (std::size_t index = 0; index < frame.telemetryPlot.seriesCount; ++index) {
                    if (!std::isfinite(frame.telemetryPlot.values[index])) return false;
                }
                return true;
            case ComponentKind::SegmentedAllocationGrid:
                return ValidateGridFrame(frame.segmentedGrid, descriptor.segmentedGrid);
            case ComponentKind::RadialResponse:
                return frame.structSize >= kLiveFrameV1RadialResponseSize &&
                    frame.radialResponse.structSize >=
                        sizeof(RadialResponseFrameV1) &&
                    frame.radialResponse.available <= 1 &&
                    std::isfinite(frame.radialResponse.liveX) &&
                    std::isfinite(frame.radialResponse.liveY) &&
                    (!frame.radialResponse.available ||
                        std::hypot(frame.radialResponse.liveX,
                            frame.radialResponse.liveY) <=
                            descriptor.radialResponse.maximumRadius);
            case ComponentKind::HeadPose:
                if (frame.structSize < kLiveFrameV1HeadPoseSize ||
                    frame.headPose.structSize < sizeof(HeadPoseFrameV1) ||
                    frame.headPose.axisCount != descriptor.headPose.axisCount) {
                    return false;
                }
                for (std::size_t index = 0;
                     index < frame.headPose.axisCount; ++index) {
                    const auto& axis = frame.headPose.axes[index];
                    if (axis.structSize < sizeof(HeadPoseAxisFrameV1) ||
                        axis.available > 1 ||
                        !std::isfinite(axis.trackerDegrees) ||
                        !std::isfinite(axis.outputDegrees)) return false;
                }
                return true;
            }
            return false;
        }

        [[nodiscard]] bool ValidKeyArguments(const char* moduleId, const char* pageId, const char* channelId) noexcept
        {
            if (!moduleId || !pageId || !channelId) return false;
            return IsIdentifier(std::string_view{ moduleId }) && IsIdentifier(std::string_view{ pageId }) &&
                   IsIdentifier(std::string_view{ channelId });
        }

        [[nodiscard]] bool ContainsId(const GridTierDescriptorV1* tiers, std::size_t count,
            std::string_view value) noexcept
        {
            return std::ranges::any_of(std::span{ tiers, count }, [&](const auto& tier) {
                return View(tier.tierId) == value;
            });
        }

        [[nodiscard]] const GridColumnDescriptorV1* FindColumn(
            const SegmentedGridDescriptorV1& descriptor, std::string_view columnId) noexcept
        {
            for (std::size_t index = 0; index < descriptor.columnCount; ++index) {
                if (View(descriptor.columns[index].columnId) == columnId) return &descriptor.columns[index];
            }
            return nullptr;
        }

        [[nodiscard]] bool ValidateOperation(const CompoundOperationV1& operation,
            const SegmentedGridDescriptorV1& descriptor) noexcept
        {
            if (operation.structSize < sizeof(CompoundOperationV1) || operation.abiVersion != kAbiVersion ||
                !IsIdentifier(operation.moduleId) || !IsIdentifier(operation.pageId) ||
                !IsIdentifier(operation.channelId) || !IsIdentifier(operation.controlId) ||
                !IsIdentifier(operation.columnId, true) || !IsIdentifier(operation.tierId, true) ||
                View(operation.controlId) != View(descriptor.controlId)) return false;
            const auto columnId = View(operation.columnId);
            const auto tierId = View(operation.tierId);
            switch (operation.kind) {
            case CompoundOperationKind::SetSegmentCount: {
                const auto* column = FindColumn(descriptor, columnId);
                return column && ContainsId(descriptor.tiers, descriptor.tierCount, tierId) &&
                       operation.count <= column->maximumSegments;
            }
            case CompoundOperationKind::TrimColumn: {
                const auto* column = FindColumn(descriptor, columnId);
                return column && tierId.empty() && operation.count <= column->maximumSegments;
            }
            case CompoundOperationKind::SetTier:
                return columnId.empty() && operation.count == 0 &&
                        ContainsId(descriptor.tiers, descriptor.tierCount, tierId);
            case CompoundOperationKind::SetSegmentTier: {
                const auto* column = FindColumn(descriptor, columnId);
                return column && operation.count < column->maximumSegments &&
                       ContainsId(descriptor.tiers, descriptor.tierCount, tierId);
            }
            }
            return false;
        }

        [[nodiscard]] Result __cdecl RegisterChannel(const LiveChannelDescriptorV1* descriptor) noexcept
        {
            return descriptor ? HostRegistry().Register(*descriptor) : Result::InvalidArgument;
        }

        [[nodiscard]] Result __cdecl UnregisterModuleApi(const char* moduleId) noexcept
        {
            return HostRegistry().UnregisterModule(moduleId);
        }

        [[nodiscard]] Result __cdecl RequestImmediateRefreshApi(
            const char* moduleId, const char* pageId, const char* channelId) noexcept
        {
            return HostRegistry().RequestImmediateRefresh(moduleId, pageId, channelId);
        }
    }

    Result Registry::Register(const LiveChannelDescriptorV1& descriptor) noexcept
    {
        if (!ValidateDescriptor(descriptor)) return Result::InvalidArgument;
        const auto duplicate = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, View(descriptor.moduleId),
                View(descriptor.pageId), View(descriptor.channelId));
        });
        if (duplicate != slots_.end()) return Result::Duplicate;
        const auto empty = std::ranges::find_if(slots_, [](const Slot& slot) { return !slot.occupied; });
        if (empty == slots_.end()) return Result::CapacityExceeded;
        LiveChannelDescriptorV1 normalized{};
        std::memcpy(&normalized, &descriptor,
            (std::min)(static_cast<std::size_t>(descriptor.structSize),
                sizeof(normalized)));
        if (descriptor.structSize < kLiveChannelDescriptorV1FlagsSize) {
            normalized.flags = kSegmentedGridNone;
        }
        if (descriptor.structSize <
            kLiveChannelDescriptorV1AssociationsSize) {
            normalized.associationCount = 0;
            std::fill(std::begin(normalized.associations),
                std::end(normalized.associations), GridControlAssociationV1{});
        }
        normalized.structSize = sizeof(normalized);
        *empty = Slot{};
        empty->occupied = true;
        empty->descriptor = normalized;
        empty->model = CopyModel(normalized);
        return Result::Ok;
    }

    Result Registry::UnregisterModule(const char* moduleId) noexcept
    {
        if (!moduleId || !IsIdentifier(std::string_view{ moduleId })) return Result::InvalidArgument;
        bool erased{};
        for (auto& slot : slots_) {
            if (slot.occupied && View(slot.descriptor.moduleId) == moduleId) {
                slot = Slot{};
                erased = true;
            }
        }
        return erased ? Result::Ok : Result::NotFound;
    }

    Result Registry::RequestImmediateRefresh(
        const char* moduleId, const char* pageId, const char* channelId) noexcept
    {
        if (!ValidKeyArguments(moduleId, pageId, channelId)) return Result::InvalidArgument;
        const auto found = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, moduleId, pageId, channelId);
        });
        if (found == slots_.end()) return Result::NotFound;
        found->refreshRequested = true;
        return Result::Ok;
    }

    Result Registry::TakeImmediateRefreshRequest(
        const char* moduleId, const char* pageId, const char* channelId,
        std::uint32_t& requested) noexcept
    {
        requested = 0;
        if (!ValidKeyArguments(moduleId, pageId, channelId)) return Result::InvalidArgument;
        const auto found = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, moduleId, pageId, channelId);
        });
        if (found == slots_.end()) return Result::NotFound;
        requested = found->refreshRequested ? 1U : 0U;
        found->refreshRequested = false;
        return Result::Ok;
    }

    void Registry::SetMenuActive(bool active) noexcept { menuActive_ = active; }

    Result Registry::SetVisiblePage(const char* moduleId, const char* pageId) noexcept
    {
        if (!moduleId || !pageId || !IsIdentifier(std::string_view{ moduleId }) ||
            !IsIdentifier(std::string_view{ pageId }) || strlen(moduleId) >= kIdentifierCapacity ||
            strlen(pageId) >= kIdentifierCapacity) return Result::InvalidArgument;
        strcpy_s(visibleModuleId_, moduleId);
        strcpy_s(visiblePageId_, pageId);
        return Result::Ok;
    }

    PollPublication Registry::Poll(const char* moduleId, const char* pageId, const char* channelId) noexcept
    {
        PollPublication publication;
        if (!ValidKeyArguments(moduleId, pageId, channelId)) {
            publication.result = Result::InvalidArgument;
            return publication;
        }
        const auto found = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, moduleId, pageId, channelId);
        });
        if (found == slots_.end()) {
            publication.result = Result::NotFound;
            return publication;
        }
        publication.channel = found->model;
        if (!menuActive_ || std::string_view{ visibleModuleId_ } != moduleId ||
            std::string_view{ visiblePageId_ } != pageId) {
            publication.result = Result::Suspended;
            if (found->hasFrame) {
                publication.publish = 1;
                publication.frame = found->latestFrame;
                publication.frame.flags |= kFrameSuspended | kFrameStale;
            }
            return publication;
        }

        LiveFrameV1 candidate;
        const auto callbackResult = found->descriptor.readLiveFrame(found->descriptor.context, &candidate);
        if (callbackResult != Result::Ok) {
            publication.result = callbackResult;
            if (found->hasFrame) {
                publication.publish = 1;
                publication.frame = found->latestFrame;
                publication.frame.flags |= kFrameStale;
            }
            return publication;
        }
        if (!ValidateFrame(candidate, found->descriptor)) {
            publication.result = Result::InvalidArgument;
            return publication;
        }

        const bool didNotAdvance = found->hasFrame &&
            (candidate.sequence <= found->lastSequence || candidate.monotonicTimestampUs <= found->lastTimestampUs);
        const bool stale = didNotAdvance || (candidate.flags & kFrameStale) != 0;
        if (!candidate.rangeMeter.available && candidate.kind == ComponentKind::RangeMeter)
            candidate.flags |= kFrameUnavailable;
        if (stale) candidate.flags |= kFrameStale;

        found->latestFrame = candidate;
        found->hasFrame = true;
        found->refreshRequested = false;
        publication.publish = 1;
        publication.frame = candidate;
        publication.result = stale ? Result::Stale : Result::Ok;
        if (!stale) {
            found->lastSequence = candidate.sequence;
            found->lastTimestampUs = candidate.monotonicTimestampUs;
            if (candidate.kind == ComponentKind::TelemetryPlot) {
                const auto capacity = found->descriptor.telemetryPlot.historyCapacity;
                auto& history = found->telemetryHistory;
                history.samples[history.writeIndex] = candidate.telemetryPlot;
                history.writeIndex = (history.writeIndex + 1) % capacity;
                history.count = std::min<std::uint32_t>(history.count + 1, capacity);
            }
        }
        return publication;
    }

    CompoundPublication Registry::Apply(const CompoundOperationV1& operation) noexcept
    {
        CompoundPublication publication;
        if (operation.structSize < sizeof(CompoundOperationV1) || operation.abiVersion != kAbiVersion ||
            !IsIdentifier(operation.moduleId) || !IsIdentifier(operation.pageId) ||
            !IsIdentifier(operation.channelId)) {
            publication.result = Result::InvalidArgument;
            return publication;
        }
        const auto found = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, View(operation.moduleId),
                View(operation.pageId), View(operation.channelId));
        });
        if (found == slots_.end()) {
            publication.result = Result::NotFound;
            return publication;
        }
        publication.channel = found->model;
        if (!menuActive_ || View(operation.moduleId) != std::string_view{ visibleModuleId_ } ||
            View(operation.pageId) != std::string_view{ visiblePageId_ }) {
            publication.result = Result::Suspended;
            return publication;
        }
        if (found->descriptor.kind != ComponentKind::SegmentedAllocationGrid ||
            !found->descriptor.applyCompoundOperation ||
            !ValidateOperation(operation, found->descriptor.segmentedGrid)) {
            publication.result = Result::InvalidArgument;
            return publication;
        }
        CompoundSnapshotV1 replacement;
        const auto callbackResult = found->descriptor.applyCompoundOperation(
            found->descriptor.context, &operation, &replacement);
        if (callbackResult != Result::Ok) {
            publication.result = callbackResult;
            return publication;
        }
        if (replacement.structSize < sizeof(CompoundSnapshotV1) || replacement.revision == 0 ||
            replacement.revision <= found->compoundRevision ||
            !ValidateGridFrame(replacement.segmentedGrid, found->descriptor.segmentedGrid)) {
            publication.result = Result::InvalidArgument;
            return publication;
        }
        found->compoundRevision = replacement.revision;
        publication.snapshot = replacement;
        publication.publish = 1;
        publication.result = Result::Ok;
        return publication;
    }

    Result Registry::Describe(const char* moduleId, const char* pageId, const char* channelId,
        LiveChannelModelV1& model) const noexcept
    {
        if (!ValidKeyArguments(moduleId, pageId, channelId)) return Result::InvalidArgument;
        const auto found = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, moduleId, pageId, channelId);
        });
        if (found == slots_.end()) return Result::NotFound;
        model = found->model;
        return Result::Ok;
    }

    Result Registry::TelemetryHistory(const char* moduleId, const char* pageId, const char* channelId,
        TelemetryHistoryV1& history) const noexcept
    {
        if (!ValidKeyArguments(moduleId, pageId, channelId)) return Result::InvalidArgument;
        const auto found = std::ranges::find_if(slots_, [&](const Slot& slot) {
            return slot.occupied && SameKey(slot.descriptor, moduleId, pageId, channelId);
        });
        if (found == slots_.end()) return Result::NotFound;
        if (found->descriptor.kind != ComponentKind::TelemetryPlot) return Result::InvalidArgument;
        history = found->telemetryHistory;
        return Result::Ok;
    }

    std::size_t Registry::ChannelCount() const noexcept
    {
        return static_cast<std::size_t>(std::ranges::count_if(slots_, [](const Slot& slot) {
            return slot.occupied;
        }));
    }

    std::vector<SubscriberDiagnostics> Registry::Diagnostics() const
    {
        std::vector<SubscriberDiagnostics> subscribers;
        for (const auto& slot : slots_) {
            if (!slot.occupied) continue;
            const std::string_view moduleId{ slot.descriptor.moduleId };
            const auto found = std::ranges::find_if(subscribers,
                [&](const SubscriberDiagnostics& subscriber) {
                    return subscriber.moduleId == moduleId;
                });
            if (found == subscribers.end()) {
                subscribers.push_back({ std::string{ moduleId }, 1 });
            } else {
                ++found->channelCount;
            }
        }
        return subscribers;
    }

    bool Registry::PollVisiblePage() noexcept
    {
        if (!menuActive_ || visibleModuleId_[0] == '\0' ||
            visiblePageId_[0] == '\0') return false;
        bool advanced{};
        // Poll performs the descriptor lookup again, but the channel cap keeps
        // this bounded and avoids exposing provider callbacks to MenuSession.
        for (const auto& slot : slots_) {
            if (!slot.occupied ||
                View(slot.descriptor.moduleId) != visibleModuleId_ ||
                View(slot.descriptor.pageId) != visiblePageId_) continue;
            const auto publication = Poll(
                slot.descriptor.moduleId, slot.descriptor.pageId,
                slot.descriptor.channelId);
            advanced = advanced ||
                (publication.publish != 0 && publication.result == Result::Ok);
        }
        return advanced;
    }

    std::vector<PollPublication> Registry::SnapshotPage(
        const char* moduleId, const char* pageId) const
    {
        std::vector<PollPublication> result;
        if (!moduleId || !pageId || !IsIdentifier(std::string_view{ moduleId }) ||
            !IsIdentifier(std::string_view{ pageId })) return result;
        result.reserve(kMaximumChannels);
        for (const auto& slot : slots_) {
            if (!slot.occupied || View(slot.descriptor.moduleId) != moduleId ||
                View(slot.descriptor.pageId) != pageId) continue;
            PollPublication publication;
            publication.channel = slot.model;
            publication.publish = slot.hasFrame ? 1U : 0U;
            publication.result = slot.hasFrame ? Result::Ok : Result::NotReady;
            if (slot.hasFrame) publication.frame = slot.latestFrame;
            result.push_back(publication);
        }
        return result;
    }

    bool Registry::HasPageChannels(
        const char* moduleId, const char* pageId) const noexcept
    {
        if (!moduleId || !pageId || !IsIdentifier(std::string_view{ moduleId }) ||
            !IsIdentifier(std::string_view{ pageId })) return false;
        return std::ranges::any_of(slots_, [&](const Slot& slot) {
            return slot.occupied && View(slot.descriptor.moduleId) == moduleId &&
                   View(slot.descriptor.pageId) == pageId;
        });
    }

    Registry& HostRegistry() noexcept
    {
        static Registry registry;
        return registry;
    }

    const ExperimentalApiV1 g_experimentalApi{
        sizeof(ExperimentalApiV1),
        kAbiVersion,
        &RegisterChannel,
        &UnregisterModuleApi,
        &RequestImmediateRefreshApi,
        kLiveCapabilities
    };
}

extern "C" ABSOLUTE_CONTROL_PANEL_EXPERIMENTAL_API
const AbsoluteControlPanelExperimental::ExperimentalApiV1*
AbsoluteControlPanel_QueryLiveComponentsExperimental(std::uint32_t requestedAbiVersion) noexcept
{
    if (requestedAbiVersion != AbsoluteControlPanelExperimental::kAbiVersion) return nullptr;
    return &AbsoluteControlPanelResearch::LiveComponents::g_experimentalApi;
}
