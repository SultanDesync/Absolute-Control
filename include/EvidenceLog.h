#pragma once

#include <filesystem>
#include <string_view>

namespace AbsoluteControlPanelResearch::EvidenceLog
{
    void Initialize(std::string_view a_runId) noexcept;
    void Event(std::string_view a_event, std::string_view a_detail = {}) noexcept;
    [[nodiscard]] std::filesystem::path Path() noexcept;
}
