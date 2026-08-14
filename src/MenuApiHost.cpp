#include "MenuApiHost.h"

#include "SlopAPI.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>
#include <ranges>
#include <utility>

namespace AbsoluteControlPanelResearch::MenuApiHost
{
    using Api = AbsoluteControlPanelApi::Result;

    struct ProviderState
    {
        std::mutex mutex;
        std::size_t inFlight{};
        std::size_t transactions{};
        bool retired{};
        void* context{};
        AbsoluteControlPanelApi::ReadValueCallback readValue{};
        AbsoluteControlPanelApi::WriteDraftCallback writeDraft{};
        AbsoluteControlPanelApi::InvokeActionCallback invokeAction{};
        AbsoluteControlPanelApi::ApplyCallback apply{};
        AbsoluteControlPanelApi::CancelCallback cancel{};
    };

    namespace
    {
        constexpr double kMaximumExactScaleformInteger = 9007199254740991.0;

        struct Module
        {
            std::string moduleId;
            std::string displayName;
            std::string description;
        };

        std::mutex g_registryMutex;
        std::vector<Module> g_modules;
        std::vector<Page> g_pages;
        std::atomic<std::uint64_t> g_revision{ 0 };
        std::atomic<std::uint64_t> g_refreshRevision{ 0 };
        std::atomic<HostLifecycle> g_lifecycle{ HostLifecycle::Initializing };
        std::atomic_bool g_menuOpen{ false };
        std::atomic_bool g_inputCaptureActive{ false };

        class CallbackLease final
        {
        public:
            explicit CallbackLease(std::shared_ptr<ProviderState> a_provider) noexcept :
                provider_(std::move(a_provider))
            {
                if (!provider_) return;
                std::scoped_lock lock{ provider_->mutex };
                if (provider_->retired) {
                    provider_.reset();
                    return;
                }
                ++provider_->inFlight;
            }

            ~CallbackLease()
            {
                if (!provider_) return;
                std::scoped_lock lock{ provider_->mutex };
                --provider_->inFlight;
            }

            CallbackLease(const CallbackLease&) = delete;
            CallbackLease& operator=(const CallbackLease&) = delete;

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return provider_ != nullptr;
            }

        private:
            std::shared_ptr<ProviderState> provider_;
        };

        template <std::size_t N>
        [[nodiscard]] bool IsTerminated(const char (&a_value)[N]) noexcept
        {
            return std::memchr(a_value, '\0', N) != nullptr;
        }

        [[nodiscard]] bool IsIdentifier(std::string_view a_value) noexcept
        {
            if (a_value.empty()) return false;
            return std::ranges::all_of(a_value, [](unsigned char a_character) {
                return std::isalnum(a_character) != 0 || a_character == '.' ||
                       a_character == '_' || a_character == '-';
            });
        }

        [[nodiscard]] bool ValidControlKind(
            AbsoluteControlPanelApi::ControlKind a_kind) noexcept
        {
            return a_kind >= AbsoluteControlPanelApi::ControlKind::Toggle &&
                   a_kind <= AbsoluteControlPanelApi::ControlKind::InputBinding;
        }

        [[nodiscard]] bool ValidControlDescriptor(
            const AbsoluteControlPanelApi::ControlDescriptorV1& a_control) noexcept
        {
            using Kind = AbsoluteControlPanelApi::ControlKind;
            if (a_control.structSize < sizeof(a_control) ||
                !IsTerminated(a_control.controlId) ||
                !IsIdentifier(a_control.controlId) ||
                !IsTerminated(a_control.label) || a_control.label[0] == '\0' ||
                !IsTerminated(a_control.description) ||
                !ValidControlKind(a_control.kind)) {
                return false;
            }
            if (a_control.kind == Kind::Toggle || a_control.kind == Kind::Action ||
                a_control.kind == Kind::InputBinding) {
                return true;
            }
            if (!std::isfinite(a_control.minimumValue) ||
                !std::isfinite(a_control.maximumValue) ||
                !std::isfinite(a_control.stepValue) ||
                a_control.minimumValue > a_control.maximumValue ||
                a_control.stepValue <= 0.0) {
                return false;
            }
            return (a_control.kind != Kind::IntegerSlider &&
                       a_control.kind != Kind::Choice) ||
                   (a_control.minimumValue >= -kMaximumExactScaleformInteger &&
                       a_control.maximumValue <= kMaximumExactScaleformInteger);
        }

