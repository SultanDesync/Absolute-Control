#include "interop/OSFUICompatibility.h"

#include "EvidenceLog.h"

#include <RE/Starfield.h>

#include <format>

namespace AbsoluteControlPanelResearch::Interop::OSFUICompatibility
{
    bool IsRuntimePresent() noexcept
    {
        return ::GetModuleHandleW(kRuntimeModuleName) != nullptr;
    }

    bool IsPresentationActive(RE::UI* a_ui) noexcept
    {
        return a_ui &&
            a_ui->IsMenuOpen(RE::BSFixedString(kFocusMenuName));
    }

    void LogStartupState() noexcept
    {
        const bool present = IsRuntimePresent();
        EvidenceLog::Event(
            "osf_ui_coexistence",
            std::format(
                "runtime_present={} mode=parallel-host api_emulation=false "
                "focus_menu={}",
                present, kFocusMenuName));
    }
}
