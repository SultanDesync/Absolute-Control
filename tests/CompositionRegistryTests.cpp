#include "CompositionRegistry.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <vector>

#ifdef NDEBUG
#undef assert
#define assert(expression) \
    ((expression) ? static_cast<void>(0) : std::abort())
#endif

using namespace AbsoluteControlPanelResearch;
namespace CompositionApi = AbsoluteControlCompositionExperimental;
namespace StableApi = AbsoluteControlPanelApi;

namespace
{
    template <std::size_t N>
    void Copy(char (&destination)[N], std::string_view source)
    {
        assert(source.size() < N);
        std::memcpy(destination, source.data(), source.size());
        destination[source.size()] = '\0';
    }

    MenuApiHost::Control Control(StableApi::ControlKind kind,
                                 std::string id,
                                 std::uint32_t flags = StableApi::kControlNone)
    {
        return MenuApiHost::Control{
            kind, flags, std::move(id), "Label", "Description", 0.0, 1.0, 1.0
        };
    }

    MenuApiHost::Page Page(std::string pageId = "axes")
    {
        MenuApiHost::Page page;
        page.moduleId = "test.composition";
        page.pageId = std::move(pageId);
        page.displayName = "Axes";
        page.controls.push_back(Control(
            StableApi::ControlKind::GroupHeader, "axes-header"));
        page.controls.push_back(Control(
            StableApi::ControlKind::InputBinding, "axis-binding",
            StableApi::kBindingController | StableApi::kBindingClearable));
        page.controls.push_back(Control(
            StableApi::ControlKind::Toggle, "axis-invert"));
        page.controls.push_back(Control(
            StableApi::ControlKind::RecordCollection, "profiles",
            StableApi::kControlTransientSelection));
        return page;
    }

    CompositionApi::NodeDescriptorV1 Node(
        CompositionApi::NodeKind kind, std::string_view id,
        std::string_view parent = {}, std::string_view reference = {},
        std::string_view label = {})
    {
        CompositionApi::NodeDescriptorV1 node;
        node.kind = kind;
        Copy(node.nodeId, id);
        Copy(node.parentNodeId, parent);
        Copy(node.referenceId, reference);
        Copy(node.label, label);
        return node;
    }

    std::vector<CompositionApi::NodeDescriptorV1> ValidNodes()
    {
        std::vector<CompositionApi::NodeDescriptorV1> nodes;
        nodes.push_back(Node(CompositionApi::NodeKind::Root, "root"));
        nodes.push_back(Node(CompositionApi::NodeKind::AnchorSet,
            "axis-nav", "root"));
        nodes.push_back(Node(CompositionApi::NodeKind::Anchor,
            "axis-jump", "axis-nav", "axis-card", "Flight axis"));
        nodes.push_back(Node(CompositionApi::NodeKind::Card,
            "axis-card", "root", {}, "Flight axis"));
        nodes.push_back(Node(CompositionApi::NodeKind::Row,
            "binding-row", "axis-card"));
        nodes.push_back(Node(CompositionApi::NodeKind::ControlSlot,
            "binding-slot", "binding-row", "axis-binding"));
        nodes.back().role = CompositionApi::SemanticRole::Binding;
        nodes.push_back(Node(CompositionApi::NodeKind::ControlSlot,
            "invert-slot", "axis-card", "axis-invert"));
        nodes.back().role = CompositionApi::SemanticRole::Tuning;
        nodes.push_back(Node(CompositionApi::NodeKind::RecordView,
            "profile-view", "root", "profiles"));
        nodes.back().auxiliaryValue = static_cast<std::uint32_t>(
            CompositionApi::RecordPresentation::MasterDetail);
        return nodes;
    }

