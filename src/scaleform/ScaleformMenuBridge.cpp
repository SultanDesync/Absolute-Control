
#include "scaleform/ScaleformMenuBridge.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
#include "MenuInputRouter.h"
#include "MenuSession.h"
#include "LiveComponentsRegistry.h"
#include "SlopAPI.h"
#include "input/NativeMenuInputAdapter.h"
#include "input/PlatformInputServices.h"
#include "ui/MenuMessaging.h"
#include "ui/PauseMenuIntegration.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace AbsoluteControlPanelResearch::Scaleform
{
    namespace
    {
        constexpr auto kCodeObjectName = "BGSCodeObj";
    }

    class MenuBridge::Impl final : public Input::NativeMenuInputSink
    {
    public:
        ~Impl() override
        {
            // External menu destruction can bypass the normal close command.  Do
            // not leave product input capture armed, and do not let a deferred
            // movie publication survive the movie object that owns this bridge.
            if (session.IsCaptureActive()) {
                EvidenceLog::Event(
                    session.IsTextCaptureActive() ? "text_capture_cancelled" :
                                                    "binding_capture_cancelled",
                    "source=bridge-teardown");
            }
            session.Teardown();
            LiveComponents::HostRegistry().SetMenuActive(false);
            MenuApiHost::SetInputCaptureActive(false);
            pendingModel.reset();
            movie = nullptr;
            EvidenceLog::Event("bridge_teardown", "deferred model cleared");
        }

        void Attach(RE::Scaleform::GFx::Movie* a_movie,
            const RE::Scaleform::GFx::Value& a_menuObject)
        {
            movie = a_movie;
            menuObj = a_menuObject;
        }

            void LogMovieState(std::string_view a_phase)
            {
                RE::Scaleform::GFx::Value codeObject;
                RE::Scaleform::GFx::Value readyFunction;
                RE::Scaleform::GFx::Value constructed;
                RE::Scaleform::GFx::Value drawn;
                RE::Scaleform::GFx::Value childCount;
                const bool gotCodeObject = menuObj.GetMember(kCodeObjectName, &codeObject);
                const bool gotReady = gotCodeObject && codeObject.IsObject() &&
                                      codeObject.GetMember("ready", &readyFunction);
                const bool gotConstructed = menuObj.GetMember("ACPConstructed", &constructed);
                const bool gotDrawn = menuObj.GetMember("ACPDrawn", &drawn);
                const bool gotChildCount = menuObj.GetMember("numChildren", &childCount);
                EvidenceLog::Event(
                    "movie_root_state",
                    std::format(
                        "phase={} root_type={} display={} code_object={} code_type={} "
                        "ready={} ready_type={} constructed={} drawn={} child_count_type={} "
                        "child_count={}",
                        a_phase, static_cast<std::int32_t>(menuObj.GetType()),
                        menuObj.IsDisplayObject(), gotCodeObject && codeObject.IsObject(),
                        static_cast<std::int32_t>(codeObject.GetType()), gotReady,
                        static_cast<std::int32_t>(readyFunction.GetType()),
                        gotConstructed && constructed.IsBoolean() && constructed.GetBoolean(),
                        gotDrawn && drawn.IsBoolean() && drawn.GetBoolean(),
                        static_cast<std::int32_t>(childCount.GetType()),
                        gotChildCount && childCount.IsInt() ? childCount.GetInt() :
                            gotChildCount && childCount.IsUInt() ?
                                static_cast<std::int32_t>(childCount.GetUInt()) : -1));
            }

            void Call(const RE::Scaleform::GFx::FunctionHandler::Params& a_params)
            {
                const auto function = static_cast<NativeFunction>(
                    reinterpret_cast<std::uintptr_t>(a_params.userData));
                switch (function) {
                case NativeFunction::Ready:
                    EvidenceLog::Event("bridge_ready", std::format("argument_count={}", a_params.argCount));
                    // A fresh menu snapshot already includes every refresh that happened
                    // before the movie became ready. Start the wakeup cursor there so the
                    // first acknowledgement does not immediately publish a duplicate model.
                    refreshCursor = MenuApiHost::RefreshRevision();
                    DeferModel(session.Snapshot(), "bridge-ready");
                    break;
                case NativeFunction::Close:
                    CloseSession();
                    break;
                case NativeFunction::Dispatch:
                    DispatchFlat(a_params);
                    break;
                case NativeFunction::Compound:
                    DispatchCompound(a_params);
                    break;
                case NativeFunction::Focus: {
                    const auto region = a_params.argCount >= 1 ?
                        ReadUnsigned(a_params.args[0]) :
                        std::numeric_limits<std::uint32_t>::max();
                    const auto action = a_params.argCount >= 2 ?
                        ReadUnsigned(a_params.args[1]) :
                        std::numeric_limits<std::uint32_t>::max();
                    if (a_params.argCount != 2 || region > static_cast<std::uint32_t>(
                            MenuInputRouter::FocusRegion::Grid) || action > 2) {
                        EvidenceLog::Event("bridge_focus_rejected", "invalid focus payload");
                        break;
                    }
                    inputFocus.region = static_cast<MenuInputRouter::FocusRegion>(region);
                    inputFocus.actionIndex = action;
                    EvidenceLog::Event("bridge_focus_updated",
                        std::format("region={} action_index={}", region, action));
                    break;
                }
                case NativeFunction::ModelApplied: {
                    std::uint64_t generation{};
                    if (a_params.argCount != 1 ||
                        !ReadExactGeneration(a_params.args[0], generation)) {
                        EvidenceLog::Event(
                            "bridge_model_applied_rejected", "invalid generation");
                    } else if (generation != lastAppliedGeneration) {
                        lastAppliedGeneration = generation;
                        EvidenceLog::Event(
                            "bridge_model_applied",
                            std::format("generation={}", generation));
                    }
                    // applyModel acknowledges synchronously.  Publishing another
                    // replacement model from that acknowledgement would recurse
                    // through Scaleform.  Only the later ENTER_FRAME heartbeat may
                    // flush work queued by a pointer/keyboard/provider callback.
                    if (!modelPublicationActive) {
                        if (auto capture = session.RefreshBindingCapture()) {
                            if (!capture->bindingCaptureActive) {
                                MenuApiHost::SetInputCaptureActive(false);
                            }
                            EvidenceLog::Event(
                                capture->error.empty() ?
                                    "provider_binding_capture_completed" :
                                    "provider_binding_capture_ended",
                                std::format("error={}", capture->error));
                            DeferModel(std::move(*capture),
                                "provider-binding-capture");
                        }
                        PollRefresh();
                        const auto now = std::chrono::steady_clock::now();
                        if (lastLivePoll.time_since_epoch().count() == 0 ||
                            now - lastLivePoll >= std::chrono::milliseconds(100)) {
                            lastLivePoll = now;
                            if (auto live = session.RefreshLive()) {
                                DeferModel(std::move(*live), "live-component");
                            }
                        }
                        FlushDeferredModel();
                        AdvancePointerInputBarrier();
                    }
                    break;
                }
                }
            }

            void DispatchFlat(const RE::Scaleform::GFx::FunctionHandler::Params& a_params)
            {
                MenuSession::Command command;
                const bool bridgeV1 = a_params.argCount == 10;
                const bool generationAware = a_params.argCount == 11;
                if ((!bridgeV1 && !generationAware) || !a_params.args[1].IsString() ||
                    !a_params.args[2].IsString() || !a_params.args[3].IsString() ||
                    !a_params.args[4].IsString() || !a_params.args[9].IsString() ||
                    !ReadBoolean(a_params.args[6], command.value.booleanValue) ||
                    !ReadInteger(a_params.args[7], command.value.integerValue) ||
                    !ReadFiniteNumber(a_params.args[8], command.value.floatValue) ||
                    (generationAware && !ReadExactGeneration(
                        a_params.args[10], command.expectedGeneration)) ||
                    strnlen_s(a_params.args[9].GetString(), SlopApi::kStringValueCapacity) >=
                        SlopApi::kStringValueCapacity) {
                    command.schemaVersion = 0;
                    EvidenceLog::Event("bridge_command_rejected", "invalid flat dispatch arguments");
                    DeferModel(session.Dispatch(command), "invalid-dispatch");
                    return;
                }
                command.schemaVersion = ReadUnsigned(a_params.args[0]);
                // Ten-argument bridge-v1 clients intentionally leave this at zero.
                // MenuSession treats zero as the compatibility opt-out while every
                // current ActionScript command echoes its published generation.
                command.moduleId = a_params.args[2].GetString();
                command.pageId = a_params.args[3].GetString();
                command.controlId = a_params.args[4].GetString();
                command.value.kind = static_cast<SlopApi::ValueKind>(ReadUnsigned(a_params.args[5]));
                strcpy_s(command.value.stringValue, a_params.args[9].GetString());
                const std::string_view name = a_params.args[1].GetString();
                if (name == "selectPage") command.kind = MenuSession::CommandKind::SelectPage;
                else if (name == "selectControl") command.kind = MenuSession::CommandKind::SelectControl;
                else if (name == "selectGridColumn") command.kind = MenuSession::CommandKind::SelectGridColumn;
                else if (name == "write") command.kind = MenuSession::CommandKind::Write;
                else if (name == "invoke") command.kind = MenuSession::CommandKind::Invoke;
                else if (name == "beginBindingCapture") command.kind = MenuSession::CommandKind::BeginBindingCapture;
                else if (name == "beginTextCapture") command.kind = MenuSession::CommandKind::BeginTextCapture;
                else if (name == "apply") command.kind = MenuSession::CommandKind::Apply;
                else if (name == "cancel") command.kind = MenuSession::CommandKind::Cancel;
                else if (name == "close") command.kind = MenuSession::CommandKind::Close;
                else if (name == "dirtyApply") command.kind = MenuSession::CommandKind::ResolveDirtyApply;
                else if (name == "dirtyDiscard") command.kind = MenuSession::CommandKind::ResolveDirtyDiscard;
                else if (name == "dirtyStay") command.kind = MenuSession::CommandKind::ResolveDirtyStay;
                else if (name == "bindingReassign") command.kind = MenuSession::CommandKind::ResolveBindingReassign;
                else if (name == "bindingCancel") command.kind = MenuSession::CommandKind::ResolveBindingCancel;
                else command.schemaVersion = 0;
                if (name == "selectGridColumn") {
                    command.columnId = command.controlId;
                    command.channelId = command.value.stringValue;
                    command.controlId.clear();
                }
                DispatchCommand(command, name, "scaleform");
            }

            void DispatchCompound(
                const RE::Scaleform::GFx::FunctionHandler::Params& a_params)
            {
                MenuSession::Command command;
                command.kind = MenuSession::CommandKind::Compound;
                std::uint64_t generation{};
                const auto operation = a_params.argCount == 10 ?
                    ReadUnsigned(a_params.args[7]) :
                    std::numeric_limits<std::uint32_t>::max();
                const auto count = a_params.argCount == 10 ?
                    ReadUnsigned(a_params.args[8]) :
                    std::numeric_limits<std::uint32_t>::max();
                if (a_params.argCount != 10 || !a_params.args[1].IsString() ||
                    !a_params.args[2].IsString() || !a_params.args[3].IsString() ||
                    !a_params.args[4].IsString() || !a_params.args[5].IsString() ||
                    !a_params.args[6].IsString() || operation > 3 ||
                    count == std::numeric_limits<std::uint32_t>::max() ||
                    !ReadExactGeneration(a_params.args[9], generation)) {
                    command.schemaVersion = 0;
                    DeferModel(session.Dispatch(command), "invalid-compound-dispatch");
                    return;
                }
                command.schemaVersion = ReadUnsigned(a_params.args[0]);
                command.expectedGeneration = generation;
                command.moduleId = a_params.args[1].GetString();
                command.pageId = a_params.args[2].GetString();
                command.channelId = a_params.args[3].GetString();
                command.controlId = a_params.args[4].GetString();
                command.columnId = a_params.args[5].GetString();
                command.tierId = a_params.args[6].GetString();
                command.compoundKind =
                    static_cast<AbsoluteControlPanelExperimental::CompoundOperationKind>(
                        operation);
                command.count = count;
                DispatchCommand(command, "compound", "scaleform");
            }

            void PollRefresh()
            {
                if (closing || !MenuApiHost::ConsumeRefresh(
                        refreshCursor, session.ActiveModuleId(),
                        session.ActivePageId())) {
                    return;
                }
                EvidenceLog::Event(
                    "bridge_refresh_consumed",
                    std::format("refresh_revision={}", refreshCursor));
                DeferModel(session.Snapshot(), "provider-refresh");
            }

            void DeferModel(MenuSession::Model a_model, std::string_view a_source)
            {
                if (pendingModel &&
                    a_model.generation < pendingModel->generation) {
                    EvidenceLog::Event(
                        "bridge_model_discarded",
                        std::format(
                            "generation={} pending_generation={} source={}",
                            a_model.generation, pendingModel->generation, a_source));
                    return;
                }
                const bool replaced = pendingModel.has_value();
                const auto generation = a_model.generation;
                pendingModel = std::move(a_model);
                EvidenceLog::Event(
                    "bridge_model_deferred",
                    std::format(
                        "generation={} source={} replaced={}", generation,
                        a_source, replaced));
            }

            void FlushDeferredModel()
            {
                if (closing) {
                    pendingModel.reset();
                    return;
                }
                if (!pendingModel || modelPublicationActive) {
                    return;
                }
                auto model = std::move(*pendingModel);
                pendingModel.reset();
                EvidenceLog::Event(
                    "bridge_model_flush",
                    std::format("generation={} boundary=enter-frame", model.generation));
                PublishModel(model);
            }

            void DispatchCommand(const MenuSession::Command& a_command,
                std::string_view a_name, std::string_view a_source)
            {
                if (closing) {
                    EvidenceLog::Event(
                        "bridge_command_rejected",
                        std::format(
                            "command={} source={} error=menu closing",
                            a_name, a_source));
                    return;
                }
                EvidenceLog::Event("bridge_command",
                    std::format("command={} source={}", a_name, a_source));
                const auto model = session.Dispatch(a_command);
                if (model.dirtyDecisionActive && !dirtyDecisionReturnFocus) {
                    dirtyDecisionReturnFocus = lastPublishedInputFocus;
                } else if (!model.dirtyDecisionActive &&
                    dirtyDecisionReturnFocus) {
                    if (a_command.kind ==
                        MenuSession::CommandKind::ResolveDirtyStay &&
                        model.error.empty()) {
                        inputFocus = *dirtyDecisionReturnFocus;
                    }
                    dirtyDecisionReturnFocus.reset();
                }
                if (a_command.kind == MenuSession::CommandKind::BeginBindingCapture &&
                    model.error.empty() && model.bindingCaptureActive) {
                    // A release barrier is only needed when the same keyboard event that
                    // activates the row could otherwise become the new binding. Pointer
                    // activation has no initiating keyboard key to release; arming it lazily
                    // would consume the user's first intended modifier instead.
                    const bool awaitingRelease = a_source == "native-keyboard";
                    inputAdapter.BeginBindingCapture(awaitingRelease);
                    MenuApiHost::SetInputCaptureActive(true);
                    EvidenceLog::Event(
                        "binding_capture_started",
                        std::format(
                            "module={} page={} control={} flags=0x{:08X} awaiting_release={}",
                            model.captureModuleId, model.capturePageId,
                            model.captureControlId, session.BindingCaptureFlags(),
                            awaitingRelease));

                    if (!awaitingRelease) {
                        EvidenceLog::Event(
                            "binding_capture_armed", "source=pointer-or-scaleform");
                    }
                }
                if (a_command.kind == MenuSession::CommandKind::BeginTextCapture &&
                    model.error.empty() && model.textCaptureActive) {
                    const bool awaitingRelease = a_source == "native-keyboard";
                    inputAdapter.BeginTextCapture(awaitingRelease);
                    MenuApiHost::SetInputCaptureActive(true);
                    EvidenceLog::Event(
                        "text_capture_started",
                        std::format(
                            "module={} page={} control={} awaiting_release={}",
                            model.captureModuleId, model.capturePageId,
                            model.captureControlId, awaitingRelease));
                    if (!awaitingRelease) {
                        EvidenceLog::Event(
                            "text_capture_armed", "source=pointer-or-scaleform");
                    }
                }
                EvidenceLog::Event(
                    model.error.empty() ? "bridge_command_accepted" :
                                          "bridge_command_rejected",

                    std::format("command={} source={} error={}",
                        a_name, a_source, model.error));
                if (model.closeRequested && model.error.empty()) {
                    // A successful close needs no replacement display tree.  The
                    // queued UI message is processed only after this command stack
                    // unwinds; teardown drops any older deferred publication.
                    closing = true;
                    pendingModel.reset();
                    MenuApiHost::SetInputCaptureActive(false);
                    Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
                    return;
                }
                DeferModel(model, a_source);
            }

            void CloseSession()
            {
                if (closing) {
                    return;
                }
                if (session.IsCaptureActive()) {
                    if (session.IsTextCaptureActive()) {
                        (void)session.CancelTextCapture();
                    } else {
                        (void)session.CancelBindingCapture();
                    }
                    MenuApiHost::SetInputCaptureActive(false);
                }
                const auto model = session.Dispatch(MenuSession::Command{ .kind = MenuSession::CommandKind::Close });
                if (model.closeRequested && model.error.empty()) {
                    closing = true;
                    pendingModel.reset();
                    MenuApiHost::SetInputCaptureActive(false);
                    Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
                } else {
                    DeferModel(model, "native-close");
                }
            }

            [[nodiscard]] bool HandleButtonInput(const RE::ButtonEvent& a_event)
            {
                if (closing) {
                    return true;
                }
                return inputAdapter.HandleButtonInput(a_event, *this);
            }

            void HandlePointerPhase(Input::PointerPhase a_phase)
            {
                if (closing || !menuShown || !pointerInputArmed) {
                    if (a_phase != Input::PointerPhase::Move) {
                        EvidenceLog::Event(
                            "pointer_phase_suppressed",
                            std::format(
                                "phase={} closing={} shown={} armed={}",
                                static_cast<std::uint32_t>(a_phase), closing,
                                menuShown, pointerInputArmed));
                    }
                    return;
                }
                const auto method = a_phase == Input::PointerPhase::Down ? "handlePointerDown" :
                    (a_phase == Input::PointerPhase::Move ? "handlePointerMove" :
                                                     "handlePointerUp");
                const auto failureEvent = a_phase == Input::PointerPhase::Down ?
                    "pointer_down_rejected" :
                    (a_phase == Input::PointerPhase::Move ? "pointer_move_rejected" :
                                                    "pointer_up_rejected");
                POINT point{};
                double stageX{};
                double stageY{};
                if (!Input::ResolvePointerStage(point, stageX, stageY, failureEvent)) return;


                if (!movie || !movie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event(failureEvent, "movie root unavailable");
                    return;
                }
                RE::Scaleform::GFx::Value args[2]{
                    RE::Scaleform::GFx::Value(stageX),
                    RE::Scaleform::GFx::Value(stageY)
                };
                RE::Scaleform::GFx::Value handled;
                const bool invoked =
                    menuObj.Invoke(method, &handled, args, std::size(args));
                const bool hit = invoked && handled.IsBoolean() && handled.GetBoolean();
                if (a_phase != Input::PointerPhase::Move) {

                    EvidenceLog::Event(
                        hit ? (a_phase == Input::PointerPhase::Down ? "pointer_down_hit" :
                                                               "pointer_up_hit") :
                              (a_phase == Input::PointerPhase::Down ? "pointer_down_missed" :
                                                               "pointer_up_missed"),
                        std::format(
                            "client={},{} stage={:.1f},{:.1f} owner=scaleform "
                            "invoked={}",
                            point.x, point.y, stageX, stageY, invoked));
                }
            }

            void HandlePointerWheel(std::int32_t a_direction)
            {
                if (closing || !menuShown || !pointerInputArmed) {
                    EvidenceLog::Event(
                        "mouse_wheel_suppressed",
                        std::format(
                            "direction={} closing={} shown={} armed={}",
                            a_direction, closing, menuShown, pointerInputArmed));
                    return;
                }
                POINT point{};
                double stageX{};
                double stageY{};
                if (!Input::ResolvePointerStage(point, stageX, stageY, "mouse_wheel_rejected")) return;
                if (!movie || !movie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event("mouse_wheel_rejected", "movie root unavailable");
                    return;
                }
                RE::Scaleform::GFx::Value args[3]{
                    RE::Scaleform::GFx::Value(stageX),
                    RE::Scaleform::GFx::Value(stageY),
                    RE::Scaleform::GFx::Value(a_direction)
                };
                RE::Scaleform::GFx::Value handled;
                const bool invoked = menuObj.Invoke(
                    "handlePointerWheel", &handled, args, std::size(args));
                const bool hit = invoked && handled.IsBoolean() && handled.GetBoolean();
                EvidenceLog::Event(
                    hit ? "mouse_wheel_hit" : "mouse_wheel_missed",
                    std::format(
                        "client={},{} stage={:.1f},{:.1f} direction={} invoked={}",
                        point.x, point.y, stageX, stageY, a_direction, invoked));
            }

            [[nodiscard]] bool SetString(RE::Scaleform::GFx::Value& a_object,
                const char* a_name, std::string_view a_value) const
            {
                RE::Scaleform::GFx::Value text;
                movie->asMovieRoot->CreateString(&text, std::string(a_value).c_str());
                return a_object.SetMember(a_name, text);
            }

            [[nodiscard]] bool SerializeControl(RE::Scaleform::GFx::Value& a_target,
                const MenuSession::Control& a_control) const
            {
                const auto& descriptor = a_control.descriptor;
                auto* root = movie->asMovieRoot.get();
                RE::Scaleform::GFx::Value choiceOptions;
                root->CreateArray(&choiceOptions);
                bool ok = choiceOptions.IsArray();
                for (const auto& source : a_control.choiceOptions) {
                    RE::Scaleform::GFx::Value option;
                    root->CreateObject(&option);
                    ok = ok && option.IsObject() &&
                        option.SetMember("value", RE::Scaleform::GFx::Value(
                            static_cast<double>(source.value))) &&
                        SetString(option, "label", source.label) &&
                        choiceOptions.PushBack(option);
                    if (!ok) return false;
                }
                return ok && SetString(a_target, "controlId", descriptor.controlId) &&
                    a_target.SetMember("kind", RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(descriptor.kind))) &&
                    a_target.SetMember("flags", RE::Scaleform::GFx::Value(descriptor.flags)) &&
                    SetString(a_target, "label", descriptor.label) && SetString(a_target, "description", descriptor.description) &&
                    a_target.SetMember("minimum", RE::Scaleform::GFx::Value(descriptor.minimumValue)) &&
                    a_target.SetMember("maximum", RE::Scaleform::GFx::Value(descriptor.maximumValue)) &&
                    a_target.SetMember("step", RE::Scaleform::GFx::Value(descriptor.stepValue)) &&
                    a_target.SetMember("available", RE::Scaleform::GFx::Value(a_control.available)) &&
                    a_target.SetMember("valueKind", RE::Scaleform::GFx::Value(static_cast<std::uint32_t>(a_control.value.kind))) &&
                    a_target.SetMember("booleanValue", RE::Scaleform::GFx::Value(a_control.value.booleanValue != 0)) &&
                    a_target.SetMember("integerValue", RE::Scaleform::GFx::Value(static_cast<double>(a_control.value.integerValue))) &&
                    a_target.SetMember("floatValue", RE::Scaleform::GFx::Value(a_control.value.floatValue)) &&
                    SetString(a_target, "stringValue", a_control.value.stringValue) &&
                    SetString(a_target, "error", a_control.error) &&
                    a_target.SetMember("choiceOptions", choiceOptions);
            }

            [[nodiscard]] bool SerializeLiveComponent(
                RE::Scaleform::GFx::Value& target,
                const MenuSession::Page::LiveComponent& component) const
            {
                auto* root = movie->asMovieRoot.get();
                const auto& descriptor = component.descriptor;
                bool ok = SetString(target, "channelId", descriptor.channelId) &&
                    SetString(target, "title", descriptor.title) &&
                    target.SetMember("kind", RE::Scaleform::GFx::Value(
                        static_cast<std::uint32_t>(descriptor.kind))) &&
                    target.SetMember("interactionFlags", RE::Scaleform::GFx::Value(
                        descriptor.flags)) &&
                    target.SetMember("available", RE::Scaleform::GFx::Value(
                        component.available)) &&
                    SetString(target, "error", component.error);
                if (!ok || descriptor.kind !=
                        AbsoluteControlPanelExperimental::ComponentKind::SegmentedAllocationGrid) {
                    return ok;
                }

                const auto& grid = descriptor.segmentedGrid;
                const auto& frame = component.frame.segmentedGrid;
                RE::Scaleform::GFx::Value columns, tiers;
                root->CreateArray(&columns);
                root->CreateArray(&tiers);
                ok = columns.IsArray() && tiers.IsArray() &&
                    SetString(target, "controlId", grid.controlId) &&
                    target.SetMember("sequence", RE::Scaleform::GFx::Value(
                        static_cast<double>(component.frame.sequence))) &&
                    target.SetMember("flags", RE::Scaleform::GFx::Value(
                        component.frame.flags));
                for (std::uint32_t tierIndex = 0;
                     ok && tierIndex < grid.tierCount; ++tierIndex) {
                    RE::Scaleform::GFx::Value tier;
                    root->CreateObject(&tier);
                    const auto& source = grid.tiers[tierIndex];
                    ok = tier.IsObject() &&
                        SetString(tier, "tierId", source.tierId) &&
                        SetString(tier, "label", source.label) &&
                        tier.SetMember("visualRole", RE::Scaleform::GFx::Value(
                            static_cast<std::uint32_t>(source.visualRole))) &&
                        tiers.PushBack(tier);
                }
                for (std::uint32_t columnIndex = 0;
                     ok && columnIndex < grid.columnCount; ++columnIndex) {
                    RE::Scaleform::GFx::Value column, segments;
                    root->CreateObject(&column);
                    root->CreateArray(&segments);
                    const auto& source = grid.columns[columnIndex];
                    const auto& state = frame.columns[columnIndex];
                    ok = column.IsObject() && segments.IsArray() &&
                        SetString(column, "columnId", source.columnId) &&
                        SetString(column, "label", source.label) &&
                        column.SetMember("maximumSegments", RE::Scaleform::GFx::Value(
                            source.maximumSegments)) &&
                        column.SetMember("segmentCount", RE::Scaleform::GFx::Value(
                            state.segmentCount)) &&
                        column.SetMember("currentCount", RE::Scaleform::GFx::Value(
                            state.currentCount)) &&
                        column.SetMember("maximumCount", RE::Scaleform::GFx::Value(
                            state.maximumCount)) &&
                        column.SetMember("targetCount", RE::Scaleform::GFx::Value(
                            state.targetCount));
                    for (std::uint32_t segmentIndex = 0;
                         ok && segmentIndex < state.segmentCount; ++segmentIndex) {
                        RE::Scaleform::GFx::Value segment;
                        root->CreateObject(&segment);
                        const auto& pip = state.segments[segmentIndex];
                        ok = segment.IsObject() &&
                            segment.SetMember("tierIndex", RE::Scaleform::GFx::Value(
                                pip.tierIndex)) &&
                            segment.SetMember("live", RE::Scaleform::GFx::Value(
                                pip.live != 0)) &&
                            segment.SetMember("preview", RE::Scaleform::GFx::Value(
                                pip.preview != 0)) &&
                            segment.SetMember("interactive", RE::Scaleform::GFx::Value(
                                pip.interactive != 0)) &&
                            segments.PushBack(segment);
                    }
                    ok = ok && column.SetMember("segments", segments) &&
                        columns.PushBack(column);
                }
                return ok && target.SetMember("tiers", tiers) &&
                    target.SetMember("columns", columns);
            }

            void PublishModel(const MenuSession::Model& a_model)
            {
                if (!movie || !movie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event("bridge_model_publish_failed", "movie root unavailable");
                    return;
                }
                if (modelPublicationActive) {
                    DeferModel(a_model, "reentrant-publication");
                    return;
                }
                auto* root = movie->asMovieRoot.get();
                RE::Scaleform::GFx::Value model, modules, pages;
                root->CreateObject(&model);
                root->CreateArray(&modules);
                root->CreateArray(&pages);
                std::uint32_t activePage{};
                std::uint32_t selectedControl{};
                for (std::uint32_t i = 0; i < a_model.pages.size(); ++i) {
                    if (a_model.pages[i].moduleId == a_model.activeModuleId &&
                        a_model.pages[i].pageId == a_model.activePageId) {
                        activePage = i;
                        for (std::uint32_t j = 0; j < a_model.pages[i].controls.size(); ++j) {
                            if (a_model.pages[i].controls[j].descriptor.controlId == a_model.selectedControlId) {
                                selectedControl = j; break;
                            }
                        }
                        break;
                    }
                }
                bool populated = model.IsObject() && modules.IsArray() && pages.IsArray() &&
                    model.SetMember("schemaVersion", RE::Scaleform::GFx::Value(a_model.schemaVersion)) &&
                    model.SetMember("generation", RE::Scaleform::GFx::Value(
                        static_cast<double>(a_model.generation))) &&
                    model.SetMember("revision", RE::Scaleform::GFx::Value(static_cast<double>(a_model.revision))) &&
                    model.SetMember("activePage", RE::Scaleform::GFx::Value(activePage)) &&
                    model.SetMember("selectedControl", RE::Scaleform::GFx::Value(selectedControl)) &&
                    SetString(model, "selectedGridColumnId",
                        a_model.selectedGridColumnId) &&
                    model.SetMember("focusRegion", RE::Scaleform::GFx::Value(
                        static_cast<std::uint32_t>(inputFocus.region))) &&
                    model.SetMember("focusedAction", RE::Scaleform::GFx::Value(
                        inputFocus.actionIndex)) &&
                    model.SetMember("dirty", RE::Scaleform::GFx::Value(a_model.dirty)) &&
                    model.SetMember("dirtyDecisionActive", RE::Scaleform::GFx::Value(
                        a_model.dirtyDecisionActive)) &&
                    model.SetMember("dirtyDecisionClosesMenu", RE::Scaleform::GFx::Value(
                        a_model.dirtyDecisionClosesMenu)) &&
                    model.SetMember("bindingCaptureActive", RE::Scaleform::GFx::Value(
                        a_model.bindingCaptureActive)) &&
                    model.SetMember("textCaptureActive", RE::Scaleform::GFx::Value(
                        a_model.textCaptureActive)) &&
                    model.SetMember("bindingConflictActive", RE::Scaleform::GFx::Value(
                        a_model.bindingConflictActive)) &&
                    SetString(model, "captureModuleId", a_model.captureModuleId) &&
                    SetString(model, "capturePageId", a_model.capturePageId) &&
                    SetString(model, "captureControlId", a_model.captureControlId) &&
                    SetString(model, "bindingConflictDetail",
                        a_model.bindingConflictDetail) &&
                    SetString(model, "error", a_model.error);
                for (std::uint32_t moduleIndex = 0;
                     populated && moduleIndex < a_model.modules.size(); ++moduleIndex) {
                    const auto& source = a_model.modules[moduleIndex];
                    RE::Scaleform::GFx::Value module;
                    root->CreateObject(&module);
                    populated = module.IsObject() &&
                        SetString(module, "moduleId", source.moduleId) &&
                        SetString(module, "moduleTitle", source.title) &&
                        SetString(module, "pageId", source.firstPageId) &&
                        modules.PushBack(module);
                }
                for (std::uint32_t pageIndex = 0; populated && pageIndex < a_model.pages.size(); ++pageIndex) {
                    const auto& source = a_model.pages[pageIndex];
                    RE::Scaleform::GFx::Value page, controls, liveComponents;
                    root->CreateObject(&page); root->CreateArray(&controls);
                    root->CreateArray(&liveComponents);
                    populated = page.IsObject() && controls.IsArray() &&
                        liveComponents.IsArray() &&
                        SetString(page, "moduleId", source.moduleId) &&
                        SetString(page, "moduleTitle", source.moduleTitle) &&
                        SetString(page, "pageId", source.pageId) &&
                        SetString(page, "title", source.title) && SetString(page, "description", source.description);
                    for (std::uint32_t controlIndex = 0; populated && controlIndex < source.controls.size(); ++controlIndex) {
                        RE::Scaleform::GFx::Value control;
                        root->CreateObject(&control);
                        populated = control.IsObject() && SerializeControl(control, source.controls[controlIndex]) &&
                            controls.PushBack(control);
                    }
                    for (std::uint32_t componentIndex = 0; populated &&
                         componentIndex < source.liveComponents.size(); ++componentIndex) {
                        RE::Scaleform::GFx::Value component;
                        root->CreateObject(&component);
                        populated = component.IsObject() &&
                            SerializeLiveComponent(component,
                                source.liveComponents[componentIndex]) &&
                            liveComponents.PushBack(component);
                    }
                    populated = populated && page.SetMember("controls", controls) &&
                        page.SetMember("liveComponents", liveComponents) &&
                        pages.PushBack(page);
                }
                populated = populated && model.SetMember("modules", modules) &&
                    model.SetMember("pages", pages);
                modelPublicationActive = true;
                const bool invoked = populated && menuObj.Invoke("applyModel", nullptr, &model, 1);
                modelPublicationActive = false;
                if (invoked) {
                    session.AcknowledgePublishedGeneration(a_model.generation);
                    lastPublishedInputFocus = inputFocus;
                }
                EvidenceLog::Event(
                    "bridge_model_published",
                    std::format(
                        "generation={} revision={} modules={} pages={} populated={} invoked={}",
                        a_model.generation, a_model.revision, a_model.modules.size(),
                        a_model.pages.size(), populated, invoked));
            }

            [[nodiscard]] static std::uint32_t ReadUnsigned(
                const RE::Scaleform::GFx::Value& a_value) noexcept
            {
                if (a_value.IsUInt()) {
                    return a_value.GetUInt();
                }
                if (a_value.IsInt() && a_value.GetInt() >= 0) {
                    return static_cast<std::uint32_t>(a_value.GetInt());

                }
                if (a_value.IsNumber() && std::isfinite(a_value.GetNumber()) &&
                    std::trunc(a_value.GetNumber()) == a_value.GetNumber() &&
                    a_value.GetNumber() >= 0.0 &&
                    a_value.GetNumber() <= static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
                    return static_cast<std::uint32_t>(a_value.GetNumber());
                }
                return std::numeric_limits<std::uint32_t>::max();
            }

            [[nodiscard]] static bool ReadBoolean(const RE::Scaleform::GFx::Value& a_value,
                std::uint32_t& a_result) noexcept
            {
                if (!a_value.IsBoolean()) return false;
                a_result = a_value.GetBoolean() ? 1U : 0U;
                return true;
            }

            [[nodiscard]] static bool ReadInteger(const RE::Scaleform::GFx::Value& a_value,
                std::int64_t& a_result) noexcept
            {
                constexpr double kMaximumExactInteger = 9007199254740991.0;
                if (a_value.IsInt()) { a_result = a_value.GetInt(); return true; }
                if (a_value.IsUInt()) { a_result = a_value.GetUInt(); return true; }
                if (!a_value.IsNumber() || !std::isfinite(a_value.GetNumber()) ||
                    std::trunc(a_value.GetNumber()) != a_value.GetNumber() ||
                    a_value.GetNumber() < -kMaximumExactInteger ||
                    a_value.GetNumber() > kMaximumExactInteger) return false;
                a_result = static_cast<std::int64_t>(a_value.GetNumber());
                return true;
            }


            [[nodiscard]] static bool ReadFiniteNumber(const RE::Scaleform::GFx::Value& a_value,
                double& a_result) noexcept
            {
                if (!a_value.IsNumber() || !std::isfinite(a_value.GetNumber())) return false;
                a_result = a_value.GetNumber();
                return true;
            }

            [[nodiscard]] static bool ReadExactGeneration(
                const RE::Scaleform::GFx::Value& a_value,
                std::uint64_t& a_result) noexcept
            {
                constexpr double kMaximumExactInteger = 9007199254740991.0;
                if (a_value.IsUInt()) {
                    a_result = a_value.GetUInt();
                    return true;
                }
                if (a_value.IsInt() && a_value.GetInt() >= 0) {
                    a_result = static_cast<std::uint64_t>(a_value.GetInt());
                    return true;
                }
                if (!a_value.IsNumber() || !std::isfinite(a_value.GetNumber()) ||
                    std::trunc(a_value.GetNumber()) != a_value.GetNumber() ||
                    a_value.GetNumber() < 0.0 ||
                    a_value.GetNumber() > kMaximumExactInteger) {
                    return false;
                }
                a_result = static_cast<std::uint64_t>(a_value.GetNumber());
                return true;
            }


            [[nodiscard]] MenuSession::Session& InputSession() noexcept override
            {
                return session;
            }

            [[nodiscard]] MenuInputRouter::FocusState& InputFocus() noexcept override
            {
                return inputFocus;
            }

            void PublishInputModel(const MenuSession::Model& a_model) override
            {
                DeferModel(a_model, "native-input");
            }

            void DispatchInputCommand(const MenuSession::Command& a_command,
                std::string_view a_name, std::string_view a_source) override
            {
                DispatchCommand(a_command, a_name, a_source);
            }

            void HandleInputWheel(std::int32_t a_direction) override
            {
                HandlePointerWheel(a_direction);
            }

            void OnShown()
            {
                // UI::IsMenuOpen becomes true while the factory and Show lifecycle are
                // still in flight.  The platform pointer worker can therefore deliver
                // the click that selected the PauseMenu entry to this new movie.  Do
                // not Invoke into Scaleform until Show has completed and that click has
                // crossed a release + one-frame quarantine.
                if (!menuShown) {
                    returnToPauseOnClose =
                        Ui::PauseMenuIntegration::ConsumeReturnToPauseOnClose();
                    EvidenceLog::Event(
                        "control_panel_session_origin",
                        std::format(
                            "return_target={}",
                            returnToPauseOnClose ? "PauseMenu" : "none"));
                }
                menuShown = true;
                LiveComponents::HostRegistry().SetMenuActive(true);
                closing = false;
                pointerInputArmed = false;
                pointerReleaseObserved = false;
                EvidenceLog::Event(
                    "bridge_pointer_barrier_started", "reason=menu-shown");
            }

            [[nodiscard]] bool OnHidden()
            {
                const bool returnToPause = closing && returnToPauseOnClose;
                menuShown = false;
                LiveComponents::HostRegistry().SetMenuActive(false);
                pointerInputArmed = false;
                pointerReleaseObserved = false;
                returnToPauseOnClose = false;
                // Discard an origin queued by a duplicate Show while this
                // session was already active; it must not leak to the next one.
                (void)Ui::PauseMenuIntegration::ConsumeReturnToPauseOnClose();
                if (session.IsCaptureActive()) {
                    EvidenceLog::Event(
                        session.IsTextCaptureActive() ? "text_capture_cancelled" :
                                                        "binding_capture_cancelled",
                        "source=menu-hidden");
                }
                session.Teardown();
                dirtyDecisionReturnFocus.reset();
                MenuApiHost::SetInputCaptureActive(false);
                pendingModel.reset();
                EvidenceLog::Event(
                    "bridge_pointer_disarmed", "reason=menu-hidden");
                return returnToPause;
            }

            void AdvancePointerInputBarrier()
            {
                if (!menuShown || pointerInputArmed) {
                    return;
                }
                if ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) {
                    pointerReleaseObserved = false;
                    return;
                }
                if (!pointerReleaseObserved) {
                    pointerReleaseObserved = true;
                    EvidenceLog::Event(
                        "bridge_pointer_release_observed", "armed=false");
                    return;
                }
                pointerInputArmed = true;
                EvidenceLog::Event(
                    "bridge_pointer_armed", "release_barrier_frames=2");
            }

            MenuSession::Session session;
            MenuInputRouter::FocusState inputFocus;
            MenuInputRouter::FocusState lastPublishedInputFocus;
            std::optional<MenuInputRouter::FocusState> dirtyDecisionReturnFocus;
            Input::NativeMenuInputAdapter inputAdapter;

            std::uint64_t refreshCursor{};
            std::uint64_t lastAppliedGeneration{};
            std::chrono::steady_clock::time_point lastLivePoll{};
            std::optional<MenuSession::Model> pendingModel;
            bool modelPublicationActive{};
            bool closing{};
            bool menuShown{};
            bool pointerInputArmed{};
            bool pointerReleaseObserved{};
            bool returnToPauseOnClose{};

            RE::Scaleform::GFx::Movie* movie{};
            RE::Scaleform::GFx::Value menuObj;
    };

    MenuBridge::MenuBridge() : impl_(std::make_unique<Impl>()) {}
    MenuBridge::~MenuBridge() = default;

    void MenuBridge::Attach(RE::Scaleform::GFx::Movie* a_movie,
        const RE::Scaleform::GFx::Value& a_menuObject)
    {
        impl_->Attach(a_movie, a_menuObject);
    }

    void MenuBridge::LogMovieState(std::string_view a_phase)
    {
        impl_->LogMovieState(a_phase);
    }

    void MenuBridge::Call(
        const RE::Scaleform::GFx::FunctionHandler::Params& a_params)
    {
        impl_->Call(a_params);
    }

    void MenuBridge::OnShown()
    {
        impl_->OnShown();
    }

    bool MenuBridge::OnHidden()
    {
        return impl_->OnHidden();
    }

    bool MenuBridge::HandleButtonInput(const RE::ButtonEvent& a_event)
    {
        return impl_->HandleButtonInput(a_event);
    }

    void MenuBridge::HandlePointerPhase(Input::PointerPhase a_phase)
    {
        impl_->HandlePointerPhase(a_phase);
    }
}
