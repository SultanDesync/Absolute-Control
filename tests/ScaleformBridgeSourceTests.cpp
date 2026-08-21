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
        Read("src/ui/MenuMessaging.cpp") +
        Read("src/ui/PauseMenuIntegration.cpp");
    const auto evidence = Read("src/EvidenceLog.cpp");
    const auto actionScriptRoot = Read("interface/src/AbsoluteControlPanelMenu.as");
    const auto widgets = Read("interface/src/acp/ui/ControlWidgets.as");
    const auto selection = Read("interface/src/acp/ui/MenuSelectionState.as");
    const auto shell = Read("interface/src/acp/ui/MenuShellRenderer.as");
    const auto dirtyDecision =
        Read("interface/src/acp/ui/DirtyDecisionDialog.as");
    const auto bindingConflict =
        Read("interface/src/acp/ui/BindingConflictDialog.as");
    const auto actionConfirmation =
        Read("interface/src/acp/ui/ActionConfirmationDialog.as");
    const auto modalInput = Read("interface/src/acp/ui/ModalInputRouter.as");
    const auto pointer = Read("interface/src/acp/ui/PointerInteraction.as");
    const auto dispatcher = Read("interface/src/acp/ui/BridgeCommandDispatcher.as");
    const auto sliderWrites = Read("interface/src/acp/ui/SliderWriteCoordinator.as");
    const auto semanticRenderer =
        Read("interface/src/acp/ui/SemanticCompositionRenderer.as");
    const auto semanticAnchorInput =
        Read("interface/src/acp/ui/SemanticAnchorInputRouter.as");
    const auto livePatch =
        Read("interface/src/acp/ui/LivePatchCoordinator.as");
    const auto liveSurfaceInput =
        Read("interface/src/acp/ui/LiveSurfaceInputRouter.as");
    const auto layout = Read("interface/src/acp/ui/PanelLayout.as");
    const auto theme = Read("interface/src/acp/ui/PanelTheme.as");
    const auto vectorText = Read("interface/src/acp/ui/VectorTextRenderer.as");
    const auto actionScript = actionScriptRoot + widgets + selection + shell +
        dirtyDecision + bindingConflict + actionConfirmation + modalInput + pointer + dispatcher +
        sliderWrites + semanticRenderer + semanticAnchorInput + livePatch +
        liveSurfaceInput +
        layout + theme + vectorText;
    CHECK(!native.empty() && !evidence.empty() && !actionScriptRoot.empty());
    CHECK(!widgets.empty() && !selection.empty() && !shell.empty() && !pointer.empty() &&
        !dirtyDecision.empty() && !bindingConflict.empty() &&
        !actionConfirmation.empty() && !modalInput.empty() &&
        !dispatcher.empty() && !sliderWrites.empty() &&
        !semanticRenderer.empty() && !semanticAnchorInput.empty() &&
        !livePatch.empty() && !liveSurfaceInput.empty());
    CHECK(!layout.empty() && !theme.empty() && !vectorText.empty());
    CHECK(!Read("interface/src/acp/ui/FontAssets/Roboto-Regular.ttf").empty());
    CHECK(!Read("interface/src/acp/ui/FontAssets/Roboto-Bold.ttf").empty());
    CHECK(native.find("applyModel") != std::string::npos);
    CHECK(actionScript.find("applyModel") != std::string::npos);
    CHECK(native.find("a_params.argCount == 10") != std::string::npos);
    CHECK(native.find("a_params.argCount == 11") != std::string::npos);
    CHECK(native.find("command.expectedGeneration") != std::string::npos);
    CHECK(native.find("ReadExactGeneration") != std::string::npos);
    CHECK(native.find("MenuApiHost::ConsumeRefresh(\n                        refreshCursor, session.ActiveModuleId(),") !=
        std::string::npos);
    CHECK(native.find("session.AcknowledgePublishedGeneration(a_model.generation)") !=
        std::string::npos);
    CHECK(native.find("bridge_model_deferred") != std::string::npos);
    CHECK(native.find("bridge_model_flush") != std::string::npos);
    CHECK(native.find("DeferModel(session.Snapshot(), \"bridge-ready\")") !=
        std::string::npos);
    CHECK(native.find("MenuApiHost::ConsumeOpenRequest(request)") !=
        std::string::npos);
    CHECK(native.find("DeferModel(session.Dispatch(command), \"provider-open-route\")") !=
        std::string::npos);
    CHECK(native.find("tasks->AddTask([]") != std::string::npos);
    CHECK(native.find("RE::UI_MESSAGE_TYPE::kShow, \"provider-request\"") !=
        std::string::npos);
    CHECK(native.find("PublishModel(session.Snapshot())") == std::string::npos);
    CHECK(native.find("if (!modelPublicationActive)") != std::string::npos);
    CHECK(native.find("FlushDeferredModel();") != std::string::npos);
    CHECK(native.find("DeferModel(model, a_source);") != std::string::npos);
    CHECK(native.find("PublishInputModel(const MenuSession::Model& a_model)") !=
        std::string::npos);
    CHECK(native.find("DeferModel(a_model, \"native-input\")") != std::string::npos);
    CHECK(native.find(
        "pendingModel.reset();\n                    MenuApiHost::SetInputCaptureActive(false);\n"
        "                    Ui::QueueControlPanelMessage") != std::string::npos);
    CHECK(native.find("binding_capture_cancelled") != std::string::npos);
    CHECK(native.find("text_capture_cancelled") != std::string::npos);
    CHECK(native.find("source=bridge-teardown") != std::string::npos);
    CHECK(native.find("error=menu closing") != std::string::npos);
    CHECK(native.find("if (closing) {\n                    return true;") !=
        std::string::npos);
    CHECK(native.find("closing = true;\n                    pendingModel.reset();") !=
        std::string::npos);
    CHECK(native.find("model.SetMember(\"generation\"") != std::string::npos);
    CHECK(native.find("model.SetMember(\"modules\", modules)") != std::string::npos);
    CHECK(native.find("SerializeCompositionNode") != std::string::npos);
    CHECK(native.find("SerializeCompositionAssociation") != std::string::npos);
    CHECK(native.find("page.SetMember(\"compositionEnhanced\"") !=
        std::string::npos);
    CHECK(native.find("page.SetMember(\"compositionNodes\", compositionNodes)") !=
        std::string::npos);
    CHECK(native.find("FocusRegion::Anchors") != std::string::npos);
    CHECK(native.find("SetString(module, \"pageId\", source.firstPageId)") !=
        std::string::npos);
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
    CHECK(native.find("bridge_pointer_barrier_started") != std::string::npos);
    CHECK(native.find("bridge_pointer_release_observed") != std::string::npos);
    CHECK(native.find("bridge_pointer_armed") != std::string::npos);
    CHECK(native.find("closing || !menuShown || !pointerInputArmed") !=
        std::string::npos);
    CHECK(native.find("returnToPause = bridge.OnHidden();\n                }\n"
                      "                const auto result = RE::IMenu::ProcessMessage") !=
        std::string::npos);
    CHECK(native.find("bridge.OnShown();") != std::string::npos);
    CHECK(native.find("REL::ID(130625)") != std::string::npos);
    CHECK(native.find("REL::ID(130630)") != std::string::npos);
    CHECK(native.find("REL::ID(130634)") != std::string::npos);
    CHECK(native.find("REL::ID(130642)") != std::string::npos);
    CHECK(native.find("func(static_cast<sink_t*>(this), a_event, a_source)") !=
        std::string::npos);
    CHECK(native.find("SetReturnToPauseOnClose(openedFromPause)") !=
        std::string::npos);
    CHECK(native.find("control_panel_show_ignored") != std::string::npos);
    CHECK(native.find("compare_exchange_strong") != std::string::npos);
    CHECK(native.find("returnToPauseOnClose =\n"
                      "                        Ui::PauseMenuIntegration::"
                      "ConsumeReturnToPauseOnClose()") != std::string::npos);
    CHECK(native.find("const bool returnToPause = closing && returnToPauseOnClose") !=
        std::string::npos);
    CHECK(native.find("session.Teardown();") != std::string::npos);
    CHECK(native.find("CancelBindingCapture(\"Menu closed during input capture\")") ==
        std::string::npos);
    CHECK(native.find("control_panel_return_queued") != std::string::npos);
    CHECK(native.find("control_panel_return_revealed") != std::string::npos);
    CHECK(native.find(
        "\"PauseMenu\", RE::UI_MESSAGE_TYPE::kShow,\n"
        "                                \"control-panel-return-recovery\"") !=
        std::string::npos);
    CHECK(native.find("float Unk1A() override { return 0.0F; }") ==
        std::string::npos);
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
    CHECK(native.find("pause_entry_underlay_retained") != std::string::npos);
    CHECK(native.find("slop-pause-entry") == std::string::npos);
    CHECK(native.find("0x534C4F50") != std::string::npos);
    CHECK(native.find("DeviceType::kGamepad") != std::string::npos);
    CHECK(native.find("SFSE::InputMap::kGamepadButtonOffset_B") !=
        std::string::npos);
    CHECK(native.find("REX::W32::XINPUT_GAMEPAD_B") != std::string::npos);
    CHECK(native.find("native controller requested close") != std::string::npos);
    CHECK(native.find("source=controller") != std::string::npos);
    CHECK(actionScriptRoot.find("Keyboard.TAB") != std::string::npos);
    CHECK(shell.find("TAB ESC ") != std::string::npos);
    CHECK(shell.find("B \" : \"TAB ESC ") != std::string::npos);
    CHECK(native.find("source=native-keyboard") == std::string::npos);  // evidence uses format arguments
    CHECK(dispatcher.find("bridge.dispatch(1, command") != std::string::npos);
    CHECK(dispatcher.find("bridge.compound(1") != std::string::npos);
    CHECK(native.find("DispatchCompound") != std::string::npos);
    CHECK(native.find("SerializeLiveComponent") != std::string::npos);
    CHECK(native.find("PublishLivePatch") != std::string::npos);
    CHECK(native.find("session.RefreshLivePatch()") != std::string::npos);
    CHECK(native.find("std::chrono::milliseconds(100)") == std::string::npos);
    CHECK(actionScriptRoot.find("public function applyLiveComponents") !=
        std::string::npos);
    CHECK(livePatch.find("existing.samples.push") != std::string::npos);
    CHECK(shell.find("function refreshLive") != std::string::npos);
    CHECK(shell.find("LIVE_COLLAPSED:uint") != std::string::npos);
    CHECK(shell.find("LIVE_PINNED:uint") != std::string::npos);
    CHECK(native.find("ComponentKind::RangeMeter") != std::string::npos);
    CHECK(native.find("SetString(target, \"valueFormat\"") != std::string::npos);
    CHECK(native.find("ComponentKind::TelemetryPlot") != std::string::npos);
    CHECK(native.find("component.telemetryHistory.samples[") != std::string::npos);
    CHECK(native.find("target.SetMember(\"samples\", samples)") != std::string::npos);
    CHECK(native.find("descriptor.associations[associationIndex]") !=
        std::string::npos);
    CHECK(native.find("SetString(column, \"associatedControlId\"") !=
        std::string::npos);
    CHECK(native.find("SetString(item, \"recordId\", source.recordId)") !=
        std::string::npos);
    CHECK(native.find("a_target.SetMember(\"recordItems\", recordItems)") !=
        std::string::npos);
    CHECK(native.find("actionConfirmationActive") != std::string::npos);
    CHECK(shell.find("drawSegmentedGrid") != std::string::npos);
    CHECK(shell.find("drawRangeMeter") != std::string::npos);
    CHECK(shell.find("drawTelemetryPlot") != std::string::npos);
    CHECK(shell.find("LIVE_COMPONENT_LIMIT:int = 6") != std::string::npos);
    CHECK(shell.find("roleColor(uint(band.visualRole))") != std::string::npos);
    CHECK(shell.find("function rangeBandColor") != std::string::npos);
    CHECK(shell.find("PanelTheme.RANGE_CRUISE") != std::string::npos);
    CHECK(shell.find("legendChipWidth") != std::string::npos);
    CHECK(shell.find("card.graphics.drawRoundRect(legendX, height - 25") !=
        std::string::npos);
    CHECK(shell.find("DRAG COLOURED LANDMARKS") != std::string::npos);
    CHECK(shell.find("GREEN = SHAPED OUTPUT") != std::string::npos);
    CHECK(shell.find("THROTTLE ZONES ARE READ-ONLY HERE") !=
        std::string::npos);
    CHECK(shell.find("guidanceOpenThrottle") != std::string::npos);
    CHECK(pointer.find("function beginRange") != std::string::npos);
    CHECK(pointer.find("function rangeControlAt") != std::string::npos);
    CHECK(pointer.find("draggingRangeScale") != std::string::npos);
    CHECK(pointer.find("Number(marker.value) - value") != std::string::npos);
    CHECK(shell.find("uint(weightControl.kind) == 5 ? 2 : 1") !=
        std::string::npos);
    CHECK(shell.find("ControlWidgets.displayValue(control)") !=
        std::string::npos);
    CHECK(liveSurfaceInput.find("kind == \"rangeMeter\"") !=
        std::string::npos);
    CHECK(shell.find("sampleIndex / (sampleCount - 1)") != std::string::npos);
    CHECK(shell.find("GREEN: FIRST") != std::string::npos);
    CHECK(shell.find("CYAN OUTLINE: LIVE") != std::string::npos);
    CHECK(shell.find("HOLLOW > 1 > 2 > 3 > HOLLOW") != std::string::npos);
    CHECK(shell.find("operationKind\":3") != std::string::npos);
    CHECK(shell.find("quickLabels:Array = [\"+G\", \"+Y\", \"+R\", \"-\"]") !=
        std::string::npos);
    CHECK(shell.find("\"REQUESTS\"") == std::string::npos);
    CHECK(actionScriptRoot.find("sendCompound") != std::string::npos);
    CHECK(dispatcher.find("expectedGeneration") != std::string::npos);
    CHECK(actionScriptRoot.find("Event.ENTER_FRAME") != std::string::npos);
    CHECK(actionScriptRoot.find("model == null ? 0 : Number(model.generation)") !=
        std::string::npos);
    CHECK(actionScriptRoot.find("next.modules == null") != std::string::npos);
    CHECK(actionScriptRoot.find("refreshPollFrame") == std::string::npos);
    CHECK(actionScriptRoot.find("safe frame boundary for replacement models") !=
        std::string::npos);
    CHECK(sliderWrites.find("Number(write.expectedGeneration) != Number(model.generation)") !=
        std::string::npos);
    CHECK(sliderWrites.find("pending = null") != std::string::npos);
    CHECK(sliderWrites.find("Timer") == std::string::npos);
    // The root owns the native-facing bridge entry points; reusable layout,
    // interaction, selection, and widget policy belongs to named components.
    CHECK(actionScriptRoot.find("public function applyModel") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerDown") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerMove") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerUp") != std::string::npos);
    CHECK(actionScriptRoot.find("public function handlePointerWheel") != std::string::npos);
    CHECK(widgets.find("writeSliderFromPointer") != std::string::npos);
    CHECK(widgets.find("PanelLayout.SLIDER_TRACK_X") != std::string::npos);
    CHECK(widgets.find("PanelLayout.SLIDER_THUMB_RADIUS") != std::string::npos);
    CHECK(widgets.find("beginFill(PanelTheme.WIDGET_FILL, 1.0)") !=
        std::string::npos);
    CHECK(widgets.find("registerHit(sliderHit, \"slider\"") !=
        std::string::npos);
    CHECK(widgets.find("registerHit(widget, \"choice\"") != std::string::npos);
    CHECK(widgets.find("\"RUN\"") == std::string::npos);
    CHECK(widgets.find("Boolean(control.available) &&\n"
                       "                (uint(control.flags) & 1) == 0") !=
        std::string::npos);
    CHECK(selection.find("activeModulePages") != std::string::npos);
    CHECK(selection.find("semanticAnchors") != std::string::npos);
    CHECK(selection.find("nodeIsEffective") != std::string::npos);
    CHECK(semanticRenderer.find("ANCHORS_PER_ROW:int = 8") !=
        std::string::npos);
    CHECK(semanticRenderer.find("function drawFrame") != std::string::npos);
    CHECK(semanticRenderer.find("severityColor") != std::string::npos);
    CHECK(semanticRenderer.find("function liveComponentForControl") !=
        std::string::npos);
    CHECK(semanticRenderer.find("function rowHeight") != std::string::npos);
    CHECK(selection.find("Boolean(current.compositionEnhanced)") !=
        std::string::npos);
    CHECK(selection.find("preserveSemanticViewport") != std::string::npos);
    CHECK(selection.find("function pinnedControls") != std::string::npos);
    CHECK(shell.find("function drawPinnedContext") != std::string::npos);
    CHECK(liveSurfaceInput.find(
        "selection.selectedRow = int(target.index)") != std::string::npos);
    CHECK(liveSurfaceInput.find("selection.selectControlId(") !=
        std::string::npos);
    CHECK(shell.find("SemanticCompositionRenderer.EMBEDDED_PLOT_HEIGHT") !=
        std::string::npos);
    CHECK(shell.find("uint(current.compositionNodes[semanticIndex].kind) == 10") !=
        std::string::npos);
    CHECK(semanticAnchorInput.find("FOCUS_ANCHORS") != std::string::npos);
    CHECK(selection.find("model.modules[int(starts[i])].moduleId") !=
        std::string::npos);
    CHECK(shell.find("addModuleButton(model.modules[moduleIndex]") !=
        std::string::npos);
    CHECK(shell.find("drawFooter") != std::string::npos);
    CHECK(shell.find("drawChoicePopup") != std::string::npos);
    CHECK(shell.find("drawRecordCollectionPopup") != std::string::npos);
    CHECK(shell.find("Math.min(8, itemCount - recordFirstVisible)") !=
        std::string::npos);
    CHECK(widgets.find("drawRecordCollection") != std::string::npos);
    CHECK(shell.find("DirtyDecisionDialog.draw") != std::string::npos);
    CHECK(shell.find("ActionConfirmationDialog.draw") != std::string::npos);
    CHECK(dirtyDecision.find("SAVE CHANGES BEFORE CLOSING?") !=
        std::string::npos);
    CHECK(dirtyDecision.find("dirtyApply") != std::string::npos);
    CHECK(bindingConflict.find("BINDING ALREADY IN USE") != std::string::npos);
    CHECK(bindingConflict.find("bindingReassign") != std::string::npos);
    CHECK(modalInput.find("bindingCancel") != std::string::npos);
    CHECK(actionConfirmation.find("CONFIRM ACTION") != std::string::npos);
    CHECK(actionConfirmation.find("actionConfirm") != std::string::npos);
    CHECK(modalInput.find("actionCancel") != std::string::npos);
    CHECK(shell.find("addInlineRow") != std::string::npos);
    CHECK(shell.find("addGroupHeader") != std::string::npos);
    CHECK(shell.find("selectedGridControl") != std::string::npos);
    CHECK(actionScriptRoot.find("shell.selectedGridControl") !=
        std::string::npos);
    CHECK(selection.find("\"order-\" + colId") == std::string::npos);
    CHECK(shell.find("showBindingTooltip") != std::string::npos);
    CHECK(shell.find("hiddenBelow") != std::string::npos);
    CHECK(shell.find("action ? \"activate\" : \"select\"") !=
        std::string::npos);
    CHECK(pointer.find("beginSlider") != std::string::npos);
    CHECK(actionScriptRoot.find("SliderWriteCoordinator") != std::string::npos);
    CHECK(pointer.find("hitTestPoint(stageX, stageY, false)") != std::string::npos);
    CHECK(actionScript.find("sendSelectPage") != std::string::npos);
    CHECK(actionScript.find("A D COLUMN") == std::string::npos);
    CHECK(actionScript.find("addModuleButton") != std::string::npos);
    CHECK(actionScript.find("addPageTab") != std::string::npos);
    CHECK(actionScript.find("activeModulePages") != std::string::npos);
    CHECK(actionScript.find("INSTALLED MODULES") != std::string::npos);
    CHECK(actionScript.find("MOUSE_WHEEL") != std::string::npos);
    CHECK(actionScript.find("ChoiceInputRouter") != std::string::npos);
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
    CHECK(actionScript.find("ABSOLUTE CONTROL\"") != std::string::npos);
    CHECK(shell.find("VectorTextRenderer") != std::string::npos);
    CHECK(shell.find("PixelTextRenderer") == std::string::npos);
    CHECK(vectorText.find("Roboto-Regular.ttf") != std::string::npos);
    CHECK(vectorText.find("Roboto-Bold.ttf") != std::string::npos);
    CHECK(vectorText.find("text.embedFonts = true") != std::string::npos);
    CHECK(vectorText.find("$MainFont") == std::string::npos);
    CHECK(layout.find("SAFE_MARGIN:Number = 56") != std::string::npos);
    CHECK(layout.find("WORKSPACE_WIDTH:Number = 1448") != std::string::npos);
    CHECK(layout.find("VISIBLE_ROWS:int = 12") != std::string::npos);
    for (const auto forbidden : std::array{
             "toggle" "Feature", "increment" "Level", "decrement" "Level",
             "response" "Level" }) {
        CHECK(native.find(forbidden) == std::string::npos);
        CHECK(actionScript.find(forbidden) == std::string::npos);
    }
    return 0;
}