    CompositionApi::PageCompositionDescriptorV1 Descriptor(
        const std::vector<CompositionApi::NodeDescriptorV1>& nodes,
        const std::vector<CompositionApi::AssociationDescriptorV1>* associations = nullptr,
        void* context = nullptr,
        CompositionApi::ReadNodeStatesCallback callback = nullptr,
        std::string_view pageId = "axes")
    {
        CompositionApi::PageCompositionDescriptorV1 descriptor;
        Copy(descriptor.moduleId, "test.composition");
        Copy(descriptor.pageId, pageId);
        descriptor.nodeCount = static_cast<std::uint32_t>(nodes.size());
        descriptor.nodes = nodes.data();
        if (associations) {
            descriptor.associationCount =
                static_cast<std::uint32_t>(associations->size());
            descriptor.associations = associations->data();
        }
        descriptor.context = context;
        descriptor.readNodeStates = callback;
        return descriptor;
    }

    CompositionApi::AssociationDescriptorV1 Association(
        CompositionApi::AssociationKind kind, std::string_view id,
        std::string_view source, std::string_view target,
        std::string_view semantic = {})
    {
        CompositionApi::AssociationDescriptorV1 association;
        association.kind = kind;
        Copy(association.associationId, id);
        Copy(association.sourceId, source);
        Copy(association.targetNodeId, target);
        Copy(association.semanticId, semantic);
        return association;
    }

    std::vector<CompositionApi::AssociationDescriptorV1> ValidAssociations()
    {
        return { Association(
            CompositionApi::AssociationKind::ControlSummarizedByNode,
            "invert-summary", "axis-invert", "axis-card") };
    }

    struct StateProvider
    {
        enum class Mode { Valid, UnknownNode, DuplicateNode, TooMany, Rejected };
        Mode mode{ Mode::Valid };
        std::atomic_bool entered{};
        std::atomic_bool release{ true };
    };

    CompositionApi::Result __cdecl ReadStates(
        void* context, const char* moduleId, const char* pageId,
        CompositionApi::NodeStateV1* states, std::uint32_t capacity,
        std::uint32_t* count) noexcept
    {
        auto& provider = *static_cast<StateProvider*>(context);
        if (!moduleId || !pageId || !states || !count ||
            std::string_view{ moduleId } != "test.composition" ||
            std::string_view{ pageId } != "axes") {
            return CompositionApi::Result::InvalidArgument;
        }
        provider.entered.store(true, std::memory_order_release);
        while (!provider.release.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        if (provider.mode == StateProvider::Mode::Rejected) {
            return CompositionApi::Result::Rejected;
        }
        if (provider.mode == StateProvider::Mode::TooMany) {
            *count = capacity + 1;
            return CompositionApi::Result::Ok;
        }
        assert(capacity >= 2);
        Copy(states[0].nodeId, provider.mode == StateProvider::Mode::UnknownNode ?
            "missing" : "axis-card");
        states[0].flags = CompositionApi::kNodeStateVisible |
            CompositionApi::kNodeStateEnabled |
            CompositionApi::kNodeStateOverridden;
        states[0].severity = CompositionApi::StatusSeverity::Warning;
        Copy(states[0].value, "Needs attention");
        Copy(states[0].sourceLabel, "Profile: Combat");
        states[0].sequence = 7;
        *count = 1;
        if (provider.mode == StateProvider::Mode::DuplicateNode) {
            states[1] = states[0];
            *count = 2;
        }
        return CompositionApi::Result::Ok;
    }

    void CheckAbiVocabulary()
    {
        static_assert(CompositionApi::kMaximumNodesPerPage == 128);
        static_assert(CompositionApi::kMaximumAssociationsPerPage == 192);
        static_assert(CompositionApi::kMaximumAnchorsPerPage == 16);
        static_assert(CompositionApi::kMaximumColumns == 8);
        static_assert(static_cast<std::uint32_t>(
            CompositionApi::NodeKind::ControlSlot) == 11);
        static_assert(offsetof(CompositionApi::NodeDescriptorV1, kind) == 4);
        static_assert(offsetof(CompositionApi::NodeStateV1, flags) == 4);
        static_assert(offsetof(CompositionApi::AssociationDescriptorV1, kind) == 4);
        static_assert(offsetof(CompositionApi::PageCompositionDescriptorV1,
            moduleId) == 4);
        static_assert((CompositionApi::kCapabilitySemanticComposition &
            CompositionApi::kCapabilityWorkflows) == 0);
    }

    void CheckFallback()
    {
        Composition::Registry registry;
        const auto page = Page();
        const auto snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::Ok);
        assert(!snapshot.model.enhanced);
        assert(!snapshot.model.fallbackReason.empty());
        assert(snapshot.model.nodes.size() == page.controls.size() + 1);
        assert(snapshot.model.nodes[0].kind == CompositionApi::NodeKind::Root);
        assert(snapshot.model.nodes[1].kind == CompositionApi::NodeKind::Section);
        assert(snapshot.model.nodes[2].kind == CompositionApi::NodeKind::ControlSlot);
        assert(snapshot.model.nodes[2].parentNodeId == "axes-header");
        assert(snapshot.model.nodes[2].role == CompositionApi::SemanticRole::Binding);
        assert(snapshot.model.nodes[4].kind == CompositionApi::NodeKind::RecordView);
        assert(snapshot.model.nodes[4].auxiliaryValue == static_cast<std::uint32_t>(
            CompositionApi::RecordPresentation::Popup));

        auto collidingPage = page;
        collidingPage.controls[0].controlId = "ac-flat-root";
        const auto colliding = registry.Snapshot(collidingPage).model;
        assert(colliding.nodes[0].nodeId == "ac-flat-root-root");
        assert(colliding.nodes[1].nodeId == "ac-flat-root");
        assert(colliding.nodes[1].parentNodeId == "ac-flat-root-root");
    }

