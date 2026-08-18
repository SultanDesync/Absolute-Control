#include "MenuApiHost.h"
#include "MenuInputRouter.h"
#include "MenuSession.h"
#include "LiveComponentsRegistry.h"
#include "AbsoluteControlPanelAPI.h"
#include "SlopAPI.h"

#include <cmath>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace
{
    struct Provider
    {
        int reads{};
        int writes{};
        int invokes{};
        int applies{};
        int cancels{};
        bool failApply{};
        bool failInvoke{};
        bool wrongReadKind{};
        bool duplicateWrites{};
        int reassigns{};
        std::string reassignedBinding;
        int captureBegins{};
        int capturePolls{};
        int captureCancels{};
        AbsoluteControlPanelApi::BindingCaptureState captureState{
            AbsoluteControlPanelApi::BindingCaptureState::Capturing };
        std::string captureBinding;
        std::string captureDetail;
        SlopApi::ValueV1 lastWrite{};
    };

    struct BlockingProvider
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool entered{};
        bool release{};
    };

    struct BlockingCancelProvider
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool entered{};
        bool release{};
        int cancels{};
    };

    struct CompoundProvider
    {
        AbsoluteControlPanelExperimental::LiveFrameV1 frame{};
        std::uint64_t revision{};
        int operations{};
    };

    AbsoluteControlPanelExperimental::Result __cdecl ReadCompoundFrame(
        void* context,
        AbsoluteControlPanelExperimental::LiveFrameV1* output) noexcept
    {
        *output = static_cast<CompoundProvider*>(context)->frame;
        return AbsoluteControlPanelExperimental::Result::Ok;
    }

    AbsoluteControlPanelExperimental::Result __cdecl ApplyCompoundOperation(
        void* context,
        const AbsoluteControlPanelExperimental::CompoundOperationV1* operation,
        AbsoluteControlPanelExperimental::CompoundSnapshotV1* output) noexcept
    {
        auto& provider = *static_cast<CompoundProvider*>(context);
        ++provider.operations;
        ++provider.revision;
        ++provider.frame.sequence;
        ++provider.frame.monotonicTimestampUs;
        provider.frame.segmentedGrid.columns[0].segments[0].tierIndex = 1;
        *output = {};
        output->revision = provider.revision;
        output->segmentedGrid = provider.frame.segmentedGrid;
        (void)operation;
        return AbsoluteControlPanelExperimental::Result::Ok;
    }

    SlopApi::Result __cdecl BlockingRead(
        void* a_context, const char*, SlopApi::ValueV1* a_value) noexcept
    {
        auto& provider = *static_cast<BlockingProvider*>(a_context);
        {
            std::unique_lock lock{ provider.mutex };
            provider.entered = true;
            provider.condition.notify_all();
            provider.condition.wait(lock, [&] { return provider.release; });
        }
        a_value->kind = SlopApi::ValueKind::Boolean;
        a_value->booleanValue = 1;
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl TeardownRead(
        void*, const char*, SlopApi::ValueV1* a_value) noexcept
    {
        a_value->kind = SlopApi::ValueKind::Boolean;
        a_value->booleanValue = 1;
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl TeardownWrite(
        void*, const char*, const SlopApi::ValueV1*) noexcept
    {
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl TeardownApply(void*) noexcept
    {
        return SlopApi::Result::Ok;
    }

    void __cdecl BlockingCancel(void* a_context) noexcept
    {
        auto& provider = *static_cast<BlockingCancelProvider*>(a_context);
        std::unique_lock lock{ provider.mutex };
        ++provider.cancels;
        provider.entered = true;
        provider.condition.notify_all();
        provider.condition.wait(lock, [&] { return provider.release; });
    }

    SlopApi::Result __cdecl ReadValue(void* a_context, const char* a_control, SlopApi::ValueV1* a_value) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        ++provider.reads;
        a_value->structSize = sizeof(*a_value);
        if (strcmp(a_control, "broken") == 0 || provider.wrongReadKind) {
            a_value->kind = SlopApi::ValueKind::String;
            strcpy_s(a_value->stringValue, "wrong");
            return SlopApi::Result::Ok;
        }
        if (strcmp(a_control, "enabled") == 0 ||
            strncmp(a_control, "value.", 6) == 0) {
            a_value->kind = SlopApi::ValueKind::Boolean;
            a_value->booleanValue = 1;
        } else if (strcmp(a_control, "count") == 0 ||
                   strcmp(a_control, "profile") == 0) {
            a_value->kind = SlopApi::ValueKind::Integer;
            a_value->integerValue = 3;
        } else if (strcmp(a_control, "scale") == 0) {
            a_value->kind = SlopApi::ValueKind::Float;
            a_value->floatValue = 1.5;
        } else {
            a_value->kind = SlopApi::ValueKind::String;
            strcpy_s(a_value->stringValue, "K");
        }
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl WriteDraft(void* a_context, const char*, const SlopApi::ValueV1* a_value) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        ++provider.writes;
        provider.lastWrite = *a_value;
        return provider.duplicateWrites ? SlopApi::Result::Duplicate :
            SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl Invoke(void* a_context, const char*) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        ++provider.invokes;
        return provider.failInvoke ? SlopApi::Result::Rejected :
                                     SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl Apply(void* a_context) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        ++provider.applies;
        return provider.failApply ? SlopApi::Result::WriteFailure : SlopApi::Result::Ok;
    }

    void __cdecl Cancel(void* a_context) noexcept { ++static_cast<Provider*>(a_context)->cancels; }

    AbsoluteControlPanelApi::Result __cdecl BeginProviderCapture(
        void* a_context, const char* a_control) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        if (!a_control || std::string_view(a_control) != "controller-action") {
            return AbsoluteControlPanelApi::Result::NotFound;
        }
        ++provider.captureBegins;
        provider.captureState =
            AbsoluteControlPanelApi::BindingCaptureState::Capturing;
        provider.captureBinding.clear();
        provider.captureDetail.clear();
        return AbsoluteControlPanelApi::Result::Ok;
    }

    AbsoluteControlPanelApi::Result __cdecl PollProviderCapture(
        void* a_context, const char* a_control,
        AbsoluteControlPanelApi::BindingCaptureV1* a_capture) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        if (!a_control || !a_capture ||
            std::string_view(a_control) != "controller-action") {
            return AbsoluteControlPanelApi::Result::InvalidArgument;
        }
        ++provider.capturePolls;
        *a_capture = {};
        a_capture->state = provider.captureState;
        strcpy_s(a_capture->binding, provider.captureBinding.c_str());
        strcpy_s(a_capture->detail, provider.captureDetail.c_str());
        return AbsoluteControlPanelApi::Result::Ok;
    }

    AbsoluteControlPanelApi::Result __cdecl CancelProviderCapture(
        void* a_context, const char* a_control) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        if (!a_control || std::string_view(a_control) != "controller-action") {
            return AbsoluteControlPanelApi::Result::NotFound;
        }
        ++provider.captureCancels;
        provider.captureState =
            AbsoluteControlPanelApi::BindingCaptureState::Cancelled;
        return AbsoluteControlPanelApi::Result::Ok;
    }

    AbsoluteControlPanelApi::Result __cdecl ReassignProviderBinding(
        void* a_context, const char* a_control, const char* a_binding) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        if (!a_control || !a_binding ||
            std::string_view{a_control} != "controller-action" ||
            std::string_view{a_binding}.empty()) {
            return AbsoluteControlPanelApi::Result::InvalidArgument;
        }
        ++provider.reassigns;
        provider.reassignedBinding = a_binding;
        return AbsoluteControlPanelApi::Result::Ok;
    }

    SlopApi::Result __cdecl ReadChoiceOptions(void*, const char* a_control,
        SlopApi::ChoiceOptionV1* a_options, std::uint32_t a_capacity,
        std::uint32_t* a_count) noexcept
    {
        if (!a_control || !a_options || !a_count ||
            std::string_view(a_control) != "profile") {
            return SlopApi::Result::NotFound;
        }
        if (a_capacity < 2) return SlopApi::Result::CapacityExceeded;
        a_options[0].value = 3;
        strcpy_s(a_options[0].label, "Cruise");
        a_options[1].value = 7;
        strcpy_s(a_options[1].label, "Combat");
        *a_count = 2;
        return SlopApi::Result::Ok;
    }

    SlopApi::ControlDescriptorV1 MakeDescriptor(SlopApi::ControlKind a_kind, const char* a_id,
        double a_minimum = 0, double a_maximum = 0, double a_step = 0)
    {
        SlopApi::ControlDescriptorV1 control;
        control.kind = a_kind;
        strcpy_s(control.controlId, a_id);
        strcpy_s(control.label, a_id);
        control.minimumValue = a_minimum;
        control.maximumValue = a_maximum;
        control.stepValue = a_step;
        return control;
    }

    AbsoluteControlPanelResearch::MenuSession::Command MakeCommand(
        AbsoluteControlPanelResearch::MenuSession::CommandKind a_kind, const char* a_page,
        const char* a_control = "")
    {
        using namespace AbsoluteControlPanelResearch::MenuSession;
        AbsoluteControlPanelResearch::MenuSession::Command command;
        command.kind = a_kind;
        command.moduleId = "test.module";
        command.pageId = a_page;
        command.controlId = a_control;
        return command;
    }
}
int main()
{
    using namespace SlopApi;
    using namespace AbsoluteControlPanelResearch;
    using namespace MenuSession;

    CHECK(SLOP_QueryApi(kAbiVersion + 1) == nullptr);
    const auto* api = SLOP_QueryApi(kAbiVersion);
    CHECK(api && api->abiVersion == kAbiVersion && api->structSize >= sizeof(ApiV1));
    CHECK(api->registerModule != nullptr);
    CHECK(AbsoluteControlPanel_QueryApi(AbsoluteControlPanelApi::kAbiVersion + 1) == nullptr);
    const auto* publicApi =
        AbsoluteControlPanel_QueryApi(AbsoluteControlPanelApi::kAbiVersion);
    CHECK(publicApi && publicApi->moduleId &&
        std::string_view(publicApi->moduleId) == AbsoluteControlPanelApi::kModuleId &&
        publicApi->isOpen && publicApi->isInputCaptureActive);
    CHECK(publicApi->structSize >= offsetof(AbsoluteControlPanelApi::ApiV1,
        capabilities) + sizeof(publicApi->capabilities));
    CHECK((publicApi->capabilities &
        AbsoluteControlPanelApi::kCapabilityLabeledChoices) != 0);
    CHECK((publicApi->capabilities &
        AbsoluteControlPanelApi::kCapabilityProviderBindingCapture) != 0);
    CHECK((publicApi->capabilities &
        AbsoluteControlPanelApi::kCapabilityBindingConflictResolution) != 0);
    CHECK((publicApi->capabilities &
        AbsoluteControlPanelApi::kCapabilityStructuredLayout) != 0);
    CHECK(std::string_view(publicApi->version) == ACP_PRODUCT_VERSION);
    CHECK(std::string_view(api->version) == ACP_PRODUCT_VERSION);
    CHECK(MenuApiHost::Lifecycle() == MenuApiHost::HostLifecycle::Initializing);
    CHECK(api->registerPage(nullptr) == Result::NotReady);
    CHECK(publicApi->registerPage(nullptr) == Result::NotReady);
    CHECK(api->requestRefresh("test.module", "general") == Result::NotReady);
    MenuApiHost::MarkRuntimeReady();
    CHECK(MenuApiHost::Lifecycle() == MenuApiHost::HostLifecycle::Ready);
    CHECK(api->registerPage(nullptr) == Result::InvalidArgument);

    ModuleDescriptorV1 module;
    strcpy_s(module.moduleId, "test.module");
    strcpy_s(module.displayName, "Test Plugin");
    strcpy_s(module.description, "A subscriber with more than one options page.");
    // Product and legacy discovery tables feed the same product-authoritative
    // registry without descriptor reinterpretation.
    CHECK(publicApi->registerModule(&module) == Result::Ok);

    Provider generalProvider, bindingsProvider, noRollbackProvider;
    ControlDescriptorV1 generalControls[]{
        MakeDescriptor(ControlKind::Toggle, "enabled"),
        MakeDescriptor(ControlKind::IntegerSlider, "count", 0, 10, 1),
        MakeDescriptor(ControlKind::FloatSlider, "scale", 0, 2, 0.25),
        MakeDescriptor(ControlKind::ButtonBinding, "bind"),
        MakeDescriptor(ControlKind::Action, "locked"),
        MakeDescriptor(ControlKind::Action, "run"),
        MakeDescriptor(ControlKind::IntegerSlider, "broken", 0, 10, 1),
        MakeDescriptor(ControlKind::Action, "mutate"),
        MakeDescriptor(ControlKind::TextInput, "name", 0, 31, 1),
        MakeDescriptor(ControlKind::Action, "save-run")
    };
    generalControls[3].flags = kBindingKeyboard | kBindingModifiers | kBindingClearable;
    generalControls[4].flags = kControlReadOnly;
    generalControls[7].flags = kControlMutatesDraft;
    generalControls[9].flags = kControlAppliesDraftBeforeInvoke;
    PageDescriptorV1 general;
    strcpy_s(general.moduleId, "test.module"); strcpy_s(general.pageId, "general"); strcpy_s(general.displayName, "General");
    general.controlCount = static_cast<std::uint32_t>(std::size(generalControls)); general.controls = generalControls;
    general.context = &generalProvider; general.readValue = &ReadValue; general.writeDraft = &WriteDraft;
    general.invokeAction = &Invoke; general.apply = &Apply; general.cancel = &Cancel;
    CHECK(publicApi->registerPage(&general) == Result::Ok);

    ControlDescriptorV1 bindingControls[]{ MakeDescriptor(ControlKind::Toggle, "other") };
    PageDescriptorV1 bindings;
    strcpy_s(bindings.moduleId, "test.module"); strcpy_s(bindings.pageId, "bindings"); strcpy_s(bindings.displayName, "Bindings");
    bindings.controlCount = 1; bindings.controls = bindingControls; bindings.context = &bindingsProvider;
    bindings.readValue = &ReadValue; bindings.writeDraft = &WriteDraft;
    bindings.apply = &Apply; bindings.cancel = &Cancel;
    CHECK(api->registerPage(&bindings) == Result::Ok);

    // Descriptors must be copied before provider-owned storage is changed.
    strcpy_s(generalControls[0].label, "MUTATED");
    const auto copied = MenuApiHost::FindPage("test.module", "general");
    CHECK(copied && copied->controls.front().label == "enabled");

    Session session;
    auto model = session.Snapshot();
    CHECK(model.generation != 0);
    CHECK(model.pages.size() == 2 && model.pages[0].controls.size() == 10);
    CHECK(model.pages[0].moduleTitle == "Test Plugin" &&
        model.pages[1].moduleTitle == "Test Plugin");
    CHECK(model.pages[0].controls[0].available && model.pages[0].controls[0].value.booleanValue == 1);
    CHECK(model.pages[0].controls[1].value.integerValue == 3 && model.pages[0].controls[2].value.floatValue == 1.5);
    CHECK(!model.pages[0].controls[4].available);  // read-only actions cannot invoke
    CHECK(model.pages[0].controls[5].available &&
        model.pages[0].controls[7].available &&
        model.pages[0].controls[9].available && generalProvider.reads == 6);  // actions never read
    CHECK(model.pages[0].controls[6].available == false);  // bad reads are isolated to one row

    const auto cleanApplies = generalProvider.applies;
    const auto cleanCancels = generalProvider.cancels;
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Apply, "general")).error.empty());
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Cancel, "general")).error.empty());
    CHECK(generalProvider.applies == cleanApplies &&
        generalProvider.cancels == cleanCancels);

    Session generationSession;
    const auto generationModel = generationSession.Snapshot();
    generationSession.AcknowledgePublishedGeneration(generationModel.generation);
    auto generationCommand = MakeCommand(CommandKind::SelectPage, "general");
    generationCommand.expectedGeneration = generationModel.generation;
    const auto nextGenerationModel = generationSession.Dispatch(generationCommand);
    CHECK(nextGenerationModel.error.empty() &&
        nextGenerationModel.generation > generationModel.generation);
    // The newer model is deferred, not visible. More input from the still-visible
    // generation must remain valid until publication acknowledges its successor.
    const auto coalescedGenerationModel = generationSession.Dispatch(generationCommand);
    CHECK(coalescedGenerationModel.error.empty() &&
        coalescedGenerationModel.generation > nextGenerationModel.generation);
    generationSession.AcknowledgePublishedGeneration(
        coalescedGenerationModel.generation);
    const auto staleGenerationModel = generationSession.Dispatch(generationCommand);
    CHECK(staleGenerationModel.error == "Stale menu command" &&
        staleGenerationModel.generation > coalescedGenerationModel.generation);

    auto staleWrite = MakeCommand(CommandKind::Write, "general", "enabled");
    staleWrite.value.kind = ValueKind::Boolean;
    staleWrite.value.booleanValue = 0;
    staleWrite.expectedGeneration = coalescedGenerationModel.generation;
    const auto supersedingModel = generationSession.Snapshot();
    generationSession.AcknowledgePublishedGeneration(supersedingModel.generation);
    const auto writesBeforeStale = generalProvider.writes;
    const auto rejectedStaleWrite = generationSession.Dispatch(staleWrite);
    CHECK(rejectedStaleWrite.error == "Stale menu command" &&
        rejectedStaleWrite.activeModuleId == supersedingModel.activeModuleId &&
        rejectedStaleWrite.activePageId == supersedingModel.activePageId &&
        generalProvider.writes == writesBeforeStale);

    // The UI schema is mods vertically, with the selected mod's pages as horizontal tabs.
    auto navigationModel = model;
    navigationModel.modules.push_back(MenuSession::Module{
        .moduleId = "other.module",
        .title = "Other Plugin",
        .firstPageId = "other",
    });

    // The game-thread router must be sufficient even when Scaleform keyboard events are absent.
    CHECK(MenuInputRouter::IsMenuKey(MenuInputRouter::kAccept));
    CHECK(!MenuInputRouter::IsMenuKey('V'));
    auto routed = MenuInputRouter::Route(model, MenuInputRouter::kAccept);
    CHECK(routed.command && routed.command->kind == CommandKind::Write &&
        routed.command->controlId == "enabled" &&
        routed.command->value.kind == ValueKind::Boolean &&
        routed.command->value.booleanValue == 0);
    routed = MenuInputRouter::Route(model, MenuInputRouter::kDown);
    CHECK(routed.command && routed.command->kind == CommandKind::SelectControl &&
        routed.command->controlId == "count");
    routed = MenuInputRouter::Route(model, MenuInputRouter::kUp);
    CHECK(routed.command && routed.command->kind == CommandKind::SelectControl &&
        routed.command->controlId == "save-run");
    model.selectedControlId = "name";
    routed = MenuInputRouter::Route(model, MenuInputRouter::kAccept);
    CHECK(routed.command &&
        routed.command->kind == CommandKind::BeginTextCapture);
    routed = MenuInputRouter::Route(navigationModel, MenuInputRouter::kNextPage);
    CHECK(routed.command && routed.command->kind == CommandKind::SelectPage &&
        routed.command->moduleId == "test.module" && routed.command->pageId == "bindings");
    auto bindingsModel = navigationModel;
    bindingsModel.activePageId = "bindings";
    routed = MenuInputRouter::Route(bindingsModel, MenuInputRouter::kNextPage);
    CHECK(routed.command && routed.command->kind == CommandKind::SelectPage &&
        routed.command->moduleId == "test.module" && routed.command->pageId == "general");
    routed = MenuInputRouter::Route(model, MenuInputRouter::kApply);
    CHECK(routed.command && routed.command->kind == CommandKind::Apply &&
        routed.command->pageId == "general" && routed.command->controlId.empty());
    routed = MenuInputRouter::Route(model, MenuInputRouter::kCancel);
    CHECK(routed.command && routed.command->kind == CommandKind::Cancel &&
        routed.command->pageId == "general");
    model.selectedControlId = "count";
    routed = MenuInputRouter::Route(model, MenuInputRouter::kIncrease);
    CHECK(routed.command && routed.command->kind == CommandKind::Write &&
        routed.command->value.integerValue == 4);
    routed = MenuInputRouter::Route(model, MenuInputRouter::kRight);
    CHECK(routed.handled && !routed.command &&
        routed.focus.region == MenuInputRouter::FocusRegion::Actions &&
        routed.focus.actionIndex == 0);
    routed = MenuInputRouter::Route(model, MenuInputRouter::kAccept, routed.focus);
    CHECK(routed.command && routed.command->kind == CommandKind::Apply);
    routed = MenuInputRouter::Route(model, MenuInputRouter::kLeft);
    routed = MenuInputRouter::Route(model, MenuInputRouter::kLeft, routed.focus);
    CHECK(routed.handled && !routed.command &&
        routed.focus.region == MenuInputRouter::FocusRegion::Modules);
    routed = MenuInputRouter::Route(navigationModel, MenuInputRouter::kDown, routed.focus);
    CHECK(routed.command && routed.command->kind == CommandKind::SelectPage &&
        routed.command->moduleId == "other.module" && routed.command->pageId == "other");
    model.selectedControlId = "broken";
    CHECK(!MenuInputRouter::Route(model, MenuInputRouter::kAccept).command);
    model.selectedControlId = "enabled";
    Model emptyModel;
    routed = MenuInputRouter::Route(emptyModel, MenuInputRouter::kEscape);
    CHECK(routed.command && routed.command->kind == CommandKind::Close);
    routed = MenuInputRouter::Route(emptyModel, MenuInputRouter::kTab);
    CHECK(routed.command && routed.command->kind == CommandKind::Close);
    auto decisionModel = emptyModel;
    decisionModel.dirtyDecisionActive = true;
    routed = MenuInputRouter::Route(decisionModel, MenuInputRouter::kEscape);
    CHECK(routed.command &&
        routed.command->kind == CommandKind::ResolveDirtyStay);
    routed = MenuInputRouter::Route(decisionModel, MenuInputRouter::kApply);
    CHECK(routed.command &&
        routed.command->kind == CommandKind::ResolveDirtyApply);
    routed = MenuInputRouter::Route(decisionModel, MenuInputRouter::kCancel);
    CHECK(routed.command &&
        routed.command->kind == CommandKind::ResolveDirtyDiscard);
    routed = MenuInputRouter::Route(decisionModel, MenuInputRouter::kDown);
    CHECK(!routed.command &&
        routed.focus.region == MenuInputRouter::FocusRegion::Actions &&
        routed.focus.actionIndex == 1);
    routed = MenuInputRouter::Route(
        decisionModel, MenuInputRouter::kAccept, routed.focus);
    CHECK(routed.command &&
        routed.command->kind == CommandKind::ResolveDirtyDiscard);
    model = session.Snapshot();

    model = session.Dispatch(MakeCommand(CommandKind::SelectControl, "general", "missing"));
    CHECK(!model.error.empty());
    model = session.Dispatch(MakeCommand(CommandKind::SelectControl, "general", "enabled"));
    CHECK(model.selectedControlId == "enabled");

    auto write = MakeCommand(CommandKind::Write, "general", "count");
    write.value.kind = ValueKind::String; strcpy_s(write.value.stringValue, "bad");
    model = session.Dispatch(write); CHECK(!model.error.empty() && generalProvider.writes == 0);
    write.value.kind = ValueKind::Integer; write.value.integerValue = 11;
    model = session.Dispatch(write); CHECK(!model.error.empty() && generalProvider.writes == 0);
    write.value.integerValue = 7;
    model = session.Dispatch(write); CHECK(model.dirty && generalProvider.writes == 1 && model.selectedControlId == "count");
    // A dirty transaction pins callback lifetime and makes unregistration a
    // deterministic, retryable rejection rather than stranding the session.
    CHECK(publicApi->unregisterModule("test.module") == Result::Rejected);

    // Route changes open the guarded decision while preserving the provider
    // draft and exact active control. Stay dismisses it without navigation.
    model = session.Dispatch(MakeCommand(CommandKind::SelectPage, "bindings"));
    CHECK(model.error.empty() && model.dirty && model.dirtyDecisionActive &&
        !model.dirtyDecisionClosesMenu && model.activePageId == "general" &&
        model.selectedControlId == "count");
    model = session.Dispatch(MakeCommand(CommandKind::ResolveDirtyStay, ""));
    CHECK(model.error.empty() && model.dirty && !model.dirtyDecisionActive &&
        model.activePageId == "general" && model.selectedControlId == "count");
    // Non-navigation commands still cannot bypass dirty-page ownership.
    CHECK(!session.Dispatch(MakeCommand(CommandKind::SelectControl, "bindings", "other")).error.empty());
    auto foreignWrite = MakeCommand(CommandKind::Write, "bindings", "other"); foreignWrite.value.kind = ValueKind::Boolean;
    CHECK(!session.Dispatch(foreignWrite).error.empty() && bindingsProvider.writes == 0);
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Apply, "bindings")).error.empty());
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Cancel, "bindings")).error.empty());

    CHECK(session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "enabled")).error.size() > 0 && generalProvider.invokes == 0);
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "locked")).error.empty() &&
        generalProvider.invokes == 0);
    model = session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "run"));
    CHECK(model.error.empty() && generalProvider.invokes == 1 && model.selectedControlId == "run");
    CHECK(session.Dispatch(MakeCommand(CommandKind::Apply, "general")).dirty == false && generalProvider.applies == 1);

    // Draft-mutating actions pin the same page transaction as writes and
    // compound edits. A successful action becomes dirty; Cancel resolves the
    // lease through the provider before unregister could succeed.
    const auto cancelsBeforeMutatingAction = generalProvider.cancels;
    generalProvider.failInvoke = true;
    model = session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "mutate"));
    CHECK(!model.error.empty() && !model.dirty && generalProvider.invokes == 2 &&
        generalProvider.cancels == cancelsBeforeMutatingAction + 1);
    generalProvider.failInvoke = false;
    model = session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "mutate"));
    CHECK(model.error.empty() && model.dirty && generalProvider.invokes == 3 &&
        publicApi->unregisterModule("test.module") == Result::Rejected);
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Cancel, "general")).dirty &&
        generalProvider.cancels == cancelsBeforeMutatingAction + 2);

    // The page registered through the legacy query receives the same transaction
    // semantics as the product-registered page.
    auto legacyWrite = MakeCommand(CommandKind::Write, "bindings", "other");
    legacyWrite.value.kind = ValueKind::Boolean;
    legacyWrite.value.booleanValue = 0;
    CHECK(session.Dispatch(legacyWrite).dirty && bindingsProvider.writes == 1);
    CHECK(session.Dispatch(MakeCommand(CommandKind::Cancel, "bindings")).dirty == false &&
        bindingsProvider.cancels == 1);

    auto nonFinite = MakeCommand(CommandKind::Write, "general", "scale");
    nonFinite.value.kind = ValueKind::Float; nonFinite.value.floatValue = std::numeric_limits<double>::quiet_NaN();
    CHECK(!session.Dispatch(nonFinite).error.empty() && generalProvider.writes == 1);

    auto binding = MakeCommand(CommandKind::Write, "general", "bind");
    binding.value.kind = ValueKind::String; strcpy_s(binding.value.stringValue, "Mouse1");
    const auto cancelsBeforeBinding = generalProvider.cancels;
    CHECK(session.Dispatch(binding).dirty && generalProvider.lastWrite.kind == ValueKind::String);
    CHECK(session.Dispatch(MakeCommand(CommandKind::Cancel, "general")).dirty == false &&
        generalProvider.cancels == cancelsBeforeBinding + 1);

    auto beginCapture = MakeCommand(CommandKind::BeginBindingCapture, "general", "bind");
    model = session.Dispatch(beginCapture);
    CHECK(model.bindingCaptureActive && model.captureControlId == "bind" &&
        session.BindingCaptureFlags() == generalControls[3].flags);
    CHECK(!session.Dispatch(MakeCommand(CommandKind::SelectPage, "bindings")).error.empty());
    model = session.CompleteBindingCapture(
        "keyboard:0x48;ctrl=1;alt=1;shift=0");
    CHECK(!model.bindingCaptureActive && model.dirty && generalProvider.writes == 3 &&
        std::strcmp(generalProvider.lastWrite.stringValue,
            "keyboard:0x48;ctrl=1;alt=1;shift=0") == 0);
    const auto cancelsBeforeCapturedBinding = generalProvider.cancels;
    CHECK(session.Dispatch(MakeCommand(CommandKind::Cancel, "general")).dirty == false &&
        generalProvider.cancels == cancelsBeforeCapturedBinding + 1);

    model = session.Dispatch(beginCapture);
    CHECK(model.bindingCaptureActive);
    model = session.CancelBindingCapture();
    CHECK(!model.bindingCaptureActive && model.error.empty());

    auto beginText = MakeCommand(CommandKind::BeginTextCapture, "general", "name");
    model = session.Dispatch(beginText);
    CHECK(model.textCaptureActive && model.captureControlId == "name");
    model = session.BackspaceTextCapture();
    CHECK(model.textCaptureActive);
    for (const char character : std::string_view{"Power"}) {
        model = session.AppendTextCapture(character);
        CHECK(model.textCaptureActive && model.error.empty());
    }
    const auto writesBeforeText = generalProvider.writes;
    model = session.CompleteTextCapture();
    CHECK(!model.textCaptureActive && model.dirty &&
        generalProvider.writes == writesBeforeText + 1 &&
        std::strcmp(generalProvider.lastWrite.stringValue, "Power") == 0);
    // Apply-before-invoke is one host-ordered operation. Failed persistence
    // suppresses the action and keeps the draft pinned; success cleans the
    // transaction before the provider action runs.
    const auto appliesBeforeSaveRun = generalProvider.applies;
    const auto invokesBeforeSaveRun = generalProvider.invokes;
    generalProvider.failApply = true;
    model = session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "save-run"));
    CHECK(!model.error.empty() && model.dirty &&
        generalProvider.applies == appliesBeforeSaveRun + 1 &&
        generalProvider.invokes == invokesBeforeSaveRun);
    generalProvider.failApply = false;
    model = session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "save-run"));
    CHECK(model.error.empty() && !model.dirty &&
        generalProvider.applies == appliesBeforeSaveRun + 2 &&
        generalProvider.invokes == invokesBeforeSaveRun + 1);

    model = session.Dispatch(beginText);
    CHECK(model.textCaptureActive);
    model = session.CancelTextCapture();
    CHECK(!model.textCaptureActive && model.error.empty());

    write.value.kind = ValueKind::Integer; write.value.integerValue = 4;
    const auto appliesBeforeClose = generalProvider.applies;
    CHECK(session.Dispatch(write).dirty);
    model = session.Dispatch(MakeCommand(CommandKind::Close, ""));
    CHECK(model.dirty && model.dirtyDecisionActive &&
        model.dirtyDecisionClosesMenu && !model.closeRequested);
    generalProvider.failApply = true;
    model = session.Dispatch(MakeCommand(CommandKind::ResolveDirtyApply, ""));
    CHECK(!model.error.empty() && model.dirty && model.dirtyDecisionActive &&
        !model.closeRequested && generalProvider.applies == appliesBeforeClose + 1);
    generalProvider.failApply = false;
    model = session.Dispatch(MakeCommand(CommandKind::ResolveDirtyApply, ""));
    CHECK(model.error.empty() && !model.dirty && !model.dirtyDecisionActive &&
        model.closeRequested && generalProvider.applies == appliesBeforeClose + 2);

    // A compound edit is attached to the same page transaction as scalar
    // controls. It pins provider lifetime, marks the page dirty, and resolves
    // only through that page's ordinary Apply/Cancel callbacks.
    namespace Live = AbsoluteControlPanelExperimental;
    const auto* liveApi = AbsoluteControlPanel_QueryLiveComponentsExperimental(
        Live::kAbiVersion);
    CHECK(liveApi && liveApi->registerLiveChannel);
    CompoundProvider compoundProvider;
    compoundProvider.frame.kind = Live::ComponentKind::SegmentedAllocationGrid;
    compoundProvider.frame.sequence = 1;
    compoundProvider.frame.monotonicTimestampUs = 1;
    compoundProvider.frame.segmentedGrid.columnCount = 1;
    compoundProvider.frame.segmentedGrid.columns[0].segmentCount = 4;
    compoundProvider.frame.segmentedGrid.columns[0].maximumCount = 4;
    for (std::size_t index = 0; index < 4; ++index) {
        compoundProvider.frame.segmentedGrid.columns[0].segments[index].interactive = 1;
    }
    Live::LiveChannelDescriptorV1 channel;
    strcpy_s(channel.moduleId, "test.module");
    strcpy_s(channel.pageId, "general");
    strcpy_s(channel.channelId, "power");
    strcpy_s(channel.title, "Power grid");
    channel.kind = Live::ComponentKind::SegmentedAllocationGrid;
    channel.context = &compoundProvider;
    channel.readLiveFrame = &ReadCompoundFrame;
    channel.applyCompoundOperation = &ApplyCompoundOperation;
    channel.flags = Live::kSegmentedGridCycleOnClick;
    strcpy_s(channel.segmentedGrid.controlId, "allocation");
    channel.segmentedGrid.columnCount = 1;
    channel.segmentedGrid.tierCount = 2;
    strcpy_s(channel.segmentedGrid.columns[0].columnId, "engines");
    strcpy_s(channel.segmentedGrid.columns[0].label, "Engines");
    channel.segmentedGrid.columns[0].maximumSegments = 4;
    strcpy_s(channel.segmentedGrid.tiers[0].tierId, "hollow");
    strcpy_s(channel.segmentedGrid.tiers[0].label, "Hollow");
    strcpy_s(channel.segmentedGrid.tiers[1].tierId, "green");
    strcpy_s(channel.segmentedGrid.tiers[1].label, "Green");
    CHECK(liveApi->registerLiveChannel(&channel) == Live::Result::Ok);
    LiveComponents::HostRegistry().SetMenuActive(true);

    Session compoundSession;
    auto compoundModel = compoundSession.Snapshot();
    CHECK(!compoundModel.pages.empty() &&
        compoundModel.pages[0].liveComponents.size() == 1 &&
        compoundModel.pages[0].liveComponents[0].available &&
        compoundModel.pages[0].liveComponents[0].descriptor.flags ==
            Live::kSegmentedGridCycleOnClick);
    auto gridFocus = MenuInputRouter::FocusState{
        .region = MenuInputRouter::FocusRegion::Grid };
    auto gridRoute = MenuInputRouter::Route(
        compoundModel, MenuInputRouter::kRight, gridFocus);
    CHECK(gridRoute.command && gridRoute.command->kind == CommandKind::Compound &&
        gridRoute.command->compoundKind ==
            Live::CompoundOperationKind::SetSegmentCount &&
        gridRoute.command->columnId == "engines" &&
        gridRoute.command->tierId == "green" && gridRoute.command->count == 1);
    gridRoute = MenuInputRouter::Route(
        compoundModel, MenuInputRouter::kDown, gridFocus);
    CHECK(gridRoute.command && gridRoute.command->kind ==
        CommandKind::SelectGridColumn && gridRoute.command->columnId == "engines");
    auto controlsToGrid = MenuInputRouter::Route(
        compoundModel, MenuInputRouter::kLeft);
    CHECK(!controlsToGrid.command && controlsToGrid.focus.region ==
        MenuInputRouter::FocusRegion::Grid);
    auto compound = MakeCommand(CommandKind::Compound, "general", "allocation");
    compound.channelId = "power";
    compound.columnId = "engines";
    compound.tierId = "green";
    compound.compoundKind = Live::CompoundOperationKind::SetSegmentCount;
    compound.count = 1;
    const auto appliesBeforeCompound = generalProvider.applies;
    compoundModel = compoundSession.Dispatch(compound);
    CHECK(compoundModel.error.empty() && compoundModel.dirty &&
        compoundProvider.operations == 1);
    CHECK(publicApi->unregisterModule("test.module") == Result::Rejected);
    compoundModel = compoundSession.Dispatch(
        MakeCommand(CommandKind::Apply, "general"));
    CHECK(compoundModel.error.empty() && !compoundModel.dirty &&
        generalProvider.applies == appliesBeforeCompound + 1);
    const auto cancelsBeforeCompound = generalProvider.cancels;
    compoundModel = compoundSession.Dispatch(compound);
    CHECK(compoundModel.dirty && compoundProvider.operations == 2);
    compoundModel = compoundSession.Dispatch(
        MakeCommand(CommandKind::Cancel, "general"));
    CHECK(!compoundModel.dirty &&
        generalProvider.cancels == cancelsBeforeCompound + 1);
    CHECK(liveApi->unregisterModule("test.module") == Live::Result::Ok);
    LiveComponents::HostRegistry().SetMenuActive(false);

    ControlDescriptorV1 noRollbackControls[]{ MakeDescriptor(ControlKind::Toggle, "unsafe") };
    PageDescriptorV1 noRollback;
    strcpy_s(noRollback.moduleId, "test.module"); strcpy_s(noRollback.pageId, "unsafe"); strcpy_s(noRollback.displayName, "Unsafe");
    noRollback.controlCount = 1; noRollback.controls = noRollbackControls; noRollback.context = &noRollbackProvider;
    noRollback.readValue = &ReadValue; noRollback.writeDraft = &WriteDraft;
    // Editable pages must provide the complete Apply/Cancel transaction contract,
    // preventing a successful write from creating an uncloseable menu.
    CHECK(api->registerPage(&noRollback) == Result::InvalidArgument);
    PageDescriptorV1 oversized = noRollback;
    oversized.controlCount =
        static_cast<std::uint32_t>(MenuApiHost::kMaximumControlsPerPage + 1);
    CHECK(api->registerPage(&oversized) == Result::CapacityExceeded);

    auto invalidFlagControl = MakeDescriptor(ControlKind::Toggle, "invalid.flag");
    invalidFlagControl.flags = 1U << 31;
    PageDescriptorV1 invalidFlagPage;
    strcpy_s(invalidFlagPage.moduleId, "test.module");
    strcpy_s(invalidFlagPage.pageId, "invalid-flags");
    strcpy_s(invalidFlagPage.displayName, "Invalid Flags");
    invalidFlagPage.controlCount = 1;
    invalidFlagPage.controls = &invalidFlagControl;
    invalidFlagPage.context = &generalProvider;
    invalidFlagPage.readValue = &ReadValue;
    CHECK(api->registerPage(&invalidFlagPage) == Result::InvalidArgument);
    invalidFlagControl.flags = kBindingKeyboard;
    CHECK(api->registerPage(&invalidFlagPage) == Result::InvalidArgument);
    invalidFlagControl.flags = kControlAppliesDraftBeforeInvoke;
    CHECK(api->registerPage(&invalidFlagPage) == Result::InvalidArgument);
    invalidFlagControl.flags = kControlLayoutInline;
    CHECK(api->registerPage(&invalidFlagPage) == Result::InvalidArgument);
    invalidFlagControl = MakeDescriptor(ControlKind::Action, "invalid.action-flags");
    invalidFlagControl.flags =
        kControlMutatesDraft | kControlAppliesDraftBeforeInvoke;
    CHECK(api->registerPage(&invalidFlagPage) == Result::InvalidArgument);

    ModuleDescriptorV1 sectionsModule;
    strcpy_s(sectionsModule.moduleId, "sections.module");
    strcpy_s(sectionsModule.displayName, "Section Provider");
    CHECK(publicApi->registerModule(&sectionsModule) == Result::Ok);
    Provider sectionsProvider;
    ControlDescriptorV1 sectionControls[]{
        MakeDescriptor(ControlKind::GroupHeader, "profile-identity"),
        MakeDescriptor(ControlKind::Action, "previous"),
        MakeDescriptor(ControlKind::Action, "next")
    };
    sectionControls[1].flags = kControlLayoutInline;
    sectionControls[2].flags = kControlLayoutInline;
    PageDescriptorV1 sectionsPage;
    strcpy_s(sectionsPage.moduleId, "sections.module");
    strcpy_s(sectionsPage.pageId, "layout");
    strcpy_s(sectionsPage.displayName, "Layout");
    sectionsPage.controlCount = static_cast<std::uint32_t>(
        std::size(sectionControls));
    sectionsPage.controls = sectionControls;
    sectionsPage.context = &sectionsProvider;
    sectionsPage.invokeAction = &Invoke;
    CHECK(publicApi->registerPage(&sectionsPage) == Result::Ok);
    Session sectionsSession;
    Command selectSections;
    selectSections.kind = CommandKind::SelectPage;
    selectSections.moduleId = "sections.module";
    selectSections.pageId = "layout";
    auto sectionsModel = sectionsSession.Dispatch(selectSections);
    CHECK(sectionsModel.pages.size() == 1 &&
        sectionsModel.pages[0].controls[0].available &&
        sectionsProvider.reads == 0);
    auto sectionRoute = MenuInputRouter::Route(
        sectionsModel, MenuInputRouter::kUp);
    CHECK(sectionRoute.command && sectionRoute.command->controlId == "next");
    CHECK(publicApi->unregisterModule("sections.module") == Result::Ok);

    // A transient labeled Choice is provider-owned navigation state: all
    // options reach the model, the write succeeds, and no transaction/dirty
    // state is created. Such a page does not need Apply/Cancel callbacks.
    ModuleDescriptorV1 choicesModule;
    strcpy_s(choicesModule.moduleId, "choices.module");
    strcpy_s(choicesModule.displayName, "Choice Provider");
    CHECK(publicApi->registerModule(&choicesModule) == Result::Ok);
    Provider choicesProvider;
    auto choiceControl = MakeDescriptor(ControlKind::Choice,
        "profile", 0, 10, 1);
    choiceControl.flags = kControlTransientChoice;
    PageDescriptorV1 choicesPage;
    strcpy_s(choicesPage.moduleId, "choices.module");
    strcpy_s(choicesPage.pageId, "profiles");
    strcpy_s(choicesPage.displayName, "Profiles");
    choicesPage.controlCount = 1;
    choicesPage.controls = &choiceControl;
    choicesPage.context = &choicesProvider;
    choicesPage.readValue = &ReadValue;
    choicesPage.writeDraft = &WriteDraft;
    choicesPage.readChoiceOptions = &ReadChoiceOptions;
    CHECK(publicApi->registerPage(&choicesPage) == Result::Ok);
    Session choiceSession;
    Command selectChoices;
    selectChoices.kind = CommandKind::SelectPage;
    selectChoices.moduleId = "choices.module";
    selectChoices.pageId = "profiles";
    auto choiceModel = choiceSession.Dispatch(selectChoices);
    CHECK(choiceModel.error.empty() && choiceModel.pages.size() == 1 &&
        choiceModel.pages[0].controls.size() == 1 &&
        choiceModel.pages[0].controls[0].choiceOptions.size() == 2 &&
        choiceModel.pages[0].controls[0].choiceOptions[0].label == "Cruise" &&
        choiceModel.pages[0].controls[0].choiceOptions[1].value == 7);
    Command chooseCombat;
    chooseCombat.kind = CommandKind::Write;
    chooseCombat.moduleId = "choices.module";
    chooseCombat.pageId = "profiles";
    chooseCombat.controlId = "profile";
    chooseCombat.value.kind = ValueKind::Integer;
    chooseCombat.value.integerValue = 7;
    choiceModel = choiceSession.Dispatch(chooseCombat);
    CHECK(choiceModel.error.empty() && !choiceModel.dirty &&
        choicesProvider.writes == 1 &&
        choicesProvider.lastWrite.integerValue == 7);
    CHECK(publicApi->unregisterModule("choices.module") == Result::Ok);

    // Device recording is provider-owned while the shell owns the capture
    // session, navigation lock, draft write, and cancellation surface.
    ModuleDescriptorV1 captureModule;
    strcpy_s(captureModule.moduleId, "capture.module");
    strcpy_s(captureModule.displayName, "Capture Provider");
    CHECK(publicApi->registerModule(&captureModule) == Result::Ok);
    Provider captureProvider;
    auto controllerControl = MakeDescriptor(ControlKind::InputBinding,
        "controller-action");
    controllerControl.flags = kBindingController | kBindingClearable;
    PageDescriptorV1 capturePage;
    strcpy_s(capturePage.moduleId, "capture.module");
    strcpy_s(capturePage.pageId, "bindings");
    strcpy_s(capturePage.displayName, "Bindings");
    capturePage.controlCount = 1;
    capturePage.controls = &controllerControl;
    capturePage.context = &captureProvider;
    capturePage.readValue = &ReadValue;
    capturePage.writeDraft = &WriteDraft;
    capturePage.apply = &Apply;
    capturePage.cancel = &Cancel;
    capturePage.beginBindingCapture = &BeginProviderCapture;
    // A current-size descriptor must provide the complete capture callback set.
    CHECK(publicApi->registerPage(&capturePage) == Result::InvalidArgument);
    capturePage.pollBindingCapture = &PollProviderCapture;
    capturePage.cancelBindingCapture = &CancelProviderCapture;
    capturePage.reassignBinding = &ReassignProviderBinding;
    CHECK(publicApi->registerPage(&capturePage) == Result::Ok);

    Session captureSession;
    Command beginProviderCapture;
    beginProviderCapture.kind = CommandKind::BeginBindingCapture;
    beginProviderCapture.moduleId = "capture.module";
    beginProviderCapture.pageId = "bindings";
    beginProviderCapture.controlId = "controller-action";
    auto captureModel = captureSession.Dispatch(beginProviderCapture);
    CHECK(captureModel.bindingCaptureActive &&
        captureProvider.captureBegins == 1);
    CHECK(!captureSession.RefreshBindingCapture().has_value() &&
        captureProvider.capturePolls == 1);
    captureProvider.captureState =
        AbsoluteControlPanelApi::BindingCaptureState::Captured;
    captureProvider.captureBinding =
        "{12345678-1234-1234-1234-123456789ABC}@button:7";
    auto capturedModel = captureSession.RefreshBindingCapture();
    CHECK(capturedModel && !capturedModel->bindingCaptureActive &&
        capturedModel->dirty && captureProvider.writes == 1 &&
        captureProvider.captureCancels == 0 &&
        std::string_view(captureProvider.lastWrite.stringValue) ==
            captureProvider.captureBinding);
    Command cancelCaptureDraft;
    cancelCaptureDraft.kind = CommandKind::Cancel;
    cancelCaptureDraft.moduleId = "capture.module";
    cancelCaptureDraft.pageId = "bindings";
    captureModel = captureSession.Dispatch(cancelCaptureDraft);
    CHECK(captureModel.error.empty() && !captureModel.dirty &&
        captureProvider.cancels == 1);

    captureModel = captureSession.Dispatch(beginProviderCapture);
    CHECK(captureModel.bindingCaptureActive &&
        captureProvider.captureBegins == 2);
    captureModel = captureSession.CancelBindingCapture();
    CHECK(!captureModel.bindingCaptureActive && captureModel.error.empty() &&
        captureProvider.captureCancels == 1);

    captureModel = captureSession.Dispatch(beginProviderCapture);
    captureProvider.captureState =
        AbsoluteControlPanelApi::BindingCaptureState::TimedOut;
    captureProvider.captureDetail = "No controller input received";
    auto timedOutModel = captureSession.RefreshBindingCapture();
    CHECK(timedOutModel && !timedOutModel->bindingCaptureActive &&
        timedOutModel->error == "No controller input received" &&
        captureProvider.captureCancels == 1);

    captureModel = captureSession.Dispatch(beginProviderCapture);
    captureProvider.captureState =
        AbsoluteControlPanelApi::BindingCaptureState::Error;
    captureProvider.captureBinding = "controller:button:12";
    captureProvider.captureDetail = "Binding already in use by 'Combat'";
    auto conflictModel = captureSession.RefreshBindingCapture();
    CHECK(conflictModel && conflictModel->bindingConflictActive &&
        !conflictModel->bindingCaptureActive && conflictModel->error.empty() &&
        conflictModel->bindingConflictDetail == captureProvider.captureDetail);
    auto conflictRoute = MenuInputRouter::Route(
        *conflictModel, MenuInputRouter::kDown);
    CHECK(!conflictRoute.command && conflictRoute.focus.actionIndex == 1);
    conflictRoute = MenuInputRouter::Route(
        *conflictModel, MenuInputRouter::kAccept,
        MenuInputRouter::FocusState{
            .region = MenuInputRouter::FocusRegion::Actions,
            .actionIndex = 0 });
    CHECK(conflictRoute.command && conflictRoute.command->kind ==
        CommandKind::ResolveBindingReassign);
    captureModel = captureSession.Dispatch(*conflictRoute.command);
    CHECK(!captureModel.bindingConflictActive && captureModel.dirty &&
        captureProvider.reassigns == 1 &&
        captureProvider.reassignedBinding == "controller:button:12");
    captureModel = captureSession.Dispatch(cancelCaptureDraft);
    CHECK(!captureModel.dirty && captureModel.error.empty());

    captureModel = captureSession.Dispatch(beginProviderCapture);
    captureProvider.captureState =
        AbsoluteControlPanelApi::BindingCaptureState::Error;
    captureProvider.captureBinding = "controller:button:13";
    captureProvider.captureDetail = "Binding already in use";
    conflictModel = captureSession.RefreshBindingCapture();
    CHECK(conflictModel && conflictModel->bindingConflictActive);
    Command dismissConflict;
    dismissConflict.kind = CommandKind::ResolveBindingCancel;
    captureModel = captureSession.Dispatch(dismissConflict);
    CHECK(!captureModel.bindingConflictActive && !captureModel.dirty &&
        captureProvider.reassigns == 1);

    captureModel = captureSession.Dispatch(beginProviderCapture);
    captureProvider.captureState =
        AbsoluteControlPanelApi::BindingCaptureState::Captured;
    captureProvider.captureBinding = "controller:button:14";
    captureProvider.duplicateWrites = true;
    conflictModel = captureSession.RefreshBindingCapture();
    captureProvider.duplicateWrites = false;
    CHECK(conflictModel && conflictModel->bindingConflictActive &&
        conflictModel->bindingConflictDetail.find("already assigned") !=
            std::string::npos);
    captureModel = captureSession.Dispatch(dismissConflict);
    CHECK(!captureModel.bindingConflictActive && !captureModel.dirty);
    CHECK(publicApi->unregisterModule("capture.module") == Result::Ok);

    // Destruction is an abnormal close path: clean sessions are inert, while a
    // dirty session rolls back exactly once before releasing its transaction pin.
    {
        const auto cancelsBefore = generalProvider.cancels;
        { Session cleanSession; (void)cleanSession.Snapshot(); }
        CHECK(generalProvider.cancels == cancelsBefore);
    }
    {
        const auto cancelsBefore = generalProvider.cancels;
        {
            Session abandonedSession;
            auto abandonedWrite = MakeCommand(CommandKind::Write, "general", "enabled");
            abandonedWrite.value.kind = ValueKind::Boolean;
            CHECK(abandonedSession.Dispatch(abandonedWrite).dirty);
        }
        CHECK(generalProvider.cancels == cancelsBefore + 1);
    }
    {
        const auto cancelsBefore = generalProvider.cancels;
        int cancelsAfterNormal{};
        {
            Session cancelledSession;
            auto cancelledWrite = MakeCommand(CommandKind::Write, "general", "enabled");
            cancelledWrite.value.kind = ValueKind::Boolean;
            CHECK(cancelledSession.Dispatch(cancelledWrite).dirty);
            CHECK(!cancelledSession.Dispatch(
                MakeCommand(CommandKind::Cancel, "general")).dirty);
            cancelsAfterNormal = generalProvider.cancels;
        }
        CHECK(cancelsAfterNormal == cancelsBefore + 1);
        CHECK(generalProvider.cancels == cancelsAfterNormal);
    }
    {
        const auto cancelsBefore = generalProvider.cancels;
        Session hiddenSession;
        auto hiddenWrite = MakeCommand(CommandKind::Write, "general", "enabled");
        hiddenWrite.value.kind = ValueKind::Boolean;
        CHECK(hiddenSession.Dispatch(hiddenWrite).dirty);
        hiddenSession.Teardown();
        hiddenSession.Teardown();
        CHECK(generalProvider.cancels == cancelsBefore + 1);
    }
    {
        const auto cancelsBefore = generalProvider.cancels;
        int cancelsAfterNormal{};
        {
            Session closedSession;
            auto closedWrite = MakeCommand(CommandKind::Write, "general", "enabled");
            closedWrite.value.kind = ValueKind::Boolean;
            CHECK(closedSession.Dispatch(closedWrite).dirty);
            auto closeDecision = closedSession.Dispatch(
                MakeCommand(CommandKind::Close, ""));
            CHECK(closeDecision.dirty && closeDecision.dirtyDecisionActive &&
                !closeDecision.closeRequested);
            closeDecision = closedSession.Dispatch(
                MakeCommand(CommandKind::ResolveDirtyDiscard, ""));
            CHECK(!closeDecision.dirty && !closeDecision.dirtyDecisionActive &&
                closeDecision.closeRequested);
            cancelsAfterNormal = generalProvider.cancels;
        }
        CHECK(cancelsAfterNormal == cancelsBefore + 1);
        CHECK(generalProvider.cancels == cancelsAfterNormal);
    }

    ModuleDescriptorV1 teardownModule;
    strcpy_s(teardownModule.moduleId, "teardown.module");
    strcpy_s(teardownModule.displayName, "Teardown Provider");
    CHECK(api->registerModule(&teardownModule) == Result::Ok);
    BlockingCancelProvider teardownProvider;
    auto teardownControl = MakeDescriptor(ControlKind::Toggle, "enabled");
    PageDescriptorV1 teardownPage;
    strcpy_s(teardownPage.moduleId, "teardown.module");
    strcpy_s(teardownPage.pageId, "general");
    strcpy_s(teardownPage.displayName, "General");
    teardownPage.controlCount = 1;
    teardownPage.controls = &teardownControl;
    teardownPage.context = &teardownProvider;
    teardownPage.readValue = &TeardownRead;
    teardownPage.writeDraft = &TeardownWrite;
    teardownPage.apply = &TeardownApply;
    teardownPage.cancel = &BlockingCancel;
    CHECK(api->registerPage(&teardownPage) == Result::Ok);

    auto teardownSession = std::make_unique<Session>();
    Command teardownWrite;
    teardownWrite.kind = CommandKind::Write;
    teardownWrite.moduleId = "teardown.module";
    teardownWrite.pageId = "general";
    teardownWrite.controlId = "enabled";
    teardownWrite.value.kind = ValueKind::Boolean;
    CHECK(teardownSession->Dispatch(teardownWrite).dirty);
    std::thread teardownThread{ [ownedSession = std::move(teardownSession)]() mutable {
        ownedSession.reset();
    } };
    {
        std::unique_lock lock{ teardownProvider.mutex };
        teardownProvider.condition.wait(lock, [&] { return teardownProvider.entered; });
    }
    CHECK(api->unregisterModule("teardown.module") == Result::Rejected);
    {
        std::scoped_lock lock{ teardownProvider.mutex };
        teardownProvider.release = true;
    }
    teardownProvider.condition.notify_all();
    teardownThread.join();
    CHECK(teardownProvider.cancels == 1);
    CHECK(api->unregisterModule("teardown.module") == Result::Ok);

    const auto beforeRefresh = MenuApiHost::Revision();
    auto refreshCursor = MenuApiHost::RefreshRevision();
    CHECK(api->requestRefresh("test.module", "general") == Result::Ok && MenuApiHost::Revision() == beforeRefresh + 1);
    CHECK(MenuApiHost::ConsumeRefresh(refreshCursor));
    CHECK(!MenuApiHost::ConsumeRefresh(refreshCursor));
    CHECK(api->requestRefresh("test.module", "missing") == Result::NotFound);
    auto scopedRefreshCursor = MenuApiHost::RefreshRevision();
    CHECK(api->requestRefresh("test.module", "bindings") == Result::Ok);
    CHECK(!MenuApiHost::ConsumeRefresh(
        scopedRefreshCursor, "test.module", "general"));
    CHECK(api->requestRefresh("test.module", "general") == Result::Ok);
    CHECK(MenuApiHost::ConsumeRefresh(
        scopedRefreshCursor, "test.module", "general"));

    ModuleDescriptorV1 blockingModule;
    strcpy_s(blockingModule.moduleId, "blocking.module");
    strcpy_s(blockingModule.displayName, "Blocking Provider");
    auto directoryRefreshCursor = MenuApiHost::RefreshRevision();
    CHECK(api->registerModule(&blockingModule) == Result::Ok);
    CHECK(MenuApiHost::ConsumeRefresh(
        directoryRefreshCursor, "test.module", "general"));
    BlockingProvider blockingProvider;
    auto blockingControl = MakeDescriptor(ControlKind::Toggle, "status");
    blockingControl.flags = kControlReadOnly;
    PageDescriptorV1 blockingPage;
    strcpy_s(blockingPage.moduleId, "blocking.module");
    strcpy_s(blockingPage.pageId, "status");
    strcpy_s(blockingPage.displayName, "Status");
    blockingPage.controlCount = 1;
    blockingPage.controls = &blockingControl;
    blockingPage.context = &blockingProvider;
    blockingPage.readValue = &BlockingRead;
    CHECK(api->registerPage(&blockingPage) == Result::Ok);
    const auto blockingSnapshot = MenuApiHost::FindPage("blocking.module", "status");
    CHECK(blockingSnapshot);
    bool callbackSucceeded{};
    std::thread callbackThread{ [&] {
        ValueV1 value;
        callbackSucceeded =
            MenuApiHost::ReadValue(*blockingSnapshot, "status", value) == Result::Ok;
    } };
    {
        std::unique_lock lock{ blockingProvider.mutex };
        blockingProvider.condition.wait(lock, [&] { return blockingProvider.entered; });
    }
    CHECK(api->unregisterModule("blocking.module") == Result::Rejected);
    {
        std::scoped_lock lock{ blockingProvider.mutex };
        blockingProvider.release = true;
    }
    blockingProvider.condition.notify_all();
    callbackThread.join();
    CHECK(callbackSucceeded);
    CHECK(api->unregisterModule("blocking.module") == Result::Ok);
    ValueV1 retiredValue;
    CHECK(MenuApiHost::ReadValue(*blockingSnapshot, "status", retiredValue) ==
        Result::Rejected);

    const auto beforeUnregister = MenuApiHost::Revision();
    CHECK(publicApi->unregisterModule("test.module") == Result::Ok && MenuApiHost::Revision() == beforeUnregister + 1);
    CHECK(api->unregisterModule("test.module") == Result::NotFound);

    // One subscriber remains capped at 512 registered controls even though the
    // host now admits a much larger aggregate catalog. Only the active page's
    // controls enter an ordinary render snapshot.
    ModuleDescriptorV1 capacityModule;
    strcpy_s(capacityModule.moduleId, "capacity.module");
    strcpy_s(capacityModule.displayName, "Capacity Module");
    CHECK(api->registerModule(&capacityModule) == Result::Ok);
    Provider capacityProvider;
    std::vector<ControlDescriptorV1> capacityControls(
        MenuApiHost::kMaximumControlsPerPage);
    for (std::size_t controlIndex{}; controlIndex < capacityControls.size(); ++controlIndex) {
        capacityControls[controlIndex] = MakeDescriptor(ControlKind::Action, "action");
        sprintf_s(capacityControls[controlIndex].controlId, "action.%03zu", controlIndex);
        sprintf_s(capacityControls[controlIndex].label, "Action %03zu", controlIndex);
    }
    constexpr std::size_t kFullPages = MenuApiHost::kMaximumControlsPerModule /
        MenuApiHost::kMaximumControlsPerPage;
    for (std::size_t pageIndex{}; pageIndex < kFullPages; ++pageIndex) {
        PageDescriptorV1 page;
        strcpy_s(page.moduleId, "capacity.module");
        sprintf_s(page.pageId, "page.%02zu", pageIndex);
        sprintf_s(page.displayName, "Page %02zu", pageIndex);
        page.controlCount = static_cast<std::uint32_t>(capacityControls.size());
        page.controls = capacityControls.data();
        page.context = &capacityProvider;
        page.invokeAction = &Invoke;
        CHECK(api->registerPage(&page) == Result::Ok);
    }
    Session fullControlSession;
    const auto fullControlModel = fullControlSession.Snapshot();
    std::size_t serializedControls{};
    for (const auto& page : fullControlModel.pages) serializedControls += page.controls.size();
    CHECK(fullControlModel.error.empty() &&
        fullControlModel.pages.size() == kFullPages &&
        serializedControls == MenuApiHost::kMaximumControlsPerPage);
    PageDescriptorV1 excessPage;
    strcpy_s(excessPage.moduleId, "capacity.module");
    strcpy_s(excessPage.pageId, "excess");
    strcpy_s(excessPage.displayName, "Excess");
    excessPage.controlCount = 1;
    excessPage.controls = capacityControls.data();
    excessPage.context = &capacityProvider;
    excessPage.invokeAction = &Invoke;
    CHECK(api->registerPage(&excessPage) == Result::CapacityExceeded);
    CHECK(api->unregisterModule("capacity.module") == Result::Ok);

    // Scalability acceptance envelope: 512 modules, three pages per module,
    // and 16 controls per page (24,576 registered controls). Snapshotting must
    // read and carry only the active page's 16 values.
    constexpr std::size_t kPagesPerCapacityModule = 3;
    constexpr std::size_t kControlsPerCapacityPage = 16;
    std::vector<ControlDescriptorV1> moduleControls(kControlsPerCapacityPage);
    for (std::size_t controlIndex{}; controlIndex < moduleControls.size(); ++controlIndex) {
        moduleControls[controlIndex] = MakeDescriptor(ControlKind::Toggle, "value");
        moduleControls[controlIndex].flags = kControlReadOnly;
        sprintf_s(moduleControls[controlIndex].controlId, "value.%02zu", controlIndex);
        sprintf_s(moduleControls[controlIndex].label, "Value %02zu", controlIndex);
    }
    capacityProvider.reads = 0;
    for (std::size_t moduleIndex{}; moduleIndex < MenuApiHost::kMaximumModules;
         ++moduleIndex) {
        ModuleDescriptorV1 capacityRegistration;
        sprintf_s(
            capacityRegistration.moduleId, "capacity.module.%02zu", moduleIndex);
        sprintf_s(capacityRegistration.displayName,
            "Capacity Module %03zu", moduleIndex);
        CHECK(api->registerModule(&capacityRegistration) == Result::Ok);
        for (std::size_t pageIndex{}; pageIndex < kPagesPerCapacityModule;
             ++pageIndex) {
            PageDescriptorV1 page;
            strcpy_s(page.moduleId, capacityRegistration.moduleId);
            sprintf_s(page.pageId, "page.%zu", pageIndex);
            sprintf_s(page.displayName, "Page %zu", pageIndex);
            page.controlCount = static_cast<std::uint32_t>(moduleControls.size());
            page.controls = moduleControls.data();
            page.context = &capacityProvider;
            page.readValue = &ReadValue;
            CHECK(api->registerPage(&page) == Result::Ok);
        }
    }
    Session fullGraphSession;
    const auto fullGraphModel = fullGraphSession.Snapshot();
    serializedControls = 0;
    for (const auto& page : fullGraphModel.pages) serializedControls += page.controls.size();
    CHECK(fullGraphModel.error.empty() &&
        fullGraphModel.modules.size() == MenuApiHost::kMaximumModules &&
        fullGraphModel.pages.size() == kPagesPerCapacityModule &&
        serializedControls == kControlsPerCapacityPage &&
        capacityProvider.reads == static_cast<int>(kControlsPerCapacityPage));
    ModuleDescriptorV1 overflowModule;
    strcpy_s(overflowModule.moduleId, "capacity.module.overflow");
    strcpy_s(overflowModule.displayName, "Overflow Module");
    CHECK(api->registerModule(&overflowModule) == Result::CapacityExceeded);
    for (std::size_t moduleIndex{}; moduleIndex < MenuApiHost::kMaximumModules;
         ++moduleIndex) {
        char moduleId[AbsoluteControlPanelApi::kIdentifierCapacity]{};
        sprintf_s(moduleId, "capacity.module.%02zu", moduleIndex);
        CHECK(api->unregisterModule(moduleId) == Result::Ok);
    }

    MenuApiHost::MarkRuntimeRejected();
    CHECK(MenuApiHost::Lifecycle() == MenuApiHost::HostLifecycle::Rejected);
    CHECK(api->registerModule(&module) == Result::Rejected);
    CHECK(publicApi->requestRefresh("test.module", "general") == Result::Rejected);
    return 0;
}
