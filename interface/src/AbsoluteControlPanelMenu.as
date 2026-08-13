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
        // A tiny 5x7 research alphabet. Starfield's Scaleform build renders neither Flash
        // device fonts nor Flex-embedded outlines reliably in a standalone movie, so the probe
        // draws its diagnostic lettering with the same vector Graphics API as the panel.
        private static const GLYPHS:Object = {
            "A":[14,17,17,31,17,17,17], "B":[30,17,17,30,17,17,30],
            "C":[14,17,16,16,16,17,14], "D":[30,17,17,17,17,17,30],
            "E":[31,16,16,30,16,16,31], "F":[31,16,16,30,16,16,16],
            "G":[14,17,16,23,17,17,14], "H":[17,17,17,31,17,17,17],
            "I":[31,4,4,4,4,4,31], "J":[7,2,2,2,18,18,12],
            "K":[17,18,20,24,20,18,17], "L":[16,16,16,16,16,16,31],
            "M":[17,27,21,21,17,17,17], "N":[17,25,21,19,17,17,17],
            "O":[14,17,17,17,17,17,14], "P":[30,17,17,30,16,16,16],
            "Q":[14,17,17,17,21,18,13], "R":[30,17,17,30,20,18,17],
            "S":[15,16,16,14,1,1,30], "T":[31,4,4,4,4,4,4],
            "U":[17,17,17,17,17,17,14], "V":[17,17,17,17,17,10,4],
            "W":[17,17,17,17,21,27,17], "X":[17,17,10,4,10,17,17],
            "Y":[17,17,10,4,4,4,4], "Z":[31,1,2,4,8,16,31],
            "0":[14,17,19,21,25,17,14], "1":[4,12,4,4,4,4,14],
            "2":[14,17,1,2,4,8,31], "3":[30,1,1,14,1,1,30],
            "4":[2,6,10,18,31,2,2], "5":[31,16,16,30,1,1,30],
            "6":[14,16,16,30,17,17,14], "7":[31,1,2,4,8,8,8],
            "8":[14,17,17,14,17,17,14], "9":[14,17,17,15,1,1,14],
            "-":[0,0,0,31,0,0,0], "+":[0,4,4,31,4,4,0],
            "/":[1,1,2,4,8,16,16], ".":[0,0,0,0,0,12,12],
            ",":[0,0,0,0,4,4,8], ":":[0,12,12,0,12,12,0],
            "'":[4,4,8,0,0,0,0], "(":[2,4,8,8,8,4,2],
            ")":[8,4,2,2,2,4,8], "?":[14,17,1,2,4,0,4],
            " ":[0,0,0,0,0,0,0]
        };

        // GameMenuBase maps native functions onto this object before calling
        // onCodeObjCreate.  Declaring the property without constructing it leaves a null
        // slot that HasMember can see but SetMember cannot populate.
        public var BGSCodeObj:Object = {};
        public var ACPConstructed:Boolean = false;
        public var ACPDrawn:Boolean = false;

        private var statusField:Sprite;
        private var toggleControl:Sprite;
        private var sliderControl:Sprite;
        private var bindingControl:Sprite;
        private var selectedIndex:int = 0;
        private var featureEnabled:Boolean = false;
        private var responseLevel:int = 50;
        private var snapshotGeneration:uint = 0;
        private var enumeratedDeviceCount:uint = 0;
        private var captureActive:Boolean = false;
        private var capturedBinding:String = "(unbound)";

        public function AbsoluteControlPanelMenu()
        {
            super();
            ACPConstructed = true;
            // Scaleform can construct a document class after its root is already attached.
            // Draw immediately in that case; otherwise wait for the normal display-list event.
            if (stage != null) {
                onAddedToStage(null);
            } else {
                addEventListener(Event.ADDED_TO_STAGE, onAddedToStage);
            }
        }

        private function onAddedToStage(event:Event):void
        {
            removeEventListener(Event.ADDED_TO_STAGE, onAddedToStage);
            drawPanel();
            if (stage != null) {
                stage.addEventListener(KeyboardEvent.KEY_DOWN, onKeyDown, true, 1000);
            }
            addEventListener(Event.ENTER_FRAME, onFrame);
        }

        private function drawPanel():void
        {
            ACPDrawn = true;
            graphics.beginFill(0x071018, 0.94);
            graphics.drawRect(0, 0, 1920, 1080);
            graphics.endFill();

            graphics.lineStyle(2, 0x66D9EF, 0.8);
            graphics.beginFill(0x10212E, 1.0);
            graphics.drawRect(250, 145, 1420, 790);
            graphics.endFill();

            // Deliberately unnatural smoke-test sentinel. The harness treats a large cluster of
            // near-FF00FF pixels as binary proof that this SWF reached Starfield's framebuffer.
            graphics.lineStyle();
            graphics.beginFill(0xFF00FF, 1.0);
            graphics.drawRect(1540, 175, 96, 96);
            graphics.endFill();

            addText("STARFIELD LOCAL OPTIONS PANEL", 320, 220, 42, 0xE8F7FF);
            addText("AI ASSISTED NATIVE CONFIGURATION HOST", 322, 278, 20, 0x66D9EF);
            addText("Gate R2 / Synthetic subscriber round trip", 322, 350, 28, 0xFFFFFF);
            addText(
                "Commands cross into native code. Native state returns as a new snapshot.",
                322, 400, 23, 0xB9CBD6);

            statusField = addText(
                "SWF loaded  -  awaiting native bridge", 322, 470, 25, 0xFFD166);

            toggleControl = new Sprite();
            toggleControl.x = 322;
            toggleControl.y = 540;
            toggleControl.buttonMode = true;
            toggleControl.addEventListener(MouseEvent.CLICK, onToggleClick);
            addChild(toggleControl);

            sliderControl = new Sprite();
            sliderControl.x = 322;
            sliderControl.y = 650;
            sliderControl.buttonMode = true;
            sliderControl.addEventListener(MouseEvent.CLICK, onSliderClick);
            addChild(sliderControl);

            bindingControl = new Sprite();
            bindingControl.x = 322;
            bindingControl.y = 760;
            bindingControl.buttonMode = true;
            bindingControl.addEventListener(MouseEvent.CLICK, onBindingClick);
            addChild(bindingControl);

            redrawControls();

            var closeButton:Sprite = new Sprite();
            closeButton.graphics.lineStyle(2, 0x66D9EF, 1.0);
            closeButton.graphics.beginFill(0x173447, 1.0);
            closeButton.graphics.drawRect(0, 0, 320, 76);
            closeButton.graphics.endFill();
            closeButton.x = 1320;
            closeButton.y = 760;
            closeButton.buttonMode = true;
            closeButton.addEventListener(MouseEvent.CLICK, onClose);
            addChild(closeButton);

            var closeLabel:Sprite = createText("CLOSE PROBE", 26, 0xFFFFFF);
            closeLabel.x = 66;
            closeLabel.y = 20;
            closeLabel.mouseChildren = false;
            closeButton.addChild(closeLabel);

            addText("W S SELECT   E ACTIVATE   A D ADJUST   ESC CLOSE   MOUSE ENABLED",
                322, 870, 20, 0x86A2B3);
        }

        private function redrawControls():void
        {
            drawControl(
                toggleControl,
                "RESEARCH TOGGLE     " + (featureEnabled ? "ON" : "OFF") +
                "\nE OR CLICK TO TOGGLE",
                selectedIndex == 0);
            drawControl(
                sliderControl,
                "RESPONSE LEVEL      " + responseLevel +
                "\nA D OR CLICK LEFT RIGHT",
                selectedIndex == 1);
            drawControl(
                bindingControl,
                "TEST BUTTON BINDING   " +
                (captureActive ? "LISTENING" : capturedBinding) +
                "\n" + enumeratedDeviceCount + " DEVICES ENUMERATED   E OR CLICK TO BIND",
                selectedIndex == 2);
        }

        private function drawControl(target:Sprite, label:String, selected:Boolean):void
        {
            if (target == null) {
                return;
            }
            target.graphics.clear();
            while (target.numChildren > 0) {
                target.removeChildAt(0);
            }
            target.graphics.lineStyle(3, selected ? 0xFFD166 : 0x467A91, 1.0);
            target.graphics.beginFill(selected ? 0x21465A : 0x142B39, 1.0);
            target.graphics.drawRect(0, 0, 950, 88);
            target.graphics.endFill();
            var labelField:Sprite = createText(label, 20, selected ? 0xFFFFFF : 0xB9CBD6);
            labelField.x = 28;
            labelField.y = 16;
            target.addChild(labelField);
        }

        private function addText(
            value:String, xPosition:Number, yPosition:Number, size:Number, color:uint):Sprite
        {
            var field:Sprite = createText(value, size, color);
            field.x = xPosition;
            field.y = yPosition;
            addChild(field);
            return field;
        }

        private function createText(value:String, size:Number, color:uint):Sprite
        {
            var field:Sprite = new Sprite();
            drawText(field, value, size, color);
            field.mouseEnabled = false;
            field.mouseChildren = false;
            return field;
        }

        private function drawText(
            target:Sprite, value:String, size:Number, color:uint):void
        {
            var pixel:Number = Math.max(1, Math.floor(size / 7));
            var cursorX:Number = 0;
            var cursorY:Number = 0;
            var normalized:String = value.toUpperCase();
            target.graphics.beginFill(color, 1.0);
            for (var index:int = 0; index < normalized.length; ++index) {
                var character:String = normalized.charAt(index);
                if (character == "\n") {
                    cursorX = 0;
                    cursorY += pixel * 9;
                    continue;
                }
                var rows:Array = GLYPHS[character] as Array;
                if (rows == null) {
                    rows = GLYPHS["?"] as Array;
                }
                for (var row:int = 0; row < 7; ++row) {
                    for (var column:int = 0; column < 5; ++column) {
                        if ((int(rows[row]) & (1 << (4 - column))) != 0) {
                            target.graphics.drawRect(
                                cursorX + column * pixel, cursorY + row * pixel,
                                pixel, pixel);
                        }
                    }
                }
                cursorX += pixel * 6;
            }
            target.graphics.endFill();
        }

        public function onCodeObjCreate():void
        {
            if (statusField != null) {
                statusField.graphics.clear();
                drawText(
                    statusField, "SWF loaded  -  native bridge version 1 connected",
                    25, 0x7CFC98);
            }
            if (BGSCodeObj != null && BGSCodeObj.ready != null) {
                BGSCodeObj.ready(1);
            }
        }

        public function applySnapshot(snapshot:Object):void
        {
            if (snapshot == null) {
                return;
            }
            featureEnabled = Boolean(snapshot.enabled);
            responseLevel = int(snapshot.level);
            snapshotGeneration = uint(snapshot.generation);
            enumeratedDeviceCount = uint(snapshot.deviceCount);
            captureActive = Boolean(snapshot.captureActive);
            capturedBinding = String(snapshot.binding);
            redrawControls();
            if (BGSCodeObj != null && BGSCodeObj.snapshotApplied != null) {
                BGSCodeObj.snapshotApplied(snapshotGeneration);
            }
        }

        public function applyFocus(index:int):void
        {
            selectedIndex = Math.max(0, Math.min(2, index));
            redrawControls();
        }

        private function onKeyDown(event:KeyboardEvent):void
        {
            var handled:Boolean = true;
            if (event.keyCode == Keyboard.ESCAPE) {
                requestClose();
            } else if (event.keyCode == Keyboard.W || event.keyCode == Keyboard.UP) {
                selectedIndex = (selectedIndex + 2) % 3;
                redrawControls();
            } else if (event.keyCode == Keyboard.S || event.keyCode == Keyboard.DOWN) {
                selectedIndex = (selectedIndex + 1) % 3;
                redrawControls();
            } else if (event.keyCode == Keyboard.E || event.keyCode == Keyboard.ENTER ||
                       event.keyCode == Keyboard.SPACE) {
                if (selectedIndex == 0) {
                    dispatchCommand("toggleFeature");
                } else if (selectedIndex == 1) {
                    dispatchCommand("incrementLevel");
                } else {
                    dispatchCommand("beginBindingCapture");
                }
            } else if ((event.keyCode == Keyboard.A || event.keyCode == Keyboard.LEFT) &&
                       selectedIndex == 1) {
                dispatchCommand("decrementLevel");
            } else if ((event.keyCode == Keyboard.D || event.keyCode == Keyboard.RIGHT) &&
                       selectedIndex == 1) {
                dispatchCommand("incrementLevel");
            } else {
                handled = false;
            }
            if (handled) {
                event.preventDefault();
                event.stopImmediatePropagation();
            }
        }

        private function onToggleClick(event:MouseEvent):void
        {
            selectedIndex = 0;
            redrawControls();
            dispatchCommand("toggleFeature");
        }

        private function onSliderClick(event:MouseEvent):void
        {
            selectedIndex = 1;
            redrawControls();
            dispatchCommand(event.localX < 475 ? "decrementLevel" : "incrementLevel");
        }

        private function onBindingClick(event:MouseEvent):void
        {
            selectedIndex = 2;
            redrawControls();
            dispatchCommand("beginBindingCapture");
        }

        private function onFrame(event:Event):void
        {
            if (captureActive && BGSCodeObj != null &&
                BGSCodeObj.pollInputCapture != null) {
                BGSCodeObj.pollInputCapture();
            }
        }

        private function onClose(event:MouseEvent):void
        {
            requestClose();
        }

        private function requestClose():void
        {
            if (BGSCodeObj != null && BGSCodeObj.close != null) {
                BGSCodeObj.close();
            }
        }

        private function dispatchCommand(command:String):void
        {
            if (BGSCodeObj != null && BGSCodeObj.dispatchCommand != null) {
                BGSCodeObj.dispatchCommand(1, command, null);
            }
        }
    }
}
