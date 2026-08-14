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
    const auto bootstrap = Read("src/NativeMenuProbe.cpp");
    const auto runtime = Read("src/runtime/RuntimeCompatibility.cpp");
    const auto pause = Read("src/ui/PauseMenuIntegration.cpp");
    const auto menu = Read("src/ui/ControlPanelMenu.cpp");
    const auto bridge = Read("src/scaleform/ScaleformMenuBridge.cpp");
    const auto semanticInput = Read("src/input/NativeMenuInputAdapter.cpp");
    const auto platformInput = Read("src/input/PlatformInputServices.cpp");
    const auto research = Read("src/research/ResearchSupport.cpp");
    const auto build = Read("xmake.lua");
    CHECK(!bootstrap.empty() && !runtime.empty() && !pause.empty() && !menu.empty());
    CHECK(!bridge.empty() && !semanticInput.empty() && !platformInput.empty());
    CHECK(!research.empty() && !build.empty());

    CHECK(bootstrap.find("Runtime::ValidateMenuRelocations") != std::string::npos);
    CHECK(bootstrap.find("PauseMenuIntegration::InstallLifecycleHook") != std::string::npos);
    CHECK(bootstrap.find("ControlPanelMenu::Register") != std::string::npos);
    CHECK(bootstrap.find("Input::StartOpenHotkey") != std::string::npos);
    CHECK(bootstrap.find("ResearchSupport::Start") != std::string::npos);
    CHECK(bootstrap.find("class AbsoluteControlPanelMenu") == std::string::npos);
    CHECK(bootstrap.find("::SendInput") == std::string::npos);
    CHECK(bootstrap.find("GetAsyncKeyState") == std::string::npos);
    CHECK(bootstrap.find("RegisterNativeFunction") == std::string::npos);
    CHECK(bootstrap.find("VerifiedRelocation") == std::string::npos);

    CHECK(runtime.find("kVerifiedRelocations") != std::string::npos);
    CHECK(pause.find("ActiveMenuInsertHook") != std::string::npos);
    CHECK(menu.find("class AbsoluteControlPanelMenu") != std::string::npos);
    CHECK(menu.find("Scaleform::MenuBridge bridge") != std::string::npos);
    CHECK(bridge.find("a_params.argCount == 10") != std::string::npos);
    CHECK(bridge.find("a_params.argCount == 11") != std::string::npos);
    CHECK(semanticInput.find("MenuInputRouter::Route") != std::string::npos);
    CHECK(platformInput.find("GetAsyncKeyState") != std::string::npos);
    CHECK(research.find("::SendInput") != std::string::npos);

    CHECK(build.find("src/runtime/RuntimeCompatibility.cpp") != std::string::npos);
    CHECK(build.find("src/ui/PauseMenuIntegration.cpp") != std::string::npos);
    CHECK(build.find("src/ui/ControlPanelMenu.cpp") != std::string::npos);
    CHECK(build.find("src/scaleform/ScaleformMenuBridge.cpp") != std::string::npos);
    CHECK(build.find("src/input/NativeMenuInputAdapter.cpp") != std::string::npos);
    CHECK(build.find("src/input/PlatformInputServices.cpp") != std::string::npos);
    return 0;
}
