#pragma once

#include "MenuApiHost.h"
#include "LiveComponentsExperimentalAPI.h"
#include "SlopAPI.h"  // legacy spelling still used by the v1 input router/bridge

#include <cstddef>
#include <cstdint>
#include <optional>
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
        BeginTextCapture,
        Apply,
        Cancel,
        Compound,
        Close,
        ResolveDirtyApply,
        ResolveDirtyDiscard,
        ResolveDirtyStay
    };

    struct Control
    {
        MenuApiHost::Control descriptor;
        AbsoluteControlPanelApi::ValueV1 value{};
        std::vector<MenuApiHost::ChoiceOption> choiceOptions;
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
        struct LiveComponent
        {
            AbsoluteControlPanelExperimental::LiveChannelModelV1 descriptor{};
            AbsoluteControlPanelExperimental::LiveFrameV1 frame{};
            bool available{};
            std::string error;
        };
        std::vector<LiveComponent> liveComponents;
    };

    struct Module
    {
        std::string moduleId;
        std::string title;
        std::string firstPageId;
    };

    struct Model
    {
        std::uint32_t schemaVersion{ kSchemaVersion };
        // Per-session publication generation. Commands that carry a non-zero
        // expectedGeneration are rejected unless they target the model most
        // recently acknowledged as visible by the presentation bridge.
        std::uint64_t generation{};
        std::uint64_t revision{};
        std::string activeModuleId;
        std::string activePageId;
        std::string selectedControlId;
        bool dirty{};
        bool dirtyDecisionActive{};
        bool dirtyDecisionClosesMenu{};
        bool closeRequested{};
        bool bindingCaptureActive{};
        bool textCaptureActive{};
        std::string captureModuleId;
        std::string capturePageId;
        std::string captureControlId;
        std::string error;
        std::vector<Module> modules;
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
        AbsoluteControlPanelExperimental::CompoundOperationKind compoundKind{
            AbsoluteControlPanelExperimental::CompoundOperationKind::SetSegmentCount };
        std::string channelId;
        std::string columnId;
        std::string tierId;
        std::uint32_t count{};
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
        [[nodiscard]] Model AppendTextCapture(char a_character);
        [[nodiscard]] Model BackspaceTextCapture();
        [[nodiscard]] Model CompleteTextCapture();
        [[nodiscard]] Model CancelTextCapture(std::string_view a_reason = {});
        // Polls only live channels on the active route. A model is returned
        // only when at least one provider sequence advances.
        [[nodiscard]] std::optional<Model> RefreshLive();
        [[nodiscard]] bool IsBindingCaptureActive() const noexcept;
        [[nodiscard]] bool IsTextCaptureActive() const noexcept;
        [[nodiscard]] bool IsCaptureActive() const noexcept;
        [[nodiscard]] std::uint32_t BindingCaptureFlags() const noexcept;
        // Idempotent abnormal-close path. It performs the best-effort dirty
        // rollback while the provider lease is still pinned, then clears all
        // capture and transaction ownership without constructing a new model.
        void Teardown() noexcept;
        [[nodiscard]] std::string_view ActiveModuleId() const noexcept;
        [[nodiscard]] std::string_view ActivePageId() const noexcept;

        // Generation validity follows what the user can actually see, not a
        // newer model merely waiting at the frame-boundary publication queue.
        void AcknowledgePublishedGeneration(std::uint64_t a_generation) noexcept;

    private:
        std::string activeModuleId_;
        std::string activePageId_;
        std::string selectedControlId_;
        std::string dirtyModuleId_;
        std::string dirtyPageId_;
        enum class DirtyDecisionKind : std::uint8_t { None, Navigate, Close };
        DirtyDecisionKind dirtyDecisionKind_{DirtyDecisionKind::None};
        std::string decisionModuleId_;
        std::string decisionPageId_;
        bool closeRequested_{};
        std::string captureModuleId_;
        std::string capturePageId_;
        std::string captureControlId_;
        std::uint32_t captureFlags_{};
        enum class CaptureKind : std::uint8_t { None, Binding, Text };
        CaptureKind captureKind_{CaptureKind::None};
        std::string captureBuffer_;
        std::size_t captureMaximum_{};
        std::uint64_t generation_{};
        std::uint64_t publishedGeneration_{};
        std::string error_;
        MenuApiHost::Transaction transaction_;

        [[nodiscard]] Model BuildSnapshot();
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] bool IsDirtyOtherPage(const Command& a_command) const noexcept;
        [[nodiscard]] bool RollbackDirtyPage() noexcept;
        [[nodiscard]] Model ResolveDirtyDecision(CommandKind a_kind);
        void BeginDirtyDecision(const Command& a_command);
        void ClearDirtyDecision() noexcept;
        void CompletePendingRoute();
        void ClearCapture() noexcept;
        void AbandonState() noexcept;
        void SetError(std::string_view a_error);
    };
}
