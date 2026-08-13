#pragma once

#include "MenuSession.h"

#include <cstdint>
#include <optional>

namespace AbsoluteControlPanelResearch::MenuInputRouter
{
    inline constexpr std::int32_t kEscape = 27;
    inline constexpr std::int32_t kAccept = 'E';
    inline constexpr std::int32_t kEnter = 13;
    inline constexpr std::int32_t kSpace = 32;
    inline constexpr std::int32_t kUp = 'W';
    inline constexpr std::int32_t kDown = 'S';
    inline constexpr std::int32_t kLeft = 'A';
    inline constexpr std::int32_t kRight = 'D';
    inline constexpr std::int32_t kArrowUp = 0x26;
    inline constexpr std::int32_t kArrowDown = 0x28;
    inline constexpr std::int32_t kArrowLeft = 0x25;
    inline constexpr std::int32_t kArrowRight = 0x27;
    inline constexpr std::int32_t kDecrease = 'Z';
    inline constexpr std::int32_t kIncrease = 'C';
    inline constexpr std::int32_t kPreviousPage = 'Q';
    inline constexpr std::int32_t kNextPage = 'R';
    inline constexpr std::int32_t kApply = 'F';
    inline constexpr std::int32_t kCancel = 'X';

    enum class FocusRegion : std::uint32_t
    {
        Modules,
        Controls,
        Actions
    };

    struct FocusState
    {
        FocusRegion region{ FocusRegion::Controls };
        std::uint32_t actionIndex{};

        bool operator==(const FocusState&) const = default;
    };

    struct RouteResult
    {
        bool handled{};
        FocusState focus;
        std::optional<MenuSession::Command> command;
    };

    [[nodiscard]] bool IsMenuKey(std::int32_t a_keyCode) noexcept;
    [[nodiscard]] RouteResult Route(const MenuSession::Model& a_model,
        std::int32_t a_keyCode, FocusState a_focus = {}) noexcept;
}
