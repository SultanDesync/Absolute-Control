#pragma once

#include "runtime/BackendRuntime.h"

#include <cstdint>
#include <memory>

// Test-hooks only: a fake controller on the input bus, so the runner can press buttons
// without hardware. It replaces the DirectInput provider when UseTestInput is set.
namespace AbsoluteControlPanelResearch::TestInput
{
    inline constexpr std::uint32_t kButtonCount = 32;
    inline constexpr std::uint32_t kAxisCount = 2;

    [[nodiscard]] std::unique_ptr<AbsoluteControl::Runtime::InputProvider> MakeProvider();
    // Button numbers start at 1, as they do in binding tokens.
    [[nodiscard]] bool SetButton(std::uint32_t button, bool down) noexcept;
    // Axis numbers start at 0, as they do in the channel names. Values run -1 to 1.
    [[nodiscard]] bool SetAxis(std::uint32_t axis, double value) noexcept;
}
