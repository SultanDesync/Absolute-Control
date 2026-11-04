#include "research/TestMenuPage.h"

#include "AbsoluteControlPanelAPI.h"
#include "EvidenceLog.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

namespace AbsoluteControlPanelResearch::TestMenuPage
{
    namespace
    {
        namespace Api = AbsoluteControlPanelApi;

        constexpr std::size_t kSections = 3;
        constexpr std::size_t kTogglesPerSection = 6;

        template <std::size_t N>
        void Copy(char (&destination)[N], std::string_view source) noexcept
        {
            const auto count = (std::min)(source.size(), N - 1);
            std::memcpy(destination, source.data(), count);
            destination[count] = '\0';
        }

        std::array<Api::ControlDescriptorV1, kSections * (kTogglesPerSection + 1)> g_controls{};
        std::array<bool, kSections * kTogglesPerSection> g_values{};

        Api::Result __cdecl ReadValue(void*, const char* rawId, Api::ValueV1* output) noexcept
        {
            if (!rawId || !output || output->structSize < sizeof(Api::ValueV1)) return Api::Result::InvalidArgument;
            unsigned index{};
            if (::sscanf_s(rawId, "toggle-%u", &index) != 1 || index >= g_values.size()) return Api::Result::NotFound;
            output->kind = Api::ValueKind::Boolean;
            output->booleanValue = g_values[index] ? 1 : 0;
            return Api::Result::Ok;
        }

        Api::Result __cdecl WriteDraft(void*, const char* rawId, const Api::ValueV1* value) noexcept
        {
            if (!rawId || !value || value->kind != Api::ValueKind::Boolean) return Api::Result::InvalidArgument;
            unsigned index{};
            if (::sscanf_s(rawId, "toggle-%u", &index) != 1 || index >= g_values.size()) return Api::Result::NotFound;
            g_values[index] = value->booleanValue != 0;
            return Api::Result::Ok;
        }

        Api::Result __cdecl Apply(void*) noexcept { return Api::Result::Ok; }
        void __cdecl Cancel(void*) noexcept {}
    }

    void Register() noexcept
    {
        try {
            const auto* api = AbsoluteControlPanel_QueryApi(Api::kAbiVersion);
            if (!api || !api->registerPage) return;
            constexpr std::array<std::string_view, kSections> kTitles{ "First section", "Second section", "Third section" };
            std::size_t next{};
            std::size_t toggle{};
            for (const auto title : kTitles) {
                auto& header = g_controls[next++];
                header.kind = Api::ControlKind::GroupHeader;
                Copy(header.controlId, std::format("header-{}", next));
                Copy(header.label, title);
                Copy(header.description, "A section header on a test page.");
                for (std::size_t row = 0; row < kTogglesPerSection; ++row, ++toggle) {
                    auto& control = g_controls[next++];
                    control.kind = Api::ControlKind::Toggle;
                    Copy(control.controlId, std::format("toggle-{}", toggle));
                    Copy(control.label, std::format("{} toggle {}", title, row + 1));
                    Copy(control.description, "A setting on a test page.");
                }
            }
            Api::PageDescriptorV1 page;
            Copy(page.moduleId, "absolute.control");
            Copy(page.pageId, "test-sections");
            Copy(page.group, "Flight");
            Copy(page.displayName, "Test Sections");
            Copy(page.description, "A long page for testing sidebar sections.");
            page.readValue = &ReadValue;
            page.writeDraft = &WriteDraft;
            page.apply = &Apply;
            page.cancel = &Cancel;
            page.controls = g_controls.data();
            page.controlCount = static_cast<std::uint32_t>(g_controls.size());
            const auto result = api->registerPage(&page);
            EvidenceLog::Event(result == Api::Result::Ok ? "test_menu_page_registered" : "test_menu_page_failed",
                std::format("result={}", static_cast<std::uint32_t>(result)));
        } catch (...) {
            EvidenceLog::Event("test_menu_page_failed", "exception during registration");
        }
    }
}
