#include "SlopAPI.h"

#include "AbsoluteControlPanelAPI.h"

#include "MenuApiHost.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <mutex>
#include <ranges>

namespace AbsoluteControlPanelResearch::MenuApiHost
{
    namespace
    {
        constexpr std::size_t kMaximumPages = 64;
        constexpr std::size_t kMaximumModules = 32;
        constexpr std::size_t kMaximumControlsPerPage = 128;

        struct Module
        {
            std::string moduleId;
            std::string displayName;
            std::string description;
        };

        std::mutex g_mutex;
        std::vector<Module> g_modules;
        std::vector<Page> g_pages;
        std::atomic<std::uint64_t> g_revision{ 0 };
        std::atomic_bool g_menuOpen{ false };
        std::atomic_bool g_inputCaptureActive{ false };

        template <std::size_t N>
        [[nodiscard]] bool IsTerminated(const char (&a_value)[N]) noexcept
        {
            return std::memchr(a_value, '\0', N) != nullptr;
        }

        [[nodiscard]] bool IsIdentifier(std::string_view a_value) noexcept
        {
            if (a_value.empty()) {
                return false;
            }
            return std::ranges::all_of(a_value, [](unsigned char a_character) {
                return std::isalnum(a_character) != 0 || a_character == '.' ||
                       a_character == '_' || a_character == '-';
            });
        }

        [[nodiscard]] SlopApi::Result __cdecl RegisterPage(
            const SlopApi::PageDescriptorV1* a_descriptor) noexcept
        {
            using namespace SlopApi;
            try {
                if (!a_descriptor || a_descriptor->structSize < sizeof(PageDescriptorV1) ||
                    !IsTerminated(a_descriptor->moduleId) ||
                    !IsTerminated(a_descriptor->pageId) ||
                    !IsTerminated(a_descriptor->displayName) ||
                    !IsTerminated(a_descriptor->description) ||
                    !IsIdentifier(a_descriptor->moduleId) ||
                    !IsIdentifier(a_descriptor->pageId) ||
                    a_descriptor->displayName[0] == '\0' || !a_descriptor->readValue ||
                    !a_descriptor->writeDraft ||
                    a_descriptor->controlCount > kMaximumControlsPerPage ||
                    (a_descriptor->controlCount != 0 && !a_descriptor->controls)) {
                    return Result::InvalidArgument;
                }

                Page page;
                page.moduleId = a_descriptor->moduleId;
                page.moduleDisplayName = page.moduleId;
                page.pageId = a_descriptor->pageId;
                page.displayName = a_descriptor->displayName;
                page.description = a_descriptor->description;
                page.context = a_descriptor->context;
                page.readValue = a_descriptor->readValue;
                page.writeDraft = a_descriptor->writeDraft;
                page.invokeAction = a_descriptor->invokeAction;
                page.apply = a_descriptor->apply;
                page.cancel = a_descriptor->cancel;
                page.controls.reserve(a_descriptor->controlCount);

                for (std::uint32_t index = 0; index < a_descriptor->controlCount; ++index) {
                    const auto& source = a_descriptor->controls[index];
                    if (source.structSize < sizeof(ControlDescriptorV1) ||
                        !IsTerminated(source.controlId) || !IsIdentifier(source.controlId) ||
                        !IsTerminated(source.label) || source.label[0] == '\0' ||
                        !IsTerminated(source.description) ||
                        std::ranges::any_of(page.controls, [&](const Control& a_control) {
                            return a_control.controlId == source.controlId;
                        })) {
                        return Result::InvalidArgument;
                    }
                    page.controls.push_back(Control{
                        source.kind,
                        source.flags,
                        source.controlId,
                        source.label,
                        source.description,
                        source.minimumValue,
                        source.maximumValue,
                        source.stepValue });
                }

                std::scoped_lock lock{ g_mutex };
                if (g_pages.size() >= kMaximumPages) {
                    return Result::CapacityExceeded;
                }
                if (std::ranges::any_of(g_pages, [&](const Page& a_page) {
                        return a_page.moduleId == page.moduleId &&
                               a_page.pageId == page.pageId;
                    })) {
                    return Result::Duplicate;
                }
                const auto module = std::ranges::find_if(g_modules, [&](const Module& a_module) {
                    return a_module.moduleId == page.moduleId;
                });
                if (module != g_modules.end()) {
                    page.moduleDisplayName = module->displayName;
                }
                g_pages.push_back(std::move(page));
                g_revision.fetch_add(1, std::memory_order_release);
                return Result::Ok;
            } catch (...) {
                return Result::Rejected;
            }
        }

