#pragma once

namespace RE
{
    class UI;
}

namespace AbsoluteControlPanelResearch::Interop::OSFUICompatibility
{
    // OSF UI deliberately keeps its capture menu non-modal. Absolute Control's
    // ordinary "another modal menu is open" protection therefore cannot see it.
    inline constexpr auto kRuntimeModuleName = L"OSFUI.dll";
    inline constexpr auto kFocusMenuName = "OSFUI_FocusMenu";

    [[nodiscard]] bool IsRuntimePresent() noexcept;
    [[nodiscard]] bool IsPresentationActive(RE::UI* a_ui) noexcept;
    void LogStartupState() noexcept;
}
