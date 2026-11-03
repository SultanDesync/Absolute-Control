#include "CompositionRegistry.h"

#include "LiveComponentsRegistry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_set>
#include <utility>

namespace AbsoluteControlPanelResearch::Composition
{
    struct ProviderState
    {
        std::mutex mutex;
        std::size_t inFlight{};
        bool retired{};
        void* context{};
        Api::ReadNodeStatesCallback readNodeStates{};
    };

    namespace
    {
        constexpr std::uint32_t kKnownNodeFlags =
            Api::kNodeCompact | Api::kNodeEmphasized | Api::kNodeAdvanced |
            Api::kNodeCollapsible | Api::kNodeCollapsedByDefault;
        constexpr std::uint32_t kKnownStateFlags =
            Api::kNodeStateVisible | Api::kNodeStateEnabled |
            Api::kNodeStateRequired | Api::kNodeStateInherited |
            Api::kNodeStateOverridden | Api::kNodeStateStale;
        constexpr std::uint32_t kKnownAssociationFlags =
            Api::kAssociationOptional | Api::kAssociationDirectManipulation;

        class CallbackLease final
        {
        public:
            explicit CallbackLease(std::shared_ptr<ProviderState> provider) noexcept :
                provider_(std::move(provider))
            {
                if (!provider_) return;
                std::scoped_lock lock{ provider_->mutex };
                if (provider_->retired) {
                    provider_.reset();
                    return;
                }
                ++provider_->inFlight;
            }

            ~CallbackLease()
            {
                if (!provider_) return;
                std::scoped_lock lock{ provider_->mutex };
                --provider_->inFlight;
            }

            CallbackLease(const CallbackLease&) = delete;
            CallbackLease& operator=(const CallbackLease&) = delete;

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return provider_ != nullptr;
            }

        private:
            std::shared_ptr<ProviderState> provider_;
        };

        template <std::size_t N>
        [[nodiscard]] bool IsTerminated(const char (&value)[N]) noexcept
        {
            return std::memchr(value, '\0', N) != nullptr;
        }

        [[nodiscard]] bool IsIdentifier(std::string_view value,
                                        bool allowEmpty = false) noexcept
        {
            if (value.empty()) return allowEmpty;
            return std::ranges::all_of(value, [](unsigned char character) {
                return std::isalnum(character) != 0 || character == '.' ||
                       character == '_' || character == '-';
            });
        }

        [[nodiscard]] bool IsContainer(Api::NodeKind kind) noexcept
        {
            switch (kind) {
            case Api::NodeKind::Root:
            case Api::NodeKind::Section:
            case Api::NodeKind::Card:
            case Api::NodeKind::Row:
            case Api::NodeKind::Columns:
            case Api::NodeKind::AnchorSet:
            case Api::NodeKind::PinnedContext:
            case Api::NodeKind::RecordView:
                return true;
            case Api::NodeKind::Anchor:
            case Api::NodeKind::WorkflowView:
            case Api::NodeKind::LiveSlot:
            case Api::NodeKind::ControlSlot:
                return false;
            }
            return false;
        }

        [[nodiscard]] bool ValidKind(Api::NodeKind kind) noexcept
        {
            return kind >= Api::NodeKind::Root &&
                   kind <= Api::NodeKind::ControlSlot;
        }

        [[nodiscard]] bool ValidRole(Api::SemanticRole role) noexcept
        {
            return role >= Api::SemanticRole::Default &&
                   role <= Api::SemanticRole::Danger;
        }

        [[nodiscard]] bool ValidSeverity(Api::StatusSeverity severity) noexcept
        {
            return severity >= Api::StatusSeverity::Normal &&
                   severity <= Api::StatusSeverity::Unavailable;
        }

        [[nodiscard]] bool ValidAssociationKind(
            Api::AssociationKind kind) noexcept
        {
            return kind >= Api::AssociationKind::ControlEditsLiveMarker &&
                   kind <= Api::AssociationKind::LiveSeriesExplainedByControl;
        }