        [[nodiscard]] Api MutationAvailability() noexcept
        {
            switch (g_lifecycle.load(std::memory_order_acquire)) {
            case HostLifecycle::Initializing: return Api::NotReady;
            case HostLifecycle::Ready: return Api::Ok;
            case HostLifecycle::Rejected: return Api::Rejected;
            }
            return Api::Rejected;
        }

        [[nodiscard]] std::size_t RegisteredControlCount() noexcept
        {
            std::size_t result{};
            for (const auto& page : g_pages) result += page.controls.size();
            return result;
        }

        [[nodiscard]] bool KnowsModule(std::string_view a_moduleId) noexcept
        {
            return std::ranges::any_of(g_modules, [&](const Module& a_module) {
                       return a_module.moduleId == a_moduleId;
                   }) ||
                   std::ranges::any_of(g_pages, [&](const Page& a_page) {
                       return a_page.moduleId == a_moduleId;
                   });
        }

        [[nodiscard]] std::size_t KnownModuleCount() noexcept
        {
            std::vector<std::string_view> identifiers;
            identifiers.reserve(g_modules.size() + g_pages.size());
            for (const auto& module : g_modules) identifiers.push_back(module.moduleId);
            for (const auto& page : g_pages) {
                if (std::ranges::find(identifiers, page.moduleId) == identifiers.end()) {
                    identifiers.push_back(page.moduleId);
                }
            }
            return identifiers.size();
        }

        [[nodiscard]] Api __cdecl RegisterProductPage(
            const AbsoluteControlPanelApi::PageDescriptorV1* a_descriptor) noexcept
        {
            if (const auto availability = MutationAvailability(); availability != Api::Ok) {
                return availability;
            }
            try {
                using Kind = AbsoluteControlPanelApi::ControlKind;
                if (!a_descriptor || a_descriptor->structSize < sizeof(*a_descriptor) ||
                    !IsTerminated(a_descriptor->moduleId) ||
                    !IsTerminated(a_descriptor->pageId) ||
                    !IsTerminated(a_descriptor->displayName) ||
                    !IsTerminated(a_descriptor->description) ||
                    !IsIdentifier(a_descriptor->moduleId) ||
                    !IsIdentifier(a_descriptor->pageId) ||
                    a_descriptor->displayName[0] == '\0' ||
                    (a_descriptor->controlCount != 0 && !a_descriptor->controls)) {
                    return Api::InvalidArgument;
                }
                if (a_descriptor->controlCount > kMaximumControlsPerPage) {
                    return Api::CapacityExceeded;
                }

                Page page;
                page.moduleId = a_descriptor->moduleId;
                page.moduleDisplayName = page.moduleId;
                page.pageId = a_descriptor->pageId;
                page.displayName = a_descriptor->displayName;
                page.description = a_descriptor->description;
                page.controls.reserve(a_descriptor->controlCount);

                bool needsRead{};
                bool editable{};
                for (std::uint32_t index = 0; index < a_descriptor->controlCount; ++index) {
                    const auto& source = a_descriptor->controls[index];
                    if (!ValidControlDescriptor(source) ||
                        std::ranges::any_of(page.controls, [&](const Control& a_control) {
                            return a_control.controlId == source.controlId;
                        })) {
                        return Api::InvalidArgument;
                    }
                    needsRead = needsRead || source.kind != Kind::Action;
                    editable = editable || (source.kind != Kind::Action &&
                        (source.flags & AbsoluteControlPanelApi::kControlReadOnly) == 0);
                    page.controls.push_back(Control{ source.kind, source.flags,
                        source.controlId, source.label, source.description,
                        source.minimumValue, source.maximumValue, source.stepValue });
                }
                if ((needsRead && !a_descriptor->readValue) ||
                    (editable && (!a_descriptor->writeDraft || !a_descriptor->apply ||
                                     !a_descriptor->cancel))) {
                    return Api::InvalidArgument;
                }

                page.provider = std::make_shared<ProviderState>();
                page.provider->context = a_descriptor->context;
                page.provider->readValue = a_descriptor->readValue;
                page.provider->writeDraft = a_descriptor->writeDraft;
                page.provider->invokeAction = a_descriptor->invokeAction;
                page.provider->apply = a_descriptor->apply;
                page.provider->cancel = a_descriptor->cancel;
                page.canInvokeAction = a_descriptor->invokeAction != nullptr;
                page.canApply = a_descriptor->apply != nullptr;
                page.canCancel = a_descriptor->cancel != nullptr;

                std::scoped_lock lock{ g_registryMutex };
                if (g_pages.size() >= kMaximumPages ||
                    RegisteredControlCount() + page.controls.size() > kMaximumControls ||
                    (!KnowsModule(page.moduleId) &&
                        KnownModuleCount() >= kMaximumModules)) {
                    return Api::CapacityExceeded;
                }
                if (std::ranges::any_of(g_pages, [&](const Page& a_page) {
                        return a_page.moduleId == page.moduleId &&
                               a_page.pageId == page.pageId;
                    })) {
                    return Api::Duplicate;
                }
                const auto module = std::ranges::find_if(g_modules,
                    [&](const Module& a_module) { return a_module.moduleId == page.moduleId; });
                if (module != g_modules.end()) page.moduleDisplayName = module->displayName;
                g_pages.push_back(std::move(page));
                g_revision.fetch_add(1, std::memory_order_release);
                return Api::Ok;
            } catch (...) {
                return Api::Rejected;
            }
        }

