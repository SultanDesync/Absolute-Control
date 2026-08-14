#include <array>
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
        for (std::size_t depth{}; depth < 8 && !std::filesystem::exists(root / a_path); ++depth) {
            root = root.parent_path();
        }
        std::ifstream input(root / a_path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }
}
int main()
{
    const auto native = Read("src/NativeMenuProbe.cpp") +
        Read("src/scaleform/ScaleformMenuBridge.cpp") +
        Read("src/input/NativeMenuInputAdapter.cpp") +
        Read("src/input/PlatformInputServices.cpp") +
        Read("src/ui/ControlPanelMenu.cpp") +
        Read("src/ui/PauseMenuIntegration.cpp");
    const auto evidence = Read("src/EvidenceLog.cpp");
    const auto actionScriptRoot = Read("interface/src/AbsoluteControlPanelMenu.as");
    const auto widgets = Read("interface/src/acp/ui/ControlWidgets.as");
    const auto selection = Read("interface/src/acp/ui/MenuSelectionState.as");
    const auto shell = Read("interface/src/acp/ui/MenuShellRenderer.as");
    const auto pointer = Read("interface/src/acp/ui/PointerInteraction.as");
    const auto dispatcher = Read("interface/src/acp/ui/BridgeCommandDispatcher.as");
    const auto sliderWrites = Read("interface/src/acp/ui/SliderWriteCoordinator.as");
    const auto actionScript = actionScriptRoot + widgets + selection + shell + pointer +
        dispatcher + sliderWrites;
    CHECK(!native.empty() && !evidence.empty() && !actionScriptRoot.empty());
    CHECK(!widgets.empty() && !selection.empty() && !shell.empty() && !pointer.empty() &&
        !dispatcher.empty() && !sliderWrites.empty());
    CHECK(native.find("applyModel") != std::string::npos);
    CHECK(actionScript.find("applyModel") != std::string::npos);
    CHECK(native.find("a_params.argCount == 10") != std::string::npos);
    CHECK(native.find("a_params.argCount == 11") != std::string::npos);
    CHECK(native.find("command.expectedGeneration") != std::string::npos);
    CHECK(native.find("ReadExactGeneration") != std::string::npos);
    CHECK(native.find("MenuApiHost::ConsumeRefresh(refreshCursor)") != std::string::npos);
    CHECK(native.find("model.SetMember(\"generation\"") != std::string::npos);
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
    CHECK(native.find("PauseMenuIntegration::LogRegistration") != std::string::npos);
    CHECK(native.find("queueing fail-closed menu hide") != std::string::npos);
    CHECK(native.find("watchdog close remains armed") == std::string::npos);
    CHECK(native.find("RE::UI_MESSAGE_TYPE::kHide, \"bridge_root_missing\"") !=
        std::string::npos);
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
    CHECK(dispatcher.find("bridge.dispatch(1, command") != std::string::npos);
    CHECK(dispatcher.find("expectedGeneration") != std::string::npos);
    CHECK(actionScriptRoot.find("Event.ENTER_FRAME") != std::string::npos);
    CHECK(actionScriptRoot.find("BGSCodeObj.modelApplied(Number(model.generation))") !=
        std::string::npos);
    CHECK(sliderWrites.find("Number(write.expectedGeneration) != Number(model.generation)") !=
        std::string::npos);
    CHECK(sliderWrites.find("pending = null") != std::string::npos);
    CHECK(sliderWrites.find("Timer") == std::string::npos);
    // The root owns the five native-facing bridge entry points; reusable layout,
    // interaction, selection, and widget policy belongs to named components.
    CHECK(actionScriptRoot.find("public function applyModel") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerDown") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerMove") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerUp") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerWheel") != std::string::npos);
    CHECK(widgets.find("writeSliderFromPointer") != std::string::npos);
    CHECK(selection.find("activeModulePages") != std::string::npos);
    CHECK(shell.find("drawFooter") != std::string::npos);
    CHECK(pointer.find("beginSlider") != std::string::npos);
    CHECK(actionScriptRoot.find("SliderWriteCoordinator") != std::string::npos);
    CHECK(pointer.find("hitTestPoint(stageX, stageY, false)") != std::string::npos);
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
    CHECK(shell.find("ControlWidgets.draw(row") != std::string::npos);
    CHECK(actionScript.find("drawFooter") != std::string::npos);
    CHECK(actionScript.find("drawHelp") != std::string::npos);
    CHECK(actionScript.find("drawRoundRect") != std::string::npos);
    CHECK(actionScript.find("moduleTitle") != std::string::npos);
    CHECK(actionScript.find("Z C ADJUST") == std::string::npos);
    CHECK(actionScript.find("String(model.error)") != std::string::npos);
    CHECK(actionScript.find("F APPLY") == std::string::npos);
    CHECK(actionScript.find("addButton(\"APPLY\"") == std::string::npos);
    CHECK(native.find("binding_capture_armed") != std::string::npos);
    CHECK(native.find("const bool awaitingRelease = a_source == \"native-keyboard\"") !=
        std::string::npos);
    CHECK(native.find("inputAdapter.BeginBindingCapture(awaitingRelease)") !=
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
