package
{
    import acp.ui.ControlWidgets;
    import acp.ui.ChoiceInputRouter;
    import acp.ui.BridgeCommandDispatcher;
    import acp.ui.MenuSelectionState;
    import acp.ui.MenuShellRenderer;
    import acp.ui.PanelLayout;
    import acp.ui.PointerInteraction;
    import acp.ui.SliderWriteCoordinator;
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.ui.Keyboard;
    [SWF(width="1920", height="1080", frameRate="30", backgroundColor="#060E14")]
    public class AbsoluteControlPanelMenu extends MovieClip
    {
        // GameMenuBase installs this object and invokes the public methods below by name.
        public var BGSCodeObj:Object = {};
        public var ACPConstructed:Boolean = false;
        public var ACPDrawn:Boolean = false;

        private var model:Object;
        private var inputMode:String = "keyboard";
        private var selection:MenuSelectionState = new MenuSelectionState();
        private var pointer:PointerInteraction = new PointerInteraction();
        private var sliderWrites:SliderWriteCoordinator = new SliderWriteCoordinator();
        private var commandBridge:BridgeCommandDispatcher;
        private var shell:MenuShellRenderer;
        private var choiceInput:ChoiceInputRouter;

        public function AbsoluteControlPanelMenu()
        {
            super();
            shell = new MenuShellRenderer(this, pointer);
            choiceInput = new ChoiceInputRouter(shell);
            commandBridge = new BridgeCommandDispatcher(BGSCodeObj);
            ACPConstructed = true;
            if (stage != null) onAddedToStage(null);
            else addEventListener(Event.ADDED_TO_STAGE, onAddedToStage);
        }

        private function onAddedToStage(event:Event):void
        {
            removeEventListener(Event.ADDED_TO_STAGE, onAddedToStage);
            shell.drawPanel();
            ACPDrawn = true;
            if (stage != null) {
                stage.addEventListener(KeyboardEvent.KEY_DOWN, onKeyDown, true, 1000);
                stage.addEventListener(MouseEvent.MOUSE_WHEEL, onMouseWheel, true, 1000);
            }
            addEventListener(Event.ENTER_FRAME, onEnterFrame);
            redraw();
        }

        public function onCodeObjCreate():void
        {
            if (BGSCodeObj != null && BGSCodeObj.ready != null) BGSCodeObj.ready(1);
        }

        public function applyModel(next:Object):void
        {
            if (next == null || uint(next.schemaVersion) != 1 ||
                next.modules == null || next.pages == null) return;
            if (sliderWrites.isStale(next)) {
                sliderWrites.clear();
                pointer.clearSliderDrag();
            }
            model = next;
            selection.applyModel(model);
            redraw();
            if (BGSCodeObj != null && BGSCodeObj.modelApplied != null) {
                BGSCodeObj.modelApplied(Number(model.generation));
            }
        }

        private function onEnterFrame(event:Event):void
        {
            // Slider motion is sampled at the SWF frame rate: at most one provider
            // write and model rebuild is requested per visible UI frame, always for
            // the latest pointer position observed during that frame.
            sliderWrites.flush(
                model, selection.page(), commandBridge.dispatchFlat,
                pointer.clearSliderDrag);

            // modelApplied is the safe frame boundary for replacement models.
            // Native command callbacks only queue work; rebuilding the display
            // tree while pointer or keyboard dispatch is still unwinding can
            // invalidate Scaleform event targets. Native also polls coalesced
            // provider refreshes here and stays allocation-free when idle.
            if (BGSCodeObj != null && BGSCodeObj.modelApplied != null) {
                BGSCodeObj.modelApplied(
                    model == null ? 0 : Number(model.generation));
            }
        }

        public function setInputMode(mode:String):void
        {
            inputMode = mode == "controller" ? "controller" :
                (mode == "mouse" ? "mouse" : "keyboard");
            redraw();
        }

        public function handlePointerDown(stageX:Number, stageY:Number):Boolean
        {
            if (model == null) return false;
            if (inputMode != "mouse") {
                inputMode = "mouse";
                redraw();
            }
            sliderWrites.clear();
            pointer.clearSliderDrag();
            var target:Object = pointer.hit(stageX, stageY);
            if (target == null) return false;

            var kind:String = String(target.kind);
            if (kind == "disabled") return true;
            if (kind == "page") {
                selection.focusRegion = int(target.index);
                syncFocus();
                sendSelectPage(target.payload);
                return true;
            }
            if (kind == "select") {
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                selection.selectedRow = int(target.index);
                syncFocus();
                send("selectControl", target.payload, false, 0, 0);
                return true;
            }
            if (kind == "choice") {
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                selection.selectedRow = int(target.index);
                syncFocus();
            }
            if (choiceInput.handlePointer(kind, target, selection.page(), send)) {
                redraw();
                return true;
            }
            if (kind == "activate") {
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                selection.selectedRow = int(target.index);
                syncFocus();
                if (!choiceInput.activate(target.payload, selection.page(), send)) {
                    ControlWidgets.activate(target.payload, send);
                } else redraw();
                return true;
            }
            if (kind == "slider") {
                var current:Object = selection.page();
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                syncFocus();
                pointer.beginSlider(current, target.payload);
                sliderWrites.prepare(model, current);
                ControlWidgets.writeSliderFromPointer(target.view as Sprite,
                    target.payload, stageX, stageY, sliderWrites.queue);
                return true;
            }
            if (kind == "gridTier") {
                shell.setGridTier(String(target.payload.channelId),
                    String(target.payload.tierId));
                redraw();
                return true;
            }
            if (kind == "compound") {
                var operation:Object = target.payload;
                commandBridge.sendCompound(model, selection.page(),
                    operation.component, uint(operation.operationKind),
                    String(operation.columnId), String(operation.tierId),
                    uint(operation.count));
                return true;
            }
            if (kind == "action") {
                selection.focusRegion = PanelLayout.FOCUS_ACTIONS;
                selection.focusedAction = int(target.index);
                syncFocus();
                activateAction();
                return true;
            }
            return false;
        }

        public function handlePointerMove(stageX:Number, stageY:Number):Boolean
        {
            if (!pointer.isDraggingSlider || model == null) return false;
            if (inputMode != "mouse") inputMode = "mouse";
            sliderWrites.prepare(model, selection.page());
            return pointer.moveSlider(
                stageX, stageY, selection.page(), sliderWrites.queue);
        }

        public function handlePointerUp(stageX:Number, stageY:Number):Boolean
        {
            if (!pointer.isDraggingSlider) return false;
            var handled:Boolean = handlePointerMove(stageX, stageY);
            sliderWrites.flush(
                model, selection.page(), commandBridge.dispatchFlat,
                pointer.clearSliderDrag);
            pointer.clearSliderDrag();
            return handled;
        }

        // Retained for compatibility with early harnesses; native uses down/move/up.
        public function handlePointerClick(stageX:Number, stageY:Number):Boolean
        {
            var handled:Boolean = handlePointerDown(stageX, stageY);
            if (pointer.isDraggingSlider) handlePointerUp(stageX, stageY);
            return handled;
        }

        public function handlePointerWheel(stageX:Number, stageY:Number,
            direction:int):Boolean
        {
            if (model == null || model.pages == null || model.pages.length == 0 ||
                direction == 0) return false;
            inputMode = "mouse";
            direction = direction < 0 ? -1 : 1;
            if (choiceInput.handleWheel(direction)) {
                redraw();
                return true;
            }
            var region:String = pointer.wheelRegion(stageX, stageY);
            if (region == "modules") {
                navigateModule(direction);
                return true;
            }
            if (region == "tabs") {
                navigatePage(direction, PanelLayout.FOCUS_CONTROLS);
                return true;
            }
            var current:Object = selection.page();
            if (region == "rows" && current != null && current.controls.length > 0) {
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                selection.selectedRow = Math.max(0,
                    Math.min(current.controls.length - 1,
                        selection.selectedRow + direction));
                selection.normalize();
                syncFocus();
                send("selectControl", current.controls[selection.selectedRow], false, 0, 0);
                return true;
            }
            return false;
        }

        private function onMouseWheel(event:MouseEvent):void
        {
            if (handlePointerWheel(event.stageX, event.stageY,
                event.delta > 0 ? -1 : (event.delta < 0 ? 1 : 0))) {
                event.preventDefault();
                event.stopImmediatePropagation();
            }
        }

        private function onKeyDown(event:KeyboardEvent):void
        {
            if (model != null && (Boolean(model.bindingCaptureActive) ||
                Boolean(model.textCaptureActive))) {
                event.preventDefault();
                event.stopImmediatePropagation();
                return;
            }
            if (inputMode != "keyboard") {
                inputMode = "keyboard";
                redraw();
            }

            var current:Object = selection.page();
            var handled:Boolean = true;
            var choiceHandled:int = choiceInput.handleKey(event.keyCode, send);
            if (choiceHandled >= 0) {
                handled = choiceHandled > 0;
                if (handled) redraw();
            } else if (event.keyCode == Keyboard.ESCAPE || event.keyCode == Keyboard.TAB) {
                sendPage("close");
            }
            else if (event.keyCode == Keyboard.A || event.keyCode == Keyboard.LEFT) {
                moveFocusLeft();
            } else if (event.keyCode == Keyboard.D || event.keyCode == Keyboard.RIGHT) {
                moveFocusRight();
            } else if (event.keyCode == Keyboard.W || event.keyCode == Keyboard.UP) {
                navigateVertical(-1, current);
            } else if (event.keyCode == Keyboard.S || event.keyCode == Keyboard.DOWN) {
                navigateVertical(1, current);
            } else if (event.keyCode == Keyboard.Q || event.keyCode == Keyboard.PAGE_UP) {
                navigatePage(-1, PanelLayout.FOCUS_CONTROLS);
            } else if (event.keyCode == Keyboard.R || event.keyCode == Keyboard.PAGE_DOWN) {
                navigatePage(1, PanelLayout.FOCUS_CONTROLS);
            } else if (event.keyCode == Keyboard.E || event.keyCode == Keyboard.ENTER ||
                event.keyCode == Keyboard.SPACE) {
                if (selection.focusRegion == PanelLayout.FOCUS_ACTIONS) activateAction();
                else if (selection.focusRegion == PanelLayout.FOCUS_CONTROLS &&
                    current != null && current.controls.length > 0) {
                    var selected:Object = current.controls[selection.selectedRow];
                    if (!choiceInput.activate(selected, current, send)) {
                        ControlWidgets.activate(selected, send);
                    } else redraw();
                }
            } else if (event.keyCode == Keyboard.Z && current != null &&
                current.controls.length > 0 &&
                selection.focusRegion == PanelLayout.FOCUS_CONTROLS) {
                ControlWidgets.adjust(current.controls[selection.selectedRow], -1, send);
            } else if (event.keyCode == Keyboard.C && current != null &&
                current.controls.length > 0 &&
                selection.focusRegion == PanelLayout.FOCUS_CONTROLS) {
                ControlWidgets.adjust(current.controls[selection.selectedRow], 1, send);
            } else if (event.keyCode == Keyboard.F) sendPage("apply");
            else if (event.keyCode == Keyboard.X) sendPage("cancel");
            else handled = false;

            if (handled) {
                event.preventDefault();
                event.stopImmediatePropagation();
            }
        }

        private function moveFocusLeft():void
        {
            if (selection.focusRegion == PanelLayout.FOCUS_ACTIONS) {
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                syncFocus();
                redraw();
            } else if (selection.focusRegion == PanelLayout.FOCUS_CONTROLS) {
                selection.focusRegion = PanelLayout.FOCUS_MODULES;
                syncFocus();
                redraw();
            }
        }

        private function moveFocusRight():void
        {
            if (selection.focusRegion == PanelLayout.FOCUS_MODULES) {
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                syncFocus();
                redraw();
            } else if (selection.focusRegion == PanelLayout.FOCUS_CONTROLS) {
                selection.focusRegion = PanelLayout.FOCUS_ACTIONS;
                selection.focusedAction = 0;
                syncFocus();
                redraw();
            }
        }

        private function navigateVertical(direction:int, current:Object):void
        {
            if (selection.focusRegion == PanelLayout.FOCUS_MODULES) {
                navigateModule(direction);
            } else if (selection.focusRegion == PanelLayout.FOCUS_ACTIONS) {
                selection.focusedAction =
                    (selection.focusedAction + direction + 3) % 3;
                syncFocus();
                redraw();
            } else if (current != null && current.controls.length > 0) {
                selection.selectedRow =
                    (selection.selectedRow + direction + current.controls.length) %
                    current.controls.length;
                selection.normalize();
                send("selectControl", current.controls[selection.selectedRow], false, 0, 0);
            }
        }

        private function activateAction():void
        {
            if (selection.focusedAction == 0) {
                if (model != null && Boolean(model.dirty)) sendPage("apply");
                return;
            }
            if (selection.focusedAction == 1) {
                if (model != null && Boolean(model.dirty)) sendPage("cancel");
                return;
            }
            sendPage("close");
        }

        private function navigatePage(direction:int, nextFocus:int):void
        {
            var target:Object = selection.pageTarget(direction);
            if (target == null) return;
            selection.focusRegion = nextFocus;
            selection.focusedAction = 0;
            syncFocus();
            sendSelectPage(target);
        }

        private function navigateModule(direction:int):void
        {
            var target:Object = selection.moduleTarget(direction);
            if (target == null) return;
            selection.focusRegion = PanelLayout.FOCUS_MODULES;
            selection.focusedAction = 0;
            syncFocus();
            sendSelectPage(target);
        }

        private function redraw():void
        {
            shell.redraw(model, selection, inputMode);
        }

        private function syncFocus():void
        {
            if (BGSCodeObj != null && BGSCodeObj.focus != null) {
                BGSCodeObj.focus(uint(selection.focusRegion), uint(selection.focusedAction));
            }
        }

        private function sendPage(command:String):void
        {
            send(command, null, false, 0, 0);
        }

        private function sendSelectPage(target:Object):void
        {
            shell.closeChoice();
            send("selectPage", target, false, 0, 0, false);
        }

        private function send(command:String, control:Object, booleanValue:Boolean,
            integerValue:Number, floatValue:Number, controlIdentity:Boolean = true):void
        {
            commandBridge.send(model, selection.page(), command, control,
                booleanValue, integerValue, floatValue, controlIdentity);
        }
    }
}
