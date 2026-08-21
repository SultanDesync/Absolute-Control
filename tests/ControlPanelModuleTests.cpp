#include "ControlPanelModule.h"

#include "AbsoluteControlPanelAPI.h"
#include "MenuApiHost.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef NDEBUG
#undef assert
#define assert(expression) \
    ((expression) ? static_cast<void>(0) : std::abort())
#endif

using namespace AbsoluteControlPanelResearch;
namespace Api = AbsoluteControlPanelApi;

namespace
{
    bool g_pauseEntry{};
    std::uint32_t g_hotkey{};

    void SetPauseEntry(bool enabled) noexcept { g_pauseEntry = enabled; }
    void SetHotkey(std::uint32_t hotkey) noexcept { g_hotkey = hotkey; }

    Api::Result __cdecl ReadExternal(void*, const char*,
        Api::ValueV1* value) noexcept
    {
        if (!value) return Api::Result::InvalidArgument;
        value->kind = Api::ValueKind::String;
        strcpy_s(value->stringValue, "available");
        return Api::Result::Ok;
    }

    void RegisterExternal(const Api::ApiV1& api, const char* moduleId,
        const char* displayName)
    {
        Api::ModuleDescriptorV1 module;
        strcpy_s(module.moduleId, moduleId);
        strcpy_s(module.displayName, displayName);
        assert(api.registerModule(&module) == Api::Result::Ok);

        Api::ControlDescriptorV1 control;
        control.kind = Api::ControlKind::TextInput;
        control.flags = Api::kControlReadOnly;
        strcpy_s(control.controlId, "status");
        strcpy_s(control.label, "Status");
        control.minimumValue = 0;
        control.maximumValue = 255;
        control.stepValue = 1;

        Api::PageDescriptorV1 page;
        strcpy_s(page.moduleId, moduleId);
        strcpy_s(page.pageId, "general");
        strcpy_s(page.displayName, "General");
        page.controlCount = 1;
        page.controls = &control;
        page.readValue = &ReadExternal;
        assert(api.registerPage(&page) == Api::Result::Ok);
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream stream{ path, std::ios::binary };
        return { std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>() };
    }
}

int main()
{
    MenuApiHost::MarkRuntimeReady();
    const auto configPath = std::filesystem::temp_directory_path() /
        "absolute-control-panel-module-test.ini";
    std::error_code ignored;
    std::filesystem::remove(configPath, ignored);

    ProbeConfig config;
    config.enablePauseMenuEntry = true;
    config.openHotkey = 0;
    config.moduleSort = ModuleSortMode::Registration;
    assert(ControlPanelModule::Register(config, configPath,
        { &SetPauseEntry, &SetHotkey }));

    const auto* api = AbsoluteControlPanel_QueryApi(Api::kAbiVersion);
    assert(api);
    RegisterExternal(*api, "test.zulu", "Zulu Module");
    RegisterExternal(*api, "test.alpha", "Alpha Module");

    auto catalog = MenuApiHost::SnapshotCatalog({}, {});
    assert(catalog.modules.size() == 3);
    assert(catalog.modules[0].moduleId == "test.zulu");
    assert(catalog.modules[1].moduleId == "test.alpha");
    assert(catalog.modules[2].moduleId == "absolute.control");

    const auto settings = MenuApiHost::FindPage("absolute.control", "menu");
    assert(settings && settings->controls.size() == 4);
    MenuApiHost::Transaction transaction;
    Api::ValueV1 value;
    value.kind = Api::ValueKind::Boolean;
    value.booleanValue = 0;
    assert(MenuApiHost::WriteDraft(*settings, "pause-menu-entry", value,
               transaction) == Api::Result::Rejected);
    assert(!transaction);

    value = {};
    value.kind = Api::ValueKind::Integer;
    value.integerValue = 0x71;
    assert(MenuApiHost::WriteDraft(*settings, "recovery-hotkey", value,
               transaction) == Api::Result::Ok);
    value = {};
    value.kind = Api::ValueKind::Boolean;
    value.booleanValue = 0;
    assert(MenuApiHost::WriteDraft(*settings, "pause-menu-entry", value,
               transaction) == Api::Result::Ok);
    value = {};
    value.kind = Api::ValueKind::Integer;
    value.integerValue = 1;
    assert(MenuApiHost::WriteDraft(*settings, "module-sort", value,
               transaction) == Api::Result::Ok);
    assert(MenuApiHost::Apply(*settings) == Api::Result::Ok);
    transaction.Reset();
    assert(!g_pauseEntry && g_hotkey == 0x71);
    const auto written = ReadFile(configPath);
    assert(written.find("EnablePauseMenuEntry=false") != std::string::npos);
    assert(written.find("OpenHotkey=0x71") != std::string::npos);
    assert(written.find("ModuleSort=Alphabetical") != std::string::npos);
    const auto reloaded = LoadProbeConfig(configPath, config);
    assert(!reloaded.enablePauseMenuEntry);
    assert(reloaded.openHotkey == 0x71);
    assert(reloaded.moduleSort == ModuleSortMode::Alphabetical);

    catalog = MenuApiHost::SnapshotCatalog({}, {});
    assert(catalog.modules[0].moduleId == "test.alpha");
    assert(catalog.modules[1].moduleId == "test.zulu");
    assert(catalog.modules[2].moduleId == "absolute.control");

    const auto registry = MenuApiHost::FindPage("absolute.control", "registry");
    assert(registry && registry->controls.size() == 7);
    std::vector<MenuApiHost::RecordItem> records;
    assert(MenuApiHost::ReadRecordItems(*registry, "registered-modules",
               records) == Api::Result::Ok);
    assert(records.size() == 3);
    assert(std::ranges::any_of(records, [](const auto& record) {
        return record.recordId == "absolute.control" &&
               record.summary.find("pages") != std::string::npos;
    }));
    assert(MenuApiHost::ReadRecordItems(*registry, "host-buses", records) ==
           Api::Result::Ok);
    assert(records.size() == 3);
    assert(records[0].recordId == "configuration-pages");
    assert(records[1].recordId == "live-components");
    assert(records[2].recordId == "semantic-composition");

    std::filesystem::remove(configPath, ignored);
    return 0;
}
