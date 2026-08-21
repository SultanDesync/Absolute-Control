#include "ControlPanelModule.h"

#include "AbsoluteControlPanelAPI.h"
#include "CompositionRegistry.h"
#include "LiveComponentsRegistry.h"
#include "MenuApiHost.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <fstream>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControlPanelResearch::ControlPanelModule
{
    namespace
    {
        namespace Api = AbsoluteControlPanelApi;

        constexpr std::string_view kModuleId = "absolute.control";
        constexpr std::string_view kSettingsPageId = "menu";
        constexpr std::string_view kRegistryPageId = "registry";

        struct State
        {
            std::mutex mutex;
            bool committedPauseEntry{ true };
            std::uint32_t committedHotkey{};
            ModuleSortMode committedSort{ ModuleSortMode::Registration };
            bool draftPauseEntry{ true };
            std::uint32_t draftHotkey{};
            ModuleSortMode draftSort{ ModuleSortMode::Registration };
            std::string selectedModule{ "absolute.control" };
            std::string selectedBus{ "configuration-pages" };
            std::filesystem::path customConfigPath;
            RuntimeCallbacks callbacks;
            bool registered{};
        };

        State g_state;

        template <std::size_t N>
        void Copy(char (&destination)[N], std::string_view source) noexcept
        {
            const auto count = (std::min)(source.size(), N - 1);
            std::memcpy(destination, source.data(), count);
            destination[count] = '\0';
        }

        Api::ControlDescriptorV1 Control(Api::ControlKind kind,
            std::uint32_t flags, std::string_view id, std::string_view label,
            std::string_view description, double minimum = 0,
            double maximum = 0, double step = 0) noexcept
        {
            Api::ControlDescriptorV1 result;
            result.kind = kind;
            result.flags = flags;
            Copy(result.controlId, id);
            Copy(result.label, label);
            Copy(result.description, description);
            result.minimumValue = minimum;
            result.maximumValue = maximum;
            result.stepValue = step;
            return result;
        }

        const std::array kSettingsControls{
            Control(Api::ControlKind::GroupHeader, Api::kControlNone,
                "menu-behavior", "Menu behavior",
                "Host-owned presentation and activation preferences."),
            Control(Api::ControlKind::Choice, Api::kControlNone,
                "module-sort", "Module order",
                "Keep plugin registration order or sort module names alphabetically. Absolute Control remains last.",
                0, 1, 1),
            Control(Api::ControlKind::Toggle, Api::kControlNone,
                "pause-menu-entry", "Show MOD OPTIONS in Pause Menu",
                "Applies immediately to future Pause Menu instances."),
            Control(Api::ControlKind::Choice, Api::kControlNone,
                "recovery-hotkey", "Recovery hotkey",
                "Optional direct activation key. The Pause Menu entry or a recovery key must remain enabled.",
                0, 255, 1)
        };

        const std::array kRegistryControls{
            Control(Api::ControlKind::GroupHeader, Api::kControlNone,
                "host-status-group", "Host status",
                "Current release host state and bounded registry revisions."),
            Control(Api::ControlKind::TextInput, Api::kControlReadOnly,
                "host-summary", "Registry summary",
                "Registered configuration providers, pages, and controls.",
                0, 255, 1),
            Control(Api::ControlKind::TextInput, Api::kControlReadOnly,
                "host-revisions", "Registry revisions",
                "Catalog and provider-refresh sequence numbers.",
                0, 255, 1),
            Control(Api::ControlKind::TextInput, Api::kControlReadOnly,
                "menu-state", "Menu state",
                "Whether the host menu and an input-capture session are active.",
                0, 255, 1),
            Control(Api::ControlKind::GroupHeader, Api::kControlNone,
                "subscriber-group", "Registered services",
                "Modules attached to the host-owned configuration, live-component, and composition buses."),
            Control(Api::ControlKind::RecordCollection,
                Api::kControlTransientSelection, "registered-modules",
                "Registered modules",
                "Inspect page/control counts and each module's host-bus participation."),
            Control(Api::ControlKind::RecordCollection,
                Api::kControlTransientSelection, "host-buses",
                "Host buses",
                "Inspect available host registries and their current subscriber modules.")
        };

        bool IsAllowedHotkey(std::uint32_t value) noexcept
        {
            return value == 0 || (value >= 0x70 && value <= 0x7B);
        }

        std::string LifecycleLabel(MenuApiHost::HostLifecycle lifecycle)
        {
            switch (lifecycle) {
            case MenuApiHost::HostLifecycle::Ready: return "Ready";
            case MenuApiHost::HostLifecycle::Rejected: return "Rejected";
            default: return "Initializing";
            }
        }

        std::string SubscriberList(const auto& subscribers)
        {
            std::string result;
            for (const auto& subscriber : subscribers) {
                if (!result.empty()) result += ", ";
                if (result.size() + subscriber.moduleId.size() >=
                    Api::kDescriptionCapacity - 1) {
                    result += "…";
                    break;
                }
                result += subscriber.moduleId;
            }
            return result.empty() ? "No subscribers" : result;
        }

        std::size_t LiveCountFor(std::string_view moduleId,
            const std::vector<LiveComponents::SubscriberDiagnostics>& live)
        {
            const auto found = std::ranges::find_if(live, [&](const auto& item) {
                return item.moduleId == moduleId;
            });
            return found == live.end() ? 0 : found->channelCount;
        }

        std::size_t CompositionCountFor(std::string_view moduleId,
            const std::vector<Composition::SubscriberDiagnostics>& composition)
        {
            const auto found = std::ranges::find_if(composition,
                [&](const auto& item) { return item.moduleId == moduleId; });
            return found == composition.end() ? 0 : found->pageCount;
        }

        Api::Result __cdecl ReadValue(void* context, const char* rawId,
            Api::ValueV1* output) noexcept
        {
            if (context != &g_state || !rawId || !output ||
                output->structSize < sizeof(Api::ValueV1)) {
                return Api::Result::InvalidArgument;
            }
            try {
                const std::string_view id{ rawId };
                if (id == "module-sort" || id == "pause-menu-entry" ||
                    id == "recovery-hotkey" || id == "registered-modules" ||
                    id == "host-buses") {
                    std::scoped_lock lock{ g_state.mutex };
                    if (id == "module-sort") {
                        output->kind = Api::ValueKind::Integer;
                        output->integerValue = g_state.draftSort ==
                            ModuleSortMode::Alphabetical ? 1 : 0;
                    } else if (id == "pause-menu-entry") {
                        output->kind = Api::ValueKind::Boolean;
                        output->booleanValue = g_state.draftPauseEntry ? 1 : 0;
                    } else if (id == "recovery-hotkey") {
                        output->kind = Api::ValueKind::Integer;
                        output->integerValue = g_state.draftHotkey;
                    } else {
                        output->kind = Api::ValueKind::String;
                        Copy(output->stringValue, id == "registered-modules" ?
                            g_state.selectedModule : g_state.selectedBus);
                    }
                    return Api::Result::Ok;
                }

                const auto diagnostics = MenuApiHost::Diagnostics();
                std::size_t pages{};
                std::size_t controls{};
                for (const auto& module : diagnostics.modules) {
                    pages += module.pageCount;
                    controls += module.controlCount;
                }
                output->kind = Api::ValueKind::String;
                if (id == "host-summary") {
                    Copy(output->stringValue, std::format(
                        "{} · {} modules · {} pages · {} controls",
                        LifecycleLabel(diagnostics.lifecycle),
                        diagnostics.modules.size(), pages, controls));
                } else if (id == "host-revisions") {
                    Copy(output->stringValue, std::format(
                        "Catalog {} · Refresh {}", diagnostics.revision,
                        diagnostics.refreshRevision));
                } else if (id == "menu-state") {
                    Copy(output->stringValue, std::format("{} · {}",
                        diagnostics.menuOpen ? "Menu open" : "Menu closed",
                        diagnostics.inputCaptureActive ? "capturing input" :
                            "input idle"));
                } else {
                    return Api::Result::NotFound;
                }
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl WriteDraft(void* context, const char* rawId,
            const Api::ValueV1* value) noexcept
        {
            if (context != &g_state || !rawId || !value ||
                value->structSize < sizeof(Api::ValueV1)) {
                return Api::Result::InvalidArgument;
            }
            try {
                const std::string_view id{ rawId };
                std::scoped_lock lock{ g_state.mutex };
                if (id == "module-sort" && value->kind == Api::ValueKind::Integer &&
                    (value->integerValue == 0 || value->integerValue == 1)) {
                    g_state.draftSort = value->integerValue == 1 ?
                        ModuleSortMode::Alphabetical : ModuleSortMode::Registration;
                } else if (id == "pause-menu-entry" &&
                    value->kind == Api::ValueKind::Boolean) {
                    const bool enabled = value->booleanValue != 0;
                    if (!enabled && g_state.draftHotkey == 0) {
                        return Api::Result::Rejected;
                    }
                    g_state.draftPauseEntry = enabled;
                } else if (id == "recovery-hotkey" &&
                    value->kind == Api::ValueKind::Integer &&
                    value->integerValue >= 0 && value->integerValue <= 255 &&
                    IsAllowedHotkey(static_cast<std::uint32_t>(value->integerValue))) {
                    const auto hotkey =
                        static_cast<std::uint32_t>(value->integerValue);
                    if (hotkey == 0 && !g_state.draftPauseEntry) {
                        return Api::Result::Rejected;
                    }
                    g_state.draftHotkey = hotkey;
                } else if ((id == "registered-modules" || id == "host-buses") &&
                    value->kind == Api::ValueKind::String &&
                    std::memchr(value->stringValue, '\0',
                        Api::kStringValueCapacity)) {
                    (id == "registered-modules" ? g_state.selectedModule :
                        g_state.selectedBus) = value->stringValue;
                } else {
                    return Api::Result::InvalidArgument;
                }
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl ReadChoices(void* context, const char* rawId,
            Api::ChoiceOptionV1* options, std::uint32_t capacity,
            std::uint32_t* outputCount) noexcept
        {
            if (context != &g_state || !rawId || !options || !outputCount) {
                return Api::Result::InvalidArgument;
            }
            const std::string_view id{ rawId };
            if (id == "module-sort") {
                *outputCount = 2;
                if (capacity < 2) return Api::Result::CapacityExceeded;
                options[0].value = 0;
                Copy(options[0].label, "Registration order");
                options[1].value = 1;
                Copy(options[1].label, "Alphabetical");
                return Api::Result::Ok;
            }
            if (id != "recovery-hotkey") return Api::Result::NotFound;
            constexpr std::uint32_t count = 13;
            *outputCount = count;
            if (capacity < count) return Api::Result::CapacityExceeded;
            options[0].value = 0;
            Copy(options[0].label, "Off");
            for (std::uint32_t index = 0; index < 12; ++index) {
                options[index + 1].value = 0x70 + index;
                Copy(options[index + 1].label, std::format("F{}", index + 1));
            }
            return Api::Result::Ok;
        }

        Api::RecordItemV1 MakeRecord(std::string_view id,
            std::string_view label, std::string_view summary,
            std::string_view detail) noexcept
        {
            Api::RecordItemV1 record;
            Copy(record.recordId, id);
            Copy(record.label, label);
            Copy(record.summary, summary);
            Copy(record.detail, detail);
            return record;
        }

        Api::Result __cdecl ReadRecords(void* context, const char* rawId,
            Api::RecordItemV1* records, std::uint32_t capacity,
            std::uint32_t* outputCount) noexcept
        {
            if (context != &g_state || !rawId || !records || !outputCount) {
                return Api::Result::InvalidArgument;
            }
            try {
                const auto stable = MenuApiHost::Diagnostics();
                const auto live = LiveComponents::HostRegistry().Diagnostics();
                const auto composition = Composition::HostRegistry().Diagnostics();
                std::vector<Api::RecordItemV1> items;
                if (std::string_view{ rawId } == "registered-modules") {
                    const auto shown = (std::min)(stable.modules.size(),
                        Api::kMaximumRecordItems);
                    items.reserve(shown);
                    for (std::size_t index = 0; index < shown; ++index) {
                        const auto& module = stable.modules[index];
                        const auto liveCount = LiveCountFor(module.moduleId, live);
                        const auto compositionCount =
                            CompositionCountFor(module.moduleId, composition);
                        items.push_back(MakeRecord(module.moduleId,
                            module.displayName,
                            std::format("{} pages · {} controls",
                                module.pageCount, module.controlCount),
                            std::format("Configuration {} · Live channels {} · Compositions {}",
                                module.pageCount, liveCount, compositionCount)));
                    }
                } else if (std::string_view{ rawId } == "host-buses") {
                    std::size_t pageCount{};
                    std::size_t controlCount{};
                    for (const auto& module : stable.modules) {
                        pageCount += module.pageCount;
                        controlCount += module.controlCount;
                    }
                    items.push_back(MakeRecord("configuration-pages",
                        "Configuration pages",
                        std::format("{} subscribers · {} pages · {} controls",
                            stable.modules.size(), pageCount, controlCount),
                        SubscriberList(stable.modules)));
                    std::size_t liveCount{};
                    for (const auto& subscriber : live) {
                        liveCount += subscriber.channelCount;
                    }
                    items.push_back(MakeRecord("live-components",
                        "Live components",
                        std::format("{} subscribers · {} channels",
                            live.size(), liveCount), SubscriberList(live)));
                    std::size_t compositionCount{};
                    for (const auto& subscriber : composition) {
                        compositionCount += subscriber.pageCount;
                    }
                    items.push_back(MakeRecord("semantic-composition",
                        "Semantic composition",
                        std::format("{} subscribers · {} pages",
                            composition.size(), compositionCount),
                        SubscriberList(composition)));
                } else {
                    return Api::Result::NotFound;
                }

                *outputCount = static_cast<std::uint32_t>(items.size());
                if (capacity < items.size()) {
                    return Api::Result::CapacityExceeded;
                }
                std::ranges::copy(items, records);
                return Api::Result::Ok;
            } catch (...) {
                *outputCount = 0;
                return Api::Result::Rejected;
            }
        }

        bool WritePreferences(bool pauseEntry, std::uint32_t hotkey,
            ModuleSortMode sortMode) noexcept
        {
            try {
                const auto path = g_state.customConfigPath;
                if (path.empty()) return false;
                if (!path.parent_path().empty()) {
                    std::filesystem::create_directories(path.parent_path());
                }
                auto temporary = path;
                temporary += ".tmp";
                {
                    std::ofstream stream{ temporary,
                        std::ios::out | std::ios::trunc };
                    stream << "; User-owned Absolute Control preferences.\n"
                           << "[ControlPanel]\n"
                           << "EnablePauseMenuEntry="
                           << (pauseEntry ? "true" : "false") << '\n'
                           << std::format("OpenHotkey=0x{:02X}\n", hotkey)
                           << "ModuleSort="
                           << (sortMode == ModuleSortMode::Alphabetical ?
                                  "Alphabetical" : "Registration") << '\n';
                    stream.flush();
                    if (!stream) return false;
                }
                if (!::MoveFileExW(temporary.c_str(), path.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                    std::error_code ignored;
                    std::filesystem::remove(temporary, ignored);
                    return false;
                }
                return true;
            } catch (...) {
                return false;
            }
        }

        Api::Result __cdecl Apply(void* context) noexcept
        {
            if (context != &g_state) return Api::Result::InvalidArgument;
            std::scoped_lock lock{ g_state.mutex };
            if (!WritePreferences(g_state.draftPauseEntry,
                    g_state.draftHotkey, g_state.draftSort)) {
                return Api::Result::WriteFailure;
            }
            if (g_state.callbacks.setPauseMenuEntry) {
                g_state.callbacks.setPauseMenuEntry(g_state.draftPauseEntry);
            }
            if (g_state.callbacks.setRecoveryHotkey) {
                g_state.callbacks.setRecoveryHotkey(g_state.draftHotkey);
            }
            MenuApiHost::SetModuleSortMode(
                g_state.draftSort == ModuleSortMode::Alphabetical ?
                    MenuApiHost::ModuleSortMode::Alphabetical :
                    MenuApiHost::ModuleSortMode::Registration);
            g_state.committedPauseEntry = g_state.draftPauseEntry;
            g_state.committedHotkey = g_state.draftHotkey;
            g_state.committedSort = g_state.draftSort;
            return Api::Result::Ok;
        }

        void __cdecl Cancel(void* context) noexcept
        {
            if (context != &g_state) return;
            std::scoped_lock lock{ g_state.mutex };
            g_state.draftPauseEntry = g_state.committedPauseEntry;
            g_state.draftHotkey = g_state.committedHotkey;
            g_state.draftSort = g_state.committedSort;
        }
    }

    bool Register(const ProbeConfig& config,
        const std::filesystem::path& customConfigPath,
        RuntimeCallbacks callbacks) noexcept
    {
        try {
            const auto* api = AbsoluteControlPanel_QueryApi(Api::kAbiVersion);
            if (!api || api->structSize < Api::kApiV1RequestOpenPageSize ||
                !api->registerModule || !api->registerPage ||
                (api->capabilities & Api::kCapabilityRecordCollections) == 0 ||
                (api->capabilities & Api::kCapabilityLabeledChoices) == 0) {
                return false;
            }
            {
                std::scoped_lock lock{ g_state.mutex };
                if (g_state.registered) return true;
                g_state.committedPauseEntry = config.enablePauseMenuEntry;
                g_state.committedHotkey = config.openHotkey;
                g_state.committedSort = config.moduleSort;
                g_state.draftPauseEntry = config.enablePauseMenuEntry;
                g_state.draftHotkey = config.openHotkey;
                g_state.draftSort = config.moduleSort;
                g_state.customConfigPath = customConfigPath;
                g_state.callbacks = callbacks;
            }

            Api::ModuleDescriptorV1 module;
            Copy(module.moduleId, kModuleId);
            Copy(module.displayName, "Absolute Control");
            Copy(module.description,
                "Host menu behavior and read-only suite registry diagnostics.");
            const auto moduleResult = api->registerModule(&module);
            if (moduleResult != Api::Result::Ok &&
                moduleResult != Api::Result::Duplicate) return false;

            std::array<Api::PageDescriptorV1, 2> pages{};
            for (auto& page : pages) {
                Copy(page.moduleId, kModuleId);
                page.context = &g_state;
                page.readValue = &ReadValue;
                page.writeDraft = &WriteDraft;
                page.readChoiceOptions = &ReadChoices;
                page.readRecordItems = &ReadRecords;
            }
            Copy(pages[0].pageId, kSettingsPageId);
            Copy(pages[0].displayName, "Menu");
            Copy(pages[0].description,
                "Ordering and safe activation settings owned by Absolute Control.");
            pages[0].controls = kSettingsControls.data();
            pages[0].controlCount =
                static_cast<std::uint32_t>(kSettingsControls.size());
            pages[0].apply = &Apply;
            pages[0].cancel = &Cancel;

            Copy(pages[1].pageId, kRegistryPageId);
            Copy(pages[1].displayName, "Registry");
            Copy(pages[1].description,
                "Live view of registered modules and subscribers to host-owned buses.");
            pages[1].controls = kRegistryControls.data();
            pages[1].controlCount =
                static_cast<std::uint32_t>(kRegistryControls.size());

            for (const auto& page : pages) {
                if (api->registerPage(&page) != Api::Result::Ok) return false;
            }
            MenuApiHost::SetModuleSortMode(
                config.moduleSort == ModuleSortMode::Alphabetical ?
                    MenuApiHost::ModuleSortMode::Alphabetical :
                    MenuApiHost::ModuleSortMode::Registration);
            std::scoped_lock lock{ g_state.mutex };
            g_state.registered = true;
            return true;
        } catch (...) {
            return false;
        }
    }
}
