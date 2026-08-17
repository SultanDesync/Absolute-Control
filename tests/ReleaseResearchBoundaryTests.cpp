#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace
{
    std::string Read(std::string_view a_path)
    {
        auto root = std::filesystem::current_path();
        for (std::size_t depth{};
             depth < 8 && !std::filesystem::exists(root / a_path); ++depth) {
            root = root.parent_path();
        }
        std::ifstream input(root / a_path, std::ios::binary);
        std::string contents{ std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>() };
        std::erase(contents, '\r');
        return contents;
    }

    std::string_view Between(std::string_view a_text, std::string_view a_begin,
        std::string_view a_end)
    {
        const auto begin = a_text.find(a_begin);
        if (begin == std::string_view::npos) {
            return {};
        }
        const auto end = a_text.find(a_end, begin + a_begin.size());
        if (end == std::string_view::npos) {
            return {};
        }
        return a_text.substr(begin, end - begin);
    }
}

int main()
{
    const auto build = Read("xmake.lua");
    const auto native = Read("src/NativeMenuProbe.cpp");
    const auto menu = Read("src/ui/ControlPanelMenu.cpp");
    const auto bridge = Read("src/scaleform/ScaleformMenuBridge.cpp");
    const auto pause = Read("src/ui/PauseMenuIntegration.cpp");
    const auto input = Read("src/input/PlatformInputServices.cpp");
    const auto researchSupport = Read("src/research/ResearchSupport.cpp");
    CHECK(!build.empty() && !native.empty() && !menu.empty() && !bridge.empty());
    CHECK(!pause.empty() && !input.empty() && !researchSupport.empty());

    CHECK(build.find("add_files(\"src/**.cpp\")") == std::string::npos);
    const auto releaseSources = Between(
        build, "local release_sources = {", "local research_only_sources = {");
    CHECK(!releaseSources.empty());
    CHECK(releaseSources.find("src/NativeMenuProbe.cpp") != std::string::npos);
    CHECK(releaseSources.find("src/ui/ControlPanelMenu.cpp") != std::string::npos);
    CHECK(releaseSources.find("src/scaleform/ScaleformMenuBridge.cpp") != std::string::npos);
    CHECK(releaseSources.find("src/input/PlatformInputServices.cpp") != std::string::npos);
    CHECK(releaseSources.find("src/ui/PauseMenuIntegration.cpp") != std::string::npos);
    CHECK(releaseSources.find("src/research/ResearchSupport.cpp") == std::string::npos);
    CHECK(releaseSources.find("ResearchModule.cpp") == std::string::npos);
    CHECK(releaseSources.find("ResearchInputCapture.cpp") == std::string::npos);
    CHECK(releaseSources.find("LiveComponentsRegistry.cpp") != std::string::npos);

    const auto researchSources = Between(
        build, "local research_only_sources = {",
        "local function add_control_panel_core_sources");
    CHECK(researchSources.find("ResearchModule.cpp") != std::string::npos);
    CHECK(researchSources.find("ResearchInputCapture.cpp") != std::string::npos);
    CHECK(researchSources.find("LiveComponentsRegistry.cpp") == std::string::npos);
    CHECK(researchSources.find("src/research/ResearchSupport.cpp") != std::string::npos);

    const auto releaseTarget = Between(
        build, "target(\"AbsoluteControlPanel\"", "target(\"AbsoluteControlPanelResearchDev\"");
    CHECK(releaseTarget.find("set_default(true)") != std::string::npos);
    CHECK(releaseTarget.find("add_installfiles") != std::string::npos);
    CHECK(releaseTarget.find("LiveComponentsExperimentalAPI.h") != std::string::npos);
    CHECK(releaseTarget.find("dinput8") == std::string::npos);
    CHECK(releaseTarget.find("ACP_ENABLE_RESEARCH_TOOLS") == std::string::npos);
    CHECK(releaseTarget.find("New-BuildArtifactManifest.ps1") != std::string::npos);

    const auto researchTarget = Between(
        build, "target(\"AbsoluteControlPanelResearchDev\"", "target(\"probe_state_test\"");
    CHECK(researchTarget.find("set_default(false)") != std::string::npos);
    CHECK(researchTarget.find("ACP_ENABLE_RESEARCH_TOOLS=1") != std::string::npos);
    CHECK(researchTarget.find("add_installfiles") == std::string::npos);
    CHECK(researchTarget.find("dinput8") != std::string::npos);
    CHECK(researchTarget.find("New-BuildArtifactManifest.ps1") != std::string::npos);
    CHECK(researchTarget.find("\"-ArtifactRole\", \"research-dev\"") !=
        std::string::npos);

    const auto includeBoundary = Between(native,
        "#if defined(ACP_ENABLE_RESEARCH_TOOLS)\n#include \"ResearchModule.h\"",
        "#endif");
    CHECK(includeBoundary.find("ResearchModule.h") != std::string::npos);

    CHECK(researchSupport.find("class ForeignMenuMemberVisitor") != std::string::npos);
    CHECK(researchSupport.find("ProbeForeignMenuRoot") != std::string::npos);
    CHECK(researchSupport.find("VisitMembers") != std::string::npos);
    CHECK(researchSupport.find("::SendInput") != std::string::npos);
    CHECK(researchSupport.find("PollMailbox") != std::string::npos);
    CHECK(researchSupport.find("probe_pause_root") != std::string::npos);
    CHECK(researchSupport.find("StartExperiment") != std::string::npos);
    CHECK(researchSupport.find("watchdog_fired") != std::string::npos);
    CHECK(menu.find("::SendInput") == std::string::npos);
    CHECK(bridge.find("::SendInput") == std::string::npos);
    CHECK(pause.find("::SendInput") == std::string::npos);
    CHECK(input.find("::SendInput") == std::string::npos);

    const auto releaseStartup = Between(native,
        "EvidenceLog::Event(\"registration_succeeded\"", "ProbePhase Phase()");
    CHECK(releaseStartup.find("Input::StartOpenHotkey") != std::string::npos);
    CHECK(releaseStartup.find("Input::StartPointerPolling") != std::string::npos);
    CHECK(releaseStartup.find("PauseMenuIntegration::LogRegistration") != std::string::npos);
    CHECK(releaseStartup.find("ResearchSupport::Start") != std::string::npos);

    const auto ready = native.find("MenuApiHost::MarkRuntimeReady();");
    const auto retainedFactory = native.rfind("ControlPanelMenu::Register", ready);
    const auto researchRegistration = native.find(
        "ResearchModule::Register();", ready);
    CHECK(ready != std::string::npos);
    CHECK(retainedFactory != std::string::npos && retainedFactory < ready);
    CHECK(researchRegistration != std::string::npos && ready < researchRegistration);
    CHECK(native.find(
        "if (!config.enableRegistration) {\n            MenuApiHost::MarkRuntimeRejected();") !=
        std::string::npos);
    return 0;
}
