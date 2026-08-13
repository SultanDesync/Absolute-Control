#pragma once

#include "SlopAPI.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControlPanelResearch::MenuApiHost
{
    struct Control
    {
        SlopApi::ControlKind kind{};
        std::uint32_t flags{};
        std::string controlId;
        std::string label;
        std::string description;
        double minimumValue{};
        double maximumValue{};
        double stepValue{};
    };

    struct Page
    {
        std::string moduleId;
        std::string moduleDisplayName;
        std::string pageId;
        std::string displayName;
        std::string description;
        std::vector<Control> controls;
        void* context{};
        SlopApi::ReadValueCallback readValue{};
        SlopApi::WriteDraftCallback writeDraft{};
        SlopApi::InvokeActionCallback invokeAction{};
        SlopApi::ApplyCallback apply{};
        SlopApi::CancelCallback cancel{};
    };

    [[nodiscard]] std::optional<Page> FindPage(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept;
    // Returns a copied registry snapshot. Provider callbacks must only be invoked
    // after this function returns, never while the registry mutex is held.
    [[nodiscard]] std::vector<Page> Pages() noexcept;
    [[nodiscard]] std::uint64_t Revision() noexcept;
    void SetMenuOpen(bool a_open) noexcept;
    void SetInputCaptureActive(bool a_active) noexcept;
    [[nodiscard]] bool IsMenuOpen() noexcept;
    [[nodiscard]] bool IsInputCaptureActive() noexcept;
}