    void CheckRegistrationAndState()
    {
        Composition::Registry registry;
        auto page = Page();
        auto nodes = ValidNodes();
        auto associations = ValidAssociations();
        StateProvider provider;
        auto descriptor = Descriptor(nodes, &associations, &provider, &ReadStates);
        assert(registry.Register(descriptor, page) == CompositionApi::Result::Ok);
        assert(registry.Register(descriptor, page) == CompositionApi::Result::Duplicate);
        assert(registry.PageCount() == 1);
        const auto diagnostics = registry.Diagnostics();
        assert(diagnostics.size() == 1 &&
            diagnostics[0].moduleId == "test.composition" &&
            diagnostics[0].pageCount == 1);

        auto snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::Ok);
        assert(snapshot.model.enhanced);
        assert(snapshot.model.nodes.size() == nodes.size());
        assert(snapshot.model.associations.size() == 1);
        assert(snapshot.model.associations[0].associationId == "invert-summary");
        const auto card = std::ranges::find_if(snapshot.model.nodes,
            [](const Composition::Node& node) { return node.nodeId == "axis-card"; });
        assert(card != snapshot.model.nodes.end());
        assert(card->state.severity == CompositionApi::StatusSeverity::Warning);
        assert(card->state.value == "Needs attention");
        assert(card->state.sourceLabel == "Profile: Combat");
        assert(card->state.sequence == 7);

