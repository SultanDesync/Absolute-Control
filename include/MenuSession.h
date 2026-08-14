#pragma once

#include "MenuApiHost.h"
#include "SlopAPI.h"  // legacy spelling still used by the v1 input router/bridge

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
        BeginBindingCapture,
        Apply,
        Cancel,
        Close
    };

    struct Control
    {
        MenuApiHost::Control descriptor;
        AbsoluteControlPanelApi::ValueV1 value{};
        bool available{};
        std::string error;
    };

    struct Page
    {
        std::string moduleId;
        std::string moduleTitle;
        std::string pageId;
        std::string title;
        std::string description;
        std::vector<Control> controls;
    };

    struct Model
    {
        std::uint32_t schemaVersion{ kSchemaVersion };
        // Per-session publication generation. Commands that carry a non-zero
        // expectedGeneration are rejected unless they target the latest model.
        std::uint64_t generation{};
        std::uint64_t revision{};
        std::string activeModuleId;
        std::string activePageId;
        std::string selectedControlId;
        bool dirty{};
        bool bindingCaptureActive{};
        std::string captureModuleId;
        std::string capturePageId;
        std::string captureControlId;
        std::string error;
        std::vector<Page> pages;
    };

    struct Command
    {
        std::uint32_t schemaVersion{ kSchemaVersion };
        // Zero preserves the current bridge-v1 behavior. Bridge v2 should echo
        // Model::generation to enable stale-command rejection end to end.
        std::uint64_t expectedGeneration{};
        CommandKind kind{};
        std::string moduleId;
        std::string pageId;
        std::string controlId;
        AbsoluteControlPanelApi::ValueV1 value{};
    };

    class Session
    {
    public:
        ~Session() noexcept;

        // A Session is owned and dispatched exclusively by the native UI/game
        // thread. It is intentionally not internally synchronized.
        [[nodiscard]] Model Snapshot();
        [[nodiscard]] Model Dispatch(const Command& a_command);
        [[nodiscard]] Model CompleteBindingCapture(std::string_view a_binding);
        [[nodiscard]] Model CancelBindingCapture(std::string_view a_reason = {});
        [[nodiscard]] bool IsBindingCaptureActive() const noexcept;
        [[nodiscard]] std::uint32_t BindingCaptureFlags() const noexcept;

    private:
        std::string activeModuleId_;
        std::string activePageId_;
        std::string selectedControlId_;
        std::string dirtyModuleId_;
        std::string dirtyPageId_;
        std::string captureModuleId_;
        std::string capturePageId_;
        std::string captureControlId_;
        std::uint32_t captureFlags_{};
        std::uint64_t generation_{};
        std::string error_;
        MenuApiHost::Transaction transaction_;

        [[nodiscard]] Model BuildSnapshot();
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] bool IsDirtyOtherPage(const Command& a_command) const noexcept;
        [[nodiscard]] bool RollbackDirtyPage() noexcept;
        void AbandonState() noexcept;
        void SetError(std::string_view a_error);
    };
}
