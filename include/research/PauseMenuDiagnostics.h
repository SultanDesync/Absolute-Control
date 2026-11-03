#pragma once
#include <cstdint>
#include <string_view>
namespace RE::Scaleform::GFx { class Movie; }
namespace AbsoluteControlPanelResearch::Research::PauseMenuDiagnostics
{
    void Observe(RE::Scaleform::GFx::Movie*, std::uint32_t cycle,
        std::string_view phase) noexcept;
    void Advance(RE::Scaleform::GFx::Movie*, std::uint32_t cycle) noexcept;
}
