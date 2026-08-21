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
    // Registration remains explicitly bounded, but the render snapshot carries
    // controls for the active page only. This keeps ordinary UI work independent
    // of the total registered control count while admitting the documented
    // hundreds-of-subscribers test envelope.
    inline constexpr std::size_t kMaximumModules = 512;
    inline constexpr std::size_t kMaximumPages = 2048;
    inline constexpr std::size_t kMaximumPagesPerModule = 32;
    inline constexpr std::size_t kMaximumControlsPerPage = 128;
    inline constexpr std::size_t kMaximumControlsPerModule = 512;
    inline constexpr std::size_t kMaximumControls = 32768;

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

    struct ChoiceOption
    {
        std::int64_t value{};
        std::string label;
    };

    struct RecordItem
    {
        std::uint32_t flags{};
        std::string recordId;
        std::string label;
        std::string summary;
        std::string detail;
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
        std::uint64_t refreshRevision{};

        // Opaque shared ownership keeps copied page snapshots safe across
        // concurrent registry mutation. Callbacks are intentionally not exposed.
        std::shared_ptr<ProviderState> provider;
    };

    struct ModuleSummary
    {
        std::string moduleId;
        std::string displayName;
        std::string firstPageId;
    };

    struct CatalogSnapshot
    {
        // Captured under the same registry lock as pages, so the graph and its
        // revision always describe one linearized point in time.
        std::uint64_t revision{};
        std::vector<ModuleSummary> modules;
        std::vector<Page> pages;
    };

    enum class ModuleSortMode : std::uint32_t
    {
        Registration,
        Alphabetical
    };

    struct ModuleDiagnostics
    {
        std::string moduleId;
        std::string displayName;
        std::size_t pageCount{};
        std::size_t controlCount{};
    };

    struct RegistryDiagnostics
    {
        HostLifecycle lifecycle{ HostLifecycle::Initializing };
        std::uint64_t revision{};
        std::uint64_t refreshRevision{};
        bool menuOpen{};
        bool inputCaptureActive{};
        std::vector<ModuleDiagnostics> modules;
    };

    struct OpenRequest
    {
        std::string moduleId;
        std::string pageId;
        std::uint64_t serial{};
    };

    using OpenRequestWakeCallback = bool(*)() noexcept;

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
        friend AbsoluteControlPanelApi::Result AttachTransaction(
            const Page&, Transaction&) noexcept;
        friend AbsoluteControlPanelApi::Result ReassignBinding(
            const Page&, std::string_view, std::string_view,
            Transaction&) noexcept;
        std::shared_ptr<ProviderState> provider_;
    };

    [[nodiscard]] std::optional<Page> FindPage(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept;
    // Copies the module directory, page metadata for the selected module, and
    // controls/provider for the selected page only. If the requested route no
    // longer exists, the nearest module page or first registered page is used.
    [[nodiscard]] CatalogSnapshot SnapshotCatalog(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept;
    [[nodiscard]] RegistryDiagnostics Diagnostics() noexcept;
    void SetModuleSortMode(ModuleSortMode a_mode) noexcept;
    [[nodiscard]] ModuleSortMode GetModuleSortMode() noexcept;
    [[nodiscard]] std::uint64_t Revision() noexcept;

    // requestRefresh has its own cursor so the active menu can consume a wakeup
    // exactly once and republish a model. Poll this on the UI/game thread.
    [[nodiscard]] std::uint64_t RefreshRevision() noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result RequestRefresh(
        const char* a_moduleId, const char* a_pageId) noexcept;
    [[nodiscard]] bool ConsumeRefresh(std::uint64_t& a_cursor) noexcept;
    [[nodiscard]] bool ConsumeRefresh(std::uint64_t& a_cursor,
        std::string_view a_activeModuleId,
        std::string_view a_activePageId) noexcept;
    // Product API callbacks enqueue a validated route from any provider thread.
    // The injected wake callback may only schedule work; the bridge consumes and
    // dispatches the request on Starfield's UI/game thread.
    void SetOpenRequestWakeCallback(OpenRequestWakeCallback a_callback) noexcept;
    [[nodiscard]] bool ConsumeOpenRequest(OpenRequest& a_request) noexcept;
    void DiscardOpenRequest() noexcept;

    // Provider callbacks must only be invoked through these lease-acquiring
    // functions. They never run under the registry or provider-state mutex.
    // The active native menu owns UI-thread dispatch; providers must keep these
    // callbacks short and must not call Starfield UI functions from them.
    [[nodiscard]] AbsoluteControlPanelApi::Result ReadValue(const Page& a_page,
        std::string_view a_controlId, AbsoluteControlPanelApi::ValueV1& a_value) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result ReadChoiceOptions(
        const Page& a_page, std::string_view a_controlId,
        std::vector<ChoiceOption>& a_options) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result ReadRecordItems(
        const Page& a_page, std::string_view a_controlId,
        std::vector<RecordItem>& a_items) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result BeginBindingCapture(
        const Page& a_page, std::string_view a_controlId) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result PollBindingCapture(
        const Page& a_page, std::string_view a_controlId,
        AbsoluteControlPanelApi::BindingCaptureV1& a_capture) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result CancelBindingCapture(
        const Page& a_page, std::string_view a_controlId) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result ReassignBinding(
        const Page& a_page, std::string_view a_controlId,
        std::string_view a_binding, Transaction& a_transaction) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result WriteDraft(const Page& a_page,
        std::string_view a_controlId, const AbsoluteControlPanelApi::ValueV1& a_value,
        Transaction& a_transaction) noexcept;
    [[nodiscard]] AbsoluteControlPanelApi::Result WriteTransientChoice(
        const Page& a_page, std::string_view a_controlId,
        const AbsoluteControlPanelApi::ValueV1& a_value) noexcept;
    // Compound components mutate the same provider-owned page draft without a
    // scalar WriteDraft callback. Attach first so unregister remains blocked
    // until page Apply/Cancel or teardown releases the transaction.
    [[nodiscard]] AbsoluteControlPanelApi::Result AttachTransaction(
        const Page& a_page, Transaction& a_transaction) noexcept;
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