        [[nodiscard]] SlopApi::Result __cdecl RegisterModule(
            const SlopApi::ModuleDescriptorV1* a_descriptor) noexcept
        {
            using namespace SlopApi;
            try {
                if (!a_descriptor || a_descriptor->structSize < sizeof(ModuleDescriptorV1) ||
                    !IsTerminated(a_descriptor->moduleId) ||
                    !IsTerminated(a_descriptor->displayName) ||
                    !IsTerminated(a_descriptor->description) ||
                    !IsIdentifier(a_descriptor->moduleId) ||
                    a_descriptor->displayName[0] == '\0') {
                    return Result::InvalidArgument;
                }

                Module module{
                    a_descriptor->moduleId,
                    a_descriptor->displayName,
                    a_descriptor->description
                };
                std::scoped_lock lock{ g_mutex };
                if (std::ranges::any_of(g_modules, [&](const Module& a_module) {
                        return a_module.moduleId == module.moduleId;
                    })) {
                    return Result::Duplicate;
                }
                if (g_modules.size() >= kMaximumModules) {
                    return Result::CapacityExceeded;
                }
                for (auto& page : g_pages) {
                    if (page.moduleId == module.moduleId) {
                        page.moduleDisplayName = module.displayName;
                    }
                }
                g_modules.push_back(std::move(module));
                g_revision.fetch_add(1, std::memory_order_release);
                return Result::Ok;
            } catch (...) {
                return Result::Rejected;
            }
        }

        [[nodiscard]] SlopApi::Result __cdecl UnregisterModule(
            const char* a_moduleId) noexcept
        {
            using namespace SlopApi;
            try {
                if (!a_moduleId) {
                    return Result::InvalidArgument;
                }
                const std::string_view moduleId{ a_moduleId };
                if (!IsIdentifier(moduleId)) {
                    return Result::InvalidArgument;
                }
                std::scoped_lock lock{ g_mutex };
                const auto previousSize = g_pages.size();
                std::erase_if(g_pages, [&](const Page& a_page) {
                    return a_page.moduleId == moduleId;
                });
                std::erase_if(g_modules, [&](const Module& a_module) {
                    return a_module.moduleId == moduleId;
                });
                if (g_pages.size() == previousSize) {
                    return Result::NotFound;
                }
                g_revision.fetch_add(1, std::memory_order_release);
                return Result::Ok;
            } catch (...) {
                return Result::Rejected;
            }
        }

        [[nodiscard]] SlopApi::Result __cdecl RequestRefresh(
            const char* a_moduleId, const char* a_pageId) noexcept
        {
            using namespace SlopApi;
            try {
                if (!a_moduleId || !a_pageId) {
                    return Result::InvalidArgument;
                }
                const std::string_view moduleId{ a_moduleId };
                const std::string_view pageId{ a_pageId };
                if (!IsIdentifier(moduleId) || !IsIdentifier(pageId)) {
                    return Result::InvalidArgument;
                }
                std::scoped_lock lock{ g_mutex };
                if (!std::ranges::any_of(g_pages, [&](const Page& a_page) {
                        return a_page.moduleId == moduleId && a_page.pageId == pageId;
                    })) {
                    return Result::NotFound;
                }
                g_revision.fetch_add(1, std::memory_order_release);
                return Result::Ok;
            } catch (...) {
                return Result::Rejected;
            }
        }

