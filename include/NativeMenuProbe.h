#pragma once

#include "ProbeState.h"

namespace AbsoluteControlPanelResearch::NativeMenuProbe
{
    inline constexpr std::string_view kMenuName = "AbsoluteControlPanelMenu";
    inline constexpr std::string_view kMoviePath = "AbsoluteControlPanelMenu";
    inline constexpr std::string_view kRootPath = "root1.AbsoluteControlPanelMenu_mc";

    void OnDataReady() noexcept;
    [[nodiscard]] ProbePhase Phase() noexcept;
    [[nodiscard]] bool IsRegistrationEnabled() noexcept;
}
