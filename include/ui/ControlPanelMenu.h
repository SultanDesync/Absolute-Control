#pragma once

#include "input/PlatformInputServices.h"

#include <cstdint>

namespace AbsoluteControlPanelResearch::Ui::ControlPanelMenu
{
    [[nodiscard]] bool Register(std::uint32_t a_menuFlags) noexcept;
    void RequestOpenFromHotkey() noexcept;
    void DispatchPointerPhase(Input::PointerPhase a_phase) noexcept;
}
