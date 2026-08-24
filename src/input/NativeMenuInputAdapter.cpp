#include "input/NativeMenuInputAdapter.h"

#include "EvidenceLog.h"
#include "MenuApiHost.h"
#include "SlopAPI.h"

#include <REX/W32/XINPUT.h>
#include <SFSE/InputMap.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>

namespace AbsoluteControlPanelResearch::Input
{
    namespace
    {
        constexpr std::int32_t kMouseWheelUpIdCode = 0x800;
        constexpr std::int32_t kMouseWheelDownIdCode = 0x900;
    }

    void NativeMenuInputAdapter::BeginBindingCapture(bool a_awaitingRelease) noexcept
    {
        captureAwaitingRelease_ = a_awaitingRelease;
    }

    void NativeMenuInputAdapter::BeginTextCapture(bool a_awaitingRelease) noexcept
    {
        captureAwaitingRelease_ = a_awaitingRelease;
    }

    bool NativeMenuInputAdapter::HandleButtonInput(
        const RE::ButtonEvent& a_event, NativeMenuInputSink& a_sink)
    {
        auto& session = a_sink.InputSession();
        auto& inputFocus = a_sink.InputFocus();
        if (a_event.deviceType == RE::InputEvent::DeviceType::kGamepad) {
            struct Route {
                std::int32_t mask;
                std::int32_t offset;
                std::int32_t menuKey;
            };
            constexpr std::array routes{
                Route{REX::W32::XINPUT_GAMEPAD_DPAD_UP,
                    SFSE::InputMap::kGamepadButtonOffset_DPAD_UP,
                    MenuInputRouter::kArrowUp},
                Route{REX::W32::XINPUT_GAMEPAD_DPAD_DOWN,
                    SFSE::InputMap::kGamepadButtonOffset_DPAD_DOWN,
                    MenuInputRouter::kArrowDown},
                Route{REX::W32::XINPUT_GAMEPAD_DPAD_LEFT,
                    SFSE::InputMap::kGamepadButtonOffset_DPAD_LEFT,
                    MenuInputRouter::kArrowLeft},
                Route{REX::W32::XINPUT_GAMEPAD_DPAD_RIGHT,
                    SFSE::InputMap::kGamepadButtonOffset_DPAD_RIGHT,
                    MenuInputRouter::kArrowRight},
                Route{REX::W32::XINPUT_GAMEPAD_A,
                    SFSE::InputMap::kGamepadButtonOffset_A,
                    MenuInputRouter::kAccept},
                Route{REX::W32::XINPUT_GAMEPAD_B,
                    SFSE::InputMap::kGamepadButtonOffset_B,
                    MenuInputRouter::kEscape},
                Route{REX::W32::XINPUT_GAMEPAD_X,
                    SFSE::InputMap::kGamepadButtonOffset_X,
                    MenuInputRouter::kDelete},
            };
            const auto route = std::ranges::find_if(
                routes, [&](const Route& candidate) {
                    return a_event.idCode == candidate.mask ||
                           a_event.idCode == candidate.offset;
                });
            if (route == routes.end()) return false;
            const auto routeIndex =
                static_cast<std::size_t>(route - routes.begin());
            const bool isBack = route->menuKey == MenuInputRouter::kEscape;
            if (session.IsCaptureActive() && !isBack) return false;
            const bool down = a_event.value > 0.0F;
            const bool pressed = down && !gamepadButtonDown_[routeIndex];
            gamepadButtonDown_[routeIndex] = down;
            if (!pressed) {
                return true;
            }
            if (session.IsCaptureActive() && isBack) {
                const bool textCapture = session.IsTextCaptureActive();
                const auto model = textCapture ?
                    session.CancelTextCapture() : session.CancelBindingCapture();
                MenuApiHost::SetInputCaptureActive(false);
                EvidenceLog::Event(
                    textCapture ? "text_capture_cancelled" :
                                  "binding_capture_cancelled",
                    "source=controller");
                a_sink.PublishInputModel(model);
                return true;
            }
            const auto previousFocus = inputFocus;
            auto routed = MenuInputRouter::Route(
                session.Snapshot(), route->menuKey, inputFocus);
            inputFocus = routed.focus;
            if (!routed.command) {
                if (previousFocus != inputFocus) {
                    a_sink.PublishInputModel(session.Snapshot());
                }
                return true;
            }
            if (routed.command->kind == MenuSession::CommandKind::Close) {
                EvidenceLog::Event(
                    "bridge_close", "native controller requested close");
            }
            EvidenceLog::Event("menu_input_routed", "source=controller");
            a_sink.DispatchInputCommand(*routed.command,
                CommandName(routed.command->kind), "native-controller");
            return true;
        }
        if (a_event.deviceType == RE::InputEvent::DeviceType::kMouse) {
            if (a_event.idCode != kMouseWheelUpIdCode &&
                a_event.idCode != kMouseWheelDownIdCode) {
                return false;
            }
            if (!std::isfinite(a_event.value) || a_event.value == 0.0F) {
                return true;
            }

            const auto now = std::chrono::steady_clock::now();
            const bool duplicate = lastWheelIdCode_ == a_event.idCode &&
                lastWheelTimeCode_ == a_event.timeCode &&
                lastWheelValue_ == a_event.value &&
                now - lastWheelHandledAt_ < std::chrono::milliseconds(5);
            if (duplicate) {
                EvidenceLog::Event(
                    "mouse_wheel_duplicate", "repeated native delivery suppressed");
                return true;
            }
            lastWheelIdCode_ = a_event.idCode;
            lastWheelTimeCode_ = a_event.timeCode;
            lastWheelValue_ = a_event.value;
            lastWheelHandledAt_ = now;
            a_sink.HandleInputWheel(
                a_event.idCode == kMouseWheelUpIdCode ? -1 : 1);
            return true;
        }
        if (a_event.deviceType != RE::InputEvent::DeviceType::kKeyboard ||
            a_event.idCode < 0 ||
            a_event.idCode >= static_cast<std::int32_t>(keyDown_.size())) {
            return false;
        }

        const auto idCode = static_cast<std::size_t>(a_event.idCode);
        const bool down = a_event.value > 0.0F;
        const bool pressed = down && !keyDown_[idCode];
        keyDown_[idCode] = down;

        if (session.IsBindingCaptureActive()) {
            if (captureAwaitingRelease_) {
                if (std::ranges::none_of(keyDown_, [](bool a_down) { return a_down; })) {
                    captureAwaitingRelease_ = false;
                    EvidenceLog::Event(
                        "binding_capture_armed", "all initiating keys released");
                }
                return true;
            }
            if (!pressed) {
                return true;
            }
            if (a_event.idCode == VK_ESCAPE) {
                const auto model = session.CancelBindingCapture();
                MenuApiHost::SetInputCaptureActive(false);
                EvidenceLog::Event("binding_capture_cancelled", "source=keyboard");
                a_sink.PublishInputModel(model);
                return true;
            }
            const auto captureFlags = session.BindingCaptureFlags();
            if (a_event.idCode == VK_BACK &&
                (captureFlags & SlopApi::kBindingClearable) != 0) {
                const auto model = session.CompleteBindingCapture({});
                MenuApiHost::SetInputCaptureActive(false);
                EvidenceLog::Event("binding_capture_cleared", "source=keyboard");
                a_sink.PublishInputModel(model);
                return true;
            }
            if (IsModifierVirtualKey(a_event.idCode)) {
                return true;
            }
            if ((captureFlags & SlopApi::kBindingKeyboard) == 0) {
                return true;
            }
            const bool modifiers = (captureFlags & SlopApi::kBindingModifiers) != 0;
            const auto binding = std::format(
                "keyboard:0x{:02X};ctrl={};alt={};shift={}",
                a_event.idCode,
                modifiers && IsCapturedModifierDown(
                    VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ? 1 : 0,
                modifiers && IsCapturedModifierDown(
                    VK_MENU, VK_LMENU, VK_RMENU) ? 1 : 0,
                modifiers && IsCapturedModifierDown(
                    VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ? 1 : 0);
            const auto model = session.CompleteBindingCapture(binding);
            MenuApiHost::SetInputCaptureActive(false);
            EvidenceLog::Event(
                model.error.empty() ? "binding_capture_completed" :
                                      "binding_capture_rejected",
                std::format(
                    "source=keyboard binding={} error={}", binding, model.error));
            a_sink.PublishInputModel(model);
            return true;
        }

        if (session.IsTextCaptureActive()) {
            if (captureAwaitingRelease_) {
                if (std::ranges::none_of(keyDown_, [](bool a_down) { return a_down; })) {
                    captureAwaitingRelease_ = false;
                    EvidenceLog::Event(
                        "text_capture_armed", "all initiating keys released");
                }
                return true;
            }
            if (!pressed) return true;
            if (a_event.idCode == VK_ESCAPE) {
                const auto model = session.CancelTextCapture();
                MenuApiHost::SetInputCaptureActive(false);
                EvidenceLog::Event("text_capture_cancelled", "source=keyboard");
                a_sink.PublishInputModel(model);
                return true;
            }
            if (a_event.idCode == VK_RETURN) {
                const auto model = session.CompleteTextCapture();
                if (!model.textCaptureActive) {
                    MenuApiHost::SetInputCaptureActive(false);
                }
                EvidenceLog::Event(
                    model.error.empty() ? "text_capture_completed" :
                                          "text_capture_rejected",
                    std::format("source=keyboard error={}", model.error));
                a_sink.PublishInputModel(model);
                return true;
            }
            if (a_event.idCode == VK_BACK) {
                a_sink.PublishInputModel(session.BackspaceTextCapture());
                return true;
            }
            if (IsModifierVirtualKey(a_event.idCode)) return true;
            const auto character = TextCharacter(
                a_event.idCode,
                IsCapturedModifierDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT));
            if (character) {
                a_sink.PublishInputModel(session.AppendTextCapture(*character));
            }
            return true;
        }

        if (!MenuInputRouter::IsMenuKey(a_event.idCode)) {
            return false;
        }
        if (!pressed) {
            return true;
        }

        const auto previousFocus = inputFocus;
        auto routed = MenuInputRouter::Route(
            session.Snapshot(), a_event.idCode, inputFocus);
        inputFocus = routed.focus;
        if (!routed.command) {
            EvidenceLog::Event(
                "menu_key_no_action",
                std::format(
                    "id_code={} focus_region={} action_index={} changed={}",
                    a_event.idCode, static_cast<std::uint32_t>(inputFocus.region),
                    inputFocus.actionIndex, previousFocus != inputFocus));
            if (previousFocus != inputFocus) {
                a_sink.PublishInputModel(session.Snapshot());
            }
            return true;
        }
        if (routed.command->kind == MenuSession::CommandKind::Close) {
            EvidenceLog::Event("bridge_close", "native keyboard requested close");
        }
        a_sink.DispatchInputCommand(
            *routed.command, CommandName(routed.command->kind), "native-keyboard");
        return true;
    }

    bool NativeMenuInputAdapter::IsCapturedModifierDown(
        std::int32_t a_generic, std::int32_t a_left,
        std::int32_t a_right) const noexcept
    {
        const auto eventDown = [this](std::int32_t a_virtualKey) {
            return a_virtualKey >= 0 &&
                a_virtualKey < static_cast<std::int32_t>(keyDown_.size()) &&
                keyDown_[static_cast<std::size_t>(a_virtualKey)];
        };
        return eventDown(a_generic) || eventDown(a_left) || eventDown(a_right) ||
            (::GetAsyncKeyState(a_generic) & 0x8000) != 0 ||
            (::GetAsyncKeyState(a_left) & 0x8000) != 0 ||
            (::GetAsyncKeyState(a_right) & 0x8000) != 0;
    }

    bool NativeMenuInputAdapter::IsModifierVirtualKey(
        std::int32_t a_virtualKey) noexcept
    {
        return a_virtualKey == VK_SHIFT || a_virtualKey == VK_LSHIFT ||
            a_virtualKey == VK_RSHIFT || a_virtualKey == VK_CONTROL ||
            a_virtualKey == VK_LCONTROL || a_virtualKey == VK_RCONTROL ||
            a_virtualKey == VK_MENU || a_virtualKey == VK_LMENU ||
            a_virtualKey == VK_RMENU || a_virtualKey == VK_LWIN ||
            a_virtualKey == VK_RWIN;
    }

    std::optional<char> NativeMenuInputAdapter::TextCharacter(
        std::int32_t a_virtualKey, bool a_shift) noexcept
    {
        if (a_virtualKey >= 'A' && a_virtualKey <= 'Z') {
            const auto upper = static_cast<char>(a_virtualKey);
            return a_shift ? upper : static_cast<char>(upper - 'A' + 'a');
        }
        if (a_virtualKey >= '0' && a_virtualKey <= '9') {
            constexpr std::string_view shifted{")!@#$%^&*("};
            return a_shift ? shifted[static_cast<std::size_t>(a_virtualKey - '0')] :
                             static_cast<char>(a_virtualKey);
        }
        switch (a_virtualKey) {
        case VK_SPACE: return ' ';
        case VK_OEM_1: return a_shift ? ':' : ';';
        case VK_OEM_PLUS: return a_shift ? '+' : '=';
        case VK_OEM_COMMA: return a_shift ? '<' : ',';
        case VK_OEM_MINUS: return a_shift ? '_' : '-';
        case VK_OEM_PERIOD: return a_shift ? '>' : '.';
        case VK_OEM_2: return a_shift ? '?' : '/';
        case VK_OEM_3: return a_shift ? '~' : '`';
        case VK_OEM_4: return a_shift ? '{' : '[';
        case VK_OEM_5: return a_shift ? '|' : '\\';
        case VK_OEM_6: return a_shift ? '}' : ']';
        case VK_OEM_7: return a_shift ? '"' : '\'';
        default: return std::nullopt;
        }
    }

    std::string_view NativeMenuInputAdapter::CommandName(
        MenuSession::CommandKind a_kind) noexcept
    {
        switch (a_kind) {
        case MenuSession::CommandKind::SelectPage: return "selectPage";
        case MenuSession::CommandKind::SelectControl: return "selectControl";
        case MenuSession::CommandKind::SelectGridColumn: return "selectGridColumn";
        case MenuSession::CommandKind::Write: return "write";
        case MenuSession::CommandKind::Invoke: return "invoke";
        case MenuSession::CommandKind::BeginBindingCapture: return "beginBindingCapture";
        case MenuSession::CommandKind::BeginTextCapture: return "beginTextCapture";
        case MenuSession::CommandKind::Apply: return "apply";
        case MenuSession::CommandKind::Cancel: return "cancel";
        case MenuSession::CommandKind::Close: return "close";
        case MenuSession::CommandKind::ResolveDirtyApply: return "dirtyApply";
        case MenuSession::CommandKind::ResolveDirtyDiscard: return "dirtyDiscard";
        case MenuSession::CommandKind::ResolveDirtyStay: return "dirtyStay";
        case MenuSession::CommandKind::ResolveBindingReassign: return "bindingReassign";
        case MenuSession::CommandKind::ResolveBindingCancel: return "bindingCancel";
        case MenuSession::CommandKind::ResolveActionConfirm: return "actionConfirm";
        case MenuSession::CommandKind::ResolveActionCancel: return "actionCancel";
        case MenuSession::CommandKind::Compound: return "compound";
        }
        return "unknown";
    }
}
