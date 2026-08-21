#include "MenuApiHost.h"

#include "CompositionRegistry.h"
#include "SlopAPI.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>
#include <ranges>
#include <unordered_set>
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
        AbsoluteControlPanelApi::ReadChoiceOptionsCallback readChoiceOptions{};
        AbsoluteControlPanelApi::BeginBindingCaptureCallback beginBindingCapture{};
        AbsoluteControlPanelApi::PollBindingCaptureCallback pollBindingCapture{};
        AbsoluteControlPanelApi::CancelBindingCaptureCallback cancelBindingCapture{};
        AbsoluteControlPanelApi::ReassignBindingCallback reassignBinding{};
        AbsoluteControlPanelApi::ReadRecordItemsCallback readRecordItems{};
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
        std::atomic<ModuleSortMode> g_moduleSortMode{
            ModuleSortMode::Registration };
        std::atomic<std::uint64_t> g_revision{ 0 };
        std::atomic<std::uint64_t> g_refreshRevision{ 0 };
        // Protected by g_registryMutex. Registry mutations wake every open
        // session; value refreshes wake only the matching active page.
        std::uint64_t g_directoryRefreshRevision{};
        std::atomic<HostLifecycle> g_lifecycle{ HostLifecycle::Initializing };
        std::atomic_bool g_menuOpen{ false };
        std::atomic_bool g_inputCaptureActive{ false };
        std::mutex g_openRequestMutex;
        std::optional<OpenRequest> g_openRequest;
        std::atomic<std::uint64_t> g_openRequestSerial{};
        std::atomic<OpenRequestWakeCallback> g_openRequestWake{};

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
                   a_kind <= AbsoluteControlPanelApi::ControlKind::RecordCollection;
        }

        [[nodiscard]] bool ValidControlDescriptor(
            const AbsoluteControlPanelApi::ControlDescriptorV1& a_control) noexcept
        {
            using Kind = AbsoluteControlPanelApi::ControlKind;
            constexpr auto kCommonFlags =
                AbsoluteControlPanelApi::kControlReadOnly |
                AbsoluteControlPanelApi::kControlRequiresRestart |
                AbsoluteControlPanelApi::kControlAdvanced |
                AbsoluteControlPanelApi::kControlMutatesDraft |
                AbsoluteControlPanelApi::kControlAppliesDraftBeforeInvoke |
                AbsoluteControlPanelApi::kControlTransientChoice |
                AbsoluteControlPanelApi::kControlLayoutInline |
                AbsoluteControlPanelApi::kControlRequiresConfirmation |
                AbsoluteControlPanelApi::kControlPinnedContext;
            constexpr auto kBindingFlags =
                AbsoluteControlPanelApi::kBindingKeyboard |
                AbsoluteControlPanelApi::kBindingMouse |
                AbsoluteControlPanelApi::kBindingController |
                AbsoluteControlPanelApi::kBindingModifiers |
                AbsoluteControlPanelApi::kBindingClearable;
            if (a_control.structSize < sizeof(a_control) ||
                !IsTerminated(a_control.controlId) ||
                !IsIdentifier(a_control.controlId) ||
                !IsTerminated(a_control.label) || a_control.label[0] == '\0' ||
                !IsTerminated(a_control.description) ||
                !ValidControlKind(a_control.kind) ||
                (a_control.flags & ~(kCommonFlags | kBindingFlags)) != 0 ||
                ((a_control.flags & AbsoluteControlPanelApi::kControlMutatesDraft) != 0 &&
                    a_control.kind != Kind::Action) ||
                ((a_control.flags &
                     AbsoluteControlPanelApi::kControlAppliesDraftBeforeInvoke) != 0 &&
                    a_control.kind != Kind::Action) ||
                ((a_control.flags &
                     AbsoluteControlPanelApi::kControlTransientChoice) != 0 &&
                    a_control.kind != Kind::Choice &&
                    a_control.kind != Kind::RecordCollection) ||
                ((a_control.flags & AbsoluteControlPanelApi::kControlMutatesDraft) != 0 &&
                    (a_control.flags &
                        AbsoluteControlPanelApi::kControlAppliesDraftBeforeInvoke) != 0) ||
                ((a_control.flags & AbsoluteControlPanelApi::kControlLayoutInline) != 0 &&
                    a_control.kind != Kind::Action) ||
                ((a_control.flags &
                     AbsoluteControlPanelApi::kControlRequiresConfirmation) != 0 &&
                    a_control.kind != Kind::Action) ||
                ((a_control.flags &
                     AbsoluteControlPanelApi::kControlPinnedContext) != 0 &&
                    a_control.kind != Kind::Choice &&
                    a_control.kind != Kind::RecordCollection &&
                    a_control.kind != Kind::InputBinding) ||
                (a_control.kind == Kind::RecordCollection &&
                    (a_control.flags & AbsoluteControlPanelApi::kControlReadOnly) == 0 &&
                    (a_control.flags &
                        AbsoluteControlPanelApi::kControlTransientSelection) == 0) ||
                (a_control.kind != Kind::InputBinding &&
                    (a_control.flags & kBindingFlags) != 0)) {
                return false;
            }
            if (a_control.kind == Kind::Toggle || a_control.kind == Kind::Action ||
                a_control.kind == Kind::InputBinding ||
                a_control.kind == Kind::GroupHeader ||
                a_control.kind == Kind::RecordCollection) {
                return true;
            }
            if (a_control.kind == Kind::TextInput) {
                return std::isfinite(a_control.minimumValue) &&
                       std::isfinite(a_control.maximumValue) &&
                       std::isfinite(a_control.stepValue) &&
                       a_control.minimumValue == 0.0 &&
                       a_control.maximumValue >= 1.0 &&
                       a_control.maximumValue <
                           AbsoluteControlPanelApi::kStringValueCapacity &&
                       std::floor(a_control.maximumValue) == a_control.maximumValue &&
                       a_control.stepValue == 1.0;
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

        [[nodiscard]] std::size_t RegisteredPageCount(
            std::string_view a_moduleId) noexcept
        {
            return static_cast<std::size_t>(std::ranges::count_if(
                g_pages, [&](const Page& a_page) {
                    return a_page.moduleId == a_moduleId;
                }));
        }

        [[nodiscard]] std::size_t RegisteredControlCount(
            std::string_view a_moduleId) noexcept
        {
            std::size_t result{};
            for (const auto& page : g_pages) {
                if (page.moduleId == a_moduleId) {
                    result += page.controls.size();
                }
            }
            return result;
        }

        void PublishRegistryMutation() noexcept
        {
            g_revision.fetch_add(1, std::memory_order_release);
            g_directoryRefreshRevision =
                g_refreshRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
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
                if (!a_descriptor ||
                    a_descriptor->structSize <
                        AbsoluteControlPanelApi::kPageDescriptorV1BaseSize ||
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
                bool writable{};
                bool transactionalEditable{};
                std::size_t pinnedContextControls{};
                for (std::uint32_t index = 0; index < a_descriptor->controlCount; ++index) {
                    const auto& source = a_descriptor->controls[index];
                    if (!ValidControlDescriptor(source) ||
                        std::ranges::any_of(page.controls, [&](const Control& a_control) {
                            return a_control.controlId == source.controlId;
                        })) {
                        return Api::InvalidArgument;
                    }
                    needsRead = needsRead || (source.kind != Kind::Action &&
                        source.kind != Kind::GroupHeader);
                    const bool editableControl = source.kind != Kind::Action &&
                        source.kind != Kind::GroupHeader &&
                        (source.flags & AbsoluteControlPanelApi::kControlReadOnly) == 0;
                    writable = writable || editableControl;
                    transactionalEditable = transactionalEditable ||
                        (editableControl &&
                         (source.flags &
                            AbsoluteControlPanelApi::kControlTransientChoice) == 0);
                    if ((source.flags &
                            AbsoluteControlPanelApi::kControlPinnedContext) != 0 &&
                        ++pinnedContextControls > 3) {
                        return Api::CapacityExceeded;
                    }
                    page.controls.push_back(Control{ source.kind, source.flags,
                        source.controlId, source.label, source.description,
                        source.minimumValue, source.maximumValue, source.stepValue });
                }
                if ((needsRead && !a_descriptor->readValue) ||
                    (writable && !a_descriptor->writeDraft) ||
                    (transactionalEditable &&
                        (!a_descriptor->apply || !a_descriptor->cancel))) {
                    return Api::InvalidArgument;
                }

                page.provider = std::make_shared<ProviderState>();
                page.provider->context = a_descriptor->context;
                page.provider->readValue = a_descriptor->readValue;
                page.provider->writeDraft = a_descriptor->writeDraft;
                page.provider->invokeAction = a_descriptor->invokeAction;
                page.provider->apply = a_descriptor->apply;
                page.provider->cancel = a_descriptor->cancel;
                constexpr auto choiceOptionsEnd =
                    offsetof(AbsoluteControlPanelApi::PageDescriptorV1,
                        readChoiceOptions) +
                    sizeof(AbsoluteControlPanelApi::ReadChoiceOptionsCallback);
                if (a_descriptor->structSize >= choiceOptionsEnd) {
                    page.provider->readChoiceOptions =
                        a_descriptor->readChoiceOptions;
                }
                constexpr auto providerCaptureEnd =
                    offsetof(AbsoluteControlPanelApi::PageDescriptorV1,
                        cancelBindingCapture) +
                    sizeof(AbsoluteControlPanelApi::CancelBindingCaptureCallback);
                if (a_descriptor->structSize >= providerCaptureEnd) {
                    const bool anyCaptureCallback =
                        a_descriptor->beginBindingCapture ||
                        a_descriptor->pollBindingCapture ||
                        a_descriptor->cancelBindingCapture;
                    const bool allCaptureCallbacks =
                        a_descriptor->beginBindingCapture &&
                        a_descriptor->pollBindingCapture &&
                        a_descriptor->cancelBindingCapture;
                    if (anyCaptureCallback && !allCaptureCallbacks) {
                        return Api::InvalidArgument;
                    }
                    page.provider->beginBindingCapture =
                        a_descriptor->beginBindingCapture;
                    page.provider->pollBindingCapture =
                        a_descriptor->pollBindingCapture;
                    page.provider->cancelBindingCapture =
                        a_descriptor->cancelBindingCapture;
                }
                constexpr auto reassignBindingEnd =
                    offsetof(AbsoluteControlPanelApi::PageDescriptorV1,
                        reassignBinding) +
                    sizeof(AbsoluteControlPanelApi::ReassignBindingCallback);
                if (a_descriptor->structSize >= reassignBindingEnd) {
                    page.provider->reassignBinding =
                        a_descriptor->reassignBinding;
                }
                constexpr auto recordItemsEnd =
                    offsetof(AbsoluteControlPanelApi::PageDescriptorV1,
                        readRecordItems) +
                    sizeof(AbsoluteControlPanelApi::ReadRecordItemsCallback);
                if (a_descriptor->structSize >= recordItemsEnd) {
                    page.provider->readRecordItems =
                        a_descriptor->readRecordItems;
                }
                if (std::ranges::any_of(page.controls, [](const Control& control) {
                        return control.kind == Kind::RecordCollection;
                    }) && !page.provider->readRecordItems) {
                    return Api::InvalidArgument;
                }
                page.canInvokeAction = a_descriptor->invokeAction != nullptr;
                page.canApply = a_descriptor->apply != nullptr;
                page.canCancel = a_descriptor->cancel != nullptr;

                std::scoped_lock lock{ g_registryMutex };
                if (const auto availability = MutationAvailability();
                    availability != Api::Ok) {
                    return availability;
                }
                if (g_pages.size() >= kMaximumPages ||
                    RegisteredPageCount(page.moduleId) >= kMaximumPagesPerModule ||
                    RegisteredControlCount() + page.controls.size() > kMaximumControls ||
                    RegisteredControlCount(page.moduleId) + page.controls.size() >
                        kMaximumControlsPerModule ||
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
                PublishRegistryMutation();
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
                if (const auto availability = MutationAvailability();
                    availability != Api::Ok) {
                    return availability;
                }
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
                PublishRegistryMutation();
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
                const auto compositionResult =
                    Composition::HostRegistry().UnregisterModule(a_moduleId);
                if (compositionResult != Api::Ok &&
                    compositionResult != Api::NotFound) {
                    return compositionResult;
                }
                for (const auto& provider : providers) provider->retired = true;
                std::erase_if(g_pages,
                    [&](const Page& a_page) { return a_page.moduleId == moduleId; });
                std::erase_if(g_modules,
                    [&](const Module& a_module) { return a_module.moduleId == moduleId; });
                PublishRegistryMutation();
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
                const auto moduleLength = strnlen_s(
                    a_moduleId, AbsoluteControlPanelApi::kIdentifierCapacity);
                const auto pageLength = strnlen_s(
                    a_pageId, AbsoluteControlPanelApi::kIdentifierCapacity);
                if (moduleLength == AbsoluteControlPanelApi::kIdentifierCapacity ||
                    pageLength == AbsoluteControlPanelApi::kIdentifierCapacity) {
                    return Api::InvalidArgument;
                }
                const std::string_view moduleId{ a_moduleId, moduleLength };
                const std::string_view pageId{ a_pageId, pageLength };
                if (!IsIdentifier(moduleId) || !IsIdentifier(pageId)) {
                    return Api::InvalidArgument;
                }
                std::scoped_lock lock{ g_registryMutex };
                if (const auto availability = MutationAvailability();
                    availability != Api::Ok) {
                    return availability;
                }
                const auto page = std::ranges::find_if(g_pages, [&](const Page& a_page) {
                    return a_page.moduleId == moduleId && a_page.pageId == pageId;
                });
                if (page == g_pages.end()) {
                    return Api::NotFound;
                }
                page->refreshRevision =
                    g_refreshRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
                g_revision.fetch_add(1, std::memory_order_release);
                return Api::Ok;
            } catch (...) {
                return Api::Rejected;
            }
        }

        [[nodiscard]] Api __cdecl RequestProductOpenPage(
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
                const auto wake = g_openRequestWake.load(std::memory_order_acquire);
                if (!wake) return Api::NotReady;
                {
                    std::scoped_lock lock{ g_registryMutex };
                    if (const auto availability = MutationAvailability();
                        availability != Api::Ok) return availability;
                    const auto page = std::ranges::find_if(
                        g_pages, [&](const Page& a_page) {
                            return a_page.moduleId == moduleId &&
                                   a_page.pageId == pageId;
                        });
                    if (page == g_pages.end()) return Api::NotFound;
                }
                const auto serial =
                    g_openRequestSerial.fetch_add(1, std::memory_order_acq_rel) + 1;
                {
                    std::scoped_lock lock{ g_openRequestMutex };
                    g_openRequest = OpenRequest{
                        std::string(moduleId), std::string(pageId), serial };
                }
                if (!wake()) {
                    std::scoped_lock lock{ g_openRequestMutex };
                    if (g_openRequest && g_openRequest->serial == serial) {
                        g_openRequest.reset();
                    }
                    return Api::NotReady;
                }
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

    CatalogSnapshot SnapshotCatalog(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept
    {
        try {
            std::scoped_lock lock{ g_registryMutex };
            CatalogSnapshot result;
            result.revision = g_revision.load(std::memory_order_relaxed);
            result.modules.reserve((std::min)(g_pages.size(), kMaximumModules));
            result.pages.reserve(kMaximumPagesPerModule);
            std::unordered_set<std::string_view> knownModules;
            knownModules.reserve((std::min)(g_pages.size(), kMaximumModules));
            auto selected = std::ranges::find_if(g_pages, [&](const Page& a_page) {
                return a_page.moduleId == a_moduleId && a_page.pageId == a_pageId;
            });
            if (selected == g_pages.end() && !a_moduleId.empty()) {
                selected = std::ranges::find_if(g_pages, [&](const Page& a_page) {
                    return a_page.moduleId == a_moduleId;
                });
            }
            if (selected == g_pages.end() && !g_pages.empty()) {
                selected = g_pages.begin();
            }
            for (auto source = g_pages.begin(); source != g_pages.end(); ++source) {
                if (knownModules.insert(source->moduleId).second) {
                    result.modules.push_back(ModuleSummary{
                        source->moduleId, source->moduleDisplayName, source->pageId });
                }
                if (selected == g_pages.end() ||
                    source->moduleId != selected->moduleId) {
                    continue;
                }
                Page page;
                page.moduleId = source->moduleId;
                page.moduleDisplayName = source->moduleDisplayName;
                page.pageId = source->pageId;
                page.displayName = source->displayName;
                page.description = source->description;
                page.canInvokeAction = source->canInvokeAction;
                page.canApply = source->canApply;
                page.canCancel = source->canCancel;
                page.refreshRevision = source->refreshRevision;
                if (source == selected) {
                    page.controls = source->controls;
                    page.provider = source->provider;
                }
                result.pages.push_back(std::move(page));
            }
            const auto controlModule = [](const ModuleSummary& module) {
                return module.moduleId == "absolute.control";
            };
            if (g_moduleSortMode.load(std::memory_order_acquire) ==
                ModuleSortMode::Alphabetical) {
                std::ranges::stable_sort(result.modules,
                    [&](const ModuleSummary& left, const ModuleSummary& right) {
                        if (controlModule(left) != controlModule(right)) {
                            return !controlModule(left);
                        }
                        return left.displayName < right.displayName;
                    });
            } else {
                std::ranges::stable_partition(result.modules,
                    [&](const ModuleSummary& module) {
                        return !controlModule(module);
                    });
            }
            return result;
        } catch (...) {
            return {};
        }
    }

    RegistryDiagnostics Diagnostics() noexcept
    {
        try {
            std::scoped_lock lock{ g_registryMutex };
            RegistryDiagnostics diagnostics;
            diagnostics.lifecycle = g_lifecycle.load(std::memory_order_acquire);
            diagnostics.revision = g_revision.load(std::memory_order_acquire);
            diagnostics.refreshRevision =
                g_refreshRevision.load(std::memory_order_acquire);
            diagnostics.menuOpen = g_menuOpen.load(std::memory_order_acquire);
            diagnostics.inputCaptureActive =
                g_inputCaptureActive.load(std::memory_order_acquire);
            diagnostics.modules.reserve(g_modules.size());
            for (const auto& module : g_modules) {
                ModuleDiagnostics item{ module.moduleId, module.displayName };
                for (const auto& page : g_pages) {
                    if (page.moduleId != module.moduleId) continue;
                    ++item.pageCount;
                    item.controlCount += page.controls.size();
                }
                diagnostics.modules.push_back(std::move(item));
            }
            return diagnostics;
        } catch (...) {
            return {};
        }
    }

    void SetModuleSortMode(ModuleSortMode a_mode) noexcept
    {
        const auto previous = g_moduleSortMode.exchange(
            a_mode, std::memory_order_acq_rel);
        if (previous == a_mode) return;
        std::scoped_lock lock{ g_registryMutex };
        g_revision.fetch_add(1, std::memory_order_release);
        g_directoryRefreshRevision =
            g_refreshRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    ModuleSortMode GetModuleSortMode() noexcept
    {
        return g_moduleSortMode.load(std::memory_order_acquire);
    }

    std::uint64_t Revision() noexcept
    {
        return g_revision.load(std::memory_order_acquire);
    }

    std::uint64_t RefreshRevision() noexcept
    {
        return g_refreshRevision.load(std::memory_order_acquire);
    }

    AbsoluteControlPanelApi::Result RequestRefresh(
        const char* a_moduleId, const char* a_pageId) noexcept
    {
        return RequestProductRefresh(a_moduleId, a_pageId);
    }

    bool ConsumeRefresh(std::uint64_t& a_cursor) noexcept
    {
        const auto current = RefreshRevision();
        if (current <= a_cursor) return false;
        a_cursor = current;
        return true;
    }

    bool ConsumeRefresh(std::uint64_t& a_cursor,
        std::string_view a_activeModuleId,
        std::string_view a_activePageId) noexcept
    {
        try {
            std::scoped_lock lock{ g_registryMutex };
            const auto previous = a_cursor;
            const auto current =
                g_refreshRevision.load(std::memory_order_acquire);
            if (current <= previous) {
                return false;
            }
            a_cursor = current;
            if (g_directoryRefreshRevision > previous) {
                return true;
            }
            const auto active = std::ranges::find_if(
                g_pages, [&](const Page& a_page) {
                    return a_page.moduleId == a_activeModuleId &&
                        a_page.pageId == a_activePageId;
                });
            return active != g_pages.end() &&
                active->refreshRevision > previous;
        } catch (...) {
            return false;
        }
    }

    void SetOpenRequestWakeCallback(OpenRequestWakeCallback a_callback) noexcept
    {
        g_openRequestWake.store(a_callback, std::memory_order_release);
    }

    bool ConsumeOpenRequest(OpenRequest& a_request) noexcept
    {
        try {
            std::scoped_lock lock{ g_openRequestMutex };
            if (!g_openRequest) return false;
            a_request = std::move(*g_openRequest);
            g_openRequest.reset();
            return true;
        } catch (...) {
            return false;
        }
    }

    void DiscardOpenRequest() noexcept
    {
        try {
            std::scoped_lock lock{ g_openRequestMutex };
            g_openRequest.reset();
        } catch (...) {}
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

    Api ReadChoiceOptions(const Page& a_page, std::string_view a_controlId,
        std::vector<ChoiceOption>& a_options) noexcept
    {
        try {
            a_options.clear();
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->readChoiceOptions) {
                return Api::NotFound;
            }
            std::array<AbsoluteControlPanelApi::ChoiceOptionV1,
                AbsoluteControlPanelApi::kMaximumChoiceOptions> records{};
            for (auto& record : records) {
                record.structSize = sizeof(record);
            }
            std::uint32_t count{};
            const std::string controlId{ a_controlId };
            const auto result = a_page.provider->readChoiceOptions(
                a_page.provider->context, controlId.c_str(), records.data(),
                static_cast<std::uint32_t>(records.size()), &count);
            if (result != Api::Ok) return result;
            if (count == 0 || count > records.size()) return Api::Rejected;
            a_options.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto& record = records[index];
                if (record.structSize < sizeof(record) ||
                    !IsTerminated(record.label) || record.label[0] == '\0' ||
                    std::ranges::any_of(a_options, [&](const ChoiceOption& option) {
                        return option.value == record.value;
                    })) {
                    a_options.clear();
                    return Api::Rejected;
                }
                a_options.push_back({ record.value, record.label });
            }
            return Api::Ok;
        } catch (...) {
            a_options.clear();
            return Api::Rejected;
        }
    }

    Api ReadRecordItems(const Page& a_page, std::string_view a_controlId,
        std::vector<RecordItem>& a_items) noexcept
    {
        try {
            a_items.clear();
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->readRecordItems) {
                return Api::NotFound;
            }
            std::array<AbsoluteControlPanelApi::RecordItemV1,
                AbsoluteControlPanelApi::kMaximumRecordItems> records{};
            for (auto& record : records) record.structSize = sizeof(record);
            std::uint32_t count{};
            const std::string controlId{ a_controlId };
            const auto result = a_page.provider->readRecordItems(
                a_page.provider->context, controlId.c_str(), records.data(),
                static_cast<std::uint32_t>(records.size()), &count);
            if (result != Api::Ok) return result;
            if (count > records.size()) return Api::Rejected;
            a_items.reserve(count);
            constexpr auto kKnownFlags =
                AbsoluteControlPanelApi::kRecordItemDisabled |
                AbsoluteControlPanelApi::kRecordItemWarning;
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto& record = records[index];
                if (record.structSize < sizeof(record) ||
                    (record.flags & ~kKnownFlags) != 0 ||
                    !IsTerminated(record.recordId) ||
                    !IsIdentifier(record.recordId) ||
                    !IsTerminated(record.label) || record.label[0] == '\0' ||
                    !IsTerminated(record.summary) ||
                    !IsTerminated(record.detail) ||
                    std::ranges::any_of(a_items, [&](const RecordItem& item) {
                        return item.recordId == record.recordId;
                    })) {
                    a_items.clear();
                    return Api::Rejected;
                }
                a_items.push_back({ record.flags, record.recordId, record.label,
                    record.summary, record.detail });
            }
            return Api::Ok;
        } catch (...) {
            a_items.clear();
            return Api::Rejected;
        }
    }

    Api BeginBindingCapture(const Page& a_page,
        std::string_view a_controlId) noexcept
    {
        try {
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->beginBindingCapture) {
                return Api::NotFound;
            }
            const std::string controlId{ a_controlId };
            return a_page.provider->beginBindingCapture(
                a_page.provider->context, controlId.c_str());
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api PollBindingCapture(const Page& a_page, std::string_view a_controlId,
        AbsoluteControlPanelApi::BindingCaptureV1& a_capture) noexcept
    {
        try {
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->pollBindingCapture) {
                return Api::NotFound;
            }
            const std::string controlId{ a_controlId };
            return a_page.provider->pollBindingCapture(
                a_page.provider->context, controlId.c_str(), &a_capture);
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api CancelBindingCapture(const Page& a_page,
        std::string_view a_controlId) noexcept
    {
        try {
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->cancelBindingCapture) {
                return Api::NotFound;
            }
            const std::string controlId{ a_controlId };
            return a_page.provider->cancelBindingCapture(
                a_page.provider->context, controlId.c_str());
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api ReassignBinding(const Page& a_page, std::string_view a_controlId,
        std::string_view a_binding, Transaction& a_transaction) noexcept
    {
        try {
            if (a_binding.empty() ||
                a_binding.size() >= AbsoluteControlPanelApi::kStringValueCapacity ||
                (a_transaction.provider_ &&
                    a_transaction.provider_ != a_page.provider)) {
                return Api::InvalidArgument;
            }
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->reassignBinding) {
                return Api::NotFound;
            }
            const std::string controlId{ a_controlId };
            const std::string binding{ a_binding };
            const auto result = a_page.provider->reassignBinding(
                a_page.provider->context, controlId.c_str(), binding.c_str());
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

    Api WriteTransientChoice(const Page& a_page, std::string_view a_controlId,
        const AbsoluteControlPanelApi::ValueV1& a_value) noexcept
    {
        try {
            CallbackLease lease{ a_page.provider };
            if (!lease || !a_page.provider->writeDraft) return Api::Rejected;
            const std::string controlId{ a_controlId };
            return a_page.provider->writeDraft(
                a_page.provider->context, controlId.c_str(), &a_value);
        } catch (...) {
            return Api::Rejected;
        }
    }

    Api AttachTransaction(const Page& a_page,
        Transaction& a_transaction) noexcept
    {
        try {
            if (!a_page.provider ||
                (a_transaction.provider_ &&
                 a_transaction.provider_ != a_page.provider)) {
                return Api::Rejected;
            }
            if (a_transaction.provider_) return Api::Ok;
            CallbackLease lease{ a_page.provider };
            if (!lease) return Api::Rejected;
            std::scoped_lock lock{ a_page.provider->mutex };
            if (a_page.provider->retired) return Api::Rejected;
            ++a_page.provider->transactions;
            a_transaction.provider_ = a_page.provider;
            return Api::Ok;
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
        const std::scoped_lock lock{ g_registryMutex };
        auto expected = HostLifecycle::Initializing;
        (void)g_lifecycle.compare_exchange_strong(expected, HostLifecycle::Ready,
            std::memory_order_acq_rel);
    }

    void MarkRuntimeRejected() noexcept
    {
        const std::scoped_lock lock{ g_registryMutex };
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
        &ProductIsInputCaptureActive,
        AbsoluteControlPanelApi::kCapabilityLabeledChoices |
            AbsoluteControlPanelApi::kCapabilityProviderBindingCapture |
            AbsoluteControlPanelApi::kCapabilityBindingConflictResolution |
            AbsoluteControlPanelApi::kCapabilityStructuredLayout |
            AbsoluteControlPanelApi::kCapabilityRecordCollections |
            AbsoluteControlPanelApi::kCapabilityActionConfirmation |
            AbsoluteControlPanelApi::kCapabilityPageOpenRequests |
            AbsoluteControlPanelApi::kCapabilityPinnedContextControls,
        &RequestProductOpenPage
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