        provider.mode = StateProvider::Mode::UnknownNode;
        snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::InvalidArgument);
        assert(!snapshot.model.enhanced);
        provider.mode = StateProvider::Mode::DuplicateNode;
        snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::InvalidArgument);
        assert(!snapshot.model.enhanced);
        provider.mode = StateProvider::Mode::TooMany;
        snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::InvalidArgument);
        assert(!snapshot.model.enhanced);
        provider.mode = StateProvider::Mode::Rejected;
        snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::Rejected);
        assert(!snapshot.model.enhanced);

        assert(registry.UnregisterModule("bad id!") ==
            CompositionApi::Result::InvalidArgument);
        assert(registry.UnregisterModule("missing") ==
            CompositionApi::Result::NotFound);
        assert(registry.UnregisterModule("test.composition") ==
            CompositionApi::Result::Ok);
        assert(registry.PageCount() == 0);
        assert(!registry.Snapshot(page).model.enhanced);
    }

    void CheckPinnedContextSynthesis()
    {
        Composition::Registry registry;
        auto page = Page();
        page.controls.push_back(Control(
            StableApi::ControlKind::RecordCollection, "edit-profile",
            StableApi::kControlTransientSelection |
                StableApi::kControlPinnedContext));
        auto nodes = ValidNodes();
        const auto descriptor = Descriptor(nodes);
        assert(registry.Register(descriptor, page) ==
            CompositionApi::Result::Ok);

        const auto snapshot = registry.Snapshot(page);
        assert(snapshot.result == CompositionApi::Result::Ok);
        assert(snapshot.model.enhanced);
        assert(snapshot.model.nodes.size() == nodes.size() + 2);
        const auto context = std::ranges::find_if(snapshot.model.nodes,
            [](const Composition::Node& node) {
                return node.kind == CompositionApi::NodeKind::PinnedContext;
            });
        assert(context != snapshot.model.nodes.end());
        const auto slot = std::ranges::find_if(snapshot.model.nodes,
            [](const Composition::Node& node) {
                return node.kind == CompositionApi::NodeKind::RecordView &&
                       node.referenceId == "edit-profile";
            });
        assert(slot != snapshot.model.nodes.end());
        assert(slot->parentNodeId == context->nodeId);
    }

    void CheckInvalidDescriptors()
    {
        auto page = Page();

        auto expectInvalid = [&](std::vector<CompositionApi::NodeDescriptorV1> nodes,
                                 auto mutate) {
            mutate(nodes);
            Composition::Registry registry;
            const auto descriptor = Descriptor(nodes);
            assert(registry.Register(descriptor, page) ==
                CompositionApi::Result::InvalidArgument);
        };

        expectInvalid(ValidNodes(), [](auto& nodes) {
            nodes[0].kind = CompositionApi::NodeKind::Card;
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            Copy(nodes[3].parentNodeId, "missing");
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            Copy(nodes[3].nodeId, nodes[2].nodeId);
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            Copy(nodes[6].referenceId, "axis-binding");
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            Copy(nodes[7].referenceId, "axis-invert");
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            Copy(nodes[2].referenceId, "missing-target");
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            nodes[3].flags = CompositionApi::kNodeCollapsedByDefault;
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            nodes[3].flags = 1U << 31;
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            nodes[3].kind = CompositionApi::NodeKind::Columns;
            nodes[3].auxiliaryValue =
                static_cast<std::uint32_t>(CompositionApi::kMaximumColumns + 1);
        });
        expectInvalid(ValidNodes(), [](auto& nodes) {
            auto live = Node(CompositionApi::NodeKind::LiveSlot,
                "missing-live", "root", "not-registered");
            nodes.push_back(live);
        });

        {
            auto nodes = ValidNodes();
            Composition::Registry registry;
            auto descriptor = Descriptor(nodes);
            Copy(descriptor.moduleId, "wrong.module");
            assert(registry.Register(descriptor, page) ==
                CompositionApi::Result::InvalidArgument);
        }
        {
            std::array<CompositionApi::NodeDescriptorV1,
                CompositionApi::kMaximumNodesPerPage + 1> nodes{};
            Composition::Registry registry;
            CompositionApi::PageCompositionDescriptorV1 descriptor;
            Copy(descriptor.moduleId, "test.composition");
            Copy(descriptor.pageId, "axes");
            descriptor.nodeCount = static_cast<std::uint32_t>(nodes.size());
            descriptor.nodes = nodes.data();
            assert(registry.Register(descriptor, page) ==
                CompositionApi::Result::CapacityExceeded);
        }
        {
            auto nodes = ValidNodes();
            for (std::size_t index = 0;
                 index <= CompositionApi::kMaximumAnchorsPerPage; ++index) {
                auto anchor = Node(CompositionApi::NodeKind::Anchor,
                    "extra-anchor-" + std::to_string(index), "axis-nav",
                    "axis-card", "Extra");
                nodes.push_back(anchor);
            }
            Composition::Registry registry;
            assert(registry.Register(Descriptor(nodes), page) ==
                CompositionApi::Result::CapacityExceeded);
        }
        {
            MenuApiHost::Page contextPage;
            contextPage.moduleId = "test.composition";
            contextPage.pageId = "context";
            std::vector<CompositionApi::NodeDescriptorV1> contextNodes;
            contextNodes.push_back(Node(CompositionApi::NodeKind::Root, "root"));
            contextNodes.push_back(Node(
                CompositionApi::NodeKind::PinnedContext, "context", "root"));
            for (std::size_t index = 0;
                 index <= CompositionApi::kMaximumPinnedContextControls;
                 ++index) {
                const auto id = "setting-" + std::to_string(index);
                contextPage.controls.push_back(Control(
                    StableApi::ControlKind::Toggle, id));
                contextNodes.push_back(Node(
                    CompositionApi::NodeKind::ControlSlot,
                    "slot-" + std::to_string(index), "context", id));
            }
            Composition::Registry registry;
            assert(registry.Register(
                Descriptor(contextNodes, nullptr, nullptr, nullptr, "context"),
                contextPage) == CompositionApi::Result::CapacityExceeded);
        }

        auto expectInvalidAssociation = [&](auto mutate) {
            auto nodes = ValidNodes();
            auto associations = ValidAssociations();
            mutate(associations);
            Composition::Registry registry;
            assert(registry.Register(
                Descriptor(nodes, &associations), page) ==
                CompositionApi::Result::InvalidArgument);
        };
        expectInvalidAssociation([](auto& associations) {
            Copy(associations[0].sourceId, "missing-control");
        });
        expectInvalidAssociation([](auto& associations) {
            Copy(associations[0].targetNodeId, "missing-node");
        });
        expectInvalidAssociation([](auto& associations) {
            associations[0].flags =
                CompositionApi::kAssociationDirectManipulation;
        });
        expectInvalidAssociation([](auto& associations) {
            associations.push_back(associations[0]);
            Copy(associations[1].associationId, "duplicate-tuple");
        });
        {
            auto nodes = ValidNodes();
            std::array<CompositionApi::AssociationDescriptorV1,
                CompositionApi::kMaximumAssociationsPerPage + 1> associations{};
            auto descriptor = Descriptor(nodes);
            descriptor.associationCount =
                static_cast<std::uint32_t>(associations.size());
            descriptor.associations = associations.data();
            Composition::Registry registry;
            assert(registry.Register(descriptor, page) ==
                CompositionApi::Result::CapacityExceeded);
        }

        {
            Composition::Registry registry;
            auto nodes = ValidNodes();
            for (std::size_t index = 0;
                 index < CompositionApi::kMaximumPagesPerModule; ++index) {
                const auto id = "page-" + std::to_string(index);
                const auto boundedPage = Page(id);
                assert(registry.Register(
                    Descriptor(nodes, nullptr, nullptr, nullptr, id),
                    boundedPage) == CompositionApi::Result::Ok);
            }
            const auto overflowPage = Page("page-overflow");
            assert(registry.Register(
                Descriptor(nodes, nullptr, nullptr, nullptr, "page-overflow"),
                overflowPage) == CompositionApi::Result::CapacityExceeded);
            assert(registry.PageCount() ==
                CompositionApi::kMaximumPagesPerModule);
        }
    }

    void CheckCallbackLease()
    {
        Composition::Registry registry;
        auto page = Page();
        auto nodes = ValidNodes();
        StateProvider provider;
        provider.release.store(false, std::memory_order_release);
        auto descriptor = Descriptor(nodes, nullptr, &provider, &ReadStates);
        assert(registry.Register(descriptor, page) == CompositionApi::Result::Ok);

        std::atomic_bool snapshotFinished{};
        std::thread reader([&] {
            const auto result = registry.Snapshot(page);
            assert(result.result == CompositionApi::Result::Ok);
            snapshotFinished.store(true, std::memory_order_release);
        });
        while (!provider.entered.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        assert(registry.UnregisterModule("test.composition") ==
            CompositionApi::Result::Rejected);
        assert(!snapshotFinished.load(std::memory_order_acquire));
        provider.release.store(true, std::memory_order_release);
        reader.join();
        assert(snapshotFinished.load(std::memory_order_acquire));
        assert(registry.UnregisterModule("test.composition") ==
            CompositionApi::Result::Ok);
    }
}

int main()
{
    CheckAbiVocabulary();
    CheckFallback();
    CheckRegistrationAndState();
    CheckPinnedContextSynthesis();
    CheckInvalidDescriptors();
    CheckCallbackLease();
    return 0;
}
