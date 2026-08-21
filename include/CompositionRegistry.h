#pragma once

#include "AbsoluteControlCompositionExperimentalAPI.h"
#include "MenuApiHost.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControlPanelResearch::Composition
{
    namespace Api = AbsoluteControlCompositionExperimental;

    struct ProviderState;

    struct NodeState
    {
        std::uint32_t flags{
            Api::kNodeStateVisible | Api::kNodeStateEnabled };
        Api::StatusSeverity severity{ Api::StatusSeverity::Normal };
        std::string value;
        std::string detail;
        std::string sourceLabel;
        std::uint64_t sequence{};
    };

    struct Node
    {
        Api::NodeKind kind{ Api::NodeKind::Root };
        std::uint32_t flags{};
        Api::SemanticRole role{ Api::SemanticRole::Default };
        std::string nodeId;
        std::string parentNodeId;
        std::string referenceId;
        std::string label;
        std::string description;
        std::uint32_t auxiliaryValue{};
        NodeState state{};
    };

    struct Association
    {
        Api::AssociationKind kind{
            Api::AssociationKind::ControlSummarizedByNode };
        std::uint32_t flags{};
        std::string associationId;
        std::string sourceId;
        std::string targetNodeId;
        std::string semanticId;
    };

    struct PageModel
    {
        std::string moduleId;
        std::string pageId;
        bool enhanced{};
        std::string fallbackReason;
        std::vector<Node> nodes;
        std::vector<Association> associations;
    };

    struct SnapshotResult
    {
        Api::Result result{ Api::Result::NotReady };
        PageModel model;
    };

    struct AnchorTarget
    {
        std::string anchorNodeId;
        std::string label;
        std::string controlId;
    };

    struct SubscriberDiagnostics
    {
        std::string moduleId;
        std::size_t pageCount{};
    };

    class Registry
    {
    public:
        [[nodiscard]] Api::Result Register(
            const Api::PageCompositionDescriptorV1& descriptor,
            const MenuApiHost::Page& page,
            std::uint64_t supportedCapabilities =
                Api::kAllCapabilities) noexcept;
        [[nodiscard]] Api::Result UnregisterModule(
            const char* moduleId) noexcept;
        [[nodiscard]] SnapshotResult Snapshot(
            const MenuApiHost::Page& page) noexcept;
        [[nodiscard]] PageModel FlatFallback(
            const MenuApiHost::Page& page,
            std::string_view reason = {}) const;
        [[nodiscard]] std::size_t PageCount() const noexcept;
        [[nodiscard]] std::vector<SubscriberDiagnostics>
            Diagnostics() const;

    private:
        struct PageSlot
        {
            PageModel model;
            std::shared_ptr<ProviderState> provider;
        };

        mutable std::mutex mutex_;
        std::vector<PageSlot> pages_;
    };

    [[nodiscard]] Registry& HostRegistry() noexcept;
    [[nodiscard]] const Node* FindNode(
        const PageModel& model, std::string_view nodeId) noexcept;
    [[nodiscard]] const Node* FindControlPlacement(
        const PageModel& model, std::string_view controlId) noexcept;
    [[nodiscard]] bool IsEffectivelyVisible(
        const PageModel& model, const Node& node) noexcept;
    [[nodiscard]] bool IsEffectivelyEnabled(
        const PageModel& model, const Node& node) noexcept;
    [[nodiscard]] std::vector<std::string> SelectableControlOrder(
        const PageModel& model);
    [[nodiscard]] std::vector<AnchorTarget> AnchorTargets(
        const PageModel& model);
}
