#pragma once

#include <cstdint>
#include "ui/MenuOpenOrigin.h"

namespace AbsoluteControlPanelResearch::Ui::PauseMenuIntegration
{
    [[nodiscard]] bool InstallLifecycleHook(bool a_enablePauseEntry, bool a_enableMainMenuEntry) noexcept;
    void AttachExistingMainMenu() noexcept;
    void MarkHostReady() noexcept;
    void SetEntryEnabled(bool a_enabled) noexcept;
    [[nodiscard]] bool IsEntryEnabled() noexcept;
    void LogRegistration(bool a_enablePauseEntry) noexcept;
    void RequestInjection(std::uint32_t a_commandId) noexcept;
    [[nodiscard]] bool SetOpenOrigin(MenuOpenOrigin origin) noexcept;
    [[nodiscard]] MenuOpenOrigin ConsumeOpenOrigin() noexcept;
}
