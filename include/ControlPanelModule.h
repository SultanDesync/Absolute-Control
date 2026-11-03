#pragma once

#include "ProbeConfig.h"

#include <cstdint>
#include <filesystem>

namespace AbsoluteControlPanelResearch::ControlPanelModule
{
    struct RuntimeCallbacks
    {
        void (*setPauseMenuEntry)(bool) noexcept{};
        void (*setRecoveryHotkey)(std::uint32_t) noexcept{};
    };

    // Registers the release-safe host-management module through the same
    // provider ABI used by daughter modules. The module owns only host
    // presentation/entry preferences and read-only host registry diagnostics.
    [[nodiscard]] bool Register(const ProbeConfig& a_config,
        const std::filesystem::path& a_customConfigPath,
        RuntimeCallbacks a_callbacks) noexcept;
}