        [[nodiscard]] Api __cdecl RegisterProductModule(
            const AbsoluteControlPanelApi::ModuleDescriptorV1* a_descriptor) noexcept
        {
            if (const auto availability = MutationAvailability(); availability != Api::Ok) {
                return availability;
            }
            try {
                if (!a_descriptor || a_descriptor->structSize < sizeof(*a_descriptor) ||
                    !IsTerminated(a_descriptor->moduleId) ||
                    !IsTerminated(a_descriptor->displayName) ||
                    !IsTerminated(a_descriptor->description) ||
                    !IsIdentifier(a_descriptor->moduleId) ||
                    a_descriptor->displayName[0] == '\0') {
                    return Api::InvalidArgument;
                }
                Module module{ a_descriptor->moduleId, a_descriptor->displayName,
                    a_descriptor->description };
                std::scoped_lock lock{ g_registryMutex };
                if (std::ranges::any_of(g_modules, [&](const Module& a_module) {
                        return a_module.moduleId == module.moduleId;
                    })) {
                    return Api::Duplicate;
                }
                if (!KnowsModule(module.moduleId) &&
                    KnownModuleCount() >= kMaximumModules) {
                    return Api::CapacityExceeded;
                }
                for (auto& page : g_pages) {
                    if (page.moduleId == module.moduleId) {
                        page.moduleDisplayName = module.displayName;
                    }
                }
                g_modules.push_back(std::move(module));
                g_revision.fetch_add(1, std::memory_order_release);
                return Api::Ok;
            } catch (...) {
                return Api::Rejected;
            }
        }

        [[nodiscard]] Api __cdecl UnregisterProductModule(const char* a_moduleId) noexcept
        {
            try {
                if (!a_moduleId) return Api::InvalidArgument;
                const std::string_view moduleId{ a_moduleId };
                if (!IsIdentifier(moduleId)) return Api::InvalidArgument;

                std::scoped_lock registryLock{ g_registryMutex };
                std::vector<std::shared_ptr<ProviderState>> providers;
                for (const auto& page : g_pages) {
                    if (page.moduleId == moduleId) providers.push_back(page.provider);
                }
                const bool hasModule = std::ranges::any_of(g_modules,
                    [&](const Module& a_module) { return a_module.moduleId == moduleId; });
                if (providers.empty() && !hasModule) return Api::NotFound;

                // Deterministic non-blocking policy: a provider receives Rejected
                // while any callback or dirty transaction is active and must retry.
                std::vector<std::unique_lock<std::mutex>> providerLocks;
                providerLocks.reserve(providers.size());
                for (const auto& provider : providers) {
                    providerLocks.emplace_back(provider->mutex);
                    if (provider->inFlight != 0 || provider->transactions != 0) {
                        return Api::Rejected;
                    }
                }
                for (const auto& provider : providers) provider->retired = true;
                std::erase_if(g_pages,
                    [&](const Page& a_page) { return a_page.moduleId == moduleId; });
                std::erase_if(g_modules,
                    [&](const Module& a_module) { return a_module.moduleId == moduleId; });
                g_revision.fetch_add(1, std::memory_order_release);
                return Api::Ok;
            } catch (...) {
                return Api::Rejected;
            }
        }

