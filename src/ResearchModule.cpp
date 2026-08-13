#include "ResearchModule.h"

#include "SlopAPI.h"
#include "EvidenceLog.h"

namespace AbsoluteControlPanelResearch::ResearchModule
{
    namespace
    {
        using SlopApi::ControlDescriptorV1;
        using SlopApi::ControlKind;
        using SlopApi::ModuleDescriptorV1;
        using SlopApi::PageDescriptorV1;
        using SlopApi::Result;
        using SlopApi::ValueKind;
        using SlopApi::ValueV1;

        struct State
        {
            std::mutex mutex;
            bool committedEnabled{};
            std::int64_t committedSensitivity{ 50 };
            std::string committedBinding{ "(unbound)" };
            bool draftEnabled{};
            std::int64_t draftSensitivity{ 50 };
            std::string draftBinding{ "(unbound)" };
        };

        State g_state;

        template <std::size_t N>
        void CopyText(char (&a_destination)[N], std::string_view a_source) noexcept
        {
            const auto count = std::min(a_source.size(), N - 1);
            std::memcpy(a_destination, a_source.data(), count);
            a_destination[count] = '\0';
        }

        [[nodiscard]] Result __cdecl ReadValue(
            void* a_context, const char* a_controlId, ValueV1* a_value) noexcept
        {
            if (a_context != &g_state || !a_controlId || !a_value ||
                a_value->structSize < sizeof(ValueV1)) {
                return Result::InvalidArgument;
            }

            std::scoped_lock lock{ g_state.mutex };
            const std::string_view controlId{ a_controlId };
            if (controlId == kToggleId) {
                a_value->kind = ValueKind::Boolean;
                a_value->booleanValue = g_state.draftEnabled ? 1U : 0U;
            } else if (controlId == kSensitivityId) {
                a_value->kind = ValueKind::Integer;
                a_value->integerValue = g_state.draftSensitivity;
            } else if (controlId == kBindingId) {
                a_value->kind = ValueKind::String;
                CopyText(a_value->stringValue, g_state.draftBinding);
            } else {
                return Result::NotFound;
            }
            return Result::Ok;
        }

        [[nodiscard]] Result __cdecl WriteDraft(
            void* a_context, const char* a_controlId, const ValueV1* a_value) noexcept
        {
            if (a_context != &g_state || !a_controlId || !a_value ||
                a_value->structSize < sizeof(ValueV1)) {
                return Result::InvalidArgument;
            }

            std::scoped_lock lock{ g_state.mutex };
            const std::string_view controlId{ a_controlId };
            if (controlId == kToggleId && a_value->kind == ValueKind::Boolean) {
                g_state.draftEnabled = a_value->booleanValue != 0;
            } else if (controlId == kSensitivityId &&
                       a_value->kind == ValueKind::Integer &&
                       a_value->integerValue >= 0 && a_value->integerValue <= 100) {
                g_state.draftSensitivity = a_value->integerValue;
            } else if (controlId == kBindingId && a_value->kind == ValueKind::String &&
                       std::memchr(
                           a_value->stringValue, '\0',
                           SlopApi::kStringValueCapacity)) {
                g_state.draftBinding = a_value->stringValue;
            } else {
                return Result::InvalidArgument;
            }
            return Result::Ok;
        }

        [[nodiscard]] Result WriteConfiguration() noexcept
        {
            try {
                const auto logPath = EvidenceLog::Path();
                if (logPath.empty()) {
                    return Result::NotReady;
                }
                const auto outputPath = logPath.parent_path() /
                                        "AbsoluteControlPanelResearch.dummy.ini";
                const auto temporaryPath = outputPath.string() + ".tmp";
                {
                    std::ofstream stream{ temporaryPath, std::ios::out | std::ios::trunc };
                    stream << "; Research-only output. AbsoluteHOTAS never reads this file.\n"
                           << "[RepresentativeControls]\n"
                           << "bAxisInvert=" << (g_state.draftEnabled ? "true" : "false")
                           << "\n"
                           << "fAxisSensitivity="
                           << std::format("{:.2f}", g_state.draftSensitivity / 100.0)
                           << "\n"
                           << "iTestButtonBinding=" << g_state.draftBinding << "\n";
                    stream.flush();
                    if (!stream) {
                        return Result::WriteFailure;
                    }
                }
                try {
                    std::filesystem::rename(temporaryPath, outputPath);
                } catch (const std::filesystem::filesystem_error&) {
                    std::filesystem::copy_file(
                        temporaryPath, outputPath,
                        std::filesystem::copy_options::overwrite_existing);
                    std::filesystem::remove(temporaryPath);
                }
                return Result::Ok;
            } catch (...) {
                return Result::WriteFailure;
            }
        }

