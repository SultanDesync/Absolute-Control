#include "AbsoluteControlCompositionExperimentalAPI.h"
#include "CompositionRegistry.h"
#include "LiveComponentsRegistry.h"
#include "MenuApiHost.h"
#include "MenuInputRouter.h"
#include "MenuSession.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace Stable = AbsoluteControlPanelApi;
namespace Composition = AbsoluteControlCompositionExperimental;
namespace HostComposition = AbsoluteControlPanelResearch::Composition;
namespace Live = AbsoluteControlPanelExperimental;
namespace HostLive = AbsoluteControlPanelResearch::LiveComponents;
namespace MenuApiHost = AbsoluteControlPanelResearch::MenuApiHost;
namespace MenuInputRouter = AbsoluteControlPanelResearch::MenuInputRouter;
namespace MenuSession = AbsoluteControlPanelResearch::MenuSession;

namespace
{
    template <std::size_t N>
    void Copy(char (&destination)[N], std::string_view source)
    {
        if (source.size() >= N) std::abort();
        std::memcpy(destination, source.data(), source.size());
        destination[source.size()] = '\0';
    }

    struct Provider
    {
        enum class Mode { Ready, Conditional };
        std::array<bool, 6> committed{};
        std::array<bool, 6> draft{};
        Mode mode{ Mode::Ready };
        std::uint64_t stateSequence{ 1 };
        std::size_t stateReads{};
    };

    Live::Result __cdecl ReadLiveFrame(
        void*, Live::LiveFrameV1* frame) noexcept
    {
        static std::uint64_t sequence{};
        if (!frame) return Live::Result::InvalidArgument;
        *frame = {};
        frame->kind = Live::ComponentKind::TelemetryPlot;
        frame->sequence = ++sequence;
        frame->monotonicTimestampUs = sequence;
        frame->telemetryPlot.seriesCount = 1;
        frame->telemetryPlot.availableMask = 1;
        frame->telemetryPlot.values[0] = 0.25;
        return Live::Result::Ok;
    }

    Live::LiveChannelDescriptorV1 LiveChannel()
    {
        Live::LiveChannelDescriptorV1 channel;
        Copy(channel.moduleId, "test.semantic");
        Copy(channel.pageId, "six-cards");
        Copy(channel.channelId, "axis-live");
        Copy(channel.title, "Axis input and output");
        channel.kind = Live::ComponentKind::TelemetryPlot;
        channel.telemetryPlot.seriesCount = 1;
        channel.telemetryPlot.historyCapacity = 16;
        channel.telemetryPlot.minimumValue = -1.0;
        channel.telemetryPlot.maximumValue = 1.0;
        Copy(channel.telemetryPlot.series[0].seriesId, "input");
        Copy(channel.telemetryPlot.series[0].label, "Input");
        channel.readLiveFrame = &ReadLiveFrame;
        return channel;
    }

    int ControlIndex(const char* controlId)
    {
        if (!controlId || std::strncmp(controlId, "axis-", 5) != 0 ||
            controlId[5] < '0' || controlId[5] > '5' || controlId[6] != '\0') {
            return -1;
        }
        return controlId[5] - '0';
    }

    Stable::Result __cdecl ReadValue(
        void* context, const char* controlId, Stable::ValueV1* value) noexcept
    {
        const auto index = ControlIndex(controlId);
        if (!context || !value || index < 0) return Stable::Result::InvalidArgument;
        auto& provider = *static_cast<Provider*>(context);
        *value = {};
        value->kind = Stable::ValueKind::Boolean;
        value->booleanValue = provider.draft[index] ? 1U : 0U;
        return Stable::Result::Ok;
    }

    Stable::Result __cdecl WriteDraft(
        void* context, const char* controlId,
        const Stable::ValueV1* value) noexcept
    {
        const auto index = ControlIndex(controlId);
        if (!context || !value || index < 0 ||
            value->kind != Stable::ValueKind::Boolean ||
            value->booleanValue > 1) return Stable::Result::InvalidArgument;
        static_cast<Provider*>(context)->draft[index] =
            value->booleanValue != 0;
        return Stable::Result::Ok;
    }

