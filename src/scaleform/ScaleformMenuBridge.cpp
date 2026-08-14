
#include "scaleform/ScaleformMenuBridge.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
#include "MenuInputRouter.h"
#include "MenuSession.h"
#include "SlopAPI.h"
#include "input/NativeMenuInputAdapter.h"
#include "input/PlatformInputServices.h"
#include "ui/MenuMessaging.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>

namespace AbsoluteControlPanelResearch::Scaleform
{
    namespace
    {
        constexpr auto kCodeObjectName = "BGSCodeObj";
    }

    class MenuBridge::Impl final : public Input::NativeMenuInputSink
    {
    public:
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
                    PublishModel(session.Snapshot());
                    break;
                case NativeFunction::Close:
                    CloseSession();
                    break;
                case NativeFunction::Dispatch:
                    DispatchFlat(a_params);
                    break;
                case NativeFunction::Focus: {
                    const auto region = a_params.argCount >= 1 ?
                        ReadUnsigned(a_params.args[0]) :
                        std::numeric_limits<std::uint32_t>::max();
                    const auto action = a_params.argCount >= 2 ?
                        ReadUnsigned(a_params.args[1]) :
                        std::numeric_limits<std::uint32_t>::max();
                    if (a_params.argCount != 2 || region > static_cast<std::uint32_t>(
                            MenuInputRouter::FocusRegion::Actions) || action > 2) {
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
                    PollRefresh();
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
                    PublishModel(session.Dispatch(command));
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
                else if (name == "write") command.kind = MenuSession::CommandKind::Write;
                else if (name == "invoke") command.kind = MenuSession::CommandKind::Invoke;
                else if (name == "beginBindingCapture") command.kind = MenuSession::CommandKind::BeginBindingCapture;
                else if (name == "apply") command.kind = MenuSession::CommandKind::Apply;
                else if (name == "cancel") command.kind = MenuSession::CommandKind::Cancel;
                else if (name == "close") command.kind = MenuSession::CommandKind::Close;
                else command.schemaVersion = 0;
                DispatchCommand(command, name, "scaleform");
            }

            void PollRefresh()
            {
                if (!MenuApiHost::ConsumeRefresh(refreshCursor)) {
                    return;
                }
                EvidenceLog::Event(
                    "bridge_refresh_consumed",
                    std::format("refresh_revision={}", refreshCursor));
                PublishModel(session.Snapshot());
            }

            void DispatchCommand(const MenuSession::Command& a_command,
                std::string_view a_name, std::string_view a_source)
            {
                EvidenceLog::Event("bridge_command",
                    std::format("command={} source={}", a_name, a_source));
                const auto model = session.Dispatch(a_command);
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
                EvidenceLog::Event(
                    model.error.empty() ? "bridge_command_accepted" :
                                          "bridge_command_rejected",

                    std::format("command={} source={} error={}",
                        a_name, a_source, model.error));
                PublishModel(model);
                if (a_command.kind == MenuSession::CommandKind::Close &&
                    model.error.empty()) {
                    Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
                }
            }

            void CloseSession()
            {
                if (session.IsBindingCaptureActive()) {
                    (void)session.CancelBindingCapture();
                    MenuApiHost::SetInputCaptureActive(false);
                }
                const auto model = session.Dispatch(MenuSession::Command{ .kind = MenuSession::CommandKind::Close });
                PublishModel(model);
                if (model.error.empty()) Ui::QueueControlPanelMessage(RE::UI_MESSAGE_TYPE::kHide, "bridge");
            }

            [[nodiscard]] bool HandleButtonInput(const RE::ButtonEvent& a_event)
            {
                return inputAdapter.HandleButtonInput(a_event, *this);
            }

            void HandlePointerPhase(Input::PointerPhase a_phase)
            {
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
                return SetString(a_target, "controlId", descriptor.controlId) &&
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
                    SetString(a_target, "stringValue", a_control.value.stringValue) && SetString(a_target, "error", a_control.error);
            }

            void PublishModel(const MenuSession::Model& a_model)
            {
                if (!movie || !movie->asMovieRoot || !menuObj.IsObject()) {
                    EvidenceLog::Event("bridge_model_publish_failed", "movie root unavailable");
                    return;
                }
                auto* root = movie->asMovieRoot.get();
                RE::Scaleform::GFx::Value model, pages;
                root->CreateObject(&model); root->CreateArray(&pages);
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
                bool populated = model.IsObject() && pages.IsArray() &&
                    model.SetMember("schemaVersion", RE::Scaleform::GFx::Value(a_model.schemaVersion)) &&
                    model.SetMember("generation", RE::Scaleform::GFx::Value(
                        static_cast<double>(a_model.generation))) &&
                    model.SetMember("revision", RE::Scaleform::GFx::Value(static_cast<double>(a_model.revision))) &&
                    model.SetMember("activePage", RE::Scaleform::GFx::Value(activePage)) &&
                    model.SetMember("selectedControl", RE::Scaleform::GFx::Value(selectedControl)) &&
                    model.SetMember("focusRegion", RE::Scaleform::GFx::Value(
                        static_cast<std::uint32_t>(inputFocus.region))) &&
                    model.SetMember("focusedAction", RE::Scaleform::GFx::Value(
                        inputFocus.actionIndex)) &&
                    model.SetMember("dirty", RE::Scaleform::GFx::Value(a_model.dirty)) &&
                    model.SetMember("bindingCaptureActive", RE::Scaleform::GFx::Value(
                        a_model.bindingCaptureActive)) &&
                    SetString(model, "captureModuleId", a_model.captureModuleId) &&
                    SetString(model, "capturePageId", a_model.capturePageId) &&
                    SetString(model, "captureControlId", a_model.captureControlId) &&
                    SetString(model, "error", a_model.error);
                for (std::uint32_t pageIndex = 0; populated && pageIndex < a_model.pages.size(); ++pageIndex) {
                    const auto& source = a_model.pages[pageIndex];
                    RE::Scaleform::GFx::Value page, controls;
                    root->CreateObject(&page); root->CreateArray(&controls);
                    populated = page.IsObject() && controls.IsArray() &&
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
                    populated = populated && page.SetMember("controls", controls) && pages.PushBack(page);
                }
                populated = populated && model.SetMember("pages", pages);
                const bool invoked = populated && menuObj.Invoke("applyModel", nullptr, &model, 1);
                EvidenceLog::Event(
                    "bridge_model_published",
                    std::format(
                        "generation={} revision={} pages={} populated={} invoked={}",
                        a_model.generation, a_model.revision, a_model.pages.size(),
                        populated, invoked));
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
                PublishModel(a_model);
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

            MenuSession::Session session;
            MenuInputRouter::FocusState inputFocus;
            Input::NativeMenuInputAdapter inputAdapter;

            std::uint64_t refreshCursor{};
            std::uint64_t lastAppliedGeneration{};

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

    bool MenuBridge::HandleButtonInput(const RE::ButtonEvent& a_event)
    {
        return impl_->HandleButtonInput(a_event);
    }

    void MenuBridge::HandlePointerPhase(Input::PointerPhase a_phase)
    {
        impl_->HandlePointerPhase(a_phase);
    }
}
