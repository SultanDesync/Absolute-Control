#pragma once

#include "MenuInputRouter.h"
#include "MenuSession.h"

#include <RE/Starfield.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace AbsoluteControlPanelResearch::Input
{
    class NativeMenuInputSink
    {
    public:
        virtual ~NativeMenuInputSink() = default;
        [[nodiscard]] virtual MenuSession::Session& InputSession() noexcept = 0;
        [[nodiscard]] virtual MenuInputRouter::FocusState& InputFocus() noexcept = 0;
        virtual void PublishInputModel(const MenuSession::Model& a_model) = 0;
        virtual void DispatchInputCommand(const MenuSession::Command& a_command,
            std::string_view a_name, std::string_view a_source) = 0;
        virtual void HandleInputWheel(std::int32_t a_direction) = 0;
    };

    class NativeMenuInputAdapter final
    {
    public:
        void BeginBindingCapture(bool a_awaitingRelease) noexcept;
        void BeginTextCapture(bool a_awaitingRelease) noexcept;
        [[nodiscard]] bool HandleButtonInput(
            const RE::ButtonEvent& a_event, NativeMenuInputSink& a_sink);

    private:
        [[nodiscard]] bool IsCapturedModifierDown(
            std::int32_t a_generic, std::int32_t a_left,
            std::int32_t a_right) const noexcept;
        [[nodiscard]] static bool IsModifierVirtualKey(
            std::int32_t a_virtualKey) noexcept;
        [[nodiscard]] static std::optional<char> TextCharacter(
            std::int32_t a_virtualKey, bool a_shift) noexcept;
        [[nodiscard]] static std::string_view CommandName(
            MenuSession::CommandKind a_kind) noexcept;

        std::array<bool, 256> keyDown_{};
        std::array<bool, 6> gamepadButtonDown_{};
        bool captureAwaitingRelease_{};
        std::int32_t lastWheelIdCode_{};
        std::uint32_t lastWheelTimeCode_{};
        float lastWheelValue_{};
        std::chrono::steady_clock::time_point lastWheelHandledAt_{};
    };
}
