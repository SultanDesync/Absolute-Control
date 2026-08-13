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
    const auto evidence = Read("src/EvidenceLog.cpp");
    const auto actionScript = Read("interface/src/AbsoluteControlPanelMenu.as");
    CHECK(!native.empty() && !evidence.empty() && !actionScript.empty());
    CHECK(native.find("applyModel") != std::string::npos);
    CHECK(actionScript.find("applyModel") != std::string::npos);
    CHECK(native.find("a_params.argCount != 10") != std::string::npos);
    CHECK(native.find("session.Dispatch(command)") != std::string::npos);
    CHECK(native.find("MenuInputRouter::Route") != std::string::npos);
    CHECK(native.find("MenuInputRouter::RoutePointer") == std::string::npos);
    CHECK(native.find("\"handlePointerDown\"") != std::string::npos);
    CHECK(native.find("\"handlePointerMove\"") != std::string::npos);
    CHECK(native.find("\"handlePointerUp\"") != std::string::npos);
    CHECK(native.find("movePending") != std::string::npos);
    CHECK(native.find("kMouseWheelUpIdCode = 0x800") != std::string::npos);
    CHECK(native.find("kMouseWheelDownIdCode = 0x900") != std::string::npos);
    CHECK(native.find("\"handlePointerWheel\", &handled") != std::string::npos);
    CHECK(native.find("mouse_wheel_duplicate") != std::string::npos);
    CHECK(native.find("open_hotkey_registered") != std::string::npos);
    CHECK(native.find("GetAsyncKeyState") != std::string::npos);
    CHECK(native.find("VK_LBUTTON") != std::string::npos);
    CHECK(native.find("ProbeForeignMenuRoot") != std::string::npos);
    CHECK(native.find("VisitMembers") != std::string::npos);
    CHECK(native.find("probe_pause_root") != std::string::npos);
    CHECK(native.find("probe_main_root") != std::string::npos);
    CHECK(native.find("SchedulePauseMenuIntegration") != std::string::npos);
    CHECK(native.find("OnPauseMenuInserted(menu)") != std::string::npos);
    CHECK(native.find("pause_entry_boundary_reached") != std::string::npos);
    CHECK(native.find("pause_entry_advance_listener_installed") != std::string::npos);
    CHECK(native.find("pause_entry_advance_listener_reused") != std::string::npos);
    CHECK(native.find("_absoluteControlPanelAdvanceInstalled") != std::string::npos);
    CHECK(native.find("_absoluteControlPanelCaptureInstalled") != std::string::npos);
    CHECK(native.find("pause_entry_advance_tick") != std::string::npos);
    CHECK(native.find("pause_entry_advance_timeout") != std::string::npos);
    CHECK(native.find("mode=active-menu-boundary+scaleform-advance") !=
        std::string::npos);
    CHECK(native.find("taskPending") == std::string::npos);
    CHECK(native.find("ReadPauseMenuReadiness") == std::string::npos);
    CHECK(native.find("ScheduleResearchInputMailbox();") != std::string::npos);
    CHECK(native.find("research_input_mailbox_registered") != std::string::npos);
    CHECK(native.find("menu_input_dispatched") == std::string::npos);
    CHECK(evidence.find("std::filesystem::path{ \"Data\" }") != std::string::npos);
    CHECK(evidence.find("AbsoluteControlPanel.evidence.jsonl") != std::string::npos);
    CHECK(evidence.find("\\\"sequence\\\"") != std::string::npos);
    CHECK(evidence.find("\\\"thread_id\\\"") != std::string::npos);
    CHECK(evidence.find("\\\"monotonic_us\\\"") != std::string::npos);
    CHECK(native.find("stopImmediatePropagation") != std::string::npos);
    CHECK(native.find("0x534C4F50") != std::string::npos);
    CHECK(native.find("DeviceType::kGamepad") != std::string::npos);
    CHECK(native.find("source=native-keyboard") == std::string::npos);  // evidence uses format arguments
    CHECK(actionScript.find("BGSCodeObj.dispatch(1, command") != std::string::npos);
    CHECK(actionScript.find("sendSelectPage") != std::string::npos);
    CHECK(actionScript.find("A D COLUMN") == std::string::npos);
    CHECK(actionScript.find("addModuleButton") != std::string::npos);
    CHECK(actionScript.find("addPageTab") != std::string::npos);
    CHECK(actionScript.find("activeModulePages") != std::string::npos);
    CHECK(actionScript.find("MODS") != std::string::npos);
    CHECK(actionScript.find("MOUSE_WHEEL") != std::string::npos);
    CHECK(actionScript.find("handlePointerClick") != std::string::npos);
    CHECK(actionScript.find("public function handlePointerDown") != std::string::npos);
    CHECK(actionScript.find("public function handlePointerMove") != std::string::npos);
    CHECK(actionScript.find("public function handlePointerUp") != std::string::npos);
    CHECK(actionScript.find("draggingControlId") != std::string::npos);
    CHECK(actionScript.find("public function handlePointerWheel") != std::string::npos);
    CHECK(actionScript.find("hitTestPoint(stageX, stageY, false)") != std::string::npos);
    CHECK(actionScript.find("drawControlWidget") != std::string::npos);
    CHECK(actionScript.find("drawFooter") != std::string::npos);
    CHECK(actionScript.find("drawHelp") != std::string::npos);
    CHECK(actionScript.find("drawRoundRect") != std::string::npos);
    CHECK(actionScript.find("moduleTitle") != std::string::npos);
    CHECK(actionScript.find("Z C ADJUST") == std::string::npos);
    CHECK(actionScript.find("String(model.error)") != std::string::npos);
    CHECK(actionScript.find("F APPLY") == std::string::npos);
    CHECK(actionScript.find("addButton(\"APPLY\"") == std::string::npos);
    CHECK(native.find("binding_capture_armed") != std::string::npos);
    CHECK(native.find("captureAwaitingRelease = a_source == \"native-keyboard\"") !=
        std::string::npos);
    CHECK(native.find("IsCapturedModifierDown") != std::string::npos);
    CHECK(native.find("keyboard:0x{:02X};ctrl={};alt={};shift={}") != std::string::npos);
    CHECK(actionScript.find("beginBindingCapture") != std::string::npos);
    CHECK(actionScript.find("PRESS KEY OR CHORD") != std::string::npos);
    CHECK(actionScript.find("0xFF00FF") == std::string::npos);
    CHECK(actionScript.find("ABSOLUTE CONTROL PANEL") != std::string::npos);
    for (const auto forbidden : std::array{
             "toggle" "Feature", "increment" "Level", "decrement" "Level",
             "response" "Level" }) {
        CHECK(native.find(forbidden) == std::string::npos);
        CHECK(actionScript.find(forbidden) == std::string::npos);
    }
    return 0;
}
