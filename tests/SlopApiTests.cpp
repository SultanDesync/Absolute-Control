#include "MenuApiHost.h"
#include "MenuSession.h"
#include "SlopAPI.h"

#include <cmath>
#include <cstring>
#include <limits>

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
        bool wrongReadKind{};
        SlopApi::ValueV1 lastWrite{};
    };

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
        if (strcmp(a_control, "enabled") == 0) {
            a_value->kind = SlopApi::ValueKind::Boolean;
            a_value->booleanValue = 1;
        } else if (strcmp(a_control, "count") == 0) {
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
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl Invoke(void* a_context, const char*) noexcept
    {
        ++static_cast<Provider*>(a_context)->invokes;
        return SlopApi::Result::Ok;
    }

    SlopApi::Result __cdecl Apply(void* a_context) noexcept
    {
        auto& provider = *static_cast<Provider*>(a_context);
        ++provider.applies;
        return provider.failApply ? SlopApi::Result::WriteFailure : SlopApi::Result::Ok;
    }

    void __cdecl Cancel(void* a_context) noexcept { ++static_cast<Provider*>(a_context)->cancels; }

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
    CHECK(api->registerPage(nullptr) == Result::InvalidArgument);

    Provider generalProvider, bindingsProvider, noRollbackProvider;
    ControlDescriptorV1 generalControls[]{
        MakeDescriptor(ControlKind::Toggle, "enabled"),
        MakeDescriptor(ControlKind::IntegerSlider, "count", 0, 10, 1),
        MakeDescriptor(ControlKind::FloatSlider, "scale", 0, 2, 0.25),
        MakeDescriptor(ControlKind::ButtonBinding, "bind"),
        MakeDescriptor(ControlKind::Action, "run"),
        MakeDescriptor(ControlKind::IntegerSlider, "broken", 0, 10, 1)
    };
    PageDescriptorV1 general;
    strcpy_s(general.moduleId, "test.module"); strcpy_s(general.pageId, "general"); strcpy_s(general.displayName, "General");
    general.controlCount = static_cast<std::uint32_t>(std::size(generalControls)); general.controls = generalControls;
    general.context = &generalProvider; general.readValue = &ReadValue; general.writeDraft = &WriteDraft;
    general.invokeAction = &Invoke; general.apply = &Apply; general.cancel = &Cancel;
    CHECK(api->registerPage(&general) == Result::Ok);

    ControlDescriptorV1 bindingControls[]{ MakeDescriptor(ControlKind::Toggle, "other") };
    PageDescriptorV1 bindings;
    strcpy_s(bindings.moduleId, "test.module"); strcpy_s(bindings.pageId, "bindings"); strcpy_s(bindings.displayName, "Bindings");
    bindings.controlCount = 1; bindings.controls = bindingControls; bindings.context = &bindingsProvider;
    bindings.readValue = &ReadValue; bindings.writeDraft = &WriteDraft; bindings.cancel = &Cancel;
    CHECK(api->registerPage(&bindings) == Result::Ok);

    // Descriptors must be copied before provider-owned storage is changed.
    strcpy_s(generalControls[0].label, "MUTATED");
    const auto copied = MenuApiHost::FindPage("test.module", "general");
    CHECK(copied && copied->controls.front().label == "enabled");

    Session session;
    auto model = session.Snapshot();
    CHECK(model.pages.size() == 2 && model.pages[0].controls.size() == 6);
    CHECK(model.pages[0].controls[0].available && model.pages[0].controls[0].value.booleanValue == 1);
    CHECK(model.pages[0].controls[1].value.integerValue == 3 && model.pages[0].controls[2].value.floatValue == 1.5);
    CHECK(model.pages[0].controls[5].available == false);  // failed/wrong-kind reads are isolated to the row
    CHECK(model.pages[0].controls[4].available && generalProvider.reads == 5);  // Action never reads

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

    // Every route to a different page is refused while this page owns the transaction.
    CHECK(!session.Dispatch(MakeCommand(CommandKind::SelectPage, "bindings")).error.empty());
    CHECK(!session.Dispatch(MakeCommand(CommandKind::SelectControl, "bindings", "other")).error.empty());
    auto foreignWrite = MakeCommand(CommandKind::Write, "bindings", "other"); foreignWrite.value.kind = ValueKind::Boolean;
    CHECK(!session.Dispatch(foreignWrite).error.empty() && bindingsProvider.writes == 0);
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Apply, "bindings")).error.empty());
    CHECK(!session.Dispatch(MakeCommand(CommandKind::Cancel, "bindings")).error.empty());

    CHECK(session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "enabled")).error.size() > 0 && generalProvider.invokes == 0);
    model = session.Dispatch(MakeCommand(CommandKind::Invoke, "general", "run"));
    CHECK(model.error.empty() && generalProvider.invokes == 1 && model.selectedControlId == "run");
    CHECK(session.Dispatch(MakeCommand(CommandKind::Apply, "general")).dirty == false && generalProvider.applies == 1);

    auto nonFinite = MakeCommand(CommandKind::Write, "general", "scale");
    nonFinite.value.kind = ValueKind::Float; nonFinite.value.floatValue = std::numeric_limits<double>::quiet_NaN();
    CHECK(!session.Dispatch(nonFinite).error.empty() && generalProvider.writes == 1);

    auto binding = MakeCommand(CommandKind::Write, "general", "bind");
    binding.value.kind = ValueKind::String; strcpy_s(binding.value.stringValue, "Mouse1");
    CHECK(session.Dispatch(binding).dirty && generalProvider.lastWrite.kind == ValueKind::String);
    CHECK(session.Dispatch(MakeCommand(CommandKind::Cancel, "general")).dirty == false && generalProvider.cancels == 1);

    write.value.kind = ValueKind::Integer; write.value.integerValue = 4;
    CHECK(session.Dispatch(write).dirty);
    CHECK(session.Dispatch(MakeCommand(CommandKind::Close, "")).dirty == false && generalProvider.cancels == 2);

    ControlDescriptorV1 noRollbackControls[]{ MakeDescriptor(ControlKind::Toggle, "unsafe") };
    PageDescriptorV1 noRollback;
    strcpy_s(noRollback.moduleId, "test.module"); strcpy_s(noRollback.pageId, "unsafe"); strcpy_s(noRollback.displayName, "Unsafe");
    noRollback.controlCount = 1; noRollback.controls = noRollbackControls; noRollback.context = &noRollbackProvider;
    noRollback.readValue = &ReadValue; noRollback.writeDraft = &WriteDraft;
    CHECK(api->registerPage(&noRollback) == Result::Ok);
    CHECK(session.Dispatch(MakeCommand(CommandKind::SelectPage, "unsafe")).error.empty());
    auto unsafeWrite = MakeCommand(CommandKind::Write, "unsafe", "unsafe"); unsafeWrite.value.kind = ValueKind::Boolean;
    CHECK(session.Dispatch(unsafeWrite).dirty);
    model = session.Dispatch(MakeCommand(CommandKind::Close, ""));
    CHECK(model.dirty && !model.error.empty());

    const auto beforeRefresh = MenuApiHost::Revision();
    CHECK(api->requestRefresh("test.module", "general") == Result::Ok && MenuApiHost::Revision() == beforeRefresh + 1);
    CHECK(api->requestRefresh("test.module", "missing") == Result::NotFound);
    const auto beforeUnregister = MenuApiHost::Revision();
    CHECK(api->unregisterModule("test.module") == Result::Ok && MenuApiHost::Revision() == beforeUnregister + 1);
    CHECK(api->unregisterModule("test.module") == Result::NotFound);
    return 0;
}
