#pragma once
#include <cstdint>
namespace RE::Scaleform::GFx { class Movie; }
namespace AbsoluteControlPanelResearch::Ui::PauseMenuPresentation
{
    void Configure(bool autoFit, bool purpleEntry, bool matchFont) noexcept;
    void Advance(RE::Scaleform::GFx::Movie*, std::uint32_t cycle) noexcept;
}