        [[nodiscard]] Api __cdecl RequestProductRefresh(
            const char* a_moduleId, const char* a_pageId) noexcept
        {
            if (const auto availability = MutationAvailability(); availability != Api::Ok) {
                return availability;
            }
            try {
                if (!a_moduleId || !a_pageId) return Api::InvalidArgument;
                const std::string_view moduleId{ a_moduleId };
                const std::string_view pageId{ a_pageId };
                if (!IsIdentifier(moduleId) || !IsIdentifier(pageId)) {
                    return Api::InvalidArgument;
                }
                std::scoped_lock lock{ g_registryMutex };
                if (!std::ranges::any_of(g_pages, [&](const Page& a_page) {
                        return a_page.moduleId == moduleId && a_page.pageId == pageId;
                    })) {
                    return Api::NotFound;
                }
                g_refreshRevision.fetch_add(1, std::memory_order_release);
                g_revision.fetch_add(1, std::memory_order_release);
                return Api::Ok;
            } catch (...) {
                return Api::Rejected;
            }
        }

        // Explicit legacy adapter edge. Descriptor and callback types are aliases
        // to the product authority, so no reinterpretation occurs here.
        [[nodiscard]] SlopApi::Result __cdecl RegisterLegacyPage(
            const SlopApi::PageDescriptorV1* a_descriptor) noexcept
        {
            return RegisterProductPage(a_descriptor);
        }

        [[nodiscard]] SlopApi::Result __cdecl RegisterLegacyModule(
            const SlopApi::ModuleDescriptorV1* a_descriptor) noexcept
        {
            return RegisterProductModule(a_descriptor);
        }

        [[nodiscard]] SlopApi::Result __cdecl UnregisterLegacyModule(
            const char* a_moduleId) noexcept
        {
            return UnregisterProductModule(a_moduleId);
        }

        [[nodiscard]] SlopApi::Result __cdecl RequestLegacyRefresh(
            const char* a_moduleId, const char* a_pageId) noexcept
        {
            return RequestProductRefresh(a_moduleId, a_pageId);
        }

        [[nodiscard]] std::uint8_t __cdecl ProductIsOpen() noexcept
        {
            return g_menuOpen.load(std::memory_order_acquire) ? 1U : 0U;
        }