        [[nodiscard]] Result __cdecl Apply(void* a_context) noexcept
        {
            if (a_context != &g_state) {
                return Result::InvalidArgument;
            }
            std::scoped_lock lock{ g_state.mutex };
            const auto result = WriteConfiguration();
            if (result != Result::Ok) {
                EvidenceLog::Event(
                    "dummy_config_write_failed",
                    std::format("provider_result={}", static_cast<std::uint32_t>(result)));
                return result;
            }
            g_state.committedEnabled = g_state.draftEnabled;
            g_state.committedSensitivity = g_state.draftSensitivity;
            g_state.committedBinding = g_state.draftBinding;
            EvidenceLog::Event(
                "dummy_config_written",
                std::format(
                    "provider={} enabled={} sensitivity={} binding={}", kModuleId,
                    g_state.committedEnabled, g_state.committedSensitivity,
                    g_state.committedBinding));
            return Result::Ok;
        }

        void __cdecl Cancel(void* a_context) noexcept
        {
            if (a_context != &g_state) {
                return;
            }
            std::scoped_lock lock{ g_state.mutex };
            g_state.draftEnabled = g_state.committedEnabled;
            g_state.draftSensitivity = g_state.committedSensitivity;
            g_state.draftBinding = g_state.committedBinding;
        }

        constexpr std::array<ControlDescriptorV1, 3> kControls{
            ControlDescriptorV1{
                sizeof(ControlDescriptorV1), ControlKind::Toggle,
                SlopApi::kControlNone, "axis-invert", "Axis invert",
                "Representative Boolean setting owned by the research subscriber.", 0.0,
                1.0, 1.0 },
            ControlDescriptorV1{
                sizeof(ControlDescriptorV1), ControlKind::IntegerSlider,
                SlopApi::kControlNone, "axis-sensitivity",
                "Axis sensitivity",
                "Representative bounded numeric setting owned by the research subscriber.",
                0.0, 100.0, 5.0 },
            ControlDescriptorV1{
                sizeof(ControlDescriptorV1), ControlKind::ButtonBinding,
                SlopApi::kBindingKeyboard | SlopApi::kBindingModifiers |
                    SlopApi::kBindingClearable,
                "test-button-binding",
                "Test button binding",
                "Representative keyboard chord captured by the menu host.",
                0.0, 0.0, 0.0 }
        };
    }

    bool Register() noexcept
    {
        const auto* api = SLOP_QueryApi(SlopApi::kAbiVersion);
        if (!api || api->structSize < sizeof(SlopApi::ApiV1) ||
            !api->registerPage) {
            EvidenceLog::Event("api_provider_registration_failed", "host API unavailable");
            return false;
        }

        constexpr auto registerModuleOffset = offsetof(SlopApi::ApiV1, registerModule) +
            sizeof(((SlopApi::ApiV1*)nullptr)->registerModule);
        if (api->structSize >= registerModuleOffset && api->registerModule) {
            const ModuleDescriptorV1 module{
                sizeof(ModuleDescriptorV1),
                "absolute-control-panel.research",
                "Control Panel Research",
                "Built-in validation provider for the Absolute Control Panel host and SDK."
            };
            const auto moduleResult = api->registerModule(&module);
            if (moduleResult != Result::Ok && moduleResult != Result::Duplicate) {
                EvidenceLog::Event("api_module_registration_failed",
                    std::format("module={} result={}", module.moduleId,
                        static_cast<std::uint32_t>(moduleResult)));
                return false;
            }
        }

        const PageDescriptorV1 pages[]{
            {
                sizeof(PageDescriptorV1),
                "absolute-control-panel.research",
                "representative-controls",
                "Research Controls",
                "Synthetic values used to validate the native MCM host contract.",
                2,
                kControls.data(),
                &g_state,
                &ReadValue,
                &WriteDraft,
                nullptr,
                &Apply,
                &Cancel
            },
            {
                sizeof(PageDescriptorV1),
                "absolute-control-panel.research",
                "input-bindings",
                "Input Bindings",
                "Synthetic binding page used to validate multi-page subscribers.",
                1,
                kControls.data() + 2,
                &g_state,
                &ReadValue,
                &WriteDraft,
                nullptr,
                &Apply,
                &Cancel
            }
        };
        for (const auto& page : pages) {
            const auto result = api->registerPage(&page);
            EvidenceLog::Event(
                result == Result::Ok ? "api_provider_registered" :
                                       "api_provider_registration_failed",
                std::format(
                    "abi={} module={} page={} controls={} result={}", api->abiVersion,
                    page.moduleId, page.pageId, page.controlCount,
                    static_cast<std::uint32_t>(result)));
            if (result != Result::Ok) {
                return false;
            }
        }
        return true;
    }
}
