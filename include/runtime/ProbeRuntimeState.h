#pragma once

#include "ProbeConfig.h"
#include "ProbeState.h"

namespace AbsoluteControlPanelResearch::Runtime
{
    void Transition(ProbeEvent a_event) noexcept;
    [[nodiscard]] ProbePhase Phase() noexcept;

    void SetConfig(ProbeConfig a_config) noexcept;
    [[nodiscard]] const ProbeConfig& Config() noexcept;
}
