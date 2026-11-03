#pragma once

#include <cstdint>

namespace AbsoluteControlPanelResearch::Runtime
{
    [[nodiscard]] bool ValidateMenuRelocations() noexcept;
    [[nodiscard]] std::uintptr_t ToImageRva(const void* a_address) noexcept;
}