        [[nodiscard]] std::uint8_t __cdecl ProductIsInputCaptureActive() noexcept
        {
            return g_inputCaptureActive.load(std::memory_order_acquire) ? 1U : 0U;
        }
    }

    Transaction::~Transaction() { Reset(); }

    Transaction::Transaction(Transaction&& a_other) noexcept :
        provider_(std::exchange(a_other.provider_, {}))
    {}

    Transaction& Transaction::operator=(Transaction&& a_other) noexcept
    {
        if (this != &a_other) {
            Reset();
            provider_ = std::exchange(a_other.provider_, {});
        }
        return *this;
    }

    Transaction::operator bool() const noexcept { return provider_ != nullptr; }

    void Transaction::Reset() noexcept
    {
        if (!provider_) return;
        {
            std::scoped_lock lock{ provider_->mutex };
            if (provider_->transactions != 0) --provider_->transactions;
        }
        provider_.reset();
    }

    std::optional<Page> FindPage(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept
    {
        try {
            std::scoped_lock lock{ g_registryMutex };
            const auto found = std::ranges::find_if(g_pages, [&](const Page& a_page) {
                return a_page.moduleId == a_moduleId && a_page.pageId == a_pageId;
            });
            return found == g_pages.end() ? std::nullopt : std::optional<Page>{ *found };
        } catch (...) {
            return std::nullopt;
        }
    }

    std::vector<Page> Pages() noexcept
    {
        try {
            std::scoped_lock lock{ g_registryMutex };
            return g_pages;
        } catch (...) {
            return {};
        }
    }

    std::uint64_t Revision() noexcept
    {
        return g_revision.load(std::memory_order_acquire);
    }

    std::uint64_t RefreshRevision() noexcept
    {
        return g_refreshRevision.load(std::memory_order_acquire);
    }

    bool ConsumeRefresh(std::uint64_t& a_cursor) noexcept
    {
        const auto current = RefreshRevision();
        if (current <= a_cursor) return false;
        a_cursor = current;
        return true;
    }

    Api ReadValue(const Page& a_page, std::string_view a_controlId,
        AbsoluteControlPanelApi::ValueV1& a_value) noexcept
    {
        try {
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->readValue) return Api::Rejected;
            const std::string controlId{ a_controlId };
            return a_page.provider->readValue(
                a_page.provider->context, controlId.c_str(), &a_value);
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api WriteDraft(const Page& a_page, std::string_view a_controlId,
        const AbsoluteControlPanelApi::ValueV1& a_value,
        Transaction& a_transaction) noexcept
    {
        try {
            if (a_transaction.provider_ && a_transaction.provider_ != a_page.provider) {
                return Api::Rejected;
            }
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->writeDraft) return Api::Rejected;
            const std::string controlId{ a_controlId };
            const auto result = a_page.provider->writeDraft(
                a_page.provider->context, controlId.c_str(), &a_value);
            if (result == Api::Ok && !a_transaction.provider_) {
                std::scoped_lock lock{ a_page.provider->mutex };
                if (a_page.provider->retired) return Api::Rejected;
                ++a_page.provider->transactions;
                a_transaction.provider_ = a_page.provider;
            }
            return result;
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api InvokeAction(const Page& a_page, std::string_view a_controlId) noexcept
    {
        try {
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->invokeAction) return Api::Rejected;
            const std::string controlId{ a_controlId };
            return a_page.provider->invokeAction(
                a_page.provider->context, controlId.c_str());
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api Apply(const Page& a_page) noexcept
    {
        CallbackLease lease{ a_page.provider };
        if (!lease || !a_page.provider->apply) return Api::Rejected;
        return a_page.provider->apply(a_page.provider->context);
    }

    Api Cancel(const Page& a_page) noexcept
    {
        CallbackLease lease{ a_page.provider };
        if (!lease || !a_page.provider->cancel) return Api::Rejected;
        a_page.provider->cancel(a_page.provider->context);
        return Api::Ok;
    }

    void MarkRuntimeReady() noexcept
    {
        auto expected = HostLifecycle::Initializing;
        (void)g_lifecycle.compare_exchange_strong(expected, HostLifecycle::Ready,
            std::memory_order_acq_rel);
    }

    void MarkRuntimeRejected() noexcept
    {
        g_lifecycle.store(HostLifecycle::Rejected, std::memory_order_release);
    }

    HostLifecycle Lifecycle() noexcept
    {
        return g_lifecycle.load(std::memory_order_acquire);
    }

    const AbsoluteControlPanelApi::ApiV1 g_publicApi{
        sizeof(AbsoluteControlPanelApi::ApiV1),
        AbsoluteControlPanelApi::kAbiVersion,
        AbsoluteControlPanelApi::kModuleId.data(),
        "Absolute Control Panel",
        ACP_PRODUCT_VERSION,
        &RegisterProductPage,
        &UnregisterProductModule,
        &RequestProductRefresh,
        &RegisterProductModule,
        &ProductIsOpen,
        &ProductIsInputCaptureActive
    };

    const SlopApi::ApiV1 g_legacyApi{
        sizeof(SlopApi::ApiV1),
        SlopApi::kAbiVersion,
        "SLOP",
        "Absolute Control Panel",
        ACP_PRODUCT_VERSION,
        &RegisterLegacyPage,
        &UnregisterLegacyModule,
        &RequestLegacyRefresh,
        &RegisterLegacyModule
    };

    void SetMenuOpen(bool a_open) noexcept
    {
        g_menuOpen.store(a_open, std::memory_order_release);
        if (!a_open) g_inputCaptureActive.store(false, std::memory_order_release);
    }

    void SetInputCaptureActive(bool a_active) noexcept
    {
        g_inputCaptureActive.store(a_active, std::memory_order_release);
    }

    bool IsMenuOpen() noexcept
    {
        return g_menuOpen.load(std::memory_order_acquire);
    }

    bool IsInputCaptureActive() noexcept
    {
        return g_inputCaptureActive.load(std::memory_order_acquire);
    }
}

extern "C" ABSOLUTE_CONTROL_PANEL_API const AbsoluteControlPanelApi::ApiV1*
AbsoluteControlPanel_QueryApi(std::uint32_t a_requestedAbiVersion) noexcept
{
    if (a_requestedAbiVersion != AbsoluteControlPanelApi::kAbiVersion) return nullptr;
    return &AbsoluteControlPanelResearch::MenuApiHost::g_publicApi;
}

extern "C" SLOP_API const SlopApi::ApiV1*
SLOP_QueryApi(std::uint32_t a_requestedAbiVersion) noexcept
{
    if (a_requestedAbiVersion != SlopApi::kAbiVersion) return nullptr;
    return &AbsoluteControlPanelResearch::MenuApiHost::g_legacyApi;
}
