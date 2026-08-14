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
    std::string Read(const std::filesystem::path& a_path)
    {
        std::ifstream input(a_path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>() };
    }

    bool Contains(std::string_view a_binary, std::string_view a_text)
    {
        return a_binary.find(a_text) != std::string_view::npos;
    }
}

int main(int a_argc, char** a_argv)
{
    CHECK(a_argc > 0 && a_argv && a_argv[0]);
    const auto outputDirectory =
        std::filesystem::absolute(a_argv[0]).parent_path();
    const auto release = Read(outputDirectory / "AbsoluteControlPanel.dll");
    const auto research = Read(
        outputDirectory / "AbsoluteControlPanelResearchDev.dll");
    CHECK(!release.empty() && !research.empty());

    // The product retains every user-facing access and bridge lane.
    CHECK(Contains(release, "open_hotkey_registered"));
    CHECK(Contains(release, "pause_entry_integration_registered"));
    CHECK(Contains(release, "pause_entry_boundary_reached"));
    CHECK(Contains(release, "AbsoluteControlPanelMenu"));
    CHECK(Contains(release, "applyModel"));
    CHECK(Contains(release, "handlePointerDown"));

    // Research facilities and the experimental ABI are absent from the product.
    for (const auto forbidden : {
             "research_input_mailbox_registered",
             "foreign_menu_probe_queued",
             "watchdog_fired",
             "Control Panel Research",
             "AbsoluteControlPanelResearch.dummy.ini",
             "AbsoluteControlPanel_QueryLiveComponentsExperimental" }) {
        CHECK(!Contains(release, forbidden));
        CHECK(Contains(research, forbidden));
    }

    // The supported public ABI remains available in both compositions.
    CHECK(Contains(release, "AbsoluteControlPanel_QueryApi"));
    CHECK(Contains(research, "AbsoluteControlPanel_QueryApi"));
    return 0;
}
