package
{
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Point;
    import flash.ui.Keyboard;

    [SWF(width="1920", height="1080", frameRate="30", backgroundColor="#071018")]
    public class AbsoluteControlPanelMenu extends MovieClip
    {
        private static const GLYPHS:Object = {
            "A":[14,17,17,31,17,17,17], "B":[30,17,17,30,17,17,30], "C":[14,17,16,16,16,17,14],
            "D":[30,17,17,17,17,17,30], "E":[31,16,16,30,16,16,31], "F":[31,16,16,30,16,16,16],
            "G":[14,17,16,23,17,17,14], "H":[17,17,17,31,17,17,17], "I":[31,4,4,4,4,4,31],
            "J":[7,2,2,2,18,18,12], "K":[17,18,20,24,20,18,17], "L":[16,16,16,16,16,16,31],
            "M":[17,27,21,21,17,17,17], "N":[17,25,21,19,17,17], "O":[14,17,17,17,17,17,14],
            "P":[30,17,17,30,16,16,16], "Q":[14,17,17,17,21,18,13], "R":[30,17,17,30,20,18,17],
            "S":[15,16,16,14,1,1,30], "T":[31,4,4,4,4,4,4], "U":[17,17,17,17,17,17,14],
            "V":[17,17,17,17,17,10,4], "W":[17,17,17,17,21,27,17], "X":[17,17,10,4,10,17,17],
            "Y":[17,17,10,4,4,4,4], "Z":[31,1,2,4,8,16,31], "0":[14,17,19,21,25,17,14],
            "1":[4,12,4,4,4,4,14], "2":[14,17,1,2,4,8,31], "3":[30,1,1,14,1,1,30],
            "4":[2,6,10,18,31,2,2], "5":[31,16,16,30,1,1,30], "6":[14,16,16,30,17,17,14],
            "7":[31,1,2,4,8,8,8], "8":[14,17,17,14,17,17,14], "9":[14,17,17,15,1,1,14],
            "-":[0,0,0,31,0,0,0], "+":[0,4,4,31,4,4,0], ".":[0,0,0,0,0,12,12],
            ":":[0,12,12,0,12,12,0], " ":[0,0,0,0,0,0,0], "?":[14,17,1,2,4,0,4]
        };

        public var BGSCodeObj:Object = {};
        public var ACPConstructed:Boolean = false;
        public var ACPDrawn:Boolean = false;

        private var model:Object;
        private var activePageIndex:int = 0;
        private var firstVisibleModule:int = 0;
        private var firstVisibleTab:int = 0;
        private var selectedRow:int = 0;
        private var firstVisibleRow:int = 0;
        private var focusRegion:int = 1;
        private var focusedAction:int = 0;
        private var pageLayer:Sprite;
        private var tabLayer:Sprite;
        private var rowLayer:Sprite;
        private var helpLayer:Sprite;
        private var footerLayer:Sprite;
        private var statusField:Sprite;
        private var hitTargets:Array = [];
        private var inputMode:String = "keyboard";
        private var draggingSlider:Boolean = false;
        private var draggingModuleId:String = "";
        private var draggingPageId:String = "";
        private var draggingControlId:String = "";

        private static const SIDEBAR_X:Number = 220;
        private static const SIDEBAR_Y:Number = 250;
        private static const SIDEBAR_WIDTH:Number = 300;
        private static const MODULE_HEIGHT:Number = 46;
        private static const MODULE_GAP:Number = 6;
        private static const WORKSPACE_X:Number = 560;
        private static const TABS_Y:Number = 215;
        private static const ROWS_Y:Number = 315;
        private static const WORKSPACE_WIDTH:Number = 1130;
        private static const ROW_HEIGHT:Number = 52;
        private static const VISIBLE_ROWS:int = 10;

        public function AbsoluteControlPanelMenu()
        {
            super(); ACPConstructed = true;
            if (stage != null) onAddedToStage(null);
            else addEventListener(Event.ADDED_TO_STAGE, onAddedToStage);
        }

        private function onAddedToStage(event:Event):void
        {
            removeEventListener(Event.ADDED_TO_STAGE, onAddedToStage);
            drawPanel();
            if (stage != null) stage.addEventListener(KeyboardEvent.KEY_DOWN, onKeyDown, true, 1000);
            if (stage != null) stage.addEventListener(MouseEvent.MOUSE_WHEEL, onMouseWheel, true, 1000);
        }

        public function onCodeObjCreate():void
        {
            if (BGSCodeObj != null && BGSCodeObj.ready != null) BGSCodeObj.ready(1);
        }

        public function applyModel(next:Object):void
        {
            if (next == null || uint(next.schemaVersion) != 1 || next.pages == null) return;
            model = next;
            activePageIndex = Math.max(0, Math.min(int(model.activePage), model.pages.length - 1));
            selectedRow = Math.max(0, int(model.selectedControl));
            focusRegion = Math.max(0, Math.min(2, int(model.focusRegion)));
            focusedAction = Math.max(0, Math.min(2, int(model.focusedAction)));
            normalizeSelection(); redraw();
            if (BGSCodeObj != null && BGSCodeObj.modelApplied != null) BGSCodeObj.modelApplied(uint(model.revision));
        }

        private function drawPanel():void
        {
            ACPDrawn = true;
            graphics.beginFill(0x071018, 0.94); graphics.drawRect(0, 0, 1920, 1080); graphics.endFill();
            graphics.lineStyle(2, 0x66D9EF, 0.8); graphics.beginFill(0x10212E, 1.0); graphics.drawRect(170, 80, 1580, 920); graphics.endFill();
            addText("ABSOLUTE CONTROL PANEL", 220, 125, 34, 0xE8F7FF);
            statusField = addText("AWAITING MENU MODEL", 220, 180, 16, 0xFFD166);
            pageLayer = new Sprite(); pageLayer.x = SIDEBAR_X; pageLayer.y = SIDEBAR_Y; addChild(pageLayer);
            tabLayer = new Sprite(); tabLayer.x = WORKSPACE_X; tabLayer.y = TABS_Y; addChild(tabLayer);
            rowLayer = new Sprite(); rowLayer.x = WORKSPACE_X; rowLayer.y = ROWS_Y; addChild(rowLayer);
            helpLayer = new Sprite(); helpLayer.x = WORKSPACE_X; helpLayer.y = 850; addChild(helpLayer);
            footerLayer = new Sprite(); footerLayer.x = WORKSPACE_X; footerLayer.y = 935; addChild(footerLayer);
            addText("MODS", SIDEBAR_X, 218, 15, 0x86A2B3);
            redraw();
        }

        private function page():Object
        {
            return model != null && model.pages != null && activePageIndex < model.pages.length ? model.pages[activePageIndex] : null;
        }

        private function normalizeSelection():void
        {
            if (model != null && model.pages != null && model.pages.length > 0) {
                var starts:Array = modulePageStarts();
                var modulePosition:int = activeModulePosition(starts);
                if (modulePosition < firstVisibleModule) firstVisibleModule = modulePosition;
                if (modulePosition > firstVisibleModule + 12) firstVisibleModule = modulePosition - 12;
                var tabs:Array = activeModulePages();
                var tabPosition:int = activeTabPosition(tabs);
                if (tabPosition < firstVisibleTab) firstVisibleTab = tabPosition;
                if (tabPosition > firstVisibleTab + 5) firstVisibleTab = tabPosition - 5;
            }
            var current:Object = page();
            if (current == null || current.controls == null || current.controls.length == 0) { selectedRow = 0; firstVisibleRow = 0; return; }
            selectedRow = Math.max(0, Math.min(selectedRow, current.controls.length - 1));
            if (selectedRow < firstVisibleRow) firstVisibleRow = selectedRow;
            if (selectedRow > firstVisibleRow + VISIBLE_ROWS - 1) firstVisibleRow = selectedRow - VISIBLE_ROWS + 1;
        }

        private function redraw():void
        {
            if (statusField == null || pageLayer == null || tabLayer == null || rowLayer == null || helpLayer == null || footerLayer == null) return;
            while (rowLayer.numChildren > 0) rowLayer.removeChildAt(0);
            rowLayer.graphics.clear();
            while (pageLayer.numChildren > 0) pageLayer.removeChildAt(0);
            while (tabLayer.numChildren > 0) tabLayer.removeChildAt(0);
            while (helpLayer.numChildren > 0) helpLayer.removeChildAt(0);
            while (footerLayer.numChildren > 0) footerLayer.removeChildAt(0);
            hitTargets = [];
            var current:Object = page();
            var title:String = current == null ? "NO REGISTERED PAGES" : String(current.moduleTitle) + "  " + String(current.title);
            var error:String = model != null && String(model.error).length > 0 ? "  " + String(model.error) : "";
            statusField.graphics.clear(); drawText(statusField, title + (model != null && model.dirty ? "  DIRTY" : "") + error, 18, error.length > 0 ? 0xFF8A8A : 0xFFD166);
            if (current == null) return;
            var starts:Array = modulePageStarts();
            var visibleModules:int = Math.min(13, starts.length - firstVisibleModule);
            for (var moduleRow:int = 0; moduleRow < visibleModules; ++moduleRow) {
                var modulePageIndex:int = int(starts[firstVisibleModule + moduleRow]);
                addModuleButton(model.pages[modulePageIndex], modulePageIndex, moduleRow * (MODULE_HEIGHT + MODULE_GAP));
            }
            var tabs:Array = activeModulePages();
            var visibleTabs:int = Math.min(6, tabs.length - firstVisibleTab);
            for (var tab:int = 0; tab < visibleTabs; ++tab) {
                var tabPageIndex:int = int(tabs[firstVisibleTab + tab]);
                addPageTab(model.pages[tabPageIndex], tabPageIndex, tab * 178);
            }
            addTextTo(rowLayer, String(current.title), 0, -48, 21, 0xE8F7FF);
            var visible:int = Math.min(VISIBLE_ROWS, current.controls.length - firstVisibleRow);
            for (var i:int = 0; i < visible; ++i) addRow(current.controls[firstVisibleRow + i], firstVisibleRow + i, i * ROW_HEIGHT);
            drawScrollBar(current);
            drawHelp(current);
            drawFooter();
        }

        private function modulePageStarts():Array
        {
            var result:Array = [];
            if (model == null || model.pages == null) return result;
            for (var i:int = 0; i < model.pages.length; ++i) {
                var moduleId:String = String(model.pages[i].moduleId);
                var known:Boolean = false;
                for (var found:int = 0; found < result.length; ++found) {
                    if (String(model.pages[int(result[found])].moduleId) == moduleId) {
                        known = true; break;
                    }
                }
                if (!known) result.push(i);
            }
            return result;
        }

        private function activeModulePosition(starts:Array):int
        {
            for (var i:int = 0; i < starts.length; ++i) {
                if (String(model.pages[int(starts[i])].moduleId) == currentPageModule()) return i;
            }
            return 0;
        }

        private function activeModulePages():Array
        {
            var result:Array = [];
            if (model == null || model.pages == null) return result;
            var moduleId:String = currentPageModule();
            for (var i:int = 0; i < model.pages.length; ++i) {
                if (String(model.pages[i].moduleId) == moduleId) result.push(i);
            }
            return result;
        }

        private function activeTabPosition(tabs:Array):int
        {
            for (var i:int = 0; i < tabs.length; ++i) if (int(tabs[i]) == activePageIndex) return i;
            return 0;
        }

        private function addModuleButton(target:Object, index:int, yPosition:Number):void
        {
            var button:Sprite = new Sprite(); var selected:Boolean = String(target.moduleId) == currentPageModule(); var focused:Boolean = selected && focusRegion == 0;
            button.graphics.lineStyle(focused ? 2 : 1, focused ? 0xFFD166 : (selected ? 0x66D9EF : 0x365668)); button.graphics.beginFill(selected ? 0x21465A : 0x142B39);
            button.graphics.drawRect(0, 0, SIDEBAR_WIDTH, MODULE_HEIGHT); button.graphics.endFill(); button.y = yPosition; button.buttonMode = true;
            addTextTo(button, String(target.moduleTitle), 14, 14, 13, selected ? 0xFFFFFF : 0xB9CBD6); pageLayer.addChild(button);
            registerHit(button, "page", target, 0);
        }

        private function addPageTab(target:Object, index:int, xPosition:Number):void
        {
            var button:Sprite = new Sprite(); var selected:Boolean = index == activePageIndex;
            button.graphics.lineStyle(1, selected ? 0x66D9EF : 0x365668); button.graphics.beginFill(selected ? 0x21465A : 0x142B39);
            button.graphics.drawRect(0, 0, 168, 42); button.graphics.endFill(); button.x = xPosition; button.buttonMode = true;
            addTextTo(button, String(target.title), 12, 12, 13, selected ? 0xFFFFFF : 0xB9CBD6); tabLayer.addChild(button);
            registerHit(button, "page", target, 1);
        }

        private function addRow(control:Object, index:int, yPosition:Number):void
        {
            var row:Sprite = new Sprite(); var selected:Boolean = focusRegion == 1 && index == selectedRow;
            row.graphics.lineStyle(selected ? 2 : 1, selected ? 0xFFD166 : 0x294758, 1.0); row.graphics.beginFill(selected ? 0x1B3C4D : (index % 2 == 0 ? 0x122A38 : 0x102531), 1.0);
            row.graphics.drawRect(0, 0, WORKSPACE_WIDTH, ROW_HEIGHT - 4); row.graphics.endFill(); row.y = yPosition; row.buttonMode = true;
            var flags:String = (uint(control.flags) & 4 ? " ADVANCED" : "") + (uint(control.flags) & 2 ? " RESTART" : "");
            var capturing:Boolean = model != null && Boolean(model.bindingCaptureActive) &&
                String(model.captureModuleId) == String(currentPageModule()) &&
                String(model.capturePageId) == String(currentPageId()) &&
                String(model.captureControlId) == String(control.controlId);
            addTextTo(row, fit(String(control.label), 34), 14, 16, 15, Boolean(control.available) ? (selected ? 0xFFFFFF : 0xB9CBD6) : 0x627985);
            if (flags.length > 0) addTextTo(row, flags, 350, 18, 10, 0xD7A856);
            registerHit(row, "select", control, index);
            drawControlWidget(row, control, capturing);
            rowLayer.addChild(row);
        }

        private function drawControlWidget(row:Sprite, control:Object, capturing:Boolean):void
        {
            var kind:uint = uint(control.kind); var enabled:Boolean = Boolean(control.available); var widget:Sprite = new Sprite();
            widget.x = 480; widget.y = 8; widget.buttonMode = enabled;
            if (kind == 0) {
                var on:Boolean = Boolean(control.booleanValue);
                widget.graphics.lineStyle(1, enabled ? 0x66D9EF : 0x526B77); widget.graphics.beginFill(on ? 0x2A8792 : 0x233844);
                widget.graphics.drawRoundRect(535, 2, 88, 30, 15, 15); widget.graphics.endFill(); widget.graphics.beginFill(on ? 0xE8F7FF : 0x718A96); widget.graphics.drawCircle(on ? 607 : 551, 17, 11); widget.graphics.endFill();
                addTextTo(widget, on ? "ON" : "OFF", 470, 10, 11, enabled ? 0xE8F7FF : 0x718A96);
                registerHit(widget, "activate", control, 0);
            } else if (kind == 1 || kind == 2) {
                var minimum:Number = Number(control.minimum); var maximum:Number = Number(control.maximum);
                var value:Number = kind == 1 ? Number(control.integerValue) : Number(control.floatValue);
                var fraction:Number = maximum > minimum ? Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum))) : 0;
                widget.graphics.beginFill(0x294758); widget.graphics.drawRect(0, 14, 430, 6); widget.graphics.endFill();
                widget.graphics.beginFill(enabled ? 0x66D9EF : 0x526B77); widget.graphics.drawRect(0, 14, 430 * fraction, 6); widget.graphics.drawCircle(430 * fraction, 17, 8); widget.graphics.endFill();
                addTextTo(widget, displayValue(control), 470, 9, 13, enabled ? 0xE8F7FF : 0x718A96);
                registerHit(widget, "slider", control, 0);
            } else if (kind == 3) {
                widget.graphics.lineStyle(1, enabled ? 0x66D9EF : 0x526B77); widget.graphics.beginFill(0x173447); widget.graphics.drawRect(230, 0, 393, 34); widget.graphics.endFill();
                addTextTo(widget, fit(displayValue(control), 24), 246, 10, 12, enabled ? 0xE8F7FF : 0x718A96); addTextTo(widget, "V", 595, 10, 12, 0x86A2B3);
                registerHit(widget, "activate", control, 0);
            } else if (kind == 4) {
                widget.graphics.lineStyle(1, enabled ? 0x66D9EF : 0x526B77); widget.graphics.beginFill(0x173447); widget.graphics.drawRect(430, 0, 193, 34); widget.graphics.endFill();
                addTextTo(widget, "RUN", 502, 10, 12, enabled ? 0xE8F7FF : 0x718A96);
                registerHit(widget, "activate", control, 0);
            } else {
                widget.graphics.lineStyle(1, enabled ? 0x66D9EF : 0x526B77); widget.graphics.beginFill(capturing ? 0x594925 : 0x173447); widget.graphics.drawRect(145, 0, 478, 34); widget.graphics.endFill();
                addTextTo(widget, fit(capturing ? "PRESS KEY OR CHORD" : displayValue(control), 31), 160, 10, 12, enabled ? 0xE8F7FF : 0x718A96);
                registerHit(widget, "activate", control, 0);
            }
            row.addChild(widget);
        }

        private function drawHelp(current:Object):void
        {
            helpLayer.graphics.clear(); helpLayer.graphics.lineStyle(1, 0x365668); helpLayer.graphics.beginFill(0x0D202B); helpLayer.graphics.drawRect(0, 0, WORKSPACE_WIDTH, 70); helpLayer.graphics.endFill();
            var description:String = String(current.description);
            if (current.controls != null && current.controls.length > 0) description = String(current.controls[selectedRow].description);
            addTextTo(helpLayer, "HELP", 14, 12, 11, 0x86A2B3); addTextTo(helpLayer, fit(description, 105), 14, 38, 12, 0xB9CBD6);
        }

        private function drawScrollBar(current:Object):void
        {
            if (current.controls == null || current.controls.length <= VISIBLE_ROWS) return;
            var trackHeight:Number = VISIBLE_ROWS * ROW_HEIGHT - 4;
            var thumbHeight:Number = Math.max(42, trackHeight * VISIBLE_ROWS / current.controls.length);
            var travel:Number = trackHeight - thumbHeight;
            var maximumStart:Number = current.controls.length - VISIBLE_ROWS;
            var thumbY:Number = maximumStart > 0 ? travel * firstVisibleRow / maximumStart : 0;
            rowLayer.graphics.beginFill(0x294758); rowLayer.graphics.drawRect(WORKSPACE_WIDTH - 7, 0, 5, trackHeight); rowLayer.graphics.endFill();
            rowLayer.graphics.beginFill(0x66D9EF); rowLayer.graphics.drawRect(WORKSPACE_WIDTH - 8, thumbY, 7, thumbHeight); rowLayer.graphics.endFill();
        }

        private function drawFooter():void
        {
            footerLayer.graphics.clear(); footerLayer.graphics.lineStyle(1, 0x365668); footerLayer.graphics.moveTo(0, 0); footerLayer.graphics.lineTo(WORKSPACE_WIDTH, 0);
            var prefix:String = inputMode == "mouse" ? "" : (inputMode == "controller" ? "A " : "ENTER ");
            addFooterButton(prefix + "APPLY", 0, 0, Boolean(model.dirty));
            addFooterButton(prefix + "CANCEL", 185, 1, Boolean(model.dirty));
            addFooterButton((inputMode == "controller" ? "B " : "ESC ") + "CLOSE", 390, 2, true);
        }

        private function addFooterButton(label:String, xPosition:Number, actionIndex:int, enabled:Boolean):void
        {
            var button:Sprite = new Sprite(); var selected:Boolean = focusRegion == 2 && focusedAction == actionIndex;
            button.graphics.lineStyle(selected ? 2 : 1, selected ? 0xFFD166 : (enabled ? 0x66D9EF : 0x365668)); button.graphics.beginFill(selected ? 0x21465A : 0x142B39); button.graphics.drawRect(0, 12, 175, 38); button.graphics.endFill();
            button.x = xPosition; button.buttonMode = enabled; addTextTo(button, label, 14, 25, 12, enabled ? 0xE8F7FF : 0x627985); footerLayer.addChild(button);
            registerHit(button, enabled ? "action" : "disabled", null, actionIndex);
        }

        private function fit(value:String, maximum:int):String
        {
            return value.length <= maximum ? value : value.substr(0, Math.max(0, maximum - 3)) + "...";
        }

        private function displayValue(control:Object):String
        {
            if (uint(control.kind) == 0) return Boolean(control.booleanValue) ? "ON" : "OFF";
            if (uint(control.kind) == 1 || uint(control.kind) == 3) return String(int(control.integerValue));
            if (uint(control.kind) == 2) return String(Number(control.floatValue));
            if (uint(control.kind) == 4) return "RUN";
            return String(control.stringValue);
        }

        private function currentPageModule():String
        {
            var current:Object = page(); return current == null ? "" : String(current.moduleId);
        }

        private function currentPageId():String
        {
            var current:Object = page(); return current == null ? "" : String(current.pageId);
        }

        private function activate(control:Object):void
        {
            if (!Boolean(control.available)) return;
            if (uint(control.kind) == 0) send("write", control, !Boolean(control.booleanValue), 0, 0);
            else if (uint(control.kind) == 4) send("invoke", control, false, 0, 0);
            else if (uint(control.kind) == 5) send("beginBindingCapture", control, false, 0, 0);
            else if (uint(control.kind) == 1 || uint(control.kind) == 2 || uint(control.kind) == 3) adjust(control, 1);
        }

        private function adjust(control:Object, direction:int):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            if (uint(control.kind) == 1 || uint(control.kind) == 3) send("write", control, false, int(Math.max(Number(control.minimum), Math.min(Number(control.maximum), Number(control.integerValue) + direction * Math.max(1, Number(control.step))))), 0);
            else if (uint(control.kind) == 2) send("write", control, false, 0, Math.max(Number(control.minimum), Math.min(Number(control.maximum), Number(control.floatValue) + direction * Number(control.step))));
        }

        private function onKeyDown(event:KeyboardEvent):void
        {
            if (model != null && Boolean(model.bindingCaptureActive)) {
                event.preventDefault(); event.stopImmediatePropagation(); return;
            }
            if (inputMode != "keyboard") { inputMode = "keyboard"; redraw(); }
            var current:Object = page(); var handled:Boolean = true;
            if (event.keyCode == Keyboard.ESCAPE) sendPage("close");
            else if (event.keyCode == Keyboard.A || event.keyCode == Keyboard.LEFT) {
                if (focusRegion == 2) { focusRegion = 1; syncFocus(); redraw(); }
                else if (focusRegion == 1) { focusRegion = 0; syncFocus(); redraw(); }
            }
            else if (event.keyCode == Keyboard.D || event.keyCode == Keyboard.RIGHT) {
                if (focusRegion == 0) { focusRegion = 1; syncFocus(); redraw(); }
                else if (focusRegion == 1) { focusRegion = 2; focusedAction = 0; syncFocus(); redraw(); }
            }
            else if (event.keyCode == Keyboard.W || event.keyCode == Keyboard.UP) {
                if (focusRegion == 0) navigateModule(-1);
                else if (focusRegion == 2) { focusedAction = (focusedAction + 2) % 3; syncFocus(); redraw(); }
                else if (current != null && current.controls.length > 0) { selectedRow = (selectedRow + current.controls.length - 1) % current.controls.length; normalizeSelection(); send("selectControl", current.controls[selectedRow], false, 0, 0); }
            }
            else if (event.keyCode == Keyboard.S || event.keyCode == Keyboard.DOWN) {
                if (focusRegion == 0) navigateModule(1);
                else if (focusRegion == 2) { focusedAction = (focusedAction + 1) % 3; syncFocus(); redraw(); }
                else if (current != null && current.controls.length > 0) { selectedRow = (selectedRow + 1) % current.controls.length; normalizeSelection(); send("selectControl", current.controls[selectedRow], false, 0, 0); }
            }
            else if (event.keyCode == Keyboard.Q || event.keyCode == Keyboard.PAGE_UP) navigatePage(-1, 1);
            else if (event.keyCode == Keyboard.R || event.keyCode == Keyboard.PAGE_DOWN) navigatePage(1, 1);
            else if (event.keyCode == Keyboard.E || event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.SPACE) {
                if (focusRegion == 2) activateAction();
                else if (focusRegion == 1 && current != null && current.controls.length > 0) activate(current.controls[selectedRow]);
            }
            else if (event.keyCode == Keyboard.Z && current != null && current.controls.length > 0 && focusRegion == 1) adjust(current.controls[selectedRow], -1);
            else if (event.keyCode == Keyboard.C && current != null && current.controls.length > 0 && focusRegion == 1) adjust(current.controls[selectedRow], 1);
            else if (event.keyCode == Keyboard.F) sendPage("apply");
            else if (event.keyCode == Keyboard.X) sendPage("cancel");
            else handled = false;
            if (handled) { event.preventDefault(); event.stopImmediatePropagation(); }
        }

        private function activateAction():void
        {
            if (focusedAction == 0) { if (model != null && Boolean(model.dirty)) sendPage("apply"); return; }
            if (focusedAction == 1) { if (model != null && Boolean(model.dirty)) sendPage("cancel"); return; }
            sendPage("close");
        }

        public function setInputMode(mode:String):void
        {
            inputMode = mode == "controller" ? "controller" : (mode == "mouse" ? "mouse" : "keyboard");
            redraw();
        }

        private function registerHit(view:Sprite, kind:String, payload:Object, index:int):void
        {
            hitTargets.push({ "view":view, "kind":kind, "payload":payload, "index":index });
        }

        public function handlePointerDown(stageX:Number, stageY:Number):Boolean
        {
            if (model == null) return false;
            if (inputMode != "mouse") { inputMode = "mouse"; redraw(); }
            clearSliderDrag();
            for (var i:int = hitTargets.length - 1; i >= 0; --i) {
                var target:Object = hitTargets[i]; var view:Sprite = target.view as Sprite;
                if (view == null || !view.hitTestPoint(stageX, stageY, false)) continue;
                if (String(target.kind) == "disabled") return true;
                if (String(target.kind) == "page") { focusRegion = int(target.index) == 0 ? 0 : 1; syncFocus(); sendSelectPage(target.payload); return true; }
                if (String(target.kind) == "select") { focusRegion = 1; selectedRow = int(target.index); syncFocus(); send("selectControl", target.payload, false, 0, 0); return true; }
                if (String(target.kind) == "activate") { focusRegion = 1; syncFocus(); activate(target.payload); return true; }
                if (String(target.kind) == "slider") {
                    var current:Object = page();
                    focusRegion = 1; syncFocus();
                    draggingSlider = true;
                    draggingModuleId = current == null ? "" : String(current.moduleId);
                    draggingPageId = current == null ? "" : String(current.pageId);
                    draggingControlId = String(target.payload.controlId);
                    setSliderFromPointer(view, target.payload, stageX, stageY);
                    return true;
                }
                if (String(target.kind) == "action") { focusRegion = 2; focusedAction = int(target.index); syncFocus(); activateAction(); return true; }
            }
            return false;
        }

        public function handlePointerMove(stageX:Number, stageY:Number):Boolean
        {
            if (!draggingSlider || model == null) return false;
            if (inputMode != "mouse") inputMode = "mouse";
            var current:Object = page();
            if (current == null || String(current.moduleId) != draggingModuleId ||
                String(current.pageId) != draggingPageId) {
                clearSliderDrag(); return false;
            }
            for (var i:int = hitTargets.length - 1; i >= 0; --i) {
                var target:Object = hitTargets[i];
                if (String(target.kind) != "slider" || target.payload == null ||
                    String(target.payload.controlId) != draggingControlId) continue;
                var view:Sprite = target.view as Sprite;
                if (view == null) { clearSliderDrag(); return false; }
                setSliderFromPointer(view, target.payload, stageX, stageY);
                return true;
            }
            clearSliderDrag(); return false;
        }

        public function handlePointerUp(stageX:Number, stageY:Number):Boolean
        {
            if (!draggingSlider) return false;
            var handled:Boolean = handlePointerMove(stageX, stageY);
            clearSliderDrag();
            return handled;
        }

        public function handlePointerClick(stageX:Number, stageY:Number):Boolean
        {
            var handled:Boolean = handlePointerDown(stageX, stageY);
            if (draggingSlider) handlePointerUp(stageX, stageY);
            return handled;
        }

        private function clearSliderDrag():void
        {
            draggingSlider = false;
            draggingModuleId = "";
            draggingPageId = "";
            draggingControlId = "";
        }

        private function setSliderFromPointer(view:Sprite, control:Object, stageX:Number, stageY:Number):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            var local:Point = view.globalToLocal(new Point(stageX, stageY));
            var fraction:Number = Math.max(0, Math.min(1, local.x / 430));
            var minimum:Number = Number(control.minimum); var maximum:Number = Number(control.maximum); var raw:Number = minimum + fraction * (maximum - minimum); var step:Number = Math.max(0.000001, Number(control.step));
            var snapped:Number = minimum + Math.round((raw - minimum) / step) * step;
            if (uint(control.kind) == 1) {
                var integerValue:int = Math.round(Math.max(minimum, Math.min(maximum, snapped)));
                if (integerValue != int(control.integerValue)) send("write", control, false, integerValue, 0);
            } else {
                var floatValue:Number = Math.max(minimum, Math.min(maximum, snapped));
                if (Math.abs(floatValue - Number(control.floatValue)) > step * 0.0001)
                    send("write", control, false, 0, floatValue);
            }
        }

        private function onMouseWheel(event:MouseEvent):void
        {
            if (handlePointerWheel(event.stageX, event.stageY, event.delta > 0 ? -1 : (event.delta < 0 ? 1 : 0))) {
                event.preventDefault(); event.stopImmediatePropagation();
            }
        }

        public function handlePointerWheel(stageX:Number, stageY:Number, direction:int):Boolean
        {
            if (model == null || model.pages == null || model.pages.length == 0 || direction == 0) return false;
            inputMode = "mouse"; direction = direction < 0 ? -1 : 1;
            var current:Object = page();
            if (stageX >= SIDEBAR_X && stageX <= SIDEBAR_X + SIDEBAR_WIDTH && stageY >= SIDEBAR_Y && stageY < 935) {
                navigateModule(direction); return true;
            }
            if (stageX >= WORKSPACE_X && stageX <= WORKSPACE_X + WORKSPACE_WIDTH && stageY >= TABS_Y && stageY <= TABS_Y + 48) {
                navigatePage(direction, 1); return true;
            }
            if (stageX >= WORKSPACE_X && stageX <= WORKSPACE_X + WORKSPACE_WIDTH && stageY >= ROWS_Y && stageY < 835 && current != null && current.controls.length > 0) {
                focusRegion = 1; selectedRow = Math.max(0, Math.min(current.controls.length - 1, selectedRow + direction));
                normalizeSelection(); syncFocus(); send("selectControl", current.controls[selectedRow], false, 0, 0); return true;
            }
            return false;
        }

        private function syncFocus():void
        {
            if (BGSCodeObj != null && BGSCodeObj.focus != null) BGSCodeObj.focus(uint(focusRegion), uint(focusedAction));
        }

        private function sendPage(command:String):void { send(command, null, false, 0, 0); }
        private function sendSelectPage(target:Object):void { send("selectPage", target, false, 0, 0, false); }
        private function navigatePage(direction:int, nextFocus:int):void
        {
            if (model == null || model.pages.length == 0) return;
            var tabs:Array = activeModulePages(); if (tabs.length == 0) return;
            var tabPosition:int = activeTabPosition(tabs);
            var next:int = int(tabs[(tabPosition + direction + tabs.length) % tabs.length]);
            focusRegion = nextFocus; focusedAction = 0; syncFocus();
            sendSelectPage(model.pages[next]);
        }
        private function navigateModule(direction:int):void
        {
            if (model == null || model.pages.length == 0) return;
            var starts:Array = modulePageStarts(); if (starts.length == 0) return;
            var modulePosition:int = activeModulePosition(starts);
            var next:int = int(starts[(modulePosition + direction + starts.length) % starts.length]);
            focusRegion = 0; focusedAction = 0; syncFocus();
            sendSelectPage(model.pages[next]);
        }
        private function send(command:String, control:Object, booleanValue:Boolean, integerValue:Number, floatValue:Number, controlIdentity:Boolean = true):void
        {
            var current:Object = page(); if (BGSCodeObj == null || BGSCodeObj.dispatch == null || (current == null && command != "close")) return;
            var moduleId:String = ""; var pageId:String = ""; var controlId:String = ""; var valueKind:uint = 3;
            if (controlIdentity && current != null) {
                moduleId = String(current.moduleId); pageId = String(current.pageId);
                if (control != null) { controlId = String(control.controlId); valueKind = uint(control.valueKind); }
            } else if (control != null) {
                moduleId = String(control.moduleId); pageId = String(control.pageId);
            }
            BGSCodeObj.dispatch(1, command, moduleId, pageId, controlId, valueKind, booleanValue, integerValue, floatValue, "");
        }

        private function addText(value:String, xPosition:Number, yPosition:Number, size:Number, color:uint):Sprite { var field:Sprite = createText(value, size, color); field.x = xPosition; field.y = yPosition; addChild(field); return field; }
        private function addTextTo(parent:Sprite, value:String, xPosition:Number, yPosition:Number, size:Number, color:uint):void { var field:Sprite = createText(value, size, color); field.x = xPosition; field.y = yPosition; parent.addChild(field); }
        private function createText(value:String, size:Number, color:uint):Sprite { var field:Sprite = new Sprite(); drawText(field, value, size, color); field.mouseEnabled = false; field.mouseChildren = false; return field; }
        private function drawText(target:Sprite, value:String, size:Number, color:uint):void
        {
            var pixel:Number = Math.max(1, Math.floor(size / 7)); var cursorX:Number = 0; var cursorY:Number = 0; var normalized:String = value.toUpperCase(); target.graphics.beginFill(color, 1.0);
            for (var index:int = 0; index < normalized.length; ++index) { var character:String = normalized.charAt(index); if (character == "\n") { cursorX = 0; cursorY += pixel * 9; continue; } var rows:Array = GLYPHS[character] as Array; if (rows == null) rows = GLYPHS["?"] as Array; for (var row:int = 0; row < 7; ++row) for (var column:int = 0; column < 5; ++column) if ((int(rows[row]) & (1 << (4 - column))) != 0) target.graphics.drawRect(cursorX + column * pixel, cursorY + row * pixel, pixel, pixel); cursorX += pixel * 6; }
            target.graphics.endFill();
        }
    }
}
