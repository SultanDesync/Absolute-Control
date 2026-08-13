#include "MenuApiHost.h"
#include "MenuSession.h"
#include "SlopAPI.h"

#include <cassert>
#include <cstring>

namespace
{
    using namespace SlopApi;
    using namespace AbsoluteControlPanelResearch;

    struct State
    {
        bool enabled{ true };
        bool other{ true };
        bool wrongReadKind{};
        int reads{};
        int actionReads{};
        int writes{};
        int applies{};
        int cancels{};
        int actions{};
    } g;

    Result __cdecl Read(void*, const char* id, ValueV1* value) noexcept
    {
        ++g.reads;
        value->structSize = sizeof(*value);
        if (std::strcmp(id, "run") == 0) {
            ++g.actionReads;
            return Result::NotFound;
        }
        if (std::strcmp(id, "binding") == 0) {
            value->kind = ValueKind::String;
            strcpy_s(value->stringValue, "none");
        } else {
            value->kind = g.wrongReadKind && std::strcmp(id, "enabled") == 0 ?
                              ValueKind::Float : ValueKind::Boolean;
            value->booleanValue = std::strcmp(id, "other") == 0 ? g.other : g.enabled;
        }
        return Result::Ok;
    }

    Result __cdecl Write(void*, const char* id, const ValueV1* value) noexcept
    {
        ++g.writes;
        if (std::strcmp(id, "enabled") == 0) g.enabled = value->booleanValue != 0;
        if (std::strcmp(id, "other") == 0) g.other = value->booleanValue != 0;
        return Result::Ok;
    }
    Result __cdecl Invoke(void*, const char*) noexcept { ++g.actions; return Result::Ok; }
    Result __cdecl Apply(void*) noexcept { ++g.applies; return Result::Ok; }
    void __cdecl Cancel(void*) noexcept { ++g.cancels; }

    void Copy(char* destination, std::size_t capacity, const char* source)
    {
        strcpy_s(destination, capacity, source);
    }

    ControlDescriptorV1 Control(ControlKind kind, const char* id)
    {
        ControlDescriptorV1 control;
        control.kind = kind;
        Copy(control.controlId, sizeof(control.controlId), id);
        Copy(control.label, sizeof(control.label), id);
        control.minimumValue = 0;
        control.maximumValue = 1;
        control.stepValue = 1;
        return control;
    }

    PageDescriptorV1 Page(
        const char* pageId, ControlDescriptorV1* controls, std::uint32_t count,
        CancelCallback cancel = &Cancel)
    {
        PageDescriptorV1 page;
        Copy(page.moduleId, sizeof(page.moduleId), "acceptance.module");
        Copy(page.pageId, sizeof(page.pageId), pageId);
        Copy(page.displayName, sizeof(page.displayName), pageId);
        page.controls = controls;
        page.controlCount = count;
        page.readValue = &Read;
        page.writeDraft = &Write;
        page.invokeAction = &Invoke;
        page.apply = &Apply;
        page.cancel = cancel;
        return page;
    }

    MenuSession::Command Command(
        MenuSession::CommandKind kind, const char* page, const char* control = "")
    {
        MenuSession::Command command;
        command.kind = kind;
        command.moduleId = "acceptance.module";
        command.pageId = page;
        command.controlId = control;
        command.value.structSize = sizeof(command.value);
        return command;
    }

    bool Accepted(SlopApi::Result result)
    {
        return result == SlopApi::Result::Ok;
    }

    bool Accepted(const MenuSession::Model& model)
    {
        return model.error.empty();
    }

    bool DispatchAccepted(MenuSession::Session& session, const MenuSession::Command& command)
    {
        return Accepted(session.Dispatch(command));
    }
}

int main()
{
    using namespace SlopApi;
    using namespace AbsoluteControlPanelResearch;

    auto* api = SLOP_QueryApi(kAbiVersion);
    assert(api);
    ControlDescriptorV1 generalControls[]{
        Control(ControlKind::Toggle, "enabled"),
        Control(ControlKind::ButtonBinding, "binding"),
        Control(ControlKind::Action, "run")
    };
    ControlDescriptorV1 advancedControls[]{ Control(ControlKind::Toggle, "other") };
    ControlDescriptorV1 noCancelControls[]{ Control(ControlKind::Toggle, "uncancellable") };
    auto general = Page("general", generalControls, 3);
    auto advanced = Page("advanced", advancedControls, 1);
    auto noCancel = Page("no-cancel", noCancelControls, 1, nullptr);
    assert(api->registerPage(&general) == Result::Ok);
    assert(api->registerPage(&advanced) == Result::Ok);
    assert(api->registerPage(&noCancel) == Result::Ok);

    MenuSession::Session session;
    auto model = session.Snapshot();
    assert(model.pages[0].controls[2].available);
    assert(g.actionReads == 0);

    g.wrongReadKind = true;
    model = session.Snapshot();
    assert(!model.pages[0].controls[0].available);
    g.wrongReadKind = false;

    assert(DispatchAccepted(session, Command(MenuSession::CommandKind::SelectPage, "general")));
    auto binding = Command(MenuSession::CommandKind::Write, "general", "binding");
    binding.value.kind = ValueKind::String;
    strcpy_s(binding.value.stringValue, "vJoy Device 1@1");
    assert(DispatchAccepted(session, binding));
    assert(DispatchAccepted(session, Command(MenuSession::CommandKind::Cancel, "general")));

    auto write = Command(MenuSession::CommandKind::Write, "general", "enabled");
    write.value.kind = ValueKind::Boolean;
    write.value.booleanValue = 0;
    assert(DispatchAccepted(session, write));
    assert(session.Snapshot().dirty);
    const int writesBeforeBypass = g.writes;
    assert(!DispatchAccepted(
        session, Command(MenuSession::CommandKind::SelectControl, "advanced", "other")));
    assert(session.Snapshot().activePageId == "general");
    auto bypassWrite = Command(MenuSession::CommandKind::Write, "advanced", "other");
    bypassWrite.value.kind = ValueKind::Boolean;
    bypassWrite.value.booleanValue = 0;
    assert(!DispatchAccepted(session, bypassWrite));
    assert(g.writes == writesBeforeBypass);
    assert(!DispatchAccepted(session, Command(MenuSession::CommandKind::Apply, "advanced")));
    assert(!DispatchAccepted(session, Command(MenuSession::CommandKind::Cancel, "advanced")));
    assert(DispatchAccepted(session, Command(MenuSession::CommandKind::Cancel, "general")));

    assert(DispatchAccepted(session, Command(MenuSession::CommandKind::SelectPage, "no-cancel")));
    auto noCancelWrite = Command(MenuSession::CommandKind::Write, "no-cancel", "uncancellable");
    noCancelWrite.value.kind = ValueKind::Boolean;
    noCancelWrite.value.booleanValue = 0;
    assert(DispatchAccepted(session, noCancelWrite));
    assert(session.Snapshot().dirty);
    assert(!DispatchAccepted(session, Command(MenuSession::CommandKind::Close, "")));
    const auto refusedClose = session.Snapshot();
    assert(!refusedClose.error.empty() && refusedClose.dirty);

    assert(api->unregisterModule("acceptance.module") == Result::Ok);
    return 0;
}
