package
{
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
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
        private var selectedRow:int = 0;
        private var firstVisibleRow:int = 0;
        private var pageLayer:Sprite;
        private var rowLayer:Sprite;
        private var statusField:Sprite;

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
            normalizeSelection(); redraw();
            if (BGSCodeObj != null && BGSCodeObj.modelApplied != null) BGSCodeObj.modelApplied(uint(model.revision));
        }

        private function drawPanel():void
        {
            ACPDrawn = true;
            graphics.beginFill(0x071018, 0.94); graphics.drawRect(0, 0, 1920, 1080); graphics.endFill();
            graphics.lineStyle(2, 0x66D9EF, 0.8); graphics.beginFill(0x10212E, 1.0); graphics.drawRect(250, 100, 1420, 870); graphics.endFill();
            graphics.lineStyle(); graphics.beginFill(0xFF00FF, 1.0); graphics.drawRect(1540, 130, 96, 96); graphics.endFill();
            addText("STARFIELD LOCAL OPTIONS PANEL", 320, 150, 38, 0xE8F7FF);
            statusField = addText("AWAITING MENU MODEL", 320, 210, 18, 0xFFD166);
            pageLayer = new Sprite(); pageLayer.x = 320; pageLayer.y = 270; addChild(pageLayer);
            rowLayer = new Sprite(); rowLayer.x = 320; rowLayer.y = 370; addChild(rowLayer);
            addText("Q R PAGE   W S SELECT   E ACTIVATE   A D ADJUST   ESC CLOSE   MOUSE ENABLED", 320, 900, 18, 0x86A2B3);
            redraw();
        }

        private function page():Object
        {
            return model != null && model.pages != null && activePageIndex < model.pages.length ? model.pages[activePageIndex] : null;
        }

        private function normalizeSelection():void
        {
            var current:Object = page();
            if (current == null || current.controls == null || current.controls.length == 0) { selectedRow = 0; firstVisibleRow = 0; return; }
            selectedRow = Math.max(0, Math.min(selectedRow, current.controls.length - 1));
            if (selectedRow < firstVisibleRow) firstVisibleRow = selectedRow;
            if (selectedRow > firstVisibleRow + 4) firstVisibleRow = selectedRow - 4;
        }

        private function redraw():void
        {
            if (statusField == null || rowLayer == null) return;
            while (rowLayer.numChildren > 0) rowLayer.removeChildAt(0);
            while (pageLayer.numChildren > 0) pageLayer.removeChildAt(0);
            var current:Object = page();
            var title:String = current == null ? "NO REGISTERED PAGES" : String(current.title);
            var error:String = model != null && String(model.error).length > 0 ? "  " + String(model.error) : "";
            statusField.graphics.clear(); drawText(statusField, title + (model != null && model.dirty ? "  DIRTY" : "") + error, 18, error.length > 0 ? 0xFF8A8A : 0xFFD166);
            if (current == null) return;
            for (var pageIndex:int = 0; pageIndex < model.pages.length; ++pageIndex) addPageButton(model.pages[pageIndex], pageIndex);
            addTextTo(rowLayer, String(current.description), 0, -55, 16, 0x86A2B3);
            var visible:int = Math.min(5, current.controls.length - firstVisibleRow);
            for (var i:int = 0; i < visible; ++i) addRow(current.controls[firstVisibleRow + i], firstVisibleRow + i, i * 105);
            addButton("APPLY", 940, 0, onApply); addButton("CANCEL", 940, 90, onCancel); addButton("CLOSE", 940, 180, onClose);
        }

        private function addPageButton(target:Object, index:int):void
        {
            var button:Sprite = new Sprite(); var selected:Boolean = index == activePageIndex;
            button.graphics.lineStyle(2, selected ? 0xFFD166 : 0x467A91); button.graphics.beginFill(selected ? 0x21465A : 0x142B39);
            button.graphics.drawRect(0, 0, 220, 52); button.graphics.endFill(); button.x = index * 235; button.buttonMode = true;
            button.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { sendSelectPage(target); });
            addTextTo(button, String(target.title), 12, 16, 14, selected ? 0xFFFFFF : 0xB9CBD6); pageLayer.addChild(button);
        }

        private function addRow(control:Object, index:int, yPosition:Number):void
        {
            var row:Sprite = new Sprite(); var selected:Boolean = index == selectedRow;
            row.graphics.lineStyle(3, selected ? 0xFFD166 : 0x467A91, 1.0); row.graphics.beginFill(selected ? 0x21465A : 0x142B39, 1.0);
            row.graphics.drawRect(0, 0, 900, 88); row.graphics.endFill(); row.y = yPosition; row.buttonMode = true;
            row.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { selectedRow = index; normalizeSelection(); activate(control); });
            var flags:String = (uint(control.flags) & 4 ? " ADVANCED" : "") + (uint(control.flags) & 2 ? " RESTART" : "");
            var unsupported:Boolean = uint(control.kind) == 3 || uint(control.kind) == 5;
            var text:String = String(control.label) + "  " + displayValue(control) + flags + (unsupported ? " UNSUPPORTED" : "") + (Boolean(control.available) ? "" : " UNAVAILABLE");
            addTextTo(row, text, 22, 16, 18, selected ? 0xFFFFFF : 0xB9CBD6); rowLayer.addChild(row);
        }

        private function displayValue(control:Object):String
        {
            if (uint(control.kind) == 0) return Boolean(control.booleanValue) ? "ON" : "OFF";
            if (uint(control.kind) == 1) return String(int(control.integerValue));
            if (uint(control.kind) == 2) return String(Number(control.floatValue));
            if (uint(control.kind) == 4) return "RUN";
            return String(control.stringValue);
        }

        private function activate(control:Object):void
        {
            if (!Boolean(control.available)) return;
            if (uint(control.kind) == 0) send("write", control, !Boolean(control.booleanValue), 0, 0);
            else if (uint(control.kind) == 4) send("invoke", control, false, 0, 0);
            else if (uint(control.kind) == 1 || uint(control.kind) == 2) adjust(control, 1);
        }

        private function adjust(control:Object, direction:int):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            if (uint(control.kind) == 1) send("write", control, false, int(Math.max(Number(control.minimum), Math.min(Number(control.maximum), Number(control.integerValue) + direction * Number(control.step)))), 0);
            else if (uint(control.kind) == 2) send("write", control, false, 0, Math.max(Number(control.minimum), Math.min(Number(control.maximum), Number(control.floatValue) + direction * Number(control.step))));
        }

        private function onKeyDown(event:KeyboardEvent):void
        {
            var current:Object = page(); var handled:Boolean = true;
            if (event.keyCode == Keyboard.ESCAPE) sendPage("close");
            else if (event.keyCode == Keyboard.W || event.keyCode == Keyboard.UP) { selectedRow--; normalizeSelection(); redraw(); }
            else if (event.keyCode == Keyboard.S || event.keyCode == Keyboard.DOWN) { selectedRow++; normalizeSelection(); redraw(); }
            else if (event.keyCode == Keyboard.Q || event.keyCode == Keyboard.PAGE_UP) navigatePage(-1);
            else if (event.keyCode == Keyboard.R || event.keyCode == Keyboard.PAGE_DOWN) navigatePage(1);
            else if ((event.keyCode == Keyboard.E || event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.SPACE) && current != null && current.controls.length > 0) activate(current.controls[selectedRow]);
            else if ((event.keyCode == Keyboard.A || event.keyCode == Keyboard.LEFT) && current != null && current.controls.length > 0) adjust(current.controls[selectedRow], -1);
            else if ((event.keyCode == Keyboard.D || event.keyCode == Keyboard.RIGHT) && current != null && current.controls.length > 0) adjust(current.controls[selectedRow], 1);
            else handled = false;
            if (handled) { event.preventDefault(); event.stopImmediatePropagation(); }
        }

        private function addButton(label:String, xPosition:Number, yPosition:Number, callback:Function):void
        {
            var button:Sprite = new Sprite(); button.graphics.lineStyle(2, 0x66D9EF); button.graphics.beginFill(0x173447); button.graphics.drawRect(0, 0, 320, 68); button.graphics.endFill();
            button.x = xPosition; button.y = yPosition; button.buttonMode = true; button.addEventListener(MouseEvent.CLICK, callback); addTextTo(button, label, 75, 20, 20, 0xFFFFFF); rowLayer.addChild(button);
        }
        private function onApply(event:MouseEvent):void { sendPage("apply"); }
        private function onCancel(event:MouseEvent):void { sendPage("cancel"); }
        private function onClose(event:MouseEvent):void { sendPage("close"); }

        private function sendPage(command:String):void { send(command, null, false, 0, 0); }
        private function sendSelectPage(target:Object):void { send("selectPage", target, false, 0, 0, false); }
        private function navigatePage(direction:int):void
        {
            if (model == null || model.pages.length == 0) return;
            var next:int = (activePageIndex + direction + model.pages.length) % model.pages.length;
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
