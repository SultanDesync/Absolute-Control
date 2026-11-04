#pragma once

#include "runtime/BackendRuntime.h"

#include <cstdint>
#include <memory>

// Test-hooks only: a fake controller on the input bus, so the runner can press buttons
// without hardware. It replaces the DirectInput provider when UseTestInput is set.
namespace AbsoluteControlPanelResearch::TestInput
{
    inline constexpr std::uint32_t kButtonCount = 32;

    [[nodiscard]] std::unique_ptr<AbsoluteControl::Runtime::InputProvider> MakeProvider();
    // Button numbers start at 1, as they do in binding tokens.
    [[nodiscard]] bool SetButton(std::uint32_t button, bool down) noexcept;
}
