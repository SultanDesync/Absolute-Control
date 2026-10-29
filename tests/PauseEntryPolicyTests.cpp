#include "ui/PauseEntryPolicy.h"
#include "ui/PauseViewportPolicy.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " #expression << '\n'; \
    return 1; } } while (false)

namespace
{
    std::string Read(const char* path)
    {
        auto root = std::filesystem::current_path();
        for (std::size_t depth{}; depth < 8 && !std::filesystem::exists(root / path); ++depth) {
            root = root.parent_path();
        }
        std::ifstream input(root / path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }
}

int main()
{
    using AbsoluteControlPanelResearch::Ui::PauseViewportPolicy::WidthDelta;
    CHECK(WidthDelta(180.0, 201.6).value() == 26.0);
    CHECK(WidthDelta(206.0, 201.6).value() == 0.0);
    CHECK(WidthDelta(180.0, 176.0).value() == 0.0);
    CHECK(!WidthDelta(180.0, 222.0));
    CHECK(!WidthDelta(0.0, 201.6));
    CHECK(!WidthDelta(180.0, -1.0));
    CHECK(!WidthDelta(180.0, std::numeric_limits<double>::infinity()));
    CHECK(!WidthDelta(std::numeric_limits<double>::quiet_NaN(), 201.6));
    using AbsoluteControlPanelResearch::Ui::PauseViewportPolicy::Target;
    CHECK(Target(13, 35.4, 428.0, 600.0).value() > 460.0);
    CHECK(Target(13, 35.4, 500.0, 600.0).value() == 500.0);
    CHECK(Target(128, 35.4, 428.0, 600.0).value() == 600.0);
    CHECK(Target(13, 35.4, 428.0, 400.0).value() == 428.0);
    const auto fitted = Target(13, 35.4, 428.0, 600.0).value();
    CHECK(Target(13, 35.4, fitted, 600.0).value() == fitted);
    CHECK(!Target(129, 35.4, 428.0, 600.0));
    CHECK(!Target(0, 35.4, 428.0, 600.0));
    CHECK(!Target(-1, 35.4, 428.0, 600.0));
    CHECK(!Target(13, 0.0, 428.0, 600.0));
    CHECK(!Target(13, -35.4, 428.0, 600.0));
    CHECK(!Target(13, std::numeric_limits<double>::infinity(), 428.0, 600.0));
    CHECK(!Target(13, 35.4, 0.0, 600.0));
    CHECK(!Target(13, 35.4, std::numeric_limits<double>::infinity(), 600.0));
    CHECK(!Target(13, 35.4, 428.0, 0.0));
    CHECK(!Target(13, 35.4, 428.0, std::numeric_limits<double>::infinity()));
    CHECK(!Target(13, 35.4, std::numeric_limits<double>::quiet_NaN(), 600.0));

    const auto presentation = Read("src/ui/PauseMenuPresentation.cpp");
    CHECK(!presentation.empty());
    CHECK(presentation.find("InitializeEntries") == std::string::npos);
    CHECK(presentation.find("SetMember(\"selectedIndex\"") == std::string::npos);
    CHECK(presentation.find("SetMember(\"scaleX\"") == std::string::npos);
    CHECK(presentation.find("SetMember(\"scaleY\"") == std::string::npos);
    CHECK(presentation.find("Invoke(\"Configure\"") == std::string::npos);
    CHECK(presentation.find("grow(row.fill,") == std::string::npos);
    CHECK(presentation.find("grow(row.fillShape,") != std::string::npos);
    CHECK(presentation.find("g_purple{ false }") != std::string::npos);
    CHECK(Read("include/ProbeConfig.h").find("pauseMenuPurpleEntry{ false }") != std::string::npos);
    CHECK(presentation.find("g_matchFont{ false }") != std::string::npos);
    CHECK(Read("include/ProbeConfig.h").find("pauseMenuMatchFont{ false }") != std::string::npos);
    CHECK(presentation.find("GetDataForEntry") != std::string::npos);
    CHECK(presentation.find("PauseEntryPolicy::kOwnerMarker") != std::string::npos);
    CHECK(presentation.find("PauseEntryPolicy::kAction") != std::string::npos);
    CHECK(presentation.find("textColor") != std::string::npos);
    CHECK(presentation.find(".swf") == std::string::npos);
    CHECK(presentation.find("interface/") == std::string::npos);
    const auto integration = Read("src/ui/PauseMenuIntegration.cpp");
    CHECK(integration.find("using PauseIntegration = EntryIntegration<false>") != std::string::npos);
    CHECK(integration.find("using MainIntegration = EntryIntegration<true>") != std::string::npos);
    CHECK(integration.find("AttachExistingMainMenu") != std::string::npos);
    CHECK(integration.find("MainPanelInteractive(a_params.movie)") != std::string::npos);
    CHECK(integration.find("g_rebuildBudget = 4") != std::string::npos);
    CHECK(integration.find("list.Invoke(\"GetDataForEntry\"") != std::string::npos);
    CHECK(integration.find("SetMember(\"selectedIndex\"") == std::string::npos);
    CHECK(integration.find("reason=host-not-ready") != std::string::npos);
    CHECK(integration.find("clip.Invoke(\"SetEntryText\"") != std::string::npos);
    CHECK(integration.find("field.SetMember(\"autoSize\", Value(\"none\"))") != std::string::npos);
    CHECK(integration.find("border.SetMember(") == std::string::npos);
    CHECK(integration.find("SetMember(\"scaleX\"") == std::string::npos);
    const auto mainSource = Read("src/Main.cpp");
    CHECK(mainSource.find("kPostPostLoad") < mainSource.find("OnEarlyMenus()"));
    const auto bootstrap = Read("src/NativeMenuProbe.cpp");
    const auto dataReady = bootstrap.find("void OnDataReady()");
    CHECK(bootstrap.find("InstallLifecycleHook(") < dataReady);
    CHECK(bootstrap.find("ControlPanelMenu::Register(") > dataReady);
    CHECK(bootstrap.find("MenuApiHost::MarkRuntimeReady()") > dataReady);
    CHECK(bootstrap.find("PauseMenuIntegration::MarkHostReady()") > bootstrap.find("ControlPanelMenu::Register("));

    using namespace AbsoluteControlPanelResearch::Ui::PauseEntryPolicy;
    const Identity ours{ kAction, true };
    const Identity osf{ 100u, false };
    const Identity unknown{ std::nullopt, false };
    const Identity collision{ kAction, false };
    const Identity malformed{ 100u, true };
    const Identity missingAction{ std::nullopt, true };

    CHECK(Inspect(std::array{ osf, unknown }) == Admission::Append);
    CHECK(Inspect(std::array{ osf, ours, unknown }) == Admission::AlreadyPresent);
    CHECK(Inspect(std::array{ ours, osf }) == Admission::AlreadyPresent);
    CHECK(Inspect(std::array{ osf, collision }) == Admission::Conflict);
    CHECK(Inspect(std::array{ collision, ours }) == Admission::Conflict);
    CHECK(Inspect(std::array{ ours, collision }) == Admission::Conflict);
    CHECK(Inspect(std::array{ ours, ours }) == Admission::Conflict);
    CHECK(Inspect(std::array{ malformed }) == Admission::Conflict);
    CHECK(Inspect(std::array{ missingAction }) == Admission::Conflict);

    CHECK(ParseAction(static_cast<double>(kAction)) == kAction);
    CHECK(ParseAction(100.0) == 100u);
    CHECK(ParseAction(0.0) == 0u);
    CHECK(ParseAction(4294967295.0) == 4294967295u);
    CHECK(!ParseAction(static_cast<double>(kAction) + 0.5));
    CHECK(!ParseAction(-1.0));
    CHECK(!ParseAction(4294967296.0));
    CHECK(!ParseAction(std::numeric_limits<double>::infinity()));
    CHECK(!ParseAction(std::numeric_limits<double>::quiet_NaN()));

    CHECK(Handles(kAction, ours, true));
    CHECK(!Handles(kAction, ours, false));
    CHECK(!Handles(kAction, collision, true));
    CHECK(!Handles(kAction, osf, true));
    CHECK(!Handles(100u, ours, true));
    CHECK(!Handles(std::nullopt, ours, true));
    CHECK(!Handles(kAction, malformed, true));
    CHECK(!Handles(kAction, missingAction, true));
    return 0;
}
