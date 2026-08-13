#include "SlopAPI.h"

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
        constexpr std::size_t kMaximumControlsPerPage = 128;

        std::mutex g_mutex;
        std::vector<Page> g_pages;
        std::atomic<std::uint64_t> g_revision{ 0 };

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
                g_pages.push_back(std::move(page));
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
        "Starfield Local Options Panel",
        "0.1.0-research",
        &RegisterPage,
        &UnregisterModule,
        &RequestRefresh
    };
}

extern "C" SLOP_API const SlopApi::ApiV1*
SLOP_QueryApi(std::uint32_t a_requestedAbiVersion) noexcept
{
    if (a_requestedAbiVersion != SlopApi::kAbiVersion) {
        return nullptr;
    }
    return &AbsoluteControlPanelResearch::MenuApiHost::g_api;
}