        [[nodiscard]] const MenuApiHost::Control* FindControl(
            const MenuApiHost::Page& page, std::string_view controlId) noexcept
        {
            const auto found = std::ranges::find_if(page.controls,
                [&](const MenuApiHost::Control& control) {
                    return control.controlId == controlId;
                });
            return found == page.controls.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool ValidNodeShape(
            const Api::NodeDescriptorV1& source,
            const MenuApiHost::Page& page) noexcept
        {
            if (source.structSize < sizeof(Api::NodeDescriptorV1) ||
                !IsTerminated(source.nodeId) ||
                !IsTerminated(source.parentNodeId) ||
                !IsTerminated(source.referenceId) ||
                !IsTerminated(source.label) ||
                !IsTerminated(source.description) ||
                !IsIdentifier(source.nodeId) ||
                !IsIdentifier(source.parentNodeId, true) ||
                !IsIdentifier(source.referenceId, true) ||
                !ValidKind(source.kind) || !ValidRole(source.role) ||
                (source.flags & ~kKnownNodeFlags) != 0 ||
                ((source.flags & Api::kNodeCollapsedByDefault) != 0 &&
                 (source.flags & Api::kNodeCollapsible) == 0)) {
                return false;
            }

            const std::string_view reference{ source.referenceId };
            switch (source.kind) {
            case Api::NodeKind::Root:
                return source.parentNodeId[0] == '\0' && reference.empty() &&
                       source.auxiliaryValue == 0;
            case Api::NodeKind::Section:
            case Api::NodeKind::Card:
                return source.label[0] != '\0' && reference.empty() &&
                       source.auxiliaryValue == 0;
            case Api::NodeKind::Row:
            case Api::NodeKind::AnchorSet:
            case Api::NodeKind::PinnedContext:
                return reference.empty() && source.auxiliaryValue == 0;
            case Api::NodeKind::Columns:
                return reference.empty() && source.auxiliaryValue >= 1 &&
                       source.auxiliaryValue <= Api::kMaximumColumns;
            case Api::NodeKind::Anchor:
                return source.label[0] != '\0' && !reference.empty() &&
                       source.auxiliaryValue == 0;
            case Api::NodeKind::RecordView: {
                const auto* control = FindControl(page, reference);
                return control && control->kind ==
                    AbsoluteControlPanelApi::ControlKind::RecordCollection &&
                    source.auxiliaryValue <= static_cast<std::uint32_t>(
                        Api::RecordPresentation::DirectionPad);
            }
            case Api::NodeKind::WorkflowView:
                return !reference.empty() && source.auxiliaryValue == 0;
            case Api::NodeKind::LiveSlot: {
                if (reference.empty() || source.auxiliaryValue != 0) return false;
                AbsoluteControlPanelExperimental::LiveChannelModelV1 ignored{};
                return LiveComponents::HostRegistry().Describe(
                    page.moduleId.c_str(), page.pageId.c_str(),
                    source.referenceId, ignored) ==
                    AbsoluteControlPanelExperimental::Result::Ok;
            }
            case Api::NodeKind::ControlSlot: {
                const auto* control = FindControl(page, reference);
                return control && control->kind !=
                    AbsoluteControlPanelApi::ControlKind::GroupHeader &&
                    source.auxiliaryValue == 0;
            }
            }
            return false;
        }

        [[nodiscard]] Node CopyNode(const Api::NodeDescriptorV1& source)
        {
            Node node;
            node.kind = source.kind;
            node.flags = source.flags;
            node.role = source.role;
            node.nodeId = source.nodeId;
            node.parentNodeId = source.parentNodeId;
            node.referenceId = source.referenceId;
            node.label = source.label;
            node.description = source.description;
            node.auxiliaryValue = source.auxiliaryValue;
            return node;
        }

        [[nodiscard]] const Node* FindModelNode(const PageModel& model,
                                                std::string_view nodeId) noexcept
        {
            const auto found = std::ranges::find_if(model.nodes,
                [&](const Node& node) { return node.nodeId == nodeId; });
            return found == model.nodes.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool ValidAssociation(
            const Api::AssociationDescriptorV1& source,
            const MenuApiHost::Page& page, const PageModel& model) noexcept
        {
            if (source.structSize < sizeof(Api::AssociationDescriptorV1) ||
                !IsTerminated(source.associationId) ||
                !IsTerminated(source.sourceId) ||
                !IsTerminated(source.targetNodeId) ||
                !IsTerminated(source.semanticId) ||
                !IsIdentifier(source.associationId) ||
                !IsIdentifier(source.sourceId) ||
                !IsIdentifier(source.targetNodeId) ||
                !IsIdentifier(source.semanticId, true) ||
                !ValidAssociationKind(source.kind) ||
                (source.flags & ~kKnownAssociationFlags) != 0 ||
                ((source.flags & Api::kAssociationDirectManipulation) != 0 &&
                 source.kind != Api::AssociationKind::ControlEditsLiveMarker)) {
                return false;
            }
            const auto* control = FindControl(page, source.sourceId);
            const auto* target = FindModelNode(model, source.targetNodeId);
            if (!control || !target || target->kind == Api::NodeKind::Root) {
                return false;
            }
            const std::string_view semanticId{ source.semanticId };
            using ControlKind = AbsoluteControlPanelApi::ControlKind;
            const auto liveModel = [&]()
                -> std::optional<
                    AbsoluteControlPanelExperimental::LiveChannelModelV1> {
                if (target->kind != Api::NodeKind::LiveSlot) return std::nullopt;
                AbsoluteControlPanelExperimental::LiveChannelModelV1 model{};
                if (LiveComponents::HostRegistry().Describe(
                        page.moduleId.c_str(), page.pageId.c_str(),
                        target->referenceId.c_str(), model) !=
                    AbsoluteControlPanelExperimental::Result::Ok) {
                    return std::nullopt;
                }
                return model;
            };
            const auto hasMarker = [&](std::string_view markerId) {
                const auto model = liveModel();
                if (!model) return false;
                if (model->kind == AbsoluteControlPanelExperimental::
                        ComponentKind::RangeMeter) {
                    return std::ranges::any_of(
                        std::span{model->rangeMeter.markers,
                            model->rangeMeter.markerCount},
                        [&](const auto& marker) {
                            return std::string_view{marker.markerId} == markerId;
                        });
                }
                if (model->kind == AbsoluteControlPanelExperimental::
                        ComponentKind::TelemetryPlot) {
                    return std::ranges::any_of(
                        std::span{model->telemetryPlot.markers,
                            model->telemetryPlot.markerCount},
                        [&](const auto& marker) {
                            return std::string_view{marker.markerId} == markerId;
                        });
                }
                return false;
            };
            const auto hasSeries = [&](std::string_view seriesId) {
                const auto model = liveModel();
                return model && model->kind ==
                    AbsoluteControlPanelExperimental::ComponentKind::TelemetryPlot &&
                    std::ranges::any_of(
                        std::span{model->telemetryPlot.series,
                            model->telemetryPlot.seriesCount},
                        [&](const auto& series) {
                            return std::string_view{series.seriesId} == seriesId;
                        });
            };
            switch (source.kind) {
            case Api::AssociationKind::ControlEditsLiveMarker:
                return (control->kind == ControlKind::IntegerSlider ||
                        control->kind == ControlKind::FloatSlider) &&
                       target->kind == Api::NodeKind::LiveSlot &&
                       !semanticId.empty() && hasMarker(semanticId);
            case Api::AssociationKind::ActionCapturesLiveMarker:
                return control->kind == ControlKind::Action &&
                       target->kind == Api::NodeKind::LiveSlot &&
                       !semanticId.empty() && hasMarker(semanticId);
            case Api::AssociationKind::StatusExplainsNode:
                return (control->flags &
                    AbsoluteControlPanelApi::kControlReadOnly) != 0 &&
                    semanticId.empty();
            case Api::AssociationKind::ControlSummarizedByNode:
                return semanticId.empty();
            case Api::AssociationKind::RecordSelectionDrivesNode:
                return control->kind == ControlKind::RecordCollection &&
                       semanticId.empty();
            case Api::AssociationKind::TableColumnUsesControl:
                return target->kind == Api::NodeKind::RecordView &&
                       !semanticId.empty();
            case Api::AssociationKind::LiveSeriesExplainedByControl:
                return target->kind == Api::NodeKind::LiveSlot &&
                       !semanticId.empty() && hasSeries(semanticId);
            }
            return false;
        }

        [[nodiscard]] Association CopyAssociation(
            const Api::AssociationDescriptorV1& source)
        {
            Association association;
            association.kind = source.kind;
            association.flags = source.flags;
            association.associationId = source.associationId;
            association.sourceId = source.sourceId;
            association.targetNodeId = source.targetNodeId;
            association.semanticId = source.semanticId;
            return association;
        }

        [[nodiscard]] bool IsDefaultStateTarget(const Node& node) noexcept
        {
            return node.kind != Api::NodeKind::Root;
        }

        [[nodiscard]] bool ValidState(const Api::NodeStateV1& source,
                                      const PageModel& model,
                                      std::unordered_set<std::string>& seen) noexcept
        {
            if (source.structSize < sizeof(Api::NodeStateV1) ||
                !IsTerminated(source.nodeId) ||
                !IsTerminated(source.value) ||
                !IsTerminated(source.detail) ||
                !IsTerminated(source.sourceLabel) ||
                !IsIdentifier(source.nodeId) ||
                !ValidSeverity(source.severity) ||
                (source.flags & ~kKnownStateFlags) != 0 ||
                ((source.flags & Api::kNodeStateInherited) != 0 &&
                    (source.flags & Api::kNodeStateOverridden) != 0) ||
                !seen.insert(source.nodeId).second) {
                return false;
            }
            const auto found = std::ranges::find_if(model.nodes,
                [&](const Node& node) { return node.nodeId == source.nodeId; });
            return found != model.nodes.end() && IsDefaultStateTarget(*found);
        }

        [[nodiscard]] bool SupportsNode(
            const Api::NodeDescriptorV1& node,
            std::uint64_t capabilities) noexcept
        {
            if ((capabilities & Api::kCapabilitySemanticComposition) == 0) {
                return false;
            }
            if (node.role == Api::SemanticRole::Status &&
                (capabilities & Api::kCapabilitySemanticStatus) == 0) {
                return false;
            }
            switch (node.kind) {
            case Api::NodeKind::AnchorSet:
            case Api::NodeKind::Anchor:
                return (capabilities & Api::kCapabilityAnchors) != 0;
            case Api::NodeKind::PinnedContext:
                return (capabilities & Api::kCapabilityPinnedContext) != 0;
            case Api::NodeKind::RecordView:
                return (capabilities &
                    Api::kCapabilityRecordPresentations) != 0;
            case Api::NodeKind::WorkflowView:
                return (capabilities & Api::kCapabilityWorkflows) != 0;
            case Api::NodeKind::LiveSlot:
                return (capabilities & Api::kCapabilityLiveAssociations) != 0;
            default:
                return true;
            }
        }

        [[nodiscard]] bool SupportsAssociation(
            const Api::AssociationDescriptorV1& association,
            std::uint64_t capabilities) noexcept
        {
            if ((association.flags & Api::kAssociationDirectManipulation) != 0 &&
                (capabilities &
                    Api::kCapabilityDirectLiveManipulation) == 0) {
                return false;
            }
            switch (association.kind) {
            case Api::AssociationKind::StatusExplainsNode:
                return (capabilities & Api::kCapabilitySemanticStatus) != 0;
            case Api::AssociationKind::RecordSelectionDrivesNode:
            case Api::AssociationKind::TableColumnUsesControl:
                return (capabilities &
                    Api::kCapabilityRecordPresentations) != 0;
            case Api::AssociationKind::ControlEditsLiveMarker:
            case Api::AssociationKind::ActionCapturesLiveMarker:
            case Api::AssociationKind::LiveSeriesExplainedByControl:
                return (capabilities &
                    Api::kCapabilityLiveAssociations) != 0;
            default:
                return true;
            }
        }

        [[nodiscard]] Api::SemanticRole FallbackRole(
            const MenuApiHost::Control& control) noexcept
        {
            using Kind = AbsoluteControlPanelApi::ControlKind;
            if (control.kind == Kind::InputBinding) return Api::SemanticRole::Binding;
            if ((control.flags & AbsoluteControlPanelApi::kControlReadOnly) != 0)
                return Api::SemanticRole::Status;
            if (control.kind == Kind::Action &&
                (control.flags &
                    AbsoluteControlPanelApi::kControlRequiresConfirmation) != 0)
                return Api::SemanticRole::Danger;
            return Api::SemanticRole::Default;
        }
    }

    Api::Result Registry::Register(
        const Api::PageCompositionDescriptorV1& descriptor,
        const MenuApiHost::Page& page,
        std::uint64_t supportedCapabilities) noexcept
    {
        try {
            if (descriptor.structSize < sizeof(descriptor) ||
                !IsTerminated(descriptor.moduleId) ||
                !IsTerminated(descriptor.pageId) ||
                !IsIdentifier(descriptor.moduleId) ||
                !IsIdentifier(descriptor.pageId) ||
                std::string_view{ descriptor.moduleId } != page.moduleId ||
                std::string_view{ descriptor.pageId } != page.pageId ||
                descriptor.nodeCount == 0 || !descriptor.nodes ||
                (descriptor.associationCount != 0 && !descriptor.associations)) {
                return Api::Result::InvalidArgument;
            }
            if (descriptor.nodeCount > Api::kMaximumNodesPerPage) {
                return Api::Result::CapacityExceeded;
            }
            if (descriptor.associationCount >
                Api::kMaximumAssociationsPerPage) {
                return Api::Result::CapacityExceeded;
            }

            PageModel model;
            model.moduleId = descriptor.moduleId;
            model.pageId = descriptor.pageId;
            model.enhanced = true;
            model.nodes.reserve(descriptor.nodeCount);
            std::unordered_set<std::string> nodeIds;
            std::unordered_set<std::string> placedControls;
            std::unordered_set<std::string> liveParents;
            std::size_t anchorCount{};
            std::size_t pinnedContextCount{};

            for (std::uint32_t index = 0; index < descriptor.nodeCount; ++index) {
                const auto& source = descriptor.nodes[index];
                if (!ValidNodeShape(source, page) ||
                    !SupportsNode(source, supportedCapabilities) ||
                    !nodeIds.insert(source.nodeId).second ||
                    (index == 0 && source.kind != Api::NodeKind::Root) ||
                    (index != 0 && source.kind == Api::NodeKind::Root)) {
                    return Api::Result::InvalidArgument;
                }
                if (index != 0) {
                    const auto parent = std::ranges::find_if(model.nodes,
                        [&](const Node& node) {
                            return node.nodeId == source.parentNodeId;
                        });
                    if (parent == model.nodes.end() || !IsContainer(parent->kind)) {
                        return Api::Result::InvalidArgument;
                    }
                    if (source.kind == Api::NodeKind::LiveSlot &&
                        ((parent->kind != Api::NodeKind::Card &&
                          parent->kind != Api::NodeKind::Section) ||
                         !liveParents.insert(parent->nodeId).second)) {
                        // C2 renders one bounded live component in a card or
                        // section. Reject shapes the movie cannot represent.
                        return Api::Result::InvalidArgument;
                    }
                }
                if (source.kind == Api::NodeKind::Anchor &&
                    ++anchorCount > Api::kMaximumAnchorsPerPage) {
                    return Api::Result::CapacityExceeded;
                }
                if (source.kind == Api::NodeKind::PinnedContext &&
                    ++pinnedContextCount > 1) {
                    return Api::Result::InvalidArgument;
                }
                if (source.kind == Api::NodeKind::ControlSlot ||
                    source.kind == Api::NodeKind::RecordView) {
                    if (!placedControls.insert(source.referenceId).second) {
                        return Api::Result::InvalidArgument;
                    }
                }
                model.nodes.push_back(CopyNode(source));
            }

            for (const auto& node : model.nodes) {
                if (node.kind == Api::NodeKind::Anchor) {
                    const auto target = std::ranges::find_if(model.nodes,
                        [&](const Node& candidate) {
                            return candidate.nodeId == node.referenceId;
                        });
                    if (target == model.nodes.end() ||
                        target->kind == Api::NodeKind::Root ||
                        target->kind == Api::NodeKind::Anchor) {
                        return Api::Result::InvalidArgument;
                    }
                }
            }

            // Pinned editing context is a stable page-control concern rather
            // than provider layout boilerplate. Synthesize its semantic
            // container when an otherwise complete composition predates the
            // pinned controls added to that page.
            std::string pinnedParent;
            for (const auto& node : model.nodes) {
                if (node.kind == Api::NodeKind::PinnedContext) {
                    pinnedParent = node.nodeId;
                    break;
                }
            }
            std::size_t synthesizedPinned{};
            for (const auto& control : page.controls) {
                if ((control.flags &
                        AbsoluteControlPanelApi::kControlPinnedContext) == 0 ||
                    placedControls.contains(control.controlId)) {
                    continue;
                }
                if (pinnedParent.empty()) {
                    pinnedParent = "ac-pinned-context";
                    while (nodeIds.contains(pinnedParent)) pinnedParent += "-x";
                    Node context;
                    context.kind = Api::NodeKind::PinnedContext;
                    context.role = Api::SemanticRole::Primary;
                    context.nodeId = pinnedParent;
                    context.parentNodeId = model.nodes.front().nodeId;
                    context.label = "Editing context";
                    model.nodes.push_back(std::move(context));
                    nodeIds.insert(pinnedParent);
                }
                Node slot;
                slot.kind = control.kind ==
                        AbsoluteControlPanelApi::ControlKind::RecordCollection
                    ? Api::NodeKind::RecordView : Api::NodeKind::ControlSlot;
                slot.role = FallbackRole(control);
                slot.nodeId = "ac-pinned-slot-" +
                    std::to_string(synthesizedPinned++);
                while (nodeIds.contains(slot.nodeId)) slot.nodeId += "-x";
                slot.parentNodeId = pinnedParent;
                slot.referenceId = control.controlId;
                slot.label = control.label;
                slot.description = control.description;
                slot.auxiliaryValue = slot.kind == Api::NodeKind::RecordView
                    ? static_cast<std::uint32_t>(Api::RecordPresentation::Popup)
                    : 0;
                nodeIds.insert(slot.nodeId);
                placedControls.insert(control.controlId);
                model.nodes.push_back(std::move(slot));
            }
            if (model.nodes.size() > Api::kMaximumNodesPerPage) {
                return Api::Result::CapacityExceeded;
            }

            for (const auto& control : page.controls) {
                if (control.kind ==
                    AbsoluteControlPanelApi::ControlKind::GroupHeader) continue;
                if (!placedControls.contains(control.controlId)) {
                    return Api::Result::InvalidArgument;
                }
            }

            std::size_t pinnedContextControls{};
            for (const auto& node : model.nodes) {
                if (node.kind != Api::NodeKind::ControlSlot &&
                    node.kind != Api::NodeKind::RecordView) continue;
                const Node* ancestor = &node;
                while (!ancestor->parentNodeId.empty()) {
                    ancestor = FindModelNode(model, ancestor->parentNodeId);
                    if (!ancestor) return Api::Result::InvalidArgument;
                    if (ancestor->kind == Api::NodeKind::PinnedContext) {
                        ++pinnedContextControls;
                        break;
                    }
                }
            }
            if (pinnedContextControls >
                Api::kMaximumPinnedContextControls) {
                return Api::Result::CapacityExceeded;
            }

            model.associations.reserve(descriptor.associationCount);
            std::unordered_set<std::string> associationIds;
            std::unordered_set<std::string> associationTuples;
            for (std::uint32_t index = 0;
                 index < descriptor.associationCount; ++index) {
                const auto& source = descriptor.associations[index];
                const std::string tuple = std::to_string(
                    static_cast<std::uint32_t>(source.kind)) + "\n" +
                    source.sourceId + "\n" + source.targetNodeId + "\n" +
                    source.semanticId;
                if (!ValidAssociation(source, page, model) ||
                    !SupportsAssociation(source, supportedCapabilities) ||
                    !associationIds.insert(source.associationId).second ||
                    !associationTuples.insert(tuple).second) {
                    return Api::Result::InvalidArgument;
                }
                model.associations.push_back(CopyAssociation(source));
            }

            auto provider = std::make_shared<ProviderState>();
            provider->context = descriptor.context;
            provider->readNodeStates = descriptor.readNodeStates;

            std::scoped_lock lock{ mutex_ };
            if (pages_.size() >= Api::kMaximumPages ||
                std::ranges::count_if(pages_, [&](const PageSlot& slot) {
                    return slot.model.moduleId == model.moduleId;
                }) >= Api::kMaximumPagesPerModule) {
                return Api::Result::CapacityExceeded;
            }
            if (std::ranges::any_of(pages_, [&](const PageSlot& slot) {
                    return slot.model.moduleId == model.moduleId &&
                           slot.model.pageId == model.pageId;
                })) {
                return Api::Result::Duplicate;
            }
            pages_.push_back(PageSlot{ std::move(model), std::move(provider) });
            return Api::Result::Ok;
        } catch (...) {
            return Api::Result::Rejected;
        }
    }

    Api::Result Registry::UnregisterModule(const char* moduleId) noexcept
    {
        try {
            if (!moduleId || !IsIdentifier(moduleId)) {
                return Api::Result::InvalidArgument;
            }
            std::scoped_lock registryLock{ mutex_ };
            std::vector<std::shared_ptr<ProviderState>> providers;
            for (const auto& page : pages_) {
                if (page.model.moduleId == moduleId) {
                    providers.push_back(page.provider);
                }
            }
            if (providers.empty()) return Api::Result::NotFound;
            for (const auto& provider : providers) {
                std::scoped_lock providerLock{ provider->mutex };
                if (provider->inFlight != 0) return Api::Result::Rejected;
            }
            for (const auto& provider : providers) {
                std::scoped_lock providerLock{ provider->mutex };
                provider->retired = true;
            }
            std::erase_if(pages_, [&](const PageSlot& page) {
                return page.model.moduleId == moduleId;
            });
            return Api::Result::Ok;
        } catch (...) {
            return Api::Result::Rejected;
        }
    }

    SnapshotResult Registry::Snapshot(const MenuApiHost::Page& page) noexcept
    {
        try {
            PageModel model;
            std::shared_ptr<ProviderState> provider;
            {
                std::scoped_lock lock{ mutex_ };
                const auto found = std::ranges::find_if(pages_,
                    [&](const PageSlot& slot) {
                        return slot.model.moduleId == page.moduleId &&
                               slot.model.pageId == page.pageId;
                    });
                if (found == pages_.end()) {
                    return { Api::Result::Ok,
                        FlatFallback(page, "No enhanced composition registered.") };
                }
                model = found->model;
                provider = found->provider;
            }

            if (!provider->readNodeStates) {
                return { Api::Result::Ok, std::move(model) };
            }

            CallbackLease lease{ provider };
            if (!lease) {
                return { Api::Result::Rejected,
                    FlatFallback(page, "Composition provider was retired.") };
            }
            std::array<Api::NodeStateV1, Api::kMaximumNodesPerPage> states{};
            std::uint32_t count{};
            const auto result = provider->readNodeStates(
                provider->context, page.moduleId.c_str(), page.pageId.c_str(),
                states.data(), static_cast<std::uint32_t>(model.nodes.size()),
                &count);
            if (result != Api::Result::Ok || count > model.nodes.size()) {
                return { result == Api::Result::Ok ?
                            Api::Result::InvalidArgument : result,
                    FlatFallback(page,
                        "Composition state was unavailable or invalid.") };
            }

            std::unordered_set<std::string> seen;
            for (std::uint32_t index = 0; index < count; ++index) {
                if (!ValidState(states[index], model, seen)) {
                    return { Api::Result::InvalidArgument,
                        FlatFallback(page,
                            "Composition state referenced an invalid node.") };
                }
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto& source = states[index];
                auto target = std::ranges::find_if(model.nodes,
                    [&](const Node& node) { return node.nodeId == source.nodeId; });
                target->state.flags = source.flags;
                target->state.severity = source.severity;
                target->state.value = source.value;
                target->state.detail = source.detail;
                target->state.sourceLabel = source.sourceLabel;
                target->state.sequence = source.sequence;
            }
            return { Api::Result::Ok, std::move(model) };
        } catch (...) {
            return { Api::Result::Rejected,
                FlatFallback(page, "Composition snapshot failed.") };
        }
    }

    PageModel Registry::FlatFallback(const MenuApiHost::Page& page,
                                     std::string_view reason) const
    {
        PageModel model;
        model.moduleId = page.moduleId;
        model.pageId = page.pageId;
        model.enhanced = false;
        model.fallbackReason = reason;
        std::string rootId{ "ac-flat-root" };
        while (std::ranges::any_of(page.controls,
            [&](const MenuApiHost::Control& control) {
                return control.controlId == rootId;
            })) {
            rootId += "-root";
        }
        model.nodes.push_back(Node{
            Api::NodeKind::Root, Api::kNodeNone, Api::SemanticRole::Default,
            rootId, {}, {}, {}, {}, 0, {}
        });

        std::string parent{ rootId };
        for (const auto& control : page.controls) {
            if (control.kind ==
                AbsoluteControlPanelApi::ControlKind::GroupHeader) {
                parent = control.controlId;
                Node section;
                section.kind = Api::NodeKind::Section;
                section.role = Api::SemanticRole::Secondary;
                section.nodeId = control.controlId;
                section.parentNodeId = rootId;
                section.label = control.label;
                section.description = control.description;
                model.nodes.push_back(std::move(section));
                continue;
            }
            Node slot;
            slot.kind = control.kind ==
                    AbsoluteControlPanelApi::ControlKind::RecordCollection ?
                Api::NodeKind::RecordView : Api::NodeKind::ControlSlot;
            slot.role = FallbackRole(control);
            slot.nodeId = control.controlId;
            slot.parentNodeId = parent;
            slot.referenceId = control.controlId;
            slot.label = control.label;
            slot.description = control.description;
            slot.auxiliaryValue = slot.kind == Api::NodeKind::RecordView ?
                static_cast<std::uint32_t>(Api::RecordPresentation::Popup) : 0;
            model.nodes.push_back(std::move(slot));
        }
        return model;
    }

    std::size_t Registry::PageCount() const noexcept
    {
        std::scoped_lock lock{ mutex_ };
        return pages_.size();
    }

    std::vector<SubscriberDiagnostics> Registry::Diagnostics() const
    {
        std::scoped_lock lock{ mutex_ };
        std::vector<SubscriberDiagnostics> subscribers;
        for (const auto& page : pages_) {
            const auto found = std::ranges::find_if(subscribers,
                [&](const SubscriberDiagnostics& subscriber) {
                    return subscriber.moduleId == page.model.moduleId;
                });
            if (found == subscribers.end()) {
                subscribers.push_back({ page.model.moduleId, 1 });
            } else {
                ++found->pageCount;
            }
        }
        return subscribers;
    }

    Registry& HostRegistry() noexcept
    {
        static Registry registry;
        return registry;
    }

    const Node* FindNode(const PageModel& model,
                         std::string_view nodeId) noexcept
    {
        return FindModelNode(model, nodeId);
    }

    const Node* FindControlPlacement(const PageModel& model,
                                     std::string_view controlId) noexcept
    {
        const auto found = std::ranges::find_if(model.nodes,
            [&](const Node& node) {
                return (node.kind == Api::NodeKind::ControlSlot ||
                        node.kind == Api::NodeKind::RecordView) &&
                       node.referenceId == controlId;
            });
        return found == model.nodes.end() ? nullptr : &*found;
    }

    namespace
    {
        [[nodiscard]] bool EffectiveFlag(
            const PageModel& model, const Node& node,
            std::uint32_t flag) noexcept
        {
            const Node* current = &node;
            for (std::size_t depth{}; current && depth < model.nodes.size();
                 ++depth) {
                if ((current->state.flags & flag) == 0) return false;
                if (current->parentNodeId.empty()) return true;
                current = FindModelNode(model, current->parentNodeId);
            }
            return false;
        }

        [[nodiscard]] bool IsDescendantOf(
            const PageModel& model, const Node& node,
            std::string_view ancestorId) noexcept
        {
            const Node* current = &node;
            for (std::size_t depth{}; current && depth < model.nodes.size();
                 ++depth) {
                if (current->nodeId == ancestorId) return true;
                if (current->parentNodeId.empty()) return false;
                current = FindModelNode(model, current->parentNodeId);
            }
            return false;
        }
    }

    bool IsEffectivelyVisible(const PageModel& model,
                              const Node& node) noexcept
    {
        return EffectiveFlag(model, node, Api::kNodeStateVisible);
    }

    bool IsEffectivelyEnabled(const PageModel& model,
                              const Node& node) noexcept
    {
        return EffectiveFlag(model, node, Api::kNodeStateEnabled);
    }

    std::vector<std::string> SelectableControlOrder(const PageModel& model)
    {
        std::vector<std::string> result;
        result.reserve(model.nodes.size());
        for (const auto& node : model.nodes) {
            if ((node.kind != Api::NodeKind::ControlSlot &&
                 node.kind != Api::NodeKind::RecordView) ||
                !IsEffectivelyVisible(model, node) ||
                !IsEffectivelyEnabled(model, node)) {
                continue;
            }
            result.push_back(node.referenceId);
        }
        return result;
    }

    std::vector<AnchorTarget> AnchorTargets(const PageModel& model)
    {
        std::vector<AnchorTarget> result;
        for (const auto& anchor : model.nodes) {
            if (anchor.kind != Api::NodeKind::Anchor ||
                !IsEffectivelyVisible(model, anchor) ||
                !IsEffectivelyEnabled(model, anchor)) {
                continue;
            }
            const auto placement = std::ranges::find_if(model.nodes,
                [&](const Node& candidate) {
                    return (candidate.kind == Api::NodeKind::ControlSlot ||
                            candidate.kind == Api::NodeKind::RecordView) &&
                           IsDescendantOf(model, candidate,
                               anchor.referenceId) &&
                           IsEffectivelyVisible(model, candidate) &&
                           IsEffectivelyEnabled(model, candidate);
                });
            if (placement != model.nodes.end()) {
                result.push_back(AnchorTarget{
                    anchor.nodeId, anchor.label, placement->referenceId });
            }
        }
        return result;
    }
}
