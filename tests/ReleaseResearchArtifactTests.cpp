#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#define CHECK(expression)                                      \
    do {                                                       \
        if (!(expression)) {                                  \
            std::cerr << "CHECK failed at line " << __LINE__  \
                      << ": " #expression << '\n';           \
            return 1;                                         \
        }                                                      \
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

    // Research automation remains absent from the product. The bounded live /
    // compound ABI is now a product lane because Absolute Power consumes it.
    for (const auto forbidden : {
             "research_input_mailbox_registered",
             "foreign_menu_probe_queued",
             "watchdog_fired" }) {
        CHECK(!Contains(release, forbidden));
        CHECK(Contains(research, forbidden));
    }
    for (const auto retired : {
             "Control Panel Research",
             "AbsoluteControlPanelResearch.dummy.ini",
             "Representative Boolean setting owned by the research subscriber." }) {
        CHECK(!Contains(release, retired));
        CHECK(!Contains(research, retired));
    }
    for (const auto productSurface : {
             "Absolute Control",
             "registered-modules",
             "host-buses",
             "Keep plugin registration order or sort module names alphabetically. Absolute Control remains last." }) {
        CHECK(Contains(release, productSurface));
        CHECK(Contains(research, productSurface));
    }
    CHECK(Contains(release,
        "AbsoluteControlPanel_QueryLiveComponentsExperimental"));
    CHECK(Contains(research,
        "AbsoluteControlPanel_QueryLiveComponentsExperimental"));

    // C1 exports the independently negotiated semantic lane only after native
    // snapshots, bridge serialization, rendering, input, and fallback exist.
    CHECK(Contains(release, "AbsoluteControlPanel_QueryCompositionApi"));
    CHECK(Contains(research, "AbsoluteControlPanel_QueryCompositionApi"));

    // The supported public ABI remains available in both compositions.
    CHECK(Contains(release, "AbsoluteControlPanel_QueryApi"));
    CHECK(Contains(research, "AbsoluteControlPanel_QueryApi"));
    return 0;
}
