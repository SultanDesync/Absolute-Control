#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace
{
    std::string Read(const char* a_path)
    {
        auto root = std::filesystem::current_path();
        for (std::size_t depth{}; depth < 8 && !std::filesystem::exists(root / a_path); ++depth) {
            root = root.parent_path();
        }
        std::ifstream input(root / a_path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }
}
int main()
{
    const auto native = Read("src/NativeMenuProbe.cpp");
    const auto actionScript = Read("interface/src/AbsoluteControlPanelMenu.as");
    CHECK(!native.empty() && !actionScript.empty());
    CHECK(native.find("applyModel") != std::string::npos);
    CHECK(actionScript.find("applyModel") != std::string::npos);
    CHECK(native.find("a_params.argCount != 10") != std::string::npos);
    CHECK(native.find("session.Dispatch(command)") != std::string::npos);
    CHECK(actionScript.find("BGSCodeObj.dispatch(1, command") != std::string::npos);
    CHECK(actionScript.find("sendSelectPage") != std::string::npos);
    CHECK(actionScript.find("String(model.error)") != std::string::npos);
    for (const auto forbidden : std::array{
             "toggle" "Feature", "increment" "Level", "decrement" "Level",
             "begin" "Binding" "Capture", "response" "Level" }) {
        CHECK(native.find(forbidden) == std::string::npos);
        CHECK(actionScript.find(forbidden) == std::string::npos);
    }
    return 0;
}