        [[nodiscard]] AbsoluteControlPanelApi::Result __cdecl RegisterPublicPage(
            const AbsoluteControlPanelApi::PageDescriptorV1* a_descriptor) noexcept
        {
            static_assert(sizeof(AbsoluteControlPanelApi::PageDescriptorV1) ==
                          sizeof(SlopApi::PageDescriptorV1));
            return static_cast<AbsoluteControlPanelApi::Result>(RegisterPage(
                reinterpret_cast<const SlopApi::PageDescriptorV1*>(a_descriptor)));
        }

        [[nodiscard]] AbsoluteControlPanelApi::Result __cdecl RegisterPublicModule(
            const AbsoluteControlPanelApi::ModuleDescriptorV1* a_descriptor) noexcept
        {
            static_assert(sizeof(AbsoluteControlPanelApi::ModuleDescriptorV1) ==
                          sizeof(SlopApi::ModuleDescriptorV1));
            return static_cast<AbsoluteControlPanelApi::Result>(RegisterModule(
                reinterpret_cast<const SlopApi::ModuleDescriptorV1*>(a_descriptor)));
        }

        [[nodiscard]] AbsoluteControlPanelApi::Result __cdecl UnregisterPublicModule(
            const char* a_moduleId) noexcept
        {
            return static_cast<AbsoluteControlPanelApi::Result>(UnregisterModule(a_moduleId));
        }

        [[nodiscard]] AbsoluteControlPanelApi::Result __cdecl RequestPublicRefresh(
            const char* a_moduleId, const char* a_pageId) noexcept
        {
            return static_cast<AbsoluteControlPanelApi::Result>(
                RequestRefresh(a_moduleId, a_pageId));
        }

        [[nodiscard]] std::uint8_t __cdecl PublicIsOpen() noexcept
        {
            return g_menuOpen.load(std::memory_order_acquire) ? 1U : 0U;
        }

        [[nodiscard]] std::uint8_t __cdecl PublicIsInputCaptureActive() noexcept
        {
            return g_inputCaptureActive.load(std::memory_order_acquire) ? 1U : 0U;
        }
    }

    std::optional<Page> FindPage(
        std::string_view a_moduleId, std::string_view a_pageId) noexcept
    {
        try {
            std::scoped_lock lock{ g_mutex };
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
            std::scoped_lock lock{ g_mutex };
            return g_pages;
        } catch (...) {
            return {};
        }
    }

    std::uint64_t Revision() noexcept
    {
        return g_revision.load(std::memory_order_acquire);
    }

    const SlopApi::ApiV1 g_api{
        sizeof(SlopApi::ApiV1),
        SlopApi::kAbiVersion,
        "SLOP",
        "Absolute Control Panel",
        "0.2.0-dev",
        &RegisterPage,
        &UnregisterModule,
        &RequestRefresh,
        &RegisterModule
    };

    const AbsoluteControlPanelApi::ApiV1 g_publicApi{
        sizeof(AbsoluteControlPanelApi::ApiV1),
        AbsoluteControlPanelApi::kAbiVersion,
        AbsoluteControlPanelApi::kModuleId.data(),
        "Absolute Control Panel",
        "0.2.0-dev",
        &RegisterPublicPage,
        &UnregisterPublicModule,
        &RequestPublicRefresh,
        &RegisterPublicModule,
        &PublicIsOpen,
        &PublicIsInputCaptureActive
    };

    void SetMenuOpen(bool a_open) noexcept
    {
        g_menuOpen.store(a_open, std::memory_order_release);
        if (!a_open) {
            g_inputCaptureActive.store(false, std::memory_order_release);
        }
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
    if (a_requestedAbiVersion != AbsoluteControlPanelApi::kAbiVersion) {
        return nullptr;
    }
    return &AbsoluteControlPanelResearch::MenuApiHost::g_publicApi;
}

extern "C" SLOP_API const SlopApi::ApiV1*
SLOP_QueryApi(std::uint32_t a_requestedAbiVersion) noexcept
{
    if (a_requestedAbiVersion != SlopApi::kAbiVersion) {
        return nullptr;
    }
    return &AbsoluteControlPanelResearch::MenuApiHost::g_api;
}
