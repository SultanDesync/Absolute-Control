#pragma once

#include <cstdint>

namespace AbsoluteControlPanelResearch::Ui::PauseMenuIntegration
{
    [[nodiscard]] bool InstallLifecycleHook(bool a_enablePauseEntry) noexcept;
    void LogRegistration(bool a_enablePauseEntry) noexcept;
    void RequestInjection(std::uint32_t a_commandId) noexcept;
}
