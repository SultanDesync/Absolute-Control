package acp.ui
{
    import flash.display.MovieClip;
    import flash.display.Sprite;

    public final class MenuShellRenderer
    {
        private var host:MovieClip;
        private var hits:PointerInteraction;
        private var moduleLayer:Sprite;
        private var tabLayer:Sprite;
        private var rowLayer:Sprite;
        private var helpLayer:Sprite;
        private var footerLayer:Sprite;
        private var statusField:Sprite;

        public function MenuShellRenderer(target:MovieClip, pointerHits:PointerInteraction)
        {
            host = target;
            hits = pointerHits;
        }

        public function drawPanel():void
        {
            host.graphics.beginFill(PanelTheme.BACKGROUND, 0.94);
            host.graphics.drawRect(0, 0, 1920, 1080);
            host.graphics.endFill();
            host.graphics.lineStyle(2, PanelTheme.CYAN, 0.8);
            host.graphics.beginFill(PanelTheme.PANEL, 1.0);
            host.graphics.drawRect(170, 80, 1580, 920);
            host.graphics.endFill();
            PixelTextRenderer.addText(host, "ABSOLUTE CONTROL PANEL", 220, 125, 34,
                PanelTheme.TEXT);
            statusField = PixelTextRenderer.addText(host, "AWAITING MENU MODEL", 220, 180,
                16, PanelTheme.GOLD);

            moduleLayer = createLayer(PanelLayout.SIDEBAR_X, PanelLayout.SIDEBAR_Y);
            tabLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.TABS_Y);
            rowLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.ROWS_Y);
            helpLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.HELP_Y);
            footerLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.FOOTER_Y);
            PixelTextRenderer.addText(host, "MODS", PanelLayout.SIDEBAR_X, 218, 15,
                PanelTheme.DIM_TEXT);
        }

        public function redraw(model:Object, state:MenuSelectionState, inputMode:String):void
        {
            if (statusField == null || moduleLayer == null || tabLayer == null ||
                rowLayer == null || helpLayer == null || footerLayer == null) return;

            clearLayer(rowLayer, true);
            clearLayer(moduleLayer, false);
            clearLayer(tabLayer, false);
            clearLayer(helpLayer, false);
            clearLayer(footerLayer, false);
            hits.resetHits();

            var current:Object = state.page();
            var title:String = current == null ? "NO REGISTERED PAGES" :
                String(current.moduleTitle) + "  " + String(current.title);
            var error:String = model != null && String(model.error).length > 0 ?
                "  " + String(model.error) : "";
            statusField.graphics.clear();
            PixelTextRenderer.drawText(statusField,
                title + (model != null && model.dirty ? "  DIRTY" : "") + error, 18,
                error.length > 0 ? PanelTheme.ERROR : PanelTheme.GOLD);
            if (current == null) return;

            drawModules(model, state);
            drawTabs(model, state);
            PixelTextRenderer.addText(rowLayer, String(current.title), 0, -48, 21,
                PanelTheme.TEXT);
            var visible:int = Math.min(PanelLayout.VISIBLE_ROWS,
                current.controls.length - state.firstVisibleRow);
            for (var i:int = 0; i < visible; ++i) {
                addRow(model, state, current.controls[state.firstVisibleRow + i],
                    state.firstVisibleRow + i, i * PanelLayout.ROW_HEIGHT);
            }
            drawScrollBar(current, state);
            drawHelp(current, state);
            drawFooter(model, state, inputMode);
        }

        private function createLayer(xPosition:Number, yPosition:Number):Sprite
        {
            var layer:Sprite = new Sprite();
            layer.x = xPosition;
            layer.y = yPosition;
            host.addChild(layer);
            return layer;
        }

        private function clearLayer(layer:Sprite, clearGraphics:Boolean):void
        {
            while (layer.numChildren > 0) layer.removeChildAt(0);
            if (clearGraphics) layer.graphics.clear();
        }

        private function drawModules(model:Object, state:MenuSelectionState):void
        {
            var starts:Array = state.modulePageStarts();
            var visibleModules:int = Math.min(PanelLayout.VISIBLE_MODULES,
                starts.length - state.firstVisibleModule);
            for (var moduleRow:int = 0; moduleRow < visibleModules; ++moduleRow) {
                var modulePageIndex:int = int(starts[state.firstVisibleModule + moduleRow]);
                addModuleButton(model.pages[modulePageIndex], state,
                    moduleRow * (PanelLayout.MODULE_HEIGHT + PanelLayout.MODULE_GAP));
            }
        }

        private function drawTabs(model:Object, state:MenuSelectionState):void
        {
            var tabs:Array = state.activeModulePages();
            var visibleTabs:int = Math.min(PanelLayout.VISIBLE_TABS,
                tabs.length - state.firstVisibleTab);
            for (var tab:int = 0; tab < visibleTabs; ++tab) {
                var tabPageIndex:int = int(tabs[state.firstVisibleTab + tab]);
                addPageTab(model.pages[tabPageIndex], tabPageIndex, state,
                    tab * PanelLayout.TAB_STEP);
            }
        }

        private function addModuleButton(target:Object, state:MenuSelectionState,
            yPosition:Number):void
        {
            var button:Sprite = new Sprite();
            var selected:Boolean = String(target.moduleId) == state.currentPageModule();
            var focused:Boolean = selected && state.focusRegion == PanelLayout.FOCUS_MODULES;
            button.graphics.lineStyle(focused ? 2 : 1,
                focused ? PanelTheme.GOLD : (selected ? PanelTheme.CYAN : PanelTheme.BORDER));
            button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL : PanelTheme.BUTTON_FILL);
            button.graphics.drawRect(0, 0, PanelLayout.SIDEBAR_WIDTH,
                PanelLayout.MODULE_HEIGHT);
            button.graphics.endFill();
            button.y = yPosition;
            button.buttonMode = true;
            PixelTextRenderer.addText(button, String(target.moduleTitle), 14, 14, 13,
                selected ? 0xFFFFFF : PanelTheme.MUTED_TEXT);
            moduleLayer.addChild(button);
            hits.register(button, "page", target, PanelLayout.FOCUS_MODULES);
        }

        private function addPageTab(target:Object, index:int, state:MenuSelectionState,
            xPosition:Number):void
        {
            var button:Sprite = new Sprite();
            var selected:Boolean = index == state.activePageIndex;
            button.graphics.lineStyle(1, selected ? PanelTheme.CYAN : PanelTheme.BORDER);
            button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL : PanelTheme.BUTTON_FILL);
            button.graphics.drawRect(0, 0, PanelLayout.TAB_WIDTH, 42);
            button.graphics.endFill();
            button.x = xPosition;
            button.buttonMode = true;
            PixelTextRenderer.addText(button, String(target.title), 12, 12, 13,
                selected ? 0xFFFFFF : PanelTheme.MUTED_TEXT);
            tabLayer.addChild(button);
            hits.register(button, "page", target, PanelLayout.FOCUS_CONTROLS);
        }

        private function addRow(model:Object, state:MenuSelectionState, control:Object,
            index:int, yPosition:Number):void
        {
            var row:Sprite = new Sprite();
            var selected:Boolean = state.focusRegion == PanelLayout.FOCUS_CONTROLS &&
                index == state.selectedRow;
            row.graphics.lineStyle(selected ? 2 : 1,
                selected ? PanelTheme.GOLD : PanelTheme.ROW_BORDER, 1.0);
            row.graphics.beginFill(selected ? PanelTheme.ROW_SELECTED :
                (index % 2 == 0 ? PanelTheme.ROW_EVEN : PanelTheme.ROW_ODD), 1.0);
            row.graphics.drawRect(0, 0, PanelLayout.WORKSPACE_WIDTH,
                PanelLayout.ROW_HEIGHT - 4);
            row.graphics.endFill();
            row.y = yPosition;
            row.buttonMode = true;

            var flags:String = (uint(control.flags) & 4 ? " ADVANCED" : "") +
                (uint(control.flags) & 2 ? " RESTART" : "");
            var capturing:Boolean = model != null && Boolean(model.bindingCaptureActive) &&
                String(model.captureModuleId) == state.currentPageModule() &&
                String(model.capturePageId) == state.currentPageId() &&
                String(model.captureControlId) == String(control.controlId);
            PixelTextRenderer.addText(row,
                PixelTextRenderer.fit(String(control.label), 34), 14, 16, 15,
                Boolean(control.available) ?
                    (selected ? 0xFFFFFF : PanelTheme.MUTED_TEXT) : PanelTheme.DISABLED_TEXT);
            if (flags.length > 0) {
                PixelTextRenderer.addText(row, flags, 350, 18, 10, PanelTheme.WARNING);
            }
            hits.register(row, "select", control, index);
            ControlWidgets.draw(row, control, capturing, hits.register);
            rowLayer.addChild(row);
        }

        private function drawHelp(current:Object, state:MenuSelectionState):void
        {
            helpLayer.graphics.clear();
            helpLayer.graphics.lineStyle(1, PanelTheme.BORDER);
            helpLayer.graphics.beginFill(PanelTheme.HELP_FILL);
            helpLayer.graphics.drawRect(0, 0, PanelLayout.WORKSPACE_WIDTH, 70);
            helpLayer.graphics.endFill();
            var description:String = String(current.description);
            if (current.controls != null && current.controls.length > 0) {
                description = String(current.controls[state.selectedRow].description);
            }
            PixelTextRenderer.addText(helpLayer, "HELP", 14, 12, 11, PanelTheme.DIM_TEXT);
            PixelTextRenderer.addText(helpLayer,
                PixelTextRenderer.fit(description, 105), 14, 38, 12, PanelTheme.MUTED_TEXT);
        }

        private function drawScrollBar(current:Object, state:MenuSelectionState):void
        {
            if (current.controls == null ||
                current.controls.length <= PanelLayout.VISIBLE_ROWS) return;
            var trackHeight:Number = PanelLayout.VISIBLE_ROWS * PanelLayout.ROW_HEIGHT - 4;
            var thumbHeight:Number = Math.max(42,
                trackHeight * PanelLayout.VISIBLE_ROWS / current.controls.length);
            var travel:Number = trackHeight - thumbHeight;
            var maximumStart:Number = current.controls.length - PanelLayout.VISIBLE_ROWS;
            var thumbY:Number = maximumStart > 0 ?
                travel * state.firstVisibleRow / maximumStart : 0;
            rowLayer.graphics.beginFill(PanelTheme.ROW_BORDER);
            rowLayer.graphics.drawRect(PanelLayout.WORKSPACE_WIDTH - 7, 0, 5, trackHeight);
            rowLayer.graphics.endFill();
            rowLayer.graphics.beginFill(PanelTheme.CYAN);
            rowLayer.graphics.drawRect(PanelLayout.WORKSPACE_WIDTH - 8, thumbY, 7, thumbHeight);
            rowLayer.graphics.endFill();
        }

        private function drawFooter(model:Object, state:MenuSelectionState,
            inputMode:String):void
        {
            footerLayer.graphics.clear();
            footerLayer.graphics.lineStyle(1, PanelTheme.BORDER);
            footerLayer.graphics.moveTo(0, 0);
            footerLayer.graphics.lineTo(PanelLayout.WORKSPACE_WIDTH, 0);
            var prefix:String = inputMode == "mouse" ? "" :
                (inputMode == "controller" ? "A " : "ENTER ");
            addFooterButton(prefix + "APPLY", 0, 0, Boolean(model.dirty), state);
            addFooterButton(prefix + "CANCEL", 185, 1, Boolean(model.dirty), state);
            addFooterButton((inputMode == "controller" ? "B " : "ESC ") + "CLOSE",
                390, 2, true, state);
        }

        private function addFooterButton(label:String, xPosition:Number, actionIndex:int,
            enabled:Boolean, state:MenuSelectionState):void
        {
            var button:Sprite = new Sprite();
            var selected:Boolean = state.focusRegion == PanelLayout.FOCUS_ACTIONS &&
                state.focusedAction == actionIndex;
            button.graphics.lineStyle(selected ? 2 : 1,
                selected ? PanelTheme.GOLD : (enabled ? PanelTheme.CYAN : PanelTheme.BORDER));
            button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL : PanelTheme.BUTTON_FILL);
            button.graphics.drawRect(0, 12, 175, 38);
            button.graphics.endFill();
            button.x = xPosition;
            button.buttonMode = enabled;
            PixelTextRenderer.addText(button, label, 14, 25, 12,
                enabled ? PanelTheme.TEXT : PanelTheme.DISABLED_TEXT);
            footerLayer.addChild(button);
            hits.register(button, enabled ? "action" : "disabled", null, actionIndex);
        }
    }
}