    Stable::Result __cdecl Apply(void* context) noexcept
    {
        if (!context) return Stable::Result::InvalidArgument;
        auto& provider = *static_cast<Provider*>(context);
        provider.committed = provider.draft;
        return Stable::Result::Ok;
    }

    void __cdecl Cancel(void* context) noexcept
    {
        if (!context) return;
        auto& provider = *static_cast<Provider*>(context);
        provider.draft = provider.committed;
    }

    Composition::Result __cdecl ReadNodeStates(
        void* context, const char* moduleId, const char* pageId,
        Composition::NodeStateV1* states, std::uint32_t capacity,
        std::uint32_t* count) noexcept
    {
        if (!context || !moduleId || !pageId || !states || !count ||
            std::string_view{ moduleId } != "test.semantic" ||
            std::string_view{ pageId } != "six-cards" || capacity < 6) {
            return Composition::Result::InvalidArgument;
        }
        auto& provider = *static_cast<Provider*>(context);
        ++provider.stateReads;
        for (std::size_t index{}; index < 6; ++index) {
            states[index] = {};
            Copy(states[index].nodeId, "axis-card-" + std::to_string(index));
            states[index].flags = Composition::kNodeStateVisible |
                Composition::kNodeStateEnabled;
            states[index].severity = index == 5 ?
                Composition::StatusSeverity::Warning :
                Composition::StatusSeverity::Information;
            Copy(states[index].value, index == 5 ? "Review tuning" : "Ready");
            Copy(states[index].sourceLabel, "Synthetic fixture");
            states[index].sequence = provider.stateSequence;
        }
        if (provider.mode == Provider::Mode::Conditional) {
            states[4].flags = Composition::kNodeStateVisible;
            Copy(states[4].value, "Unavailable in this mode");
            states[4].severity = Composition::StatusSeverity::Unavailable;
            states[5].flags = Composition::kNodeStateEnabled;
        }
        *count = 6;
        return Composition::Result::Ok;
    }

    Composition::NodeDescriptorV1 Node(
        Composition::NodeKind kind, std::string_view id,
        std::string_view parent = {}, std::string_view reference = {},
        std::string_view label = {})
    {
        Composition::NodeDescriptorV1 node;
        node.kind = kind;
        Copy(node.nodeId, id);
        Copy(node.parentNodeId, parent);
        Copy(node.referenceId, reference);
        Copy(node.label, label);
        return node;
    }

    std::vector<Composition::NodeDescriptorV1> SixCardNodes()
    {
        std::vector<Composition::NodeDescriptorV1> nodes;
        nodes.reserve(28);
        nodes.push_back(Node(Composition::NodeKind::Root, "root"));
        nodes.push_back(Node(
            Composition::NodeKind::AnchorSet, "axis-anchors", "root"));
        nodes.push_back(Node(
            Composition::NodeKind::Section, "axes", "root", {},
            "Flight axes"));
        for (std::size_t index{}; index < 6; ++index) {
            const auto suffix = std::to_string(index);
            const auto cardId = "axis-card-" + suffix;
            nodes.push_back(Node(Composition::NodeKind::Anchor,
                "axis-anchor-" + suffix, "axis-anchors", cardId,
                "Axis " + suffix));
            nodes.push_back(Node(Composition::NodeKind::Card,
                cardId, "axes", {}, "Axis card " + suffix));
            nodes.back().role = Composition::SemanticRole::Summary;
            if (index == 0) {
                nodes.push_back(Node(Composition::NodeKind::LiveSlot,
                    "axis-live-slot", cardId, "axis-live", "Live axis"));
            }
            nodes.push_back(Node(Composition::NodeKind::Row,
                "axis-row-" + suffix, cardId));
            nodes.push_back(Node(Composition::NodeKind::ControlSlot,
                "axis-slot-" + suffix, "axis-row-" + suffix,
                "axis-" + suffix));
            nodes.back().role = Composition::SemanticRole::Tuning;
        }
        return nodes;
    }

    Stable::ControlDescriptorV1 Toggle(std::size_t index)
    {
        Stable::ControlDescriptorV1 control;
        control.kind = Stable::ControlKind::Toggle;
        Copy(control.controlId, "axis-" + std::to_string(index));
        Copy(control.label, "Enable axis " + std::to_string(index));
        Copy(control.description,
            "Synthetic C2 control used to prove semantic cards and anchors.");
        return control;
    }

