#pragma once

#include "AbsoluteControlPanelAPI.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControlPanelResearch::MenuApiHost
{
    // These are both registry and render-model limits. Registration rejects a
    // descriptor graph that the v1 menu cannot render in full.
    inline constexpr std::size_t kMaximumModules = 32;
    inline constexpr std::size_t kMaximumPages = 32;
    inline constexpr std::size_t kMaximumControlsPerPage = 128;
    inline constexpr std::size_t kMaximumControls = 512;

    enum class HostLifecycle : std::uint32_t
    {
        Initializing,
        Ready,
        Rejected
    };

    struct ProviderState;

    struct Control
    {
        AbsoluteControlPanelApi::ControlKind kind{};
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
        bool canInvokeAction{};
        bool canApply{};
        bool canCancel{};

        // Opaque shared ownership keeps copied page snapshots safe across
        // concurrent registry mutation. Callbacks are intentionally not exposed.
        std::shared_ptr<ProviderState> provider;
    };

    // A successful draft write attaches the UI session to its provider. The
    // token prevents unregisterModule from succeeding until Apply, Cancel, or
    // session destruction releases it.
    class Transaction final
    {
    public:
        Transaction() = default;
        ~Transaction();
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&&) noexcept;
        Transaction& operator=(Transaction&&) noexcept;

        [[nodiscard]] explicit operator bool() const noexcept;
        void Reset() noexcept;

    private:
        friend AbsoluteControlPanelApi::Result WriteDraft(
            const Page&, std::string_view, const AbsoluteControlPanelApi::ValueV1&,
            Transaction&) noexcept;
        std::shared_ptr<ProviderState> provider_;
    };

    [[nodiscard]] std::optional<Page> FindPage(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept;
    [[nodiscard]] std::vector<Page> Pages() noexcept;
    [[nodiscard]] std::uint64_t Revision() noexcept;

    // requestRefresh has its own cursor so the active menu can consume a wakeup
    // exactly once and republish a model. Poll this on the UI/game thread.
    [[nodiscard]] std::uint64_t RefreshRevision() noexcept;
    [[nodiscard]] bool ConsumeRefresh(std::uint64_t& a_cursor) noexcept;

    // Provider callbacks must only be invoked through these lease-acquiring
    // functions. They never run under the registry or provider-state mutex.
    // The active native menu owns UI-thread dispatch; providers must keep these
    // callbacks short and must not call Starfield UI functions from them.
    [[nodiscard]] AbsoluteControlPanelApi::Result ReadValue(const Page& a_page,
        std::string_view a_controlId, AbsoluteControlPanelApi::ValueV1& a_value) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result WriteDraft(const Page& a_page,
        std::string_view a_controlId, const AbsoluteControlPanelApi::ValueV1& a_value,
        Transaction& a_transaction) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result InvokeAction(const Page& a_page,
        std::string_view a_controlId) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result Apply(const Page& a_page) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result Cancel(const Page& a_page) noexcept;

    // Discovery remains available while Initializing. Providers receive
    // NotReady until MarkRuntimeReady. Rejected is terminal for this process.
    void MarkRuntimeReady() noexcept;
    void MarkRuntimeRejected() noexcept;
    [[nodiscard]] HostLifecycle Lifecycle() noexcept;

    void SetMenuOpen(bool a_open) noexcept;
    void SetInputCaptureActive(bool a_active) noexcept;
    [[nodiscard]] bool IsMenuOpen() noexcept;
    [[nodiscard]] bool IsInputCaptureActive() noexcept;
}
