#pragma once

#include "MenuApiHost.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AbsoluteControlPanelResearch::MenuSession
{
    inline constexpr std::uint32_t kSchemaVersion = 1;

    enum class CommandKind : std::uint32_t
    {
        SelectPage,
        SelectControl,
        Write,
        Invoke,
        Apply,
        Cancel,
        Close
    };

    struct Control
    {
        MenuApiHost::Control descriptor;
        SlopApi::ValueV1 value{};
        bool available{};
        std::string error;
    };

    struct Page
    {
        std::string moduleId;
        std::string pageId;
        std::string title;
        std::string description;
        std::vector<Control> controls;
    };

    struct Model
    {
        std::uint32_t schemaVersion{ kSchemaVersion };
        std::uint64_t revision{};
        std::string activeModuleId;
        std::string activePageId;
        std::string selectedControlId;
        bool dirty{};
        std::string error;
        std::vector<Page> pages;
    };

    struct Command
    {
        std::uint32_t schemaVersion{ kSchemaVersion };
        CommandKind kind{};
        std::string moduleId;
        std::string pageId;
        std::string controlId;
        SlopApi::ValueV1 value{};
    };

    class Session
    {
    public:
        [[nodiscard]] Model Snapshot();
        [[nodiscard]] Model Dispatch(const Command& a_command);

    private:
        std::string activeModuleId_;
        std::string activePageId_;
        std::string selectedControlId_;
        std::string dirtyModuleId_;
        std::string dirtyPageId_;
        std::string error_;

        [[nodiscard]] Model BuildSnapshot();
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] bool IsDirtyOtherPage(const Command& a_command) const noexcept;
        void SetError(std::string_view a_error);
    };
}