    const MenuSession::Page* ActivePage(const MenuSession::Model& model)
    {
        for (const auto& page : model.pages) {
            if (page.moduleId == model.activeModuleId &&
                page.pageId == model.activePageId) return &page;
        }
        return nullptr;
    }
}

int main()
{
    const auto* composition = AbsoluteControlPanel_QueryCompositionApi(
        Composition::kAbiVersion);
    CHECK(composition != nullptr);
    CHECK(AbsoluteControlPanel_QueryCompositionApi(
        Composition::kAbiVersion + 1) == nullptr);
    CHECK(composition->capabilities == (Composition::kC2Capabilities |
        Composition::kCapabilityDirectLiveManipulation));
    CHECK((composition->capabilities &
        Composition::kCapabilityRecordPresentations) == 0);
    CHECK((composition->capabilities &
        Composition::kCapabilityDirectLiveManipulation) != 0);

    auto nodes = SixCardNodes();
    Provider provider;
    Composition::PageCompositionDescriptorV1 compositionPage;
    Copy(compositionPage.moduleId, "test.semantic");
    Copy(compositionPage.pageId, "six-cards");
    compositionPage.nodeCount = static_cast<std::uint32_t>(nodes.size());
    compositionPage.nodes = nodes.data();
    compositionPage.context = &provider;
    compositionPage.readNodeStates = &ReadNodeStates;
    Composition::AssociationDescriptorV1 liveAssociation;
    liveAssociation.kind =
        Composition::AssociationKind::LiveSeriesExplainedByControl;
    Copy(liveAssociation.associationId, "axis-live-input");
    Copy(liveAssociation.sourceId, "axis-0");
    Copy(liveAssociation.targetNodeId, "axis-live-slot");
    Copy(liveAssociation.semanticId, "input");
    compositionPage.associationCount = 1;
    compositionPage.associations = &liveAssociation;
    CHECK(composition->registerPageComposition(&compositionPage) ==
        Stable::Result::NotReady);

    MenuApiHost::MarkRuntimeReady();
    const auto* stable = AbsoluteControlPanel_QueryApi(Stable::kAbiVersion);
    CHECK(stable != nullptr);
    Stable::ModuleDescriptorV1 module;
    Copy(module.moduleId, "test.semantic");
    Copy(module.displayName, "Semantic fixture");
    Copy(module.description, "C2 synthetic provider");
    CHECK(stable->registerModule(&module) == Stable::Result::Ok);

    std::array<Stable::ControlDescriptorV1, 6> controls;
    for (std::size_t index{}; index < controls.size(); ++index) {
        controls[index] = Toggle(index);
    }
    Stable::PageDescriptorV1 page;
    Copy(page.moduleId, "test.semantic");
    Copy(page.pageId, "six-cards");
    Copy(page.displayName, "Six cards");
    Copy(page.description,
        "Synthetic composition fixture independent of subscriber modules.");
    page.controlCount = static_cast<std::uint32_t>(controls.size());
    page.controls = controls.data();
    page.context = &provider;
    page.readValue = &ReadValue;
    page.writeDraft = &WriteDraft;
    page.apply = &Apply;
    page.cancel = &Cancel;
    CHECK(stable->registerPage(&page) == Stable::Result::Ok);

    const auto liveChannel = LiveChannel();
    CHECK(HostLive::HostRegistry().Register(liveChannel) ==
        Live::Result::Ok);

    auto unsupportedAssociation = liveAssociation;
    unsupportedAssociation.flags = Composition::kAssociationDirectManipulation;
    Composition::PageCompositionDescriptorV1 unsupportedLive = compositionPage;
    unsupportedLive.associations = &unsupportedAssociation;
    CHECK(composition->registerPageComposition(&unsupportedLive) ==
        Stable::Result::InvalidArgument);

    auto duplicateLiveNodes = nodes;
    duplicateLiveNodes.push_back(Node(Composition::NodeKind::LiveSlot,
        "axis-live-slot-2", "axis-card-0", "axis-live", "Second live axis"));
    Composition::PageCompositionDescriptorV1 duplicateLive = compositionPage;
    duplicateLive.nodeCount = static_cast<std::uint32_t>(
        duplicateLiveNodes.size());
    duplicateLive.nodes = duplicateLiveNodes.data();
    CHECK(composition->registerPageComposition(&duplicateLive) ==
        Stable::Result::InvalidArgument);

    auto unsupportedNodes = nodes;
    unsupportedNodes[4].kind = Composition::NodeKind::PinnedContext;
    unsupportedNodes[4].label[0] = '\0';
    Composition::PageCompositionDescriptorV1 unsupported = compositionPage;
    unsupported.nodes = unsupportedNodes.data();
    CHECK(composition->registerPageComposition(&unsupported) ==
        Stable::Result::InvalidArgument);
    CHECK(composition->registerPageComposition(&compositionPage) ==
        Stable::Result::Ok);

    HostLive::HostRegistry().SetMenuActive(true);
    CHECK(HostLive::HostRegistry().SetVisiblePage(
        "test.semantic", "six-cards") == Live::Result::Ok);
    CHECK(HostLive::HostRegistry().PollVisiblePage());

    MenuSession::Session session;
    auto model = session.Snapshot();
    const auto* active = ActivePage(model);
    CHECK(active != nullptr && active->composition.enhanced);
    CHECK(active->composition.nodes.size() == 28);
    CHECK(active->composition.associations.size() == 1);
    CHECK(active->liveComponents.size() == 1);
    CHECK(HostComposition::AnchorTargets(active->composition).size() == 6);
    CHECK(model.selectedControlId == "axis-0");
    CHECK(active->composition.nodes[4].state.value == "Ready");
    CHECK(active->composition.nodes[25].state.severity ==
        Composition::StatusSeverity::Warning);

    auto routed = MenuInputRouter::Route(model, MenuInputRouter::kArrowDown,
        { MenuInputRouter::FocusRegion::Anchors, 0 });
    CHECK(routed.handled && !routed.command &&
        routed.focus.region == MenuInputRouter::FocusRegion::Anchors &&
        routed.focus.actionIndex == 1);
    routed = MenuInputRouter::Route(model, MenuInputRouter::kAccept,
        routed.focus);
    CHECK(routed.command && routed.command->kind ==
        MenuSession::CommandKind::SelectControl &&
        routed.command->controlId == "axis-1" &&
        routed.focus.region == MenuInputRouter::FocusRegion::Controls);

    MenuSession::Command selectLast;
    selectLast.kind = MenuSession::CommandKind::SelectControl;
    selectLast.moduleId = "test.semantic";
    selectLast.pageId = "six-cards";
    selectLast.controlId = "axis-5";
    model = session.Dispatch(selectLast);
    CHECK(model.selectedControlId == "axis-5");
    provider.mode = Provider::Mode::Conditional;
    ++provider.stateSequence;
    model = session.Snapshot();
    active = ActivePage(model);
    CHECK(active != nullptr && active->composition.enhanced);
    CHECK(active->controls[4].semanticVisible &&
        !active->controls[4].semanticEnabled &&
        !active->controls[4].available);
    CHECK(!active->controls[5].semanticVisible);
    CHECK(model.selectedControlId == "axis-0");

    provider.mode = Provider::Mode::Ready;
    ++provider.stateSequence;
    const auto benchmarkStart = std::chrono::steady_clock::now();
    for (std::size_t iteration{}; iteration < 1000; ++iteration) {
        model = session.Snapshot();
        CHECK(ActivePage(model)->composition.enhanced);
    }
    const auto benchmarkElapsed = std::chrono::steady_clock::now() -
        benchmarkStart;
    CHECK(benchmarkElapsed < std::chrono::seconds(2));
    std::printf("six-card snapshots: 1000 in %lld us\n",
        static_cast<long long>(std::chrono::duration_cast<
            std::chrono::microseconds>(benchmarkElapsed).count()));

    CHECK(composition->unregisterModule("test.semantic") ==
        Stable::Result::Ok);
    model = session.Snapshot();
    active = ActivePage(model);
    CHECK(active != nullptr && !active->composition.enhanced &&
        !active->composition.fallbackReason.empty());
    CHECK(stable->unregisterModule("test.semantic") == Stable::Result::Ok);
    CHECK(HostLive::HostRegistry().UnregisterModule("test.semantic") ==
        Live::Result::Ok);
    return 0;
}
