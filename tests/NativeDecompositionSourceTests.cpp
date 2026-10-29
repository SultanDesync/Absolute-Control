#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define CHECK(expression)                                         \
    do {                                                          \
        if (!(expression)) {                                      \
            std::cerr << "CHECK failed at line " << __LINE__      \
                      << ": " #expression << '\n';               \
            return 1;                                             \
        }                                                         \
    } while (false)

namespace
{
    std::string Read(const char* a_path)
    {
        auto root = std::filesystem::current_path();
        for (std::size_t depth{}; depth < 8 &&
             !std::filesystem::exists(root / a_path); ++depth) {
            root = root.parent_path();
        }
        std::ifstream input(root / a_path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>() };
    }
}

int main()
{
    const auto mainSource = Read("src/Main.cpp");
    const auto runtimeStart = mainSource.find("AbsoluteControl::Runtime::NativeBackend().Start");
    const auto menuStart = mainSource.find("NativeMenuProbe::OnEarlyMenus");
    CHECK(runtimeStart != std::string::npos && menuStart != std::string::npos && runtimeStart < menuStart);
    const auto backend = Read("src/runtime/BackendRuntime.cpp") + Read("src/runtime/DirectInputProvider.cpp");
    CHECK(backend.find("Scaleform") == std::string::npos);
    CHECK(backend.find("QueryInputBusApi") == std::string::npos);
    CHECK(backend.find("DirectInput8Create") != std::string::npos);
    CHECK(backend.find("SetCooperativeLevel") != std::string::npos);
    const auto bootstrap = Read("src/NativeMenuProbe.cpp");
    const auto config = Read("include/ProbeConfig.h");
    const auto productIni = Read("config/AbsoluteControlPanel.ini");
    const auto researchIni = Read("config/AbsoluteControlPanelResearch.ini");
    const auto deployProbe = Read("tools/research/deploy-probe.ps1");
    const auto runtime = Read("src/runtime/RuntimeCompatibility.cpp");
    const auto pause = Read("src/ui/PauseMenuIntegration.cpp") +
        Read("include/ui/PauseEntryPolicy.h");
    const auto menu = Read("src/ui/ControlPanelMenu.cpp");
    const auto menuAudio = Read("src/ui/MenuAudioIntegration.cpp");
    const auto bridge = Read("src/scaleform/ScaleformMenuBridge.cpp");
    const auto semanticInput = Read("src/input/NativeMenuInputAdapter.cpp");
    const auto platformInput = Read("src/input/PlatformInputServices.cpp");
    const auto osfCompatibility = Read("include/interop/OSFUICompatibility.h") +
        Read("src/interop/OSFUICompatibility.cpp");
    const auto research = Read("src/research/ResearchSupport.cpp");
    const auto build = Read("xmake.lua");
    CHECK(!bootstrap.empty() && !config.empty() && !productIni.empty() && !researchIni.empty());
    CHECK(!deployProbe.empty() && !runtime.empty() && !pause.empty() && !menu.empty());
    CHECK(!menuAudio.empty());
    CHECK(!bridge.empty() && !semanticInput.empty() && !platformInput.empty());
    CHECK(!osfCompatibility.empty());
    CHECK(!research.empty() && !build.empty());

    CHECK(bootstrap.find("Runtime::ValidateMenuRelocations") != std::string::npos);
    CHECK(bootstrap.find("PauseMenuIntegration::InstallLifecycleHook") != std::string::npos);
    CHECK(bootstrap.find("ControlPanelMenu::Register") != std::string::npos);
    CHECK(bootstrap.find("Input::StartOpenHotkey") != std::string::npos);
    CHECK(config.find("std::uint32_t openHotkey{ 0 }") != std::string::npos);
    CHECK(config.find("openHotkey{ 0x71 }") == std::string::npos);
    CHECK(productIni.find("OpenHotkey=0x00") != std::string::npos);
    CHECK(researchIni.find("OpenHotkey=0x00") != std::string::npos);
    CHECK(deployProbe.find("[uint32]$OpenHotkey = 0,") != std::string::npos);
    CHECK(bootstrap.find("ResearchSupport::Start") != std::string::npos);
    CHECK(bootstrap.find("class AbsoluteControlPanelMenu") == std::string::npos);
    CHECK(bootstrap.find("::SendInput") == std::string::npos);
    CHECK(bootstrap.find("GetAsyncKeyState") == std::string::npos);
    CHECK(bootstrap.find("RegisterNativeFunction") == std::string::npos);
    CHECK(bootstrap.find("VerifiedRelocation") == std::string::npos);

    CHECK(runtime.find("kVerifiedRelocations") != std::string::npos);
    CHECK(pause.find("ActiveMenuInsertHook") != std::string::npos);
    CHECK(pause.find("kLabel = \"ABSOLUTE CONTROL\"") != std::string::npos);
    CHECK(pause.find("EntryMemberCopyVisitor") != std::string::npos);
    CHECK(pause.find("ABSOLUTE CONTROL PANEL") == std::string::npos);
    CHECK(menu.find("class AbsoluteControlPanelMenu") != std::string::npos);
    CHECK(menu.find("PauseMenuAudioLease audioLease") != std::string::npos);
    CHECK(menuAudio.find("kPauseMenuAudioMode = 2") != std::string::npos);
    CHECK(menuAudio.find("PlayMenuSound") == std::string::npos);
    CHECK(menuAudio.find("menu_audio_lease_acquired") != std::string::npos);
    CHECK(menuAudio.find("menu_audio_lease_released") != std::string::npos);
    CHECK(runtime.find("\"MenuAudioMode::Acquire\", 82727, 0x012EE410") !=
        std::string::npos);
    CHECK(runtime.find("\"MenuAudioMode::Release\", 82728, 0x012EE4A0") !=
        std::string::npos);
    CHECK(menu.find("Scaleform::MenuBridge bridge") != std::string::npos);
    CHECK(bridge.find("a_params.argCount == 10") != std::string::npos);
    CHECK(bridge.find("a_params.argCount == 11") != std::string::npos);
    CHECK(semanticInput.find("MenuInputRouter::Route") != std::string::npos);
    CHECK(platformInput.find("GetAsyncKeyState") != std::string::npos);
    CHECK(osfCompatibility.find("OSFUI.dll") != std::string::npos);
    CHECK(osfCompatibility.find("OSFUI_FocusMenu") != std::string::npos);
    CHECK(osfCompatibility.find("GetProcAddress") == std::string::npos);
    const auto firstOsfOpenGuard = menu.find(
        "OSFUICompatibility::IsPresentationActive");
    CHECK(firstOsfOpenGuard != std::string::npos);
    CHECK(menu.find("OSFUICompatibility::IsPresentationActive",
              firstOsfOpenGuard + 1) != std::string::npos);
    CHECK(bootstrap.find("OSFUICompatibility::LogStartupState") !=
        std::string::npos);
    CHECK(research.find("::SendInput") != std::string::npos);

    CHECK(build.find("src/runtime/RuntimeCompatibility.cpp") != std::string::npos);
    CHECK(build.find("src/ui/PauseMenuIntegration.cpp") != std::string::npos);
    CHECK(build.find("src/ui/ControlPanelMenu.cpp") != std::string::npos);
    CHECK(build.find("src/ui/MenuAudioIntegration.cpp") != std::string::npos);
    CHECK(build.find("src/scaleform/ScaleformMenuBridge.cpp") != std::string::npos);
    CHECK(build.find("src/input/NativeMenuInputAdapter.cpp") != std::string::npos);
    CHECK(build.find("src/input/PlatformInputServices.cpp") != std::string::npos);
    CHECK(build.find("src/interop/OSFUICompatibility.cpp") != std::string::npos);
    return 0;
}
