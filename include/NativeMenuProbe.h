#pragma once

#include "ProbeState.h"

namespace AbsoluteControlPanelResearch::NativeMenuProbe
{
    inline constexpr std::string_view kMenuName = "AbsoluteControlPanelMenu";
    inline constexpr std::string_view kMoviePath = "AbsoluteControlPanelMenu";
    inline constexpr std::string_view kRootPath = "_root";
    inline const std::filesystem::path kConfigPath =
        "Data/SFSE/Plugins/AbsoluteControlPanel.ini";

    void OnDataReady() noexcept;
    [[nodiscard]] ProbePhase Phase() noexcept;
    [[nodiscard]] bool IsRegistrationEnabled() noexcept;
}
