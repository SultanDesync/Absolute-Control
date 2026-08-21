#pragma once

#include <cstdint>

namespace AbsoluteControlPanelResearch::Ui::PauseMenuIntegration
{
    [[nodiscard]] bool InstallLifecycleHook(bool a_enablePauseEntry) noexcept;
    void SetEntryEnabled(bool a_enabled) noexcept;
    [[nodiscard]] bool IsEntryEnabled() noexcept;
    void LogRegistration(bool a_enablePauseEntry) noexcept;
    void RequestInjection(std::uint32_t a_commandId) noexcept;
    [[nodiscard]] bool SetReturnToPauseOnClose(bool a_enabled) noexcept;
    [[nodiscard]] bool ConsumeReturnToPauseOnClose() noexcept;
}
