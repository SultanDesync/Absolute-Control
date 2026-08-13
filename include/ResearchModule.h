#pragma once

namespace AbsoluteControlPanelResearch::ResearchModule
{
    inline constexpr std::string_view kModuleId = "absolute-control-panel.research";
    inline constexpr std::string_view kPageId = "representative-controls";
    inline constexpr std::string_view kToggleId = "axis-invert";
    inline constexpr std::string_view kSensitivityId = "axis-sensitivity";
    inline constexpr std::string_view kBindingId = "test-button-binding";

    bool Register() noexcept;
}
