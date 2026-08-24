package acp.ui
{
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.utils.getTimer;

    public final class MenuShellRenderer
    {
        private var host:MovieClip;
        private var hits:PointerInteraction;
        private var moduleLayer:Sprite;
        private var tabLayer:Sprite;
        private var rowLayer:Sprite;
        private var liveLayer:Sprite;
        private var liveHitLayer:Sprite;
        private var scrollLayer:Sprite;
        private var helpLayer:Sprite;
        private var footerLayer:Sprite;
        private var overlayLayer:Sprite;
        private var tooltipLayer:Sprite;
        private var statusField:Sprite;
        private var semanticRenderer:SemanticCompositionRenderer;
        private var activeGridTiers:Object = {};
        private var gridFocusActive:Boolean = false;
        private var selectedGridColumnId:String = "";
        private var guidanceActive:Boolean = false;
        private var openChoiceModuleId:String = "";
        private var openChoicePageId:String = "";
        private var openChoiceControlId:String = "";
        private var openChoiceControl:Object;
        private var choiceCursor:int = 0;
        private var choiceFirstVisible:int = 0;
        private var openRecordModuleId:String = "";
        private var openRecordPageId:String = "";
        private var openRecordControlId:String = "";
        private var openRecordControl:Object;
        private var recordCursor:int = 0;
        private var recordFirstVisible:int = 0;
        private var livePlacements:Array = [];
        private var expandedLive:Object = {};
        private var radialStates:Object = {};
        private var activeRadialKey:String = "";
        private var activeSelectionState:MenuSelectionState;
        private static const LIVE_DASHBOARD_MAX_HEIGHT:Number = 520;
        private static const LIVE_COMPONENT_LIMIT:int = 6;
        private static const RANGE_CARD_HEIGHT:Number = 86;
        private static const PINNED_RANGE_CARD_HEIGHT:Number = 132;
        private static const PLOT_CARD_HEIGHT:Number = 166;
        private static const RADIAL_CARD_HEIGHT:Number = 430;
        private static const HEAD_POSE_CARD_HEIGHT:Number = 500;
        private static const LIVE_DISCLOSURE_HEIGHT:Number = 36;
        private static const LIVE_PINNED:uint = 1 << 8;
        private static const LIVE_SECONDARY:uint = 1 << 9;
        private static const LIVE_COLLAPSED:uint = 1 << 10;
        private static const PINNED_CONTEXT_HEIGHT:Number = 72;

        public function MenuShellRenderer(target:MovieClip, pointerHits:PointerInteraction)
        {
            host = target;
            hits = pointerHits;
        }

        public function drawPanel():void
        {
            host.graphics.beginFill(PanelTheme.BACKGROUND, 0.94);
            host.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH, PanelLayout.STAGE_HEIGHT);
            host.graphics.endFill();
            host.graphics.lineStyle(1, PanelTheme.BORDER, 1.0);
            host.graphics.beginFill(PanelTheme.PANEL, 1.0);
            host.graphics.drawRect(PanelLayout.SAFE_MARGIN, PanelLayout.SAFE_MARGIN,
                PanelLayout.SAFE_WIDTH, PanelLayout.SAFE_HEIGHT);
            host.graphics.endFill();

            host.graphics.beginFill(PanelTheme.ROW_ODD);
            host.graphics.drawRect(PanelLayout.HEADER_X, PanelLayout.HEADER_Y,
                PanelLayout.HEADER_WIDTH, PanelLayout.HEADER_HEIGHT);
            host.graphics.drawRect(PanelLayout.SIDEBAR_X, PanelLayout.SIDEBAR_HEADER_Y,
                PanelLayout.SIDEBAR_WIDTH, PanelLayout.HELP_Y + PanelLayout.HELP_HEIGHT -
                PanelLayout.SIDEBAR_HEADER_Y);
            host.graphics.endFill();

            VectorTextRenderer.addText(host, "ABSOLUTE CONTROL", 76, 67, 26,
                PanelTheme.TEXT, true);
            statusField = VectorTextRenderer.addText(host, "Awaiting menu model",
                PanelLayout.WORKSPACE_X, 70, 18, PanelTheme.GOLD, true,
                PanelLayout.WORKSPACE_WIDTH - 20, 28);

            moduleLayer = createLayer(PanelLayout.SIDEBAR_X, PanelLayout.SIDEBAR_Y);
            tabLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.TABS_Y);
            rowLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.ROWS_Y);
            liveLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.ROWS_Y);
            liveHitLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.ROWS_Y);
            semanticRenderer = new SemanticCompositionRenderer(rowLayer, hits);
            scrollLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.ROWS_Y);
            helpLayer = createLayer(PanelLayout.WORKSPACE_X, PanelLayout.HELP_Y);
            footerLayer = createLayer(PanelLayout.FOOTER_X, PanelLayout.FOOTER_Y);
            overlayLayer = createLayer(0, 0);
            tooltipLayer = createLayer(0, 0);
            VectorTextRenderer.addText(host, "INSTALLED MODULES", 76, 130, 14,
                PanelTheme.DIM_TEXT, true);
        }

        public function redraw(model:Object, state:MenuSelectionState,
            inputMode:String, dirtyDecisionCursor:int = 0):void
        {
            if (statusField == null || moduleLayer == null || tabLayer == null ||
                rowLayer == null || liveLayer == null || liveHitLayer == null ||
                scrollLayer == null ||
                helpLayer == null ||
                footerLayer == null || overlayLayer == null ||
                tooltipLayer == null) return;

            activeSelectionState = state;

            clearLayer(rowLayer, true);
            clearLayer(liveLayer, true);
            clearLayer(liveHitLayer, true);
            livePlacements = [];
            clearLayer(scrollLayer, true);
            clearLayer(moduleLayer, false);
            clearLayer(tabLayer, false);
            clearLayer(helpLayer, false);
            clearLayer(footerLayer, false);
            clearLayer(overlayLayer, true);
            clearLayer(tooltipLayer, true);
            hits.resetHits();

            var current:Object = state.page();
            if (activeRadialKey.length > 0 && current != null) {
                var activePagePrefix:String = String(current.moduleId) + "\n" +
                    String(current.pageId) + "\n";
                if (activeRadialKey.indexOf(activePagePrefix) != 0) {
                    activeRadialKey = "";
                }
            }
            gridFocusActive = state.focusRegion == PanelLayout.FOCUS_GRID;
            selectedGridColumnId = model == null ? "" :
                String(model.selectedGridColumnId);
            var title:String = current == null ? "NO REGISTERED PAGES" :
                String(current.moduleTitle) + "  " + String(current.title);
            var error:String = model != null && String(model.error).length > 0 ?
                "  " + String(model.error) : "";
            statusField.graphics.clear();
            VectorTextRenderer.drawText(statusField,
                title + (model != null && model.dirty ? "  DIRTY" : "") + error, 18,
                error.length > 0 ? PanelTheme.ERROR : PanelTheme.GOLD, true,
                PanelLayout.WORKSPACE_WIDTH - 20, 28);
            if (current == null) return;

            drawModules(model, state);
            drawTabs(model, state);
            VectorTextRenderer.addText(rowLayer, String(current.title), 0, -38, 19,
                PanelTheme.TEXT, true);
            var pinnedHeight:Number = drawPinnedContext(model, state, current);
            var gridHeight:Number = drawLiveComponents(current, pinnedHeight);
            var anchorHeight:Number = semanticRenderer.drawAnchors(
                current, state, pinnedHeight + gridHeight);
            var semantic:Boolean = Boolean(current.compositionEnhanced);
            var rowHeight:Number = semantic ?
                SemanticCompositionRenderer.ROW_HEIGHT : PanelLayout.ROW_HEIGHT;
            var availableHeight:Number = PanelLayout.ROWS_BOTTOM -
                PanelLayout.ROWS_Y - gridHeight - anchorHeight;
            availableHeight -= pinnedHeight;
            var visibleRows:int = semantic ? Math.max(1,
                Math.floor(availableHeight / rowHeight)) :
                (gridHeight > 0 ? 5 : PanelLayout.VISIBLE_ROWS);
            var controlRows:Array = state.controlRows(current);
            var firstLayoutRow:int = 0;
            for (var rowSearch:int = 0; rowSearch < controlRows.length;
                 ++rowSearch) {
                if (int(controlRows[rowSearch][0]) == state.firstVisibleRow) {
                    firstLayoutRow = rowSearch;
                    break;
                }
            }
            var lastVisibleControl:int = state.firstVisibleRow - 1;
            var renderedRows:int = 0;
            var nextRowY:Number = pinnedHeight + gridHeight + anchorHeight;
            var rowsBottom:Number = nextRowY + availableHeight;
            for (var layoutIndex:int = firstLayoutRow;
                 layoutIndex < controlRows.length; ++layoutIndex) {
                var physicalRow:Array = controlRows[layoutIndex];
                var firstControlIndex:int = int(physicalRow[0]);
                var firstControl:Object = current.controls[firstControlIndex];
                if (headPoseOwnsControl(current,
                    String(firstControl.controlId))) {
                    lastVisibleControl =
                        int(physicalRow[physicalRow.length - 1]);
                    ++renderedRows;
                    continue;
                }
                var actualRowHeight:Number = semantic ?
                    semanticRenderer.rowHeight(current, firstControl) : rowHeight;
                if (renderedRows > 0 &&
                    nextRowY + actualRowHeight > rowsBottom) break;
                var rowY:Number = nextRowY;
                var semanticOffset:Number = semantic ?
                    semanticRenderer.drawFrame(current, firstControl, rowY) : 0;
                var rowDrawn:Boolean = false;
                if (semantic) {
                    var embedded:Object = semanticRenderer.liveComponentForControl(
                        current, firstControl);
                    if (embedded != null) {
                        // The composition declares source/binding controls
                        // before its live slot. Preserve that hierarchy: the
                        // primary control row stays above the graph instead of
                        // being visually buried beneath it.
                        if (uint(firstControl.kind) == 7) {
                            addGroupHeader(firstControl, rowY + semanticOffset);
                        } else if (physicalRow.length > 1) {
                            addInlineRow(model, state, current, physicalRow,
                                rowY + semanticOffset);
                        } else {
                            addRow(model, state, firstControl, firstControlIndex,
                                rowY + semanticOffset);
                        }
                        rowDrawn = true;
                        semanticOffset += SemanticCompositionRenderer.ROW_HEIGHT;
                        if (uint(embedded.kind) == 0) {
                            drawRangeMeter(embedded, 0, rowY + semanticOffset,
                                PanelLayout.CONTROL_ROW_WIDTH - 8,
                                SemanticCompositionRenderer.EMBEDDED_RANGE_HEIGHT,
                                true, current);
                            semanticOffset +=
                                SemanticCompositionRenderer.EMBEDDED_RANGE_HEIGHT + 6;
                        } else if (uint(embedded.kind) == 1) {
                            drawTelemetryPlot(embedded, 0, rowY + semanticOffset,
                                PanelLayout.CONTROL_ROW_WIDTH - 8,
                                SemanticCompositionRenderer.EMBEDDED_PLOT_HEIGHT);
                            semanticOffset +=
                                SemanticCompositionRenderer.EMBEDDED_PLOT_HEIGHT + 6;
                        }
                    }
                }
                if (!rowDrawn && uint(firstControl.kind) == 7) {
                    addGroupHeader(firstControl, rowY + semanticOffset);
                } else if (!rowDrawn && physicalRow.length > 1) {
                    addInlineRow(model, state, current, physicalRow,
                        rowY + semanticOffset);
                } else if (!rowDrawn) {
                    addRow(model, state, firstControl, firstControlIndex,
                        rowY + semanticOffset);
                }
                lastVisibleControl = int(physicalRow[physicalRow.length - 1]);
                nextRowY += actualRowHeight;
                ++renderedRows;
            }
            current.visibleControlRows = Math.max(0,
                lastVisibleControl - state.firstVisibleRow + 1);
            drawScrollBar(current, state, pinnedHeight + gridHeight + anchorHeight,
                Math.max(1, renderedRows), controlRows, firstLayoutRow, rowHeight);
            drawHelp(current, state, inputMode);
            drawFooter(model, state, inputMode);
            if (Boolean(model.bindingCaptureActive)) {
                closeChoice();
                drawBindingCapture(model, current, inputMode);
            } else if (Boolean(model.actionConfirmationActive)) {
                closeChoice();
                ActionConfirmationDialog.draw(
                    overlayLayer, hits, model, inputMode == "controller" ?
                        state.focusedAction : dirtyDecisionCursor, inputMode);
            } else if (Boolean(model.bindingConflictActive)) {
                closeChoice();
                BindingConflictDialog.draw(
                    overlayLayer, hits, model, inputMode == "controller" ?
                        state.focusedAction : dirtyDecisionCursor, inputMode);
            } else if (Boolean(model.dirtyDecisionActive)) {
                closeChoice();
                DirtyDecisionDialog.draw(
                    overlayLayer, hits, model, inputMode == "controller" ?
                        state.focusedAction : dirtyDecisionCursor, inputMode);
            } else if (guidanceActive) {
                closeChoice();
                drawThrottleGuidance();
            } else {
                if (recordCollectionIsOpen) drawRecordCollectionPopup(current);
                else drawChoicePopup(current, state, gridHeight);
            }
        }

        private function drawBindingCapture(model:Object, current:Object,
            inputMode:String):void
        {
            var control:Object = null;
            if (current != null && current.controls != null &&
                String(current.moduleId) == String(model.captureModuleId) &&
                String(current.pageId) == String(model.capturePageId)) {
                for (var index:int = 0; index < current.controls.length; ++index) {
                    if (String(current.controls[index].controlId) ==
                        String(model.captureControlId)) {
                        control = current.controls[index];
                        break;
                    }
                }
            }

            var blocker:Sprite = new Sprite();
            blocker.graphics.beginFill(PanelTheme.BACKGROUND, 0.76);
            blocker.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH,
                PanelLayout.STAGE_HEIGHT);
            blocker.graphics.endFill();
            overlayLayer.addChild(blocker);

            var width:Number = 900;
            var height:Number = 330;
            var dialog:Sprite = new Sprite();
            dialog.x = (PanelLayout.STAGE_WIDTH - width) / 2;
            dialog.y = (PanelLayout.STAGE_HEIGHT - height) / 2;
            dialog.graphics.lineStyle(3, PanelTheme.GOLD);
            dialog.graphics.beginFill(PanelTheme.PANEL, 1.0);
            dialog.graphics.drawRoundRect(0, 0, width, height, 10, 10);
            dialog.graphics.endFill();
            overlayLayer.addChild(dialog);

            VectorTextRenderer.addText(dialog, "RECORDING BINDING", 36, 26, 18,
                PanelTheme.GOLD, true, width - 72, 28);
            VectorTextRenderer.addText(dialog,
                control == null ? "WAITING FOR INPUT" :
                    VectorTextRenderer.fit(String(control.label), 64),
                36, 72, 28, PanelTheme.TEXT, true, width - 72, 42);

            var flags:uint = control == null ? 0 : uint(control.flags);
            var keyboard:Boolean = (flags & 256) != 0;
            var mouse:Boolean = (flags & 512) != 0;
            var controller:Boolean = (flags & 1024) != 0;
            var prompt:String = controller && !keyboard && !mouse ?
                "RECORD THE NEW DIRECTINPUT CONTROL" :
                (keyboard && !mouse && !controller ? "PRESS A KEY OR KEY CHORD" :
                    "PRESS OR MOVE THE NEW INPUT");
            VectorTextRenderer.addText(dialog, prompt, 36, 132, 19,
                PanelTheme.CYAN, true, width - 72, 30);
            if (control != null && String(control.description).length > 0) {
                VectorTextRenderer.addText(dialog, String(control.description),
                    36, 170, 15, PanelTheme.MUTED_TEXT, false,
                    width - 72, 54, true);
            }
            VectorTextRenderer.addText(dialog,
                "INPUT CAPTURE IS ACTIVE. NAVIGATION IS PAUSED UNTIL AN INPUT IS RECORDED.",
                36, 238, 15, PanelTheme.MUTED_TEXT, false, width - 72, 24);
            var clearable:Boolean = control != null &&
                (uint(control.flags) & 4096) != 0;
            var captureActions:String = clearable &&
                inputMode != "controller" ?
                "BACKSPACE  CLEAR BINDING    ESC  CANCEL CAPTURE" :
                ((inputMode == "controller" ? "B" : "ESC") +
                    "  CANCEL CAPTURE");
            VectorTextRenderer.addText(dialog, captureActions,
                36, 286, 15, PanelTheme.DIM_TEXT, true, width - 72, 22);
        }

        public function openChoice(current:Object, control:Object):void
        {
            if (current == null || control == null || control.choiceOptions == null ||
                control.choiceOptions.length == 0 || !Boolean(control.available)) return;
            closeRecordCollection();
            openChoiceModuleId = String(current.moduleId);
            openChoicePageId = String(current.pageId);
            openChoiceControlId = String(control.controlId);
            openChoiceControl = control;
            choiceCursor = 0;
            for (var index:int = 0; index < control.choiceOptions.length; ++index) {
                if (Number(control.choiceOptions[index].value) ==
                    Number(control.integerValue)) {
                    choiceCursor = index;
                    break;
                }
            }
            normalizeChoiceWindow();
        }

        public function closeChoice():void
        {
            openChoiceModuleId = "";
            openChoicePageId = "";
            openChoiceControlId = "";
            openChoiceControl = null;
            choiceCursor = 0;
            choiceFirstVisible = 0;
            closeRecordCollection();
        }

        private function drawPinnedContext(model:Object, state:MenuSelectionState,
            current:Object):Number
        {
            var pinned:Array = state.pinnedControls(current);
            if (pinned.length == 0) return 0;
            var gap:Number = 8;
            var width:Number = (PanelLayout.CONTROL_ROW_WIDTH -
                gap * (pinned.length - 1)) / pinned.length;
            for (var index:int = 0; index < pinned.length; ++index) {
                var entry:Object = pinned[index];
                var control:Object = entry.control;
                var card:Sprite = new Sprite();
                var selected:Boolean = state.focusRegion ==
                    PanelLayout.FOCUS_CONTROLS &&
                    int(entry.index) == state.selectedRow;
                var enabled:Boolean = Boolean(control.available) &&
                    (uint(control.flags) & 1) == 0;
                var capturing:Boolean = model != null &&
                    Boolean(model.bindingCaptureActive) &&
                    String(model.captureModuleId) == String(current.moduleId) &&
                    String(model.capturePageId) == String(current.pageId) &&
                    String(model.captureControlId) == String(control.controlId);
                card.x = index * (width + gap);
                card.graphics.lineStyle(selected ? 2 : 1,
                    selected ? PanelTheme.GOLD : PanelTheme.CYAN);
                card.graphics.beginFill(selected ? PanelTheme.ROW_SELECTED :
                    PanelTheme.ROW_EVEN, 1.0);
                card.graphics.drawRoundRect(0, 0, width,
                    PINNED_CONTEXT_HEIGHT - 8, 7, 7);
                card.graphics.endFill();
                VectorTextRenderer.addText(card,
                    VectorTextRenderer.fit(String(control.label).toUpperCase(), 28),
                    12, 7, 12, PanelTheme.DIM_TEXT, true, width - 24, 18);
                VectorTextRenderer.addText(card,
                    VectorTextRenderer.fit(capturing ? "PRESS BUTTON OR SELECTOR" :
                        ControlWidgets.displayValue(control), 38),
                    12, 29, 16, enabled ? PanelTheme.TEXT :
                        PanelTheme.DISABLED_TEXT, true, width - 45, 24);
                VectorTextRenderer.addText(card,
                    uint(control.kind) == 5 ? "BIND" : "V", width - 35, 30,
                    13, enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_TEXT,
                    true, 28, 18);
                card.buttonMode = enabled;
                card.mouseChildren = false;
                rowLayer.addChild(card);
                var hitKind:String = uint(control.kind) == 8 ?
                    "recordCollection" : uint(control.kind) == 3 ?
                    "choice" : "activate";
                hits.register(card, hitKind, control, int(entry.index));
            }
            return PINNED_CONTEXT_HEIGHT;
        }

        public function openRecordCollection(current:Object, control:Object):void
        {
            if (current == null || control == null || control.recordItems == null ||
                !Boolean(control.available)) return;
            openChoiceModuleId = "";
            openChoicePageId = "";
            openChoiceControlId = "";
            openChoiceControl = null;
            openRecordModuleId = String(current.moduleId);
            openRecordPageId = String(current.pageId);
            openRecordControlId = String(control.controlId);
            openRecordControl = control;
            recordCursor = 0;
            for (var index:int = 0; index < control.recordItems.length; ++index) {
                if (String(control.recordItems[index].recordId) ==
                    String(control.stringValue)) {
                    recordCursor = index;
                    break;
                }
            }
            normalizeRecordWindow();
        }

        public function closeRecordCollection():void
        {
            openRecordModuleId = "";
            openRecordPageId = "";
            openRecordControlId = "";
            openRecordControl = null;
            recordCursor = 0;
            recordFirstVisible = 0;
        }

        public function get recordCollectionIsOpen():Boolean
        {
            return openRecordControl != null;
        }

        public function moveRecordCollection(direction:int):void
        {
            if (!recordCollectionIsOpen || openRecordControl.recordItems == null ||
                openRecordControl.recordItems.length == 0) return;
            recordCursor = Math.max(0, Math.min(
                openRecordControl.recordItems.length - 1,
                recordCursor + direction));
            normalizeRecordWindow();
        }

        public function selectedRecordItem():Object
        {
            if (!recordCollectionIsOpen || openRecordControl.recordItems == null ||
                recordCursor < 0 ||
                recordCursor >= openRecordControl.recordItems.length) return null;
            return {"control":openRecordControl,
                "item":openRecordControl.recordItems[recordCursor]};
        }

        private function normalizeRecordWindow():void
        {
            if (recordCursor < recordFirstVisible) recordFirstVisible = recordCursor;
            if (recordCursor >= recordFirstVisible + 8) {
                recordFirstVisible = recordCursor - 7;
            }
        }

        public function get choiceIsOpen():Boolean
        {
            return openChoiceControl != null;
        }

        public function moveChoice(direction:int):void
        {
            if (!choiceIsOpen || openChoiceControl.choiceOptions == null ||
                openChoiceControl.choiceOptions.length == 0) return;
            choiceCursor = Math.max(0, Math.min(
                openChoiceControl.choiceOptions.length - 1,
                choiceCursor + direction));
            normalizeChoiceWindow();
        }

        public function selectedChoice():Object
        {
            if (!choiceIsOpen || openChoiceControl.choiceOptions == null ||
                choiceCursor < 0 ||
                choiceCursor >= openChoiceControl.choiceOptions.length) return null;
            return {"control":openChoiceControl,
                "option":openChoiceControl.choiceOptions[choiceCursor]};
        }

        private function normalizeChoiceWindow():void
        {
            if (choiceCursor < choiceFirstVisible) choiceFirstVisible = choiceCursor;
            if (choiceCursor >= choiceFirstVisible +
                PanelLayout.VISIBLE_CHOICE_OPTIONS) {
                choiceFirstVisible = choiceCursor -
                    PanelLayout.VISIBLE_CHOICE_OPTIONS + 1;
            }
        }

        public function setGridTier(channelId:String, tierId:String):void
        {
            activeGridTiers[channelId] = tierId;
        }

        public function showBindingTooltip(stageX:Number, stageY:Number,
            value:String):void
        {
            if (tooltipLayer == null || value.length <= 38) {
                hideTooltip();
                return;
            }
            clearLayer(tooltipLayer, true);
            var width:Number = Math.min(780, Math.max(360,
                value.length * 9 + 28));
            var tip:Sprite = new Sprite();
            tip.x = Math.max(PanelLayout.SAFE_MARGIN,
                Math.min(PanelLayout.STAGE_WIDTH - PanelLayout.SAFE_MARGIN - width,
                    stageX + 18));
            tip.y = Math.max(PanelLayout.SAFE_MARGIN,
                Math.min(PanelLayout.HELP_Y - 52, stageY + 18));
            tip.graphics.lineStyle(1, PanelTheme.CYAN);
            tip.graphics.beginFill(PanelTheme.PANEL, 0.98);
            tip.graphics.drawRoundRect(0, 0, width, 42, 6, 6);
            tip.graphics.endFill();
            VectorTextRenderer.addText(tip, value, 14, 10, 16,
                PanelTheme.TEXT, true, width - 28, 24);
            tooltipLayer.addChild(tip);
        }

        public function hideTooltip():void
        {
            if (tooltipLayer != null) clearLayer(tooltipLayer, true);
        }

        public function showThrottleGuidance():void
        {
            guidanceActive = true;
        }

        public function hideGuidance():void
        {
            guidanceActive = false;
        }

        public function get guidanceIsOpen():Boolean
        {
            return guidanceActive;
        }

        private function drawThrottleGuidance():void
        {
            var blocker:Sprite = new Sprite();
            blocker.graphics.beginFill(PanelTheme.BACKGROUND, 0.78);
            blocker.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH,
                PanelLayout.STAGE_HEIGHT);
            blocker.graphics.endFill();
            overlayLayer.addChild(blocker);
            hits.register(blocker, "guidanceDismiss", null, 0);

            var width:Number = 820;
            var height:Number = 300;
            var dialog:Sprite = new Sprite();
            dialog.x = (PanelLayout.STAGE_WIDTH - width) / 2;
            dialog.y = (PanelLayout.STAGE_HEIGHT - height) / 2;
            dialog.graphics.lineStyle(2, PanelTheme.CYAN);
            dialog.graphics.beginFill(PanelTheme.PANEL, 1.0);
            dialog.graphics.drawRect(0, 0, width, height);
            dialog.graphics.endFill();
            overlayLayer.addChild(dialog);
            VectorTextRenderer.addText(dialog, "THROTTLE ZONES ARE READ-ONLY HERE",
                36, 28, 23, PanelTheme.CYAN, true, width - 72, 34);
            VectorTextRenderer.addText(dialog,
                "This graph mirrors the positional zones and landmarks from Throttle Setup. Edit them there so one authoritative tuning surface controls both pages.",
                36, 82, 17, PanelTheme.MUTED_TEXT, false,
                width - 72, 74, true);

            var openButton:Sprite = new Sprite();
            openButton.x = 36;
            openButton.y = 214;
            openButton.graphics.lineStyle(2, PanelTheme.CYAN);
            openButton.graphics.beginFill(PanelTheme.SELECTED_FILL);
            openButton.graphics.drawRect(0, 0, 470, 52);
            openButton.graphics.endFill();
            openButton.buttonMode = true;
            VectorTextRenderer.addText(openButton, "OPEN THROTTLE SETUP",
                18, 15, 17, PanelTheme.TEXT, true);
            dialog.addChild(openButton);
            hits.register(openButton, "guidanceOpenThrottle", null, 0);

            var stayButton:Sprite = new Sprite();
            stayButton.x = 526;
            stayButton.y = 214;
            stayButton.graphics.lineStyle(1, PanelTheme.BORDER);
            stayButton.graphics.beginFill(PanelTheme.BUTTON_FILL);
            stayButton.graphics.drawRect(0, 0, 258, 52);
            stayButton.graphics.endFill();
            stayButton.buttonMode = true;
            VectorTextRenderer.addText(stayButton, "STAY HERE",
                18, 15, 17, PanelTheme.TEXT, true);
            dialog.addChild(stayButton);
            hits.register(stayButton, "guidanceDismiss", null, 1);
        }

        public function hasGrid(current:Object):Boolean
        {
            return gridComponent(current) != null;
        }

        public function moveGridSelection(model:Object, current:Object,
            direction:int, selectColumn:Function):Boolean
        {
            var component:Object = gridComponent(current);
            if (component == null || component.columns.length == 0) return false;
            var selected:int = 0;
            for (var index:int = 0; index < component.columns.length; ++index) {
                if (String(component.columns[index].columnId) ==
                    String(model.selectedGridColumnId)) {
                    selected = index;
                    break;
                }
            }
            selected = (selected + (direction < 0 ? -1 : 1) +
                component.columns.length) % component.columns.length;
            selectColumn(model, current, component,
                String(component.columns[selected].columnId));
            return true;
        }

        public function adjustGrid(model:Object, current:Object, direction:int,
            sendCompound:Function):Boolean
        {
            var component:Object = gridComponent(current);
            if (component == null || component.columns.length == 0) return false;
            var column:Object = component.columns[0];
            for (var index:int = 0; index < component.columns.length; ++index) {
                if (String(component.columns[index].columnId) ==
                    String(model.selectedGridColumnId)) {
                    column = component.columns[index];
                    break;
                }
            }
            var filled:int = 0;
            var green:int = 0;
            var interactive:Boolean = false;
            for (var pip:int = 0; pip < column.segments.length; ++pip) {
                var tier:int = int(column.segments[pip].tierIndex);
                if (tier > 0) ++filled;
                if (tier == 1) ++green;
                interactive = interactive ||
                    Boolean(column.segments[pip].interactive);
            }
            if (!interactive) return false;
            if (direction < 0) {
                if (filled == 0) return true;
                sendCompound(model, current, component, 1,
                    String(column.columnId), "", uint(filled - 1));
            } else {
                if (filled >= int(column.maximumSegments)) return true;
                sendCompound(model, current, component, 0,
                    String(column.columnId),
                    String(component.tiers[1].tierId), uint(green + 1));
            }
            return true;
        }

        public function selectedGridControl(model:Object, current:Object):Object
        {
            var component:Object = gridComponent(current);
            if (component == null || component.columns == null ||
                component.columns.length == 0 || current == null ||
                current.controls == null) return null;
            var column:Object = component.columns[0];
            for (var columnIndex:int = 0;
                 columnIndex < component.columns.length; ++columnIndex) {
                if (String(component.columns[columnIndex].columnId) ==
                    String(model.selectedGridColumnId)) {
                    column = component.columns[columnIndex];
                    break;
                }
            }
            var association:String = column.associatedControlId == null ? "" :
                String(column.associatedControlId);
            if (association.length == 0) return null;
            for (var controlIndex:int = 0;
                 controlIndex < current.controls.length; ++controlIndex) {
                var control:Object = current.controls[controlIndex];
                if (String(control.controlId) == association &&
                    uint(control.kind) == 3) return control;
            }
            return null;
        }

        private function gridComponent(current:Object):Object
        {
            if (current == null || current.liveComponents == null) return null;
            for (var index:int = 0; index < current.liveComponents.length; ++index) {
                var component:Object = current.liveComponents[index];
                if (uint(component.kind) == 2 && component.columns != null &&
                    component.tiers != null) return component;
            }
            return null;
        }

        private function drawLiveComponents(current:Object,
            topOffset:Number = 0):Number
        {
            if (current.liveComponents == null || current.liveComponents.length == 0) {
                return 0;
            }
            if (Boolean(current.compositionEnhanced) &&
                current.compositionNodes != null) {
                for (var semanticIndex:int = 0;
                     semanticIndex < current.compositionNodes.length;
                     ++semanticIndex) {
                    if (uint(current.compositionNodes[semanticIndex].kind) == 10) {
                        return 0;
                    }
                }
            }
            var grid:Object = gridComponent(current);
            if (grid != null) {
                drawSegmentedGrid(grid, current);
                return 318;
            }

            var y:Number = topOffset;
            var rangeColumn:int = 0;
            var rendered:int = 0;
            var eligible:int = 0;
            var ordered:Array = current.liveComponents.concat();
            ordered.sort(function(left:Object, right:Object):Number {
                var leftFlags:uint = uint(left.interactionFlags);
                var rightFlags:uint = uint(right.interactionFlags);
                var leftPinned:int = (leftFlags & LIVE_PINNED) != 0 ? 0 : 1;
                var rightPinned:int = (rightFlags & LIVE_PINNED) != 0 ? 0 : 1;
                if (leftPinned != rightPinned) return leftPinned - rightPinned;
                var leftSecondary:int = (leftFlags & LIVE_SECONDARY) != 0 ? 1 : 0;
                var rightSecondary:int = (rightFlags & LIVE_SECONDARY) != 0 ? 1 : 0;
                return leftSecondary - rightSecondary;
            });
            for (var countIndex:int = 0;
                 countIndex < ordered.length; ++countIndex) {
                var candidateKind:uint = uint(ordered[countIndex].kind);
                if (candidateKind == 0 || candidateKind == 1 ||
                    candidateKind == 3 || candidateKind == 4) ++eligible;
            }
            for (var index:int = 0; index < ordered.length &&
                 rendered < LIVE_COMPONENT_LIMIT; ++index) {
                var component:Object = ordered[index];
                var kind:uint = uint(component.kind);
                if (kind != 0 && kind != 1 && kind != 3 && kind != 4) continue;
                var presentation:uint = uint(component.interactionFlags);
                var pinned:Boolean = (presentation & LIVE_PINNED) != 0;
                var collapsed:Boolean =
                    (presentation & LIVE_COLLAPSED) != 0;
                var expanded:Boolean = isLiveExpanded(current, component);
                if ((kind == 1 || kind == 3 || kind == 4) &&
                    rangeColumn != 0) {
                    y += RANGE_CARD_HEIGHT + 6;
                    rangeColumn = 0;
                }
                if (collapsed) {
                    drawLiveDisclosure(current, component, y, expanded);
                    y += LIVE_DISCLOSURE_HEIGHT + 4;
                    if (!expanded) {
                        ++rendered;
                        continue;
                    }
                }
                if (pinned && rangeColumn != 0) {
                    y += RANGE_CARD_HEIGHT + 6;
                    rangeColumn = 0;
                }
                var height:Number = kind == 0 ?
                    (pinned ? PINNED_RANGE_CARD_HEIGHT : RANGE_CARD_HEIGHT) :
                    (kind == 3 ? RADIAL_CARD_HEIGHT :
                        (kind == 4 ? HEAD_POSE_CARD_HEIGHT :
                            PLOT_CARD_HEIGHT));
                if (y + height > topOffset + LIVE_DASHBOARD_MAX_HEIGHT) break;
                if (kind == 0) {
                    if (pinned) {
                        drawRangeMeter(component, 0, y,
                            PanelLayout.CONTROL_ROW_WIDTH - 8, height,
                            true, current);
                        y += height + 6;
                    } else {
                        drawRangeMeter(component, rangeColumn * 690, y, 674,
                            height, true, current);
                        ++rangeColumn;
                        if (rangeColumn == 2) {
                            rangeColumn = 0;
                            y += RANGE_CARD_HEIGHT + 6;
                        }
                    }
                } else if (kind == 1) {
                    drawTelemetryPlot(component, 0, y,
                        PanelLayout.CONTROL_ROW_WIDTH - 8, PLOT_CARD_HEIGHT);
                    y += PLOT_CARD_HEIGHT + 6;
                } else if (kind == 3) {
                    drawRadialResponse(component, current, 0, y,
                        PanelLayout.CONTROL_ROW_WIDTH - 8,
                        RADIAL_CARD_HEIGHT);
                    y += RADIAL_CARD_HEIGHT + 6;
                } else {
                    drawHeadPose(component, current, 0, y,
                        PanelLayout.CONTROL_ROW_WIDTH - 8,
                        HEAD_POSE_CARD_HEIGHT);
                    y += HEAD_POSE_CARD_HEIGHT + 6;
                }
                ++rendered;
            }
            if (rangeColumn != 0) y += RANGE_CARD_HEIGHT + 6;
            if (rendered < eligible) {
                VectorTextRenderer.addText(rowLayer,
                    "+" + (eligible - rendered) +
                    " LIVE CHANNELS HIDDEN BY THE BOUNDED PAGE LAYOUT",
                    8, Math.min(y, topOffset + LIVE_DASHBOARD_MAX_HEIGHT - 22), 13,
                    PanelTheme.WARNING, true);
                y = Math.min(topOffset + LIVE_DASHBOARD_MAX_HEIGHT, y + 24);
            }
            return y - topOffset;
        }

        private function liveKey(moduleId:String, pageId:String,
            channelId:String):String
        {
            return moduleId + "\n" + pageId + "\n" + channelId;
        }

        private function isLiveExpanded(current:Object,
            component:Object):Boolean
        {
            return Boolean(expandedLive[liveKey(String(current.moduleId),
                String(current.pageId), String(component.channelId))]);
        }

        private function drawLiveDisclosure(current:Object, component:Object,
            y:Number, expanded:Boolean):void
        {
            var disclosure:Sprite = new Sprite();
            disclosure.y = y;
            disclosure.graphics.lineStyle(1, PanelTheme.BORDER);
            disclosure.graphics.beginFill(PanelTheme.BUTTON_FILL);
            disclosure.graphics.drawRoundRect(0, 0,
                PanelLayout.CONTROL_ROW_WIDTH - 8,
                LIVE_DISCLOSURE_HEIGHT, 6, 6);
            disclosure.graphics.endFill();
            disclosure.buttonMode = true;
            VectorTextRenderer.addText(disclosure,
                (expanded ? "HIDE  " : "SHOW  ") +
                String(component.title).toUpperCase(), 14, 8, 14,
                PanelTheme.CYAN, true,
                PanelLayout.CONTROL_ROW_WIDTH - 40, 20);
            rowLayer.addChild(disclosure);
            hits.register(disclosure, "liveDisclosure", {
                "moduleId":String(current.moduleId),
                "pageId":String(current.pageId),
                "channelId":String(component.channelId)
            }, 0);
        }

        public function toggleLiveDisclosure(payload:Object):void
        {
            if (payload == null) return;
            var key:String = liveKey(String(payload.moduleId),
                String(payload.pageId), String(payload.channelId));
            expandedLive[key] = !Boolean(expandedLive[key]);
        }

        public function refreshLive(current:Object):Boolean
        {
            if (current == null || liveLayer == null ||
                gridComponent(current) != null) return false;
            clearLayer(liveLayer, true);
            for (var placementIndex:int = 0;
                 placementIndex < livePlacements.length; ++placementIndex) {
                var placement:Object = livePlacements[placementIndex];
                var component:Object = null;
                for (var componentIndex:int = 0;
                     componentIndex < current.liveComponents.length;
                     ++componentIndex) {
                    if (String(current.liveComponents[componentIndex].channelId) ==
                        String(placement.channelId)) {
                        component = current.liveComponents[componentIndex];
                        break;
                    }
                }
                if (component == null) continue;
                if (uint(placement.kind) == 0) {
                    drawRangeMeter(component, Number(placement.x),
                        Number(placement.y), Number(placement.width),
                        Number(placement.height), false, current);
                } else if (uint(placement.kind) == 1) {
                    drawTelemetryPlot(component, Number(placement.x),
                        Number(placement.y), Number(placement.width),
                        Number(placement.height), false);
                } else if (uint(placement.kind) == 3) {
                    drawRadialResponse(component, current,
                        Number(placement.x), Number(placement.y),
                        Number(placement.width), Number(placement.height),
                        false);
                } else if (uint(placement.kind) == 4) {
                    drawHeadPose(component, current,
                        Number(placement.x), Number(placement.y),
                        Number(placement.width), Number(placement.height),
                        false);
                }
            }
            return true;
        }

        private function drawHeadPose(component:Object, current:Object,
            x:Number, y:Number, width:Number, height:Number,
            record:Boolean = true):void
        {
            if (record) livePlacements.push({
                "channelId":String(component.channelId), "kind":4,
                "x":x, "y":y, "width":width, "height":height
            });
            var card:Sprite = new Sprite();
            card.x = x;
            card.y = y;
            card.graphics.lineStyle(1, PanelTheme.ROW_BORDER);
            card.graphics.beginFill(PanelTheme.ROW_EVEN);
            card.graphics.drawRoundRect(0, 0, width, height, 8, 8);
            card.graphics.endFill();
            liveLayer.addChild(card);

            VectorTextRenderer.addText(card,
                String(component.title).toUpperCase(), 18, 10, 17,
                PanelTheme.TEXT, true, 430, 24);
            VectorTextRenderer.addText(card,
                "BLUE  NEGATIVE", 520, 12, 12,
                PanelTheme.AXIS_NEGATIVE, true, 145, 18);
            VectorTextRenderer.addText(card,
                "GREEN  POSITIVE", 680, 12, 12,
                PanelTheme.TIER_GREEN, true, 155, 18);
            VectorTextRenderer.addText(card,
                "YELLOW  LIVE", 850, 12, 12,
                PanelTheme.TIER_YELLOW, true, 130, 18);
            var deadzoneControl:Object = liveControlById(current,
                String(component.deadzoneControlId));
            var recenterControl:Object = liveControlById(current,
                String(component.recenterControlId));
            drawHeadPoseAction(card, current, recenterControl,
                "RECENTER", width - 270, 6, 150);
            VectorTextRenderer.addText(card, liveStatus(component),
                width - 100, 12, 13,
                Boolean(component.available) ? PanelTheme.CYAN :
                    PanelTheme.WARNING, true, 82, 18);

            var axes:Array = component.axes == null ? [] : component.axes;
            var count:int = Math.min(3, axes.length);
            if (count == 0) {
                VectorTextRenderer.addText(card, "WAITING FOR HEAD POSE DATA",
                    20, 68, 15, PanelTheme.WARNING, true,
                    width - 40, 24);
                return;
            }
            var gap:Number = 8;
            var panelWidth:Number = (width - 20 - gap * (count - 1)) / count;
            for (var index:int = 0; index < count; ++index) {
                var panel:Sprite = new Sprite();
                panel.x = 10 + index * (panelWidth + gap);
                panel.y = 42;
                panel.graphics.lineStyle(1, PanelTheme.BORDER);
                panel.graphics.beginFill(PanelTheme.BUTTON_FILL, 0.7);
                panel.graphics.drawRoundRect(0, 0, panelWidth,
                    height - 112, 7, 7);
                panel.graphics.endFill();
                card.addChild(panel);
                drawHeadPoseAxis(panel, current, axes[index],
                    deadzoneControl, panelWidth, height - 112);
            }
            drawHeadPoseSharedTuning(card, current, deadzoneControl,
                "DEADZONE", (width - 560) * 0.5, height - 61, 560);
        }

        private function drawHeadPoseSharedTuning(target:Sprite,
            current:Object, control:Object, label:String, x:Number,
            y:Number, width:Number):void
        {
            if (control == null) return;
            var focused:Boolean = headPoseControlFocused(current, control);
            var group:Sprite = new Sprite();
            group.x = x;
            group.y = y;
            VectorTextRenderer.addText(group, label, 0, 0, 11,
                focused ? PanelTheme.GOLD : PanelTheme.TIER_RED,
                true, width, 16);
            var widget:Sprite = new Sprite();
            widget.x = 0;
            widget.y = 18;
            if (focused) {
                widget.graphics.lineStyle(1, PanelTheme.GOLD, 0.9);
                widget.graphics.drawRoundRect(-7, -4, width + 14, 41,
                    5, 5);
            }
            var hit:Sprite = ControlWidgets.drawCompactSliderOnly(
                widget, control, width);
            group.addChild(widget);
            target.addChild(group);
            hits.register(hit, "slider", control,
                findControlIndex(current, control));
        }

        private function drawHeadPoseAction(target:Sprite, current:Object,
            control:Object, label:String, x:Number, y:Number,
            width:Number):void
        {
            if (control == null) return;
            var enabled:Boolean = Boolean(control.available) &&
                (uint(control.flags) & 1) == 0;
            var focused:Boolean = headPoseControlFocused(current, control);
            var button:Sprite = new Sprite();
            button.x = x;
            button.y = y;
            button.buttonMode = enabled;
            button.graphics.lineStyle(1, focused ? PanelTheme.GOLD :
                (enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET));
            button.graphics.beginFill(focused ? PanelTheme.SELECTED_FILL :
                PanelTheme.BUTTON_FILL, 0.95);
            button.graphics.drawRoundRect(0, 0, width, 30, 5, 5);
            button.graphics.endFill();
            VectorTextRenderer.addText(button, label, 12, 6, 13,
                enabled ? PanelTheme.TEXT : PanelTheme.DISABLED_TEXT,
                true, width - 24, 18);
            target.addChild(button);
            hits.register(button, enabled ? "activate" : "disabled",
                control, findControlIndex(current, control));
        }

        private function drawHeadPoseAxis(panel:Sprite, current:Object,
            axis:Object, deadzoneControl:Object, width:Number,
            height:Number):void
        {
            var available:Boolean = Boolean(axis.liveAvailable);
            var label:String = String(axis.label).toUpperCase();
            var output:Number = Number(axis.outputDegrees);
            var tracker:Number = Number(axis.trackerDegrees);
            var enabledControl:Object = liveControlById(current,
                String(axis.enabledControlId));
            var invertedControl:Object = liveControlById(current,
                String(axis.invertedControlId));
            var active:Boolean = enabledControl == null ||
                Boolean(enabledControl.booleanValue);
            var sensitivityControl:Object = liveControlById(current,
                String(axis.sensitivityControlId));
            var sensitivity:Number = numericLiveValue(sensitivityControl, 1);
            var deadzone:Number = Math.max(0,
                numericLiveValue(deadzoneControl, 0));
            var direction:Number = invertedControl != null &&
                Boolean(invertedControl.booleanValue) ? -1 : 1;
            var gatedInput:Number = tracker * sensitivity * direction;
            var insideDeadzone:Boolean = deadzone > 0 &&
                Math.abs(gatedInput) <= deadzone;
            VectorTextRenderer.addText(panel, label, 12, 8, 15,
                available && active ? PanelTheme.TEXT :
                    PanelTheme.DISABLED_TEXT,
                true, width - 24, 20);
            VectorTextRenderer.addText(panel,
                !active ? "AXIS DISABLED" : (available ?
                    (insideDeadzone ?
                        ("DEADZONE  " + signedDegrees(gatedInput) +
                        " / " + deadzone.toFixed(1) + " DEG") :
                        ("MAPPED  " + signedDegrees(output) +
                        "    RAW  " + signedDegrees(tracker))) :
                    "NO LIVE SIGNAL"), 12, 29, 12,
                available && active ? (insideDeadzone ?
                    PanelTheme.TIER_RED : PanelTheme.TIER_YELLOW) :
                    PanelTheme.WARNING, true, width - 24, 18);

            var minimumControl:Object = liveControlById(current,
                String(axis.minimumControlId));
            var centerControl:Object = liveControlById(current,
                String(axis.centerControlId));
            var maximumControl:Object = liveControlById(current,
                String(axis.maximumControlId));
            var minimum:Number = numericLiveValue(minimumControl, -90);
            var center:Number = numericLiveValue(centerControl, 0);
            var maximum:Number = numericLiveValue(maximumControl, 90);
            var view:uint = uint(axis.view);
            var graphX:Number = 108;
            var graphY:Number = 215;
            var radius:Number = 82;
            var wireColor:uint = available && active ? PanelTheme.CYAN :
                PanelTheme.DISABLED_WIDGET;

            panel.graphics.lineStyle(1, PanelTheme.BORDER, 0.65);
            panel.graphics.drawCircle(graphX, graphY, radius + 10);
            drawPoseDatum(panel, graphX, graphY, radius, view, wireColor);
            drawPoseArc(panel, graphX, graphY, radius + 9,
                poseScreenAngle(view, minimum),
                poseScreenAngle(view, center),
                PanelTheme.AXIS_NEGATIVE, 3);
            drawPoseArc(panel, graphX, graphY, radius + 9,
                poseScreenAngle(view, center),
                poseScreenAngle(view, maximum),
                PanelTheme.TIER_GREEN, 3);
            drawPoseTick(panel, graphX, graphY, radius + 9,
                poseScreenAngle(view, 0), PanelTheme.DIM_TEXT, 7);
            drawPoseTick(panel, graphX, graphY, radius + 9,
                poseScreenAngle(view, center), PanelTheme.TEXT, 10);
            if (deadzoneControl != null) {
                drawDeadzoneGauge(panel, graphX, graphY, radius - 18,
                    deadzoneGaugeCenterAngle(view), gatedInput, deadzone,
                    Math.max(0.0001, Number(deadzoneControl.maximum)),
                    available && active, insideDeadzone);
            }

            if (view == 0) {
                drawTopHead(panel, graphX, graphY, wireColor);
            } else if (view == 1) {
                drawProfileHead(panel, graphX, graphY, wireColor);
            } else {
                drawArtificialHorizon(panel, graphX, graphY, radius - 14,
                    poseScreenAngle(view, output), wireColor);
            }
            if (available && active && !insideDeadzone) {
                drawPoseLiveVector(panel, graphX, graphY, radius + 1,
                    poseScreenAngle(view, output),
                    PanelTheme.TIER_YELLOW);
                drawPoseLiveMarker(panel, graphX, graphY, radius + 9,
                    poseScreenAngle(view, output),
                    PanelTheme.TIER_YELLOW, 1.0);
            }

            var tuneX:Number = 216;
            var tuneWidth:Number = Math.max(174, width - tuneX - 12);
            drawHeadPoseToggle(panel, current, enabledControl,
                "ENABLE", tuneX, 58, 104);
            drawHeadPoseToggle(panel, current, invertedControl,
                "INVERT", tuneX + 116, 58, 104);
            drawHeadPoseTuning(panel, current, sensitivityControl,
                "SENSITIVITY", tuneX, 108, tuneWidth);
            drawHeadPoseTuning(panel, current, minimumControl,
                view == 1 ? "DOWN LIMIT" :
                    (view == 2 ? "LEFT BANK" : "LEFT LIMIT"),
                tuneX, 174, tuneWidth);
            drawHeadPoseTuning(panel, current, centerControl,
                view == 2 ? "LEVEL / CENTER" : "NEUTRAL / CENTER",
                tuneX, 240, tuneWidth);
            drawHeadPoseTuning(panel, current, maximumControl,
                view == 1 ? "UP LIMIT" :
                    (view == 2 ? "RIGHT BANK" : "RIGHT LIMIT"),
                tuneX, 306, tuneWidth);
        }

        private function drawHeadPoseToggle(panel:Sprite, current:Object,
            control:Object, label:String, x:Number, y:Number,
            availableWidth:Number):void
        {
            var focused:Boolean = headPoseControlFocused(current, control);
            VectorTextRenderer.addText(panel, label, x, y, 11,
                focused ? PanelTheme.GOLD : (control == null ?
                    PanelTheme.DISABLED_TEXT : PanelTheme.MUTED_TEXT),
                true, availableWidth, 16);
            if (control == null) return;
            var widget:Sprite = new Sprite();
            widget.x = x;
            widget.y = y + 18;
            if (focused) {
                widget.graphics.lineStyle(1, PanelTheme.GOLD, 0.9);
                widget.graphics.drawRoundRect(-6, -5, 110, 36, 5, 5);
            }
            var hit:Sprite = ControlWidgets.drawCompactToggleOnly(
                widget, control);
            panel.addChild(widget);
            hits.register(hit, "activate", control,
                findControlIndex(current, control));
        }

        private function drawHeadPoseTuning(panel:Sprite, current:Object,
            control:Object, label:String, x:Number, y:Number,
            availableWidth:Number):void
        {
            var focused:Boolean = headPoseControlFocused(current, control);
            VectorTextRenderer.addText(panel, label, x, y, 11,
                focused ? PanelTheme.GOLD : (control == null ?
                    PanelTheme.DISABLED_TEXT : PanelTheme.MUTED_TEXT),
                true, availableWidth, 16);
            if (control == null) return;
            var widget:Sprite = new Sprite();
            widget.x = x;
            widget.y = y + 18;
            if (focused) {
                widget.graphics.lineStyle(1, PanelTheme.GOLD, 0.9);
                widget.graphics.drawRoundRect(-7, -4,
                    availableWidth + 14, 41, 5, 5);
            }
            var hit:Sprite = ControlWidgets.drawCompactSliderOnly(
                widget, control, availableWidth);
            panel.addChild(widget);
            hits.register(hit, "slider", control,
                findControlIndex(current, control));
        }

        private function headPoseControlFocused(current:Object,
            control:Object):Boolean
        {
            if (activeSelectionState == null || control == null ||
                activeSelectionState.focusRegion !=
                    PanelLayout.FOCUS_CONTROLS) return false;
            return activeSelectionState.selectedRow ==
                findControlIndex(current, control);
        }

        private function headPoseOwnsControl(current:Object,
            controlId:String):Boolean
        {
            if (current == null || current.liveComponents == null ||
                controlId.length == 0) return false;
            for each (var component:Object in current.liveComponents) {
                if (uint(component.kind) != 4) continue;
                if (controlId == String(component.recenterControlId))
                    return true;
                if (controlId == String(component.deadzoneControlId))
                    return true;
                if (component.axes == null) continue;
                for each (var axis:Object in component.axes) {
                    if (controlId == String(axis.enabledControlId) ||
                        controlId == String(axis.invertedControlId) ||
                        controlId == String(axis.sensitivityControlId) ||
                        controlId == String(axis.minimumControlId) ||
                        controlId == String(axis.centerControlId) ||
                        controlId == String(axis.maximumControlId)) return true;
                }
            }
            return false;
        }

        private function poseScreenAngle(view:uint, degrees:Number):Number
        {
            if (view == 1) return 90 - degrees;
            return -degrees;
        }

        private function deadzoneGaugeCenterAngle(view:uint):Number
        {
            // Pitch zero is the right-hand eye-level datum. Yaw and roll use
            // the upward forward/level datum.
            return view == 1 ? 90 : 0;
        }

        private function drawPoseDatum(target:Sprite, centerX:Number,
            centerY:Number, radius:Number, view:uint, color:uint):void
        {
            target.graphics.lineStyle(1, color, 0.32);
            if (view == 1) {
                target.graphics.moveTo(centerX - radius, centerY);
                target.graphics.lineTo(centerX - 14, centerY);
                target.graphics.moveTo(centerX + 14, centerY);
                target.graphics.lineTo(centerX + radius, centerY);
                VectorTextRenderer.addText(target, "EYE LEVEL",
                    centerX + radius - 64, centerY - 18, 9,
                    PanelTheme.DIM_TEXT, true, 62, 13);
            } else {
                target.graphics.moveTo(centerX, centerY - radius);
                target.graphics.lineTo(centerX, centerY - 17);
                target.graphics.moveTo(centerX, centerY + 17);
                target.graphics.lineTo(centerX, centerY + radius);
                target.graphics.moveTo(centerX - radius, centerY);
                target.graphics.lineTo(centerX - 17, centerY);
                target.graphics.moveTo(centerX + 17, centerY);
                target.graphics.lineTo(centerX + radius, centerY);
            }
        }

        private function drawPoseArc(target:Sprite, centerX:Number,
            centerY:Number, radius:Number, startDegrees:Number,
            endDegrees:Number, color:uint, thickness:Number):void
        {
            var span:Number = endDegrees - startDegrees;
            var steps:int = Math.max(1, Math.ceil(Math.abs(span) / 4));
            target.graphics.lineStyle(thickness, color, 0.95);
            for (var index:int = 0; index <= steps; ++index) {
                var degrees:Number = startDegrees + span * index / steps;
                var radians:Number = (degrees - 90) * Math.PI / 180;
                var pointX:Number = centerX + Math.cos(radians) * radius;
                var pointY:Number = centerY + Math.sin(radians) * radius;
                if (index == 0) target.graphics.moveTo(pointX, pointY);
                else target.graphics.lineTo(pointX, pointY);
            }
        }

        private function drawPoseTick(target:Sprite, centerX:Number,
            centerY:Number, radius:Number, degrees:Number, color:uint,
            length:Number):void
        {
            var radians:Number = (degrees - 90) * Math.PI / 180;
            target.graphics.lineStyle(2, color, 1.0);
            target.graphics.moveTo(centerX + Math.cos(radians) *
                (radius - length), centerY + Math.sin(radians) *
                (radius - length));
            target.graphics.lineTo(centerX + Math.cos(radians) *
                (radius + length), centerY + Math.sin(radians) *
                (radius + length));
        }

        private function drawPoseLiveMarker(target:Sprite, centerX:Number,
            centerY:Number, radius:Number, degrees:Number, color:uint,
            alpha:Number):void
        {
            var radians:Number = (degrees - 90) * Math.PI / 180;
            var markerX:Number = centerX + Math.cos(radians) * radius;
            var markerY:Number = centerY + Math.sin(radians) * radius;
            target.graphics.lineStyle(2, color, alpha);
            target.graphics.beginFill(color, 0.3 * alpha);
            target.graphics.drawCircle(markerX, markerY, 7);
            target.graphics.endFill();
        }

        private function drawPoseLiveVector(target:Sprite, centerX:Number,
            centerY:Number, radius:Number, degrees:Number, color:uint):void
        {
            var radians:Number = (degrees - 90) * Math.PI / 180;
            target.graphics.lineStyle(2, color, 0.82);
            target.graphics.moveTo(centerX, centerY);
            target.graphics.lineTo(centerX + Math.cos(radians) * radius,
                centerY + Math.sin(radians) * radius);
        }

        private function drawDeadzoneGauge(target:Sprite, centerX:Number,
            centerY:Number, radius:Number, centerAngle:Number,
            input:Number, deadzone:Number, maximumDeadzone:Number,
            available:Boolean, inside:Boolean):void
        {
            drawPoseArc(target, centerX, centerY, radius,
                centerAngle - 90, centerAngle + 90, PanelTheme.BORDER, 1);
            drawPoseTick(target, centerX, centerY, radius, centerAngle,
                PanelTheme.DIM_TEXT, 4);
            if (deadzone <= 0) return;
            var settingFraction:Number = Math.max(0, Math.min(1,
                deadzone / maximumDeadzone));
            var halfSpan:Number = 90 * settingFraction;
            drawPoseArc(target, centerX, centerY, radius,
                centerAngle - halfSpan, centerAngle + halfSpan,
                PanelTheme.TIER_RED, 3);
            drawPoseTick(target, centerX, centerY, radius,
                centerAngle - halfSpan,
                PanelTheme.TIER_RED, 5);
            drawPoseTick(target, centerX, centerY, radius,
                centerAngle + halfSpan,
                PanelTheme.TIER_RED, 5);
            if (!available) return;
            var fraction:Number = Math.max(-1,
                Math.min(1, input / Math.max(0.0001, deadzone)));
            var gaugeAngle:Number = centerAngle + fraction * halfSpan;
            drawPoseLiveMarker(target, centerX, centerY, radius,
                gaugeAngle, PanelTheme.TIER_RED, inside ? 1.0 : 0.42);
        }

        private function drawTopHead(target:Sprite, centerX:Number,
            centerY:Number, color:uint):void
        {
            var head:Sprite = new Sprite();
            head.x = centerX;
            head.y = centerY;
            // Minimal overhead pilot helmet: one readable shell, visor and
            // centerline rather than an anatomical mesh.
            head.graphics.lineStyle(2, color, 0.78);
            head.graphics.moveTo(0, -49);
            head.graphics.curveTo(-27, -44, -36, -22);
            head.graphics.curveTo(-43, 3, -31, 28);
            head.graphics.curveTo(-18, 40, 0, 43);
            head.graphics.curveTo(18, 40, 31, 28);
            head.graphics.curveTo(43, 3, 36, -22);
            head.graphics.curveTo(27, -44, 0, -49);

            head.graphics.lineStyle(1, color, 0.52);
            head.graphics.moveTo(-29, -25);
            head.graphics.curveTo(0, -11, 29, -25);
            head.graphics.moveTo(-32, -12);
            head.graphics.curveTo(-16, 3, 0, 5);
            head.graphics.curveTo(16, 3, 32, -12);
            head.graphics.moveTo(0, -44);
            head.graphics.lineTo(0, 35);
            head.graphics.moveTo(-24, 31);
            head.graphics.curveTo(0, 20, 24, 31);

            // Earcups and short neck opening keep the symbol recognisably a
            // pilot without adding facial clutter.
            head.graphics.lineStyle(2, color, 0.68);
            head.graphics.drawRoundRect(-43, -2, 9, 23, 4, 4);
            head.graphics.drawRoundRect(34, -2, 9, 23, 4, 4);
            head.graphics.lineStyle(1, color, 0.48);
            head.graphics.moveTo(-13, 39);
            head.graphics.lineTo(-10, 50);
            head.graphics.moveTo(13, 39);
            head.graphics.lineTo(10, 50);
            head.graphics.moveTo(-10, 50);
            head.graphics.curveTo(0, 46, 10, 50);
            head.graphics.drawCircle(0, 0, 4);
            target.addChild(head);
        }

        private function drawProfileHead(target:Sprite, centerX:Number,
            centerY:Number, color:uint):void
        {
            var head:Sprite = new Sprite();
            head.x = centerX;
            head.y = centerY;
            // Clean side-profile flight helmet shell.
            head.graphics.lineStyle(2, color, 0.80);
            head.graphics.moveTo(-13, 43);
            head.graphics.lineTo(-11, 29);
            head.graphics.curveTo(-38, 16, -38, -13);
            head.graphics.curveTo(-33, -40, -6, -47);
            head.graphics.curveTo(18, -44, 26, -25);
            head.graphics.lineTo(29, -15);
            head.graphics.lineTo(43, -4);
            head.graphics.lineTo(34, 2);
            head.graphics.lineTo(36, 8);
            head.graphics.lineTo(31, 12);
            head.graphics.curveTo(28, 24, 12, 30);
            head.graphics.lineTo(9, 43);

            // Visor sweep and restrained face plane.
            head.graphics.lineStyle(1, color, 0.54);
            head.graphics.moveTo(18, -31);
            head.graphics.curveTo(31, -18, 31, 5);
            head.graphics.moveTo(7, -17);
            head.graphics.lineTo(21, -16);
            head.graphics.lineTo(27, -12);
            head.graphics.moveTo(26, 7);
            head.graphics.lineTo(32, 8);
            head.graphics.moveTo(-7, 9);
            head.graphics.lineTo(15, 27);

            // Earcup, chin strap and neck opening.
            head.graphics.lineStyle(2, color, 0.68);
            head.graphics.drawCircle(-8, 1, 11);
            head.graphics.lineStyle(1, color, 0.48);
            head.graphics.drawCircle(-8, 1, 6);
            head.graphics.moveTo(-2, 10);
            head.graphics.lineTo(19, 24);
            head.graphics.moveTo(-13, 43);
            head.graphics.lineTo(-24, 53);
            head.graphics.moveTo(9, 43);
            head.graphics.lineTo(20, 53);
            head.graphics.moveTo(-24, 53);
            head.graphics.curveTo(-2, 47, 20, 53);
            head.graphics.drawCircle(0, 0, 4);
            target.addChild(head);
        }

        private function drawArtificialHorizon(target:Sprite,
            centerX:Number, centerY:Number, radius:Number, degrees:Number,
            color:uint):void
        {
            var horizon:Sprite = new Sprite();
            horizon.x = centerX;
            horizon.y = centerY;
            horizon.rotation = -degrees;
            horizon.graphics.lineStyle(2, color, 0.72);
            horizon.graphics.moveTo(-radius, 0);
            horizon.graphics.lineTo(-24, 0);
            horizon.graphics.moveTo(24, 0);
            horizon.graphics.lineTo(radius, 0);
            horizon.graphics.lineStyle(1, color, 0.35);
            horizon.graphics.moveTo(-radius + 14, -17);
            horizon.graphics.lineTo(radius - 14, -17);
            horizon.graphics.moveTo(-radius + 14, 17);
            horizon.graphics.lineTo(radius - 14, 17);
            target.addChild(horizon);
            target.graphics.lineStyle(2, PanelTheme.TEXT, 0.9);
            target.graphics.moveTo(centerX - 24, centerY);
            target.graphics.lineTo(centerX - 7, centerY);
            target.graphics.lineTo(centerX, centerY + 7);
            target.graphics.lineTo(centerX + 7, centerY);
            target.graphics.lineTo(centerX + 24, centerY);
        }

        private function signedDegrees(value:Number):String
        {
            return (value >= 0 ? "+" : "") + value.toFixed(1) + " DEG";
        }

        private function liveControlById(current:Object,
            controlId:String):Object
        {
            if (current == null || current.controls == null) return null;
            for each (var control:Object in current.controls) {
                if (String(control.controlId) == controlId) return control;
            }
            return null;
        }

        private function numericLiveValue(control:Object,
            fallback:Number):Number
        {
            if (control == null) return fallback;
            return uint(control.kind) == 1 || uint(control.kind) == 3 ?
                Number(control.integerValue) : Number(control.floatValue);
        }

        private function drawRadialResponse(component:Object, current:Object,
            x:Number, y:Number, width:Number, height:Number,
            record:Boolean = true):void
        {
            if (record) livePlacements.push({
                "channelId":String(component.channelId), "kind":3,
                "x":x, "y":y, "width":width, "height":height
            });
            var key:String = liveKey(String(current.moduleId),
                String(current.pageId), String(component.channelId));
            var state:Object = radialStates[key];
            if (state == null) {
                state = {
                    "x":Number(component.liveX),
                    "y":Number(component.liveY),
                    "captured":true,
                    "stoppedByUser":false,
                    "lastMotionMs":getTimer(),
                    "lastTickMs":getTimer(),
                    "pollCarryMs":0,
                    "pointerReady":false,
                    "lastPointerX":0,
                    "lastPointerY":0
                };
                radialStates[key] = state;
            }
            if (Boolean(state.captured)) {
                if (activeRadialKey != key) {
                    state.pointerReady = false;
                    state.lastMotionMs = getTimer();
                    state.lastTickMs = state.lastMotionMs;
                    state.pollCarryMs = 0;
                }
                activeRadialKey = key;
            }

            var card:Sprite = new Sprite();
            card.x = x;
            card.y = y;
            card.graphics.lineStyle(1, PanelTheme.ROW_BORDER);
            card.graphics.beginFill(PanelTheme.ROW_EVEN);
            card.graphics.drawRoundRect(0, 0, width, height, 8, 8);
            card.graphics.endFill();
            liveLayer.addChild(card);

            VectorTextRenderer.addText(card,
                String(component.title).toUpperCase(), 18, 12, 17,
                PanelTheme.TEXT, true, width - 150, 24);
            VectorTextRenderer.addText(card, "LIVE TUNING LAB", width - 170,
                13, 13, PanelTheme.CYAN, true, 150, 20);

            var maximum:Number = Math.max(1, Number(component.maximumRadius));
            var radiusControl:Object = liveControlById(current,
                String(component.radiusControlId));
            var idleControl:Object = liveControlById(current,
                String(component.idleMillisecondsControlId));
            var decayControl:Object = liveControlById(current,
                String(component.decayRateControlId));
            var enabledControl:Object = liveControlById(current,
                String(component.enabledControlId));
            var pollControl:Object = liveControlById(current,
                String(component.pollRateControlId));
            var activationRadius:Number = Math.max(0, Math.min(maximum,
                numericLiveValue(radiusControl, maximum)));

            var graphX:Number = 214;
            var graphY:Number = 230;
            var outer:Number = 166;
            card.graphics.lineStyle(3, PanelTheme.TEXT, 0.9);
            card.graphics.drawCircle(graphX, graphY, outer);
            card.graphics.lineStyle(2, PanelTheme.CYAN, 0.78);
            card.graphics.drawCircle(graphX, graphY,
                outer * activationRadius / maximum);
            card.graphics.lineStyle(1, PanelTheme.BORDER, 0.8);
            card.graphics.moveTo(graphX - outer, graphY);
            card.graphics.lineTo(graphX + outer, graphY);
            card.graphics.moveTo(graphX, graphY - outer);
            card.graphics.lineTo(graphX, graphY + outer);

            VectorTextRenderer.addText(card, "FLIGHT ENVELOPE", 122, 48, 12,
                PanelTheme.DIM_TEXT, true, 190, 18);
            VectorTextRenderer.addText(card,
                "CYAN  ACTIVATION ZONE", 116, 403, 12,
                PanelTheme.CYAN, true, 230, 18);

            var graphHit:Sprite = new Sprite();
            graphHit.graphics.beginFill(0xFFFFFF, 0.0);
            graphHit.graphics.drawCircle(graphX, graphY, outer);
            graphHit.graphics.endFill();
            graphHit.mouseEnabled = false;
            card.addChild(graphHit);

            var reticle:Sprite = new Sprite();
            reticle.mouseEnabled = false;
            reticle.graphics.lineStyle(2, PanelTheme.TEXT);
            reticle.graphics.drawCircle(0, 0, 15);
            reticle.graphics.drawCircle(0, 0, 25);
            reticle.graphics.moveTo(-42, 0);
            reticle.graphics.lineTo(-25, 0);
            reticle.graphics.moveTo(25, 0);
            reticle.graphics.lineTo(42, 0);
            reticle.graphics.moveTo(0, -42);
            reticle.graphics.lineTo(0, -25);
            reticle.graphics.moveTo(0, 25);
            reticle.graphics.lineTo(0, 42);
            card.addChild(reticle);

            var status:Sprite = VectorTextRenderer.addText(card, "", 474, 55,
                14, PanelTheme.GOLD, true, width - 650, 22);
            var demoButton:Sprite = new Sprite();
            demoButton.x = width - 154;
            demoButton.y = 48;
            demoButton.graphics.lineStyle(1, PanelTheme.CYAN);
            demoButton.graphics.beginFill(PanelTheme.BUTTON_FILL);
            demoButton.graphics.drawRoundRect(0, 0, 132, 32, 5, 5);
            demoButton.graphics.endFill();
            demoButton.buttonMode = true;
            VectorTextRenderer.addText(demoButton,
                Boolean(state.captured) ? "STOP DEMO" : "START DEMO",
                10, 7, 13, PanelTheme.CYAN, true, 112, 18);
            card.addChild(demoButton);
            hits.register(demoButton, "radialDemoToggle",
                {"key":key, "component":component}, 0);
            state.reticle = reticle;
            state.status = status;
            state.graph = graphHit;
            state.graphX = graphX;
            state.graphY = graphY;
            state.outer = outer;
            state.maximum = maximum;
            state.component = component;
            state.current = current;

            VectorTextRenderer.addText(card,
                "LIVE WHILE THIS PAGE IS OPEN. MOUSE MOTION DRIVES THE RETICLE; WHEN IDLE, TUNED CENTERING RUNS.",
                474, 80, 13, PanelTheme.MUTED_TEXT, false,
                width - 500, 36);
            drawRadialTuning(card, current, radiusControl, "ACTIVATION RADIUS",
                "Changes the cyan zone that permits a return.", 474, 126);
            drawRadialTuning(card, current, idleControl, "IDLE DELAY",
                "Changes the pause after release before motion begins.", 474, 214);
            drawRadialTuning(card, current, decayControl, "CENTERING RATE",
                "Changes the exponential return speed.", 474, 302);
            VectorTextRenderer.addText(card,
                "Preview uses the same exp(-rate x dt) step at " +
                    int(numericLiveValue(pollControl, 120)) + " Hz.",
                474, 392, 12, PanelTheme.DIM_TEXT, false,
                width - 500, 18);
            state.enabled = enabledControl == null ||
                Boolean(enabledControl.booleanValue);
            updateRadialView(state);
        }

        private function drawRadialTuning(card:Sprite, current:Object,
            control:Object, label:String, detail:String,
            x:Number, y:Number):void
        {
            if (control == null) return;
            VectorTextRenderer.addText(card, label, x, y, 13,
                PanelTheme.TEXT, true, 220, 18);
            VectorTextRenderer.addText(card, detail, x + 230, y, 12,
                PanelTheme.DIM_TEXT, false, 610, 18);
            var widget:Sprite = new Sprite();
            widget.x = x;
            widget.y = y + 25;
            var hit:Sprite = ControlWidgets.drawSliderOnly(widget, control);
            var available:Number = Math.max(520,
                card.width - x - 22);
            widget.scaleX = Math.min(1, available / 748);
            card.addChild(widget);
            hits.register(hit, "slider", control,
                findControlIndex(current, control));
        }

        public function get radialDemoActive():Boolean
        {
            if (activeRadialKey.length == 0) return false;
            var state:Object = radialStates[activeRadialKey];
            return state != null && Boolean(state.captured) &&
                state.graph != null && state.graph.parent != null;
        }

        public function toggleRadialDemo(payload:Object,
            stageX:Number, stageY:Number):void
        {
            if (payload == null) return;
            var key:String = String(payload.key);
            var state:Object = radialStates[key];
            if (state == null) return;
            if (Boolean(state.captured)) {
                releaseRadialDemo();
                return;
            }
            if (activeRadialKey.length > 0 &&
                activeRadialKey != key) releaseRadialDemo();
            activeRadialKey = key;
            state.captured = true;
            state.stoppedByUser = false;
            state.pointerReady = true;
            state.lastPointerX = stageX;
            state.lastPointerY = stageY;
            state.lastMotionMs = getTimer();
            state.lastTickMs = state.lastMotionMs;
            state.pollCarryMs = 0;
            updateRadialView(state);
        }

        public function moveRadialDemo(stageX:Number,
            stageY:Number):Boolean
        {
            if (!radialDemoActive) return false;
            var state:Object = radialStates[activeRadialKey];
            if (!Boolean(state.pointerReady)) {
                state.pointerReady = true;
                state.lastPointerX = stageX;
                state.lastPointerY = stageY;
                return true;
            }
            var deltaX:Number = stageX - Number(state.lastPointerX);
            var deltaY:Number = stageY - Number(state.lastPointerY);
            state.lastPointerX = stageX;
            state.lastPointerY = stageY;
            if (Math.abs(deltaX) < 0.01 && Math.abs(deltaY) < 0.01) return true;

            var dx:Number = Number(state.x) + deltaX /
                Number(state.outer) * Number(state.maximum);
            var dy:Number = Number(state.y) + deltaY /
                Number(state.outer) * Number(state.maximum);
            var distance:Number = Math.sqrt(dx * dx + dy * dy);
            var maximum:Number = Number(state.maximum);
            if (distance > maximum) {
                dx *= maximum / distance;
                dy *= maximum / distance;
            }
            state.x = dx;
            state.y = dy;
            state.lastMotionMs = getTimer();
            state.pollCarryMs = 0;
            updateRadialView(state);
            return true;
        }

        public function releaseRadialDemo():Boolean
        {
            if (!radialDemoActive) return false;
            var state:Object = radialStates[activeRadialKey];
            state.captured = false;
            state.stoppedByUser = true;
            state.pointerReady = false;
            state.lastTickMs = getTimer();
            state.pollCarryMs = 0;
            activeRadialKey = "";
            updateRadialView(state);
            return true;
        }

        public function tickRadialDemo(current:Object):void
        {
            if (current == null || current.liveComponents == null) return;
            var now:int = getTimer();
            for each (var component:Object in current.liveComponents) {
                if (uint(component.kind) != 3) continue;
                var key:String = liveKey(String(current.moduleId),
                    String(current.pageId), String(component.channelId));
                var state:Object = radialStates[key];
                if (state == null || state.reticle == null ||
                    state.reticle.parent == null || !Boolean(state.captured)) {
                    continue;
                }
                var radius:Number = numericLiveValue(liveControlById(current,
                    String(component.radiusControlId)), Number(component.maximumRadius));
                var idle:Number = numericLiveValue(liveControlById(current,
                    String(component.idleMillisecondsControlId)), 80);
                var rate:Number = numericLiveValue(liveControlById(current,
                    String(component.decayRateControlId)), 12);
                var poll:Number = Math.max(30, numericLiveValue(liveControlById(
                    current, String(component.pollRateControlId)), 120));
                var enabled:Object = liveControlById(current,
                    String(component.enabledControlId));
                state.enabled = enabled == null || Boolean(enabled.booleanValue);
                var dt:Number = Math.max(0, Math.min(100,
                    now - int(state.lastTickMs)));
                state.lastTickMs = now;
                var distance:Number = Math.sqrt(Number(state.x) * Number(state.x) +
                    Number(state.y) * Number(state.y));
                if (Boolean(state.enabled) && distance <= radius &&
                    distance > 0.1 && now - int(state.lastMotionMs) >= idle) {
                    var stepMs:Number = 1000 / poll;
                    state.pollCarryMs = Number(state.pollCarryMs) + dt;
                    var steps:int = 0;
                    while (Number(state.pollCarryMs) >= stepMs && steps++ < 16) {
                        var factor:Number = Math.exp(-rate * stepMs / 1000);
                        state.x = Number(state.x) * factor;
                        state.y = Number(state.y) * factor;
                        state.pollCarryMs = Number(state.pollCarryMs) - stepMs;
                    }
                    if (Math.abs(Number(state.x)) < 0.1) state.x = 0;
                    if (Math.abs(Number(state.y)) < 0.1) state.y = 0;
                }
                updateRadialView(state);
            }
        }

        private function updateRadialView(state:Object):void
        {
            if (state == null || state.reticle == null ||
                state.reticle.parent == null) return;
            state.reticle.x = Number(state.graphX) + Number(state.x) /
                Number(state.maximum) * Number(state.outer);
            state.reticle.y = Number(state.graphY) + Number(state.y) /
                Number(state.maximum) * Number(state.outer);
            var message:String;
            var color:uint = PanelTheme.GOLD;
            var distance:Number = Math.sqrt(Number(state.x) * Number(state.x) +
                Number(state.y) * Number(state.y));
            if (!Boolean(state.captured)) {
                message = "DEMO STOPPED";
            } else if (!Boolean(state.enabled)) {
                message = "CENTERING DISABLED";
                color = PanelTheme.WARNING;
            } else {
                var current:Object = state.current;
                var component:Object = state.component;
                var radius:Number = numericLiveValue(liveControlById(current,
                    String(component.radiusControlId)), Number(state.maximum));
                var idle:Number = numericLiveValue(liveControlById(current,
                    String(component.idleMillisecondsControlId)), 80);
                var elapsed:Number = getTimer() - int(state.lastMotionMs);
                if (distance > radius) {
                    message = "PARKED OUTSIDE ACTIVATION ZONE  -  " +
                        distance.toFixed(1) + " / " + radius.toFixed(1) +
                        "  -  STOP DEMO TO PAUSE";
                    color = PanelTheme.WARNING;
                } else if (distance <= 0.1) {
                    message = "CENTERED  -  MOVE MOUSE";
                    color = PanelTheme.CYAN;
                } else if (elapsed < idle) {
                    message = "IDLE DELAY  -  " + int(idle - elapsed) +
                        " MS";
                } else {
                    message = "CENTERING  -  DISPLACEMENT " +
                        distance.toFixed(1);
                    color = PanelTheme.CYAN;
                }
            }
            VectorTextRenderer.drawText(state.status as Sprite, message, 14,
                color, true, 850, 22);
        }

        private function liveStatus(component:Object, range:Boolean = false):String
        {
            if (!Boolean(component.available)) return "WAITING";
            var flags:uint = uint(component.frameFlags);
            if ((flags & 4) != 0) return "SUSPENDED";
            if ((flags & 2) != 0) return "UNAVAILABLE";
            if ((flags & 1) != 0) return "STALE";
            if (range && !Boolean(component.liveAvailable)) return "NO SIGNAL";
            return "LIVE";
        }

        private function roleColor(role:uint):uint
        {
            switch (role) {
                case 1: return PanelTheme.CYAN;
                case 2: return PanelTheme.TIER_GREEN;
                case 3: return PanelTheme.WARNING;
                case 4: return PanelTheme.ERROR;
                case 5: return PanelTheme.CYAN;
                case 6: return PanelTheme.GOLD;
                case 7: return PanelTheme.TIER_GREEN;
                case 8: return PanelTheme.TIER_YELLOW;
                case 9: return PanelTheme.TIER_RED;
            }
            return PanelTheme.DIM_TEXT;
        }

        private function rangeBandColor(band:Object):uint
        {
            var semantic:uint = uint(band.semantic);
            if (semantic == 1) {
                return uint(band.visualRole) == 3 ?
                    PanelTheme.RANGE_ZERO : PanelTheme.RANGE_IDLE;
            }
            if (semantic == 2) return PanelTheme.RANGE_ACTIVE;
            if (semantic == 3) {
                return uint(band.visualRole) == 6 ?
                    PanelTheme.RANGE_FULL : PanelTheme.RANGE_CRUISE;
            }
            if (semantic == 4) return PanelTheme.RANGE_REVERSE;
            if (semantic == 5) return PanelTheme.RANGE_BOOST;
            if (uint(band.visualRole) == 6) return PanelTheme.RANGE_FULL;
            return roleColor(uint(band.visualRole));
        }

        private function formattedLiveValue(value:Number, format:String):String
        {
            var digits:int = 2;
            var token:String = "%.2f";
            if (format.indexOf("%.0f") >= 0) {
                digits = 0;
                token = "%.0f";
            } else if (format.indexOf("%.1f") >= 0) {
                digits = 1;
                token = "%.1f";
            } else if (format.indexOf("%.3f") >= 0) {
                digits = 3;
                token = "%.3f";
            }
            var number:String = value.toFixed(digits);
            return format.length == 0 ? number : format.split(token).join(number);
        }

        private function rangePosition(value:Number, minimum:Number,
            maximum:Number, width:Number):Number
        {
            if (maximum <= minimum) return 0;
            return Math.max(0, Math.min(width,
                (value - minimum) / (maximum - minimum) * width));
        }

        private function drawRangeMeter(component:Object, x:Number, y:Number,
            width:Number, height:Number, record:Boolean = true,
            current:Object = null):void
        {
            if (record) livePlacements.push({
                "channelId":String(component.channelId), "kind":0,
                "x":x, "y":y, "width":width, "height":height
            });
            var card:Sprite = new Sprite();
            card.x = x;
            card.y = y;
            card.graphics.lineStyle(1, PanelTheme.ROW_BORDER);
            card.graphics.beginFill(PanelTheme.ROW_EVEN);
            card.graphics.drawRoundRect(0, 0, width, height, 6, 6);
            card.graphics.endFill();
            liveLayer.addChild(card);

            var status:String = liveStatus(component, true);
            VectorTextRenderer.addText(card,
                VectorTextRenderer.fit(String(component.title), 34), 10, 7,
                15, PanelTheme.TEXT, true);
            VectorTextRenderer.addText(card, status, width - 112, 7, 13,
                status == "LIVE" ? PanelTheme.CYAN : PanelTheme.WARNING, true);

            var minimum:Number = Number(component.minimum);
            var maximum:Number = Number(component.maximum);
            var trackX:Number = 12;
            var pinned:Boolean =
                (uint(component.interactionFlags) & LIVE_PINNED) != 0;
            var trackY:Number = pinned ? 44 : 34;
            var trackWidth:Number = width - 24;
            var trackHeight:Number = pinned ? 34 : 24;
            card.graphics.lineStyle(1, PanelTheme.BORDER);
            card.graphics.beginFill(PanelTheme.WIDGET_FILL);
            card.graphics.drawRect(trackX, trackY, trackWidth, trackHeight);
            card.graphics.endFill();
            if (component.bands != null) {
                for (var bandIndex:int = 0;
                     bandIndex < component.bands.length; ++bandIndex) {
                    var band:Object = component.bands[bandIndex];
                    var bandStart:Number = rangePosition(Number(band.minimum),
                        minimum, maximum, trackWidth);
                    var bandEnd:Number = rangePosition(Number(band.maximum),
                        minimum, maximum, trackWidth);
                    var bandColor:uint = rangeBandColor(band);
                    card.graphics.beginFill(bandColor, pinned ? 0.68 : 0.48);
                    card.graphics.drawRect(trackX + bandStart, trackY,
                        Math.max(1, bandEnd - bandStart), trackHeight);
                    card.graphics.endFill();
                    if (pinned && bandEnd - bandStart >= 92 &&
                        String(band.label).length > 0) {
                        VectorTextRenderer.addText(card,
                            VectorTextRenderer.fit(String(band.label).toUpperCase(),
                                Math.max(5, int((bandEnd - bandStart) / 8))),
                            trackX + bandStart + 6, trackY + 8, 11,
                            PanelTheme.TEXT, true, bandEnd - bandStart - 12, 16);
                    }
                }
            }
            if (component.markers != null) {
                for (var markerIndex:int = 0;
                     markerIndex < component.markers.length; ++markerIndex) {
                    var marker:Object = component.markers[markerIndex];
                    var markerX:Number = trackX + rangePosition(Number(marker.value),
                        minimum, maximum, trackWidth);
                    var markerColor:uint = roleColor(uint(marker.visualRole));
                    var editable:Boolean = String(marker.controlId).length > 0;
                    var selected:Boolean = editable &&
                        String(marker.controlId) == hits.activeRangeControlId;
                    card.graphics.lineStyle(selected ? 4 : 2, markerColor);
                    card.graphics.moveTo(markerX, trackY - 3);
                    card.graphics.lineTo(markerX, trackY + trackHeight + 3);
                    if (editable) {
                        card.graphics.lineStyle(1, PanelTheme.TEXT);
                        card.graphics.beginFill(markerColor, 1.0);
                        card.graphics.moveTo(markerX, trackY - (selected ? 12 : 9));
                        card.graphics.lineTo(markerX - (selected ? 8 : 6), trackY - 2);
                        card.graphics.lineTo(markerX + (selected ? 8 : 6), trackY - 2);
                        card.graphics.lineTo(markerX, trackY - (selected ? 12 : 9));
                        card.graphics.endFill();
                    }
                }
            }
            if (Boolean(component.liveAvailable)) {
                var liveX:Number = trackX + rangePosition(Number(component.liveValue),
                    minimum, maximum, trackWidth);
                card.graphics.lineStyle(pinned ? 4 : 3, PanelTheme.TEXT);
                card.graphics.moveTo(liveX, trackY - 6);
                card.graphics.lineTo(liveX, trackY + trackHeight + 6);
                if (pinned) {
                    card.graphics.beginFill(PanelTheme.TEXT);
                    card.graphics.moveTo(liveX, trackY + trackHeight + 8);
                    card.graphics.lineTo(liveX - 6, trackY + trackHeight + 15);
                    card.graphics.lineTo(liveX + 6, trackY + trackHeight + 15);
                    card.graphics.lineTo(liveX, trackY + trackHeight + 8);
                    card.graphics.endFill();
                }
            }
            var valueY:Number = trackY + trackHeight + 6;
            VectorTextRenderer.addText(card,
                formattedLiveValue(Number(component.liveValue),
                    String(component.valueFormat)), 12, valueY, 13,
                Boolean(component.liveAvailable) ? PanelTheme.TEXT : PanelTheme.DIM_TEXT);
            VectorTextRenderer.addText(card,
                formattedLiveValue(minimum, String(component.valueFormat)),
                width - 174, valueY, 12, PanelTheme.DIM_TEXT);
            VectorTextRenderer.addText(card,
                formattedLiveValue(maximum, String(component.valueFormat)),
                width - 86, valueY, 12, PanelTheme.DIM_TEXT);
            if (pinned && component.bands != null) {
                var legendCount:int = 0;
                for each (var labeledBand:Object in component.bands) {
                    if (String(labeledBand.label).length > 0) ++legendCount;
                }
                legendCount = Math.max(1, legendCount);
                var legendWidth:Number = (width - 24) / legendCount;
                var legendItem:int = 0;
                for (var legendIndex:int = 0;
                     legendIndex < component.bands.length;
                     ++legendIndex) {
                    var legend:Object = component.bands[legendIndex];
                    var legendLabel:String = String(legend.label);
                    if (legendLabel.length == 0) continue;
                    var legendX:Number = 12 + legendItem * legendWidth;
                    var legendChipWidth:Number = Math.max(54, legendWidth - 8);
                    var legendColor:uint = rangeBandColor(legend);
                    card.graphics.lineStyle(1, legendColor, 1.0);
                    card.graphics.beginFill(legendColor, 0.48);
                    card.graphics.drawRoundRect(legendX, height - 25,
                        legendChipWidth, 20, 6, 6);
                    card.graphics.endFill();
                    card.graphics.beginFill(legendColor, 1.0);
                    card.graphics.drawRoundRect(legendX + 2, height - 23,
                        8, 16, 4, 4);
                    card.graphics.endFill();
                    VectorTextRenderer.addText(card,
                        VectorTextRenderer.fit(legendLabel,
                            Math.max(8, int(legendChipWidth / 7))),
                        legendX + 15, height - 23, 11, PanelTheme.TEXT, true,
                        legendChipWidth - 20, 16);
                    ++legendItem;
                }
                var hasShapedOutput:Boolean = false;
                var guideEditable:Boolean = false;
                if (component.markers != null) {
                    for each (var guideMarker:Object in component.markers) {
                        if (String(guideMarker.controlId).length > 0) {
                            guideEditable = true;
                        }
                        if (String(guideMarker.label) == "Shaped output") {
                            hasShapedOutput = true;
                            break;
                        }
                    }
                }
                VectorTextRenderer.addText(card,
                    !guideEditable ?
                        "READ-ONLY PREVIEW  /  SELECT GRAPH TO EDIT ON THROTTLE SETUP" :
                    hasShapedOutput ?
                        "DRAG EDGES  /  WHITE = INPUT  /  GREEN = SHAPED OUTPUT" :
                        "DRAG COLOURED LANDMARKS  /  WHITE = PHYSICAL LEVER",
                    12, 23, 11, PanelTheme.MUTED_TEXT, true,
                    width - 24, 16);
            }
            if (record && current != null && component.markers != null) {
                var hasEditable:Boolean = false;
                for each (marker in component.markers) {
                    if (String(marker.controlId).length > 0) {
                        hasEditable = true;
                        break;
                    }
                }
                var throttleGuidance:Boolean =
                    String(current.moduleId) == "absolute.hotas" &&
                    String(current.pageId) == "hotas-flight-axes" &&
                    String(component.channelId) == "axis-throttle";
                if (hasEditable || throttleGuidance) {
                    var rangeHit:Sprite = new Sprite();
                    rangeHit.x = x + trackX;
                    rangeHit.y = y + trackY - 14;
                    rangeHit.graphics.beginFill(0xFFFFFF, 0.0);
                    rangeHit.graphics.drawRect(0, 0, trackWidth,
                        trackHeight + 30);
                    rangeHit.graphics.endFill();
                    rangeHit.buttonMode = true;
                    liveHitLayer.addChild(rangeHit);
                    hits.register(rangeHit,
                        throttleGuidance ? "rangeGuidance" : "rangeMeter", {
                        "channelId":String(component.channelId),
                        "component":component,
                        "trackWidth":trackWidth
                    }, 0);
                }
            }
        }

        private function plotY(value:Number, minimum:Number, maximum:Number,
            y:Number, height:Number):Number
        {
            if (maximum <= minimum) return y + height * 0.5;
            return y + height - Math.max(0, Math.min(1,
                (value - minimum) / (maximum - minimum))) * height;
        }

        private function drawTelemetryPlot(component:Object, x:Number, y:Number,
            width:Number, height:Number, record:Boolean = true):void
        {
            if (record) livePlacements.push({
                "channelId":String(component.channelId), "kind":1,
                "x":x, "y":y, "width":width, "height":height
            });
            var card:Sprite = new Sprite();
            card.x = x;
            card.y = y;
            card.graphics.lineStyle(1, PanelTheme.ROW_BORDER);
            card.graphics.beginFill(PanelTheme.ROW_EVEN);
            card.graphics.drawRoundRect(0, 0, width, height, 6, 6);
            card.graphics.endFill();
            liveLayer.addChild(card);

            var status:String = liveStatus(component);
            VectorTextRenderer.addText(card,
                VectorTextRenderer.fit(String(component.title), 52), 10, 7,
                15, PanelTheme.TEXT, true);
            VectorTextRenderer.addText(card, status, width - 112, 7, 13,
                status == "LIVE" ? PanelTheme.CYAN : PanelTheme.WARNING, true);
            var plotX:Number = 12;
            var plotTop:Number = 34;
            var plotWidth:Number = width - 24;
            var plotHeight:Number = height - 58;
            card.graphics.lineStyle(1, PanelTheme.BORDER);
            card.graphics.beginFill(PanelTheme.WIDGET_FILL);
            card.graphics.drawRect(plotX, plotTop, plotWidth, plotHeight);
            card.graphics.endFill();

            var minimum:Number = Number(component.minimum);
            var maximum:Number = Number(component.maximum);
            var foundValue:Boolean = false;
            if (Boolean(component.autoRange) && component.samples != null) {
                for (var autoSample:int = 0;
                     autoSample < component.samples.length; ++autoSample) {
                    var autoRow:Object = component.samples[autoSample];
                    for (var autoSeries:int = 0;
                         autoSeries < autoRow.values.length; ++autoSeries) {
                        if ((uint(autoRow.availableMask) & (1 << autoSeries)) == 0) continue;
                        var autoValue:Number = Number(autoRow.values[autoSeries]);
                        if (!foundValue) {
                            minimum = maximum = autoValue;
                            foundValue = true;
                        } else {
                            minimum = Math.min(minimum, autoValue);
                            maximum = Math.max(maximum, autoValue);
                        }
                    }
                }
                if (foundValue && maximum <= minimum) {
                    minimum -= 1;
                    maximum += 1;
                } else if (foundValue) {
                    var pad:Number = (maximum - minimum) * 0.05;
                    minimum -= pad;
                    maximum += pad;
                }
            }
            if (component.bands != null) {
                for (var bandIndex:int = 0;
                     bandIndex < component.bands.length; ++bandIndex) {
                    var band:Object = component.bands[bandIndex];
                    var top:Number = plotY(Number(band.maximum), minimum,
                        maximum, plotTop, plotHeight);
                    var bottom:Number = plotY(Number(band.minimum), minimum,
                        maximum, plotTop, plotHeight);
                    card.graphics.beginFill(roleColor(uint(band.visualRole)), 0.24);
                    card.graphics.drawRect(plotX, top, plotWidth,
                        Math.max(1, bottom - top));
                    card.graphics.endFill();
                }
            }
            if (component.markers != null) {
                for (var markerIndex:int = 0;
                     markerIndex < component.markers.length; ++markerIndex) {
                    var marker:Object = component.markers[markerIndex];
                    var markerY:Number = plotY(Number(marker.value), minimum,
                        maximum, plotTop, plotHeight);
                    card.graphics.lineStyle(1, roleColor(uint(marker.visualRole)));
                    card.graphics.moveTo(plotX, markerY);
                    card.graphics.lineTo(plotX + plotWidth, markerY);
                }
            }
            var sampleCount:int = component.samples == null ? 0 :
                component.samples.length;
            if (component.series != null && sampleCount > 0) {
                for (var seriesIndex:int = 0;
                     seriesIndex < component.series.length; ++seriesIndex) {
                    var series:Object = component.series[seriesIndex];
                    card.graphics.lineStyle(2, roleColor(uint(series.visualRole)));
                    var drawing:Boolean = false;
                    for (var sampleIndex:int = 0;
                         sampleIndex < sampleCount; ++sampleIndex) {
                        var sample:Object = component.samples[sampleIndex];
                        if ((uint(sample.availableMask) & (1 << seriesIndex)) == 0) {
                            drawing = false;
                            continue;
                        }
                        var sampleX:Number = plotX + (sampleCount == 1 ? plotWidth :
                            sampleIndex / (sampleCount - 1) * plotWidth);
                        var sampleY:Number = plotY(Number(sample.values[seriesIndex]),
                            minimum, maximum, plotTop, plotHeight);
                        if (!drawing) card.graphics.moveTo(sampleX, sampleY);
                        else card.graphics.lineTo(sampleX, sampleY);
                        drawing = true;
                    }
                    VectorTextRenderer.addText(card,
                        VectorTextRenderer.fit(String(series.label), 20),
                        12 + seriesIndex * 180, height - 20, 12,
                        roleColor(uint(series.visualRole)), true);
                }
            } else {
                VectorTextRenderer.addText(card, "WAITING FOR BOUNDED HISTORY",
                    plotX + 12, plotTop + plotHeight * 0.5 - 8, 13,
                    PanelTheme.DIM_TEXT, true);
            }
        }

        private function drawSegmentedGrid(component:Object, current:Object = null):void
        {
            var channelId:String = String(component.channelId);
            var cycleOnClick:Boolean =
                (uint(component.interactionFlags) & 1) != 0;
            var activeTier:String = activeGridTiers[channelId] == null ? "green" :
                String(activeGridTiers[channelId]);
            activeGridTiers[channelId] = activeTier;
            VectorTextRenderer.addText(rowLayer, String(component.title), 0, 2, 16,
                PanelTheme.MUTED_TEXT, true);
            if (!cycleOnClick) {
                var tierX:Number = 744;
                for (var tier:int = 1; tier < component.tiers.length; ++tier) {
                    var tierInfo:Object = component.tiers[tier];
                    var tierButton:Sprite = new Sprite();
                    var chosen:Boolean = String(tierInfo.tierId) == activeTier;
                    tierButton.graphics.lineStyle(chosen ? 2 : 1,
                        chosen ? PanelTheme.GOLD : tierColor(tier));
                    tierButton.graphics.beginFill(chosen ? PanelTheme.SELECTED_FILL :
                        PanelTheme.BUTTON_FILL);
                    tierButton.graphics.drawRect(0, 0, 170, 28);
                    tierButton.graphics.endFill();
                    tierButton.x = tierX + (tier - 1) * 180;
                    tierButton.y = 0;
                    tierButton.buttonMode = true;
                    VectorTextRenderer.addText(tierButton,
                        VectorTextRenderer.fit(String(tierInfo.label).toUpperCase(), 20),
                        8, 5, 15, chosen ? PanelTheme.TEXT : tierColor(tier));
                    rowLayer.addChild(tierButton);
                    hits.register(tierButton, "gridTier",
                        {"channelId":channelId,
                            "tierId":String(tierInfo.tierId)}, tier);
                }
            }

            VectorTextRenderer.addText(rowLayer,
                cycleOnClick ?
                    "Click a pip to cycle HOLLOW > 1 > 2 > 3 > HOLLOW. Quick-step buttons adjust one request." :
                    "Choose a priority, then click a hollow pip to allocate; click a filled pip to trim.",
                0, 34, 15, PanelTheme.TEXT);
            VectorTextRenderer.addText(rowLayer, "1 GREEN: FIRST", 0, 57, 14,
                PanelTheme.TIER_GREEN, true);
            VectorTextRenderer.addText(rowLayer, "2 YELLOW: AFTER GREEN", 150, 57, 14,
                PanelTheme.TIER_YELLOW, true);
            VectorTextRenderer.addText(rowLayer, "3 RED: LAST", 380, 57, 14,
                PanelTheme.TIER_RED, true);
            VectorTextRenderer.addText(rowLayer, "CYAN OUTLINE: LIVE", 520, 57, 14,
                PanelTheme.CYAN);
            VectorTextRenderer.addText(rowLayer, "GOLD TICK: PREVIEW", 715, 57, 14,
                PanelTheme.GOLD);
            VectorTextRenderer.addText(rowLayer, "HOLLOW: AVAILABLE", 920, 57,
                14, PanelTheme.DIM_TEXT);
            VectorTextRenderer.addText(rowLayer,
                cycleOnClick ? "SYSTEM" : "EDITING " + activeTier.toUpperCase() +
                    " PRIORITY",
                0, 82, 13, cycleOnClick ? PanelTheme.DIM_TEXT :
                    tierColor(tierIndex(component, activeTier)), true);
            VectorTextRenderer.addText(rowLayer, "ALLOCATION", 350, 82, 13,
                PanelTheme.DIM_TEXT, true);

            for (var columnIndex:int = 0;
                 columnIndex < component.columns.length; ++columnIndex) {
                var column:Object = component.columns[columnIndex];
                var rowY:Number = 105 + columnIndex * 34;
                var focused:Boolean = gridFocusActive &&
                    (selectedGridColumnId.length == 0 && columnIndex == 0 ||
                     selectedGridColumnId == String(column.columnId));
                if (focused) {
                    rowLayer.graphics.lineStyle(1, PanelTheme.CYAN, 0.75);
                    rowLayer.graphics.drawRoundRect(-4, rowY - 3,
                        PanelLayout.CONTROL_ROW_WIDTH - 4, 28, 5, 5);
                }
                VectorTextRenderer.addText(rowLayer,
                    VectorTextRenderer.fit(String(column.label), 40), 0,
                    rowY + 4, 15, focused ? PanelTheme.TEXT :
                    PanelTheme.MUTED_TEXT, focused);
                var tierCounts:Array = [0, 0, 0, 0];
                var filledCount:int = 0;
                for (var countIndex:int = 0;
                     countIndex < column.segments.length; ++countIndex) {
                    var countTier:int = int(column.segments[countIndex].tierIndex);
                    if (countTier >= 0 && countTier < tierCounts.length) {
                        tierCounts[countTier] = int(tierCounts[countTier]) + 1;
                        if (countTier > 0) ++filledCount;
                    }
                }
                var maximumSegments:int = Math.max(1,
                    int(column.maximumSegments));
                var pipX:Number = 350;
                var pipStep:Number = 20;
                for (var pipIndex:int = 0;
                     pipIndex < column.segments.length; ++pipIndex) {
                    var segment:Object = column.segments[pipIndex];
                    var pip:Sprite = new Sprite();
                    var pipTier:int = int(segment.tierIndex);
                    pip.graphics.lineStyle(Boolean(segment.live) ? 2 : 1,
                        Boolean(segment.live) ? PanelTheme.CYAN : PanelTheme.ROW_BORDER);
                    pip.graphics.beginFill(tierColor(pipTier), pipTier == 0 ? 0.35 : 0.9);
                    pip.graphics.drawRect(0, 0, 16, 22);
                    pip.graphics.endFill();
                    if (pipTier > 0) {
                        VectorTextRenderer.addText(pip, String(pipTier),
                            4, 2, 14, PanelTheme.BACKGROUND, true, 12, 18);
                    }
                    if (Boolean(segment.preview)) {
                        pip.graphics.beginFill(PanelTheme.GOLD);
                        pip.graphics.drawRect(2, 18, 12, 2);
                        pip.graphics.endFill();
                    }
                    pip.x = pipX + pipIndex * pipStep;
                    pip.y = rowY;
                    pip.buttonMode = Boolean(segment.interactive);
                    rowLayer.addChild(pip);
                    if (Boolean(segment.interactive)) {
                        var operation:Object;
                        if (cycleOnClick) {
                            var nextTier:int = (pipTier + 1) %
                                component.tiers.length;
                            operation = {"component":component,
                                "operationKind":3,
                                "columnId":String(column.columnId),
                                "tierId":String(component.tiers[nextTier].tierId),
                                "count":uint(pipIndex)};
                        } else if (pipTier == 0) {
                            var activeIndex:int = tierIndex(component, activeTier);
                            // Hollow pips are positional: clicking the fifth pip fills
                            // through the fifth pip, rather than adding only one. The
                            // provider still validates the resulting per-tier count.
                            var added:int = Math.max(1, pipIndex + 1 - filledCount);
                            operation = {"component":component, "operationKind":0,
                                "columnId":String(column.columnId), "tierId":activeTier,
                                "count":uint(int(tierCounts[activeIndex]) + added)};
                        } else {
                            operation = {"component":component, "operationKind":1,
                                "columnId":String(column.columnId), "tierId":"",
                                "count":uint(pipIndex)};
                        }
                        hits.register(pip, "compound", operation, pipIndex);
                    }
                }

                var quickX:Number = pipX + maximumSegments * pipStep + 10;
                var quickLabels:Array = ["+G", "+Y", "+R", "-"];
                for (var quick:int = 0; quick < quickLabels.length; ++quick) {
                    var quickButton:Sprite = new Sprite();
                    var quickEnabled:Boolean = quick == 3 ? filledCount > 0 :
                        filledCount < maximumSegments;
                    var quickColor:uint = quick == 0 ? PanelTheme.TIER_GREEN :
                        (quick == 1 ? PanelTheme.TIER_YELLOW :
                        (quick == 2 ? PanelTheme.TIER_RED : PanelTheme.TEXT));
                    quickButton.x = quickX + quick * 47;
                    quickButton.y = rowY;
                    quickButton.graphics.lineStyle(1, quickEnabled ? quickColor :
                        PanelTheme.DISABLED_WIDGET);
                    quickButton.graphics.beginFill(PanelTheme.BUTTON_FILL);
                    quickButton.graphics.drawRoundRect(0, 0, 42, 22, 4, 4);
                    quickButton.graphics.endFill();
                    quickButton.buttonMode = quickEnabled;
                    VectorTextRenderer.addText(quickButton,
                        String(quickLabels[quick]), 10, 3, 13,
                        quickEnabled ? quickColor : PanelTheme.DISABLED_TEXT, true);
                    rowLayer.addChild(quickButton);
                    if (quickEnabled) {
                        var quickOperation:Object;
                        if (quick == 3) {
                            quickOperation = {"component":component,
                                "operationKind":1,
                                "columnId":String(column.columnId), "tierId":"",
                                "count":uint(filledCount - 1)};
                        } else {
                            quickOperation = {"component":component,
                                "operationKind":0,
                                "columnId":String(column.columnId),
                                "tierId":String(component.tiers[quick + 1].tierId),
                                "count":uint(int(tierCounts[quick + 1]) + 1)};
                        }
                        hits.register(quickButton, "compound", quickOperation,
                            columnIndex);
                    }
                }
                var choiceControl:Object = findGridControl(current, column);
                if (choiceControl != null) {
                    var dropdown:Sprite = new Sprite();
                    var dropX:Number = quickX + 198;
                    dropdown.x = dropX;
                    dropdown.y = rowY;
                    dropdown.graphics.lineStyle(1, PanelTheme.CYAN);
                    dropdown.graphics.beginFill(PanelTheme.WIDGET_FILL);
                    dropdown.graphics.drawRoundRect(0, 0, 280, 24, 4, 4);
                    dropdown.graphics.endFill();
                    dropdown.buttonMode = true;
                    dropdown.mouseChildren = false;
                    var valText:String = ControlWidgets.displayValue(choiceControl);
                    VectorTextRenderer.addText(dropdown,
                        VectorTextRenderer.fit(valText, 26), 10, 3, 14, PanelTheme.TEXT, false);
                    VectorTextRenderer.addText(dropdown, "V", 260, 3, 14, PanelTheme.DIM_TEXT);
                    rowLayer.addChild(dropdown);
                    var choiceIdx:int = findControlIndex(current, choiceControl);
                    hits.register(dropdown, "choice", choiceControl, choiceIdx);
                } else {
                    VectorTextRenderer.addText(rowLayer,
                        "LIVE " + int(column.currentCount) + "/" + int(column.maximumCount) +
                        "  TARGET " + int(column.targetCount), quickX + 198,
                        rowY + 4, 14,
                        Boolean(component.available) ? PanelTheme.DIM_TEXT : PanelTheme.WARNING);
                }
            }
        }

        private function findControlIndex(current:Object, control:Object):int
        {
            if (current == null || current.controls == null || control == null) return 0;
            for (var i:int = 0; i < current.controls.length; ++i) {
                if (current.controls[i] == control ||
                    String(current.controls[i].controlId) == String(control.controlId)) {
                    return i;
                }
            }
            return 0;
        }

        private function findGridControl(current:Object, column:Object):Object
        {
            if (current == null || current.controls == null || column == null) return null;
            var assocId:String = column.associatedControlId != null ? String(column.associatedControlId) : "";
            for (var i:int = 0; i < current.controls.length; ++i) {
                var control:Object = current.controls[i];
                var cId:String = String(control.controlId);
                if (assocId.length > 0 && cId == assocId &&
                    uint(control.kind) == 3) return control;
            }
            return null;
        }

        private function gridRowIndexForControl(current:Object, control:Object):int
        {
            if (current == null || control == null || current.liveComponents == null) return 0;
            var cId:String = String(control.controlId);
            for (var compIdx:int = 0; compIdx < current.liveComponents.length; ++compIdx) {
                var comp:Object = current.liveComponents[compIdx];
                if (uint(comp.kind) == 2 && comp.columns != null) {
                    for (var colIdx:int = 0; colIdx < comp.columns.length; ++colIdx) {
                        var col:Object = comp.columns[colIdx];
                        var assocId:String = col.associatedControlId != null ? String(col.associatedControlId) : "";
                        if (assocId.length > 0 && cId == assocId) {
                            return colIdx;
                        }
                    }
                }
            }
            return 0;
        }

        private function tierIndex(component:Object, tierId:String):int
        {
            for (var index:int = 0; index < component.tiers.length; ++index) {
                if (String(component.tiers[index].tierId) == tierId) return index;
            }
            return 1;
        }

        private function tierColor(index:int):uint
        {
            if (index == 1) return PanelTheme.TIER_GREEN;
            if (index == 2) return PanelTheme.TIER_YELLOW;
            if (index == 3) return PanelTheme.TIER_RED;
            return PanelTheme.PIP_HOLLOW;
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
                var moduleIndex:int = int(starts[state.firstVisibleModule + moduleRow]);
                addModuleButton(model.modules[moduleIndex], state,
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
                focused ? PanelTheme.CYAN : (selected ? PanelTheme.CYAN : PanelTheme.BORDER));
            button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL : PanelTheme.BUTTON_FILL);
            button.graphics.drawRect(0, 0, PanelLayout.SIDEBAR_WIDTH,
                PanelLayout.MODULE_HEIGHT);
            button.graphics.endFill();
            button.y = yPosition;
            button.buttonMode = true;
            VectorTextRenderer.addText(button,
                VectorTextRenderer.fit(String(target.moduleTitle), 28), 14, 12, 17,
                selected ? PanelTheme.TEXT : PanelTheme.MUTED_TEXT, selected);
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
            VectorTextRenderer.addText(button,
                VectorTextRenderer.fit(String(target.title), 22), 12, 9, 16,
                selected ? PanelTheme.TEXT : PanelTheme.MUTED_TEXT, selected);
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
                selected ? PanelTheme.CYAN : PanelTheme.ROW_BORDER, 1.0);
            row.graphics.beginFill(selected ? PanelTheme.ROW_SELECTED :
                (index % 2 == 0 ? PanelTheme.ROW_EVEN : PanelTheme.ROW_ODD), 1.0);
            row.graphics.drawRect(0, 0, PanelLayout.CONTROL_ROW_WIDTH,
                PanelLayout.ROW_HEIGHT - 4);
            row.graphics.endFill();
            row.y = yPosition;
            row.buttonMode = true;

            var flags:String = (uint(control.flags) & 4 ? "ADVANCED" : "") +
                (uint(control.flags) & 2 ?
                    ((uint(control.flags) & 4 ? " / " : "") + "RESTART") : "");
            var capturing:Boolean = model != null &&
                (Boolean(model.bindingCaptureActive) ||
                 Boolean(model.textCaptureActive)) &&
                String(model.captureModuleId) == state.currentPageModule() &&
                String(model.capturePageId) == state.currentPageId() &&
                String(model.captureControlId) == String(control.controlId);
            VectorTextRenderer.addText(row,
                VectorTextRenderer.fit(String(control.label), 40), 14, 12, 17,
                Boolean(control.available) ?
                    (selected ? PanelTheme.TEXT : PanelTheme.MUTED_TEXT) :
                    PanelTheme.DISABLED_TEXT, selected);
            if (flags.length > 0) {
                var flagBadge:Sprite = new Sprite();
                flagBadge.x = 350;
                flagBadge.y = 12;
                flagBadge.graphics.lineStyle(1, PanelTheme.WARNING, 0.8);
                flagBadge.graphics.beginFill(PanelTheme.BUTTON_FILL, 1.0);
                flagBadge.graphics.drawRoundRect(0, 0, 112, 24, 6, 6);
                flagBadge.graphics.endFill();
                VectorTextRenderer.addText(flagBadge,
                    VectorTextRenderer.fit(flags, 15), 8, 4, 12,
                    PanelTheme.WARNING, true);
                row.addChild(flagBadge);
            }
            var action:Boolean = uint(control.kind) == 4;
            hits.register(row, action ? "activate" : "select", control, index);
            var rowIndex:int = index;
            var registerWidget:Function = function(view:Sprite, kind:String,
                payload:Object, ignored:int):void {
                hits.register(view, kind, payload, rowIndex);
            };
            ControlWidgets.draw(row, control, capturing, registerWidget);
            rowLayer.addChild(row);
        }

        private function addGroupHeader(control:Object, yPosition:Number):void
        {
            var header:Sprite = new Sprite();
            header.y = yPosition;
            header.graphics.beginFill(PanelTheme.HELP_FILL, 1.0);
            header.graphics.drawRect(0, 4, PanelLayout.CONTROL_ROW_WIDTH, 42);
            header.graphics.endFill();
            header.graphics.lineStyle(1, PanelTheme.BORDER);
            header.graphics.moveTo(0, 45);
            header.graphics.lineTo(PanelLayout.CONTROL_ROW_WIDTH, 45);
            VectorTextRenderer.addText(header,
                String(control.label).toUpperCase(), 14, 13, 16,
                PanelTheme.CYAN, true);
            if (String(control.description).length > 0) {
                VectorTextRenderer.addText(header,
                    VectorTextRenderer.fit(String(control.description), 72),
                    310, 14, 14, PanelTheme.DIM_TEXT);
            }
            rowLayer.addChild(header);
        }

        private function addInlineRow(model:Object, state:MenuSelectionState,
            current:Object, indices:Array, yPosition:Number):void
        {
            var row:Sprite = new Sprite();
            row.y = yPosition;
            row.graphics.lineStyle(1, PanelTheme.ROW_BORDER);
            row.graphics.beginFill(PanelTheme.ROW_EVEN, 1.0);
            row.graphics.drawRect(0, 0, PanelLayout.CONTROL_ROW_WIDTH,
                PanelLayout.ROW_HEIGHT - 4);
            row.graphics.endFill();
            var gap:Number = 8;
            var availableWidth:Number = PanelLayout.CONTROL_ROW_WIDTH - 28 -
                gap * (indices.length - 1);
            var totalWeight:Number = 0;
            for (var weightIndex:int = 0; weightIndex < indices.length;
                 ++weightIndex) {
                var weightControl:Object = current.controls[int(indices[weightIndex])];
                totalWeight += uint(weightControl.kind) == 5 ? 2 : 1;
            }
            var nextX:Number = 14;
            for (var item:int = 0; item < indices.length; ++item) {
                var index:int = int(indices[item]);
                var control:Object = current.controls[index];
                var weight:Number = uint(control.kind) == 5 ? 2 : 1;
                var width:Number = availableWidth * weight / totalWeight;
                var button:Sprite = new Sprite();
                var selected:Boolean = state.focusRegion ==
                    PanelLayout.FOCUS_CONTROLS && state.selectedRow == index;
                button.x = nextX;
                button.y = 7;
                button.graphics.lineStyle(selected ? 2 : 1,
                    selected ? PanelTheme.CYAN : PanelTheme.BORDER);
                button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL :
                    PanelTheme.BUTTON_FILL);
                button.graphics.drawRoundRect(0, 0, width, 60, 6, 6);
                button.graphics.endFill();
                button.buttonMode = Boolean(control.available);
                VectorTextRenderer.addText(button,
                    VectorTextRenderer.fit(String(control.label), 30),
                    14, 6, 15, Boolean(control.available) ? PanelTheme.TEXT :
                    PanelTheme.DISABLED_TEXT, selected);
                if (uint(control.kind) != 4) {
                    var displayed:String = ControlWidgets.displayValue(control);
                    var valueColor:uint = Boolean(control.available) ?
                        (uint(control.kind) == 5 &&
                         (displayed.length == 0 || displayed == "(unbound)") ?
                            PanelTheme.WARNING : PanelTheme.CYAN) :
                        PanelTheme.DISABLED_TEXT;
                    VectorTextRenderer.addText(button,
                        VectorTextRenderer.fit(displayed, 42), 14, 31, 15,
                        valueColor, true, width - 28, 20);
                }
                row.addChild(button);
                var interactive:String = "disabled";
                if (Boolean(control.available)) {
                    interactive = uint(control.kind) == 1 ||
                        uint(control.kind) == 2 ? "select" : "activate";
                }
                hits.register(button, interactive, control, index);
                nextX += width + gap;
            }
            rowLayer.addChild(row);
        }

        private function drawHelp(current:Object, state:MenuSelectionState,
            inputMode:String):void
        {
            helpLayer.graphics.clear();
            helpLayer.graphics.lineStyle(1, PanelTheme.BORDER);
            helpLayer.graphics.beginFill(PanelTheme.HELP_FILL);
            helpLayer.graphics.drawRect(0, 0, PanelLayout.WORKSPACE_WIDTH,
                PanelLayout.HELP_HEIGHT);
            helpLayer.graphics.endFill();
            var description:String = String(current.description);
            if (current.controls != null && current.controls.length > 0) {
                var selectedControl:Object = current.controls[state.selectedRow];
                description = String(selectedControl.description);
                var semanticDetail:String = semanticRenderer.detailForControl(
                    current, selectedControl);
                if (semanticDetail.length > 0) {
                    description += "  " + semanticDetail;
                }
                if (uint(selectedControl.kind) == 5 &&
                    (uint(selectedControl.flags) & 4096) != 0) {
                    description += inputMode == "controller" ?
                        "  X UNBINDS." : (inputMode == "keyboard" ?
                            "  DELETE OR BACKSPACE UNBINDS." :
                            "  USE CLEAR TO UNBIND.");
                }
                if (uint(selectedControl.kind) == 5 &&
                    String(selectedControl.stringValue).length > 0) {
                    description += "  ASSIGNED: " +
                        String(selectedControl.stringValue);
                }
                if (uint(selectedControl.kind) == 8 &&
                    selectedControl.recordItems != null) {
                    var selectedRecordId:String =
                        String(selectedControl.stringValue);
                    for (var recordIndex:int = 0; recordIndex <
                        selectedControl.recordItems.length;
                        ++recordIndex) {
                        var record:Object = selectedControl.recordItems[recordIndex];
                        if (String(record.recordId) == selectedRecordId) {
                            description = String(record.summary) + "  " +
                                String(record.detail);
                            break;
                        }
                    }
                }
            }
            VectorTextRenderer.addText(helpLayer, "SELECTED CONTROL", 14, 10, 13,
                PanelTheme.DIM_TEXT, true);
            VectorTextRenderer.addText(helpLayer, description, 14, 34, 16,
                PanelTheme.MUTED_TEXT, false, PanelLayout.WORKSPACE_WIDTH - 28, 60, true);
        }

        private function drawScrollBar(current:Object, state:MenuSelectionState,
            yOffset:Number, visibleRows:int, layoutRows:Array,
            firstLayoutRow:int, rowHeight:Number = PanelLayout.ROW_HEIGHT):void
        {
            if (layoutRows == null || layoutRows.length <= visibleRows) return;
            var trackHeight:Number = visibleRows * rowHeight - 4;
            var thumbHeight:Number = Math.max(42,
                trackHeight * visibleRows / layoutRows.length);
            var travel:Number = trackHeight - thumbHeight;
            var maximumStart:Number = layoutRows.length - visibleRows;
            var thumbY:Number = maximumStart > 0 ?
                travel * firstLayoutRow / maximumStart : 0;
            var railX:Number = PanelLayout.CONTROL_ROW_WIDTH + 23;
            scrollLayer.graphics.beginFill(PanelTheme.ROW_BORDER);
            scrollLayer.graphics.drawRect(railX, yOffset + 22, 4,
                Math.max(10, trackHeight - 44));
            scrollLayer.graphics.endFill();
            scrollLayer.graphics.beginFill(PanelTheme.CYAN);
            scrollLayer.graphics.drawRect(railX - 3,
                yOffset + 22 + thumbY * Math.max(0, trackHeight - 44) / trackHeight,
                10, Math.max(32, thumbHeight * Math.max(0, trackHeight - 44) /
                    trackHeight));
            scrollLayer.graphics.endFill();

            var hiddenAbove:int = firstLayoutRow;
            var hiddenBelow:int = Math.max(0, layoutRows.length -
                firstLayoutRow - visibleRows);
            if (hiddenAbove > 0) {
                VectorTextRenderer.addText(scrollLayer, "^", railX - 5,
                    yOffset - 3, 18, PanelTheme.CYAN, true);
                VectorTextRenderer.addText(scrollLayer, "+" + hiddenAbove,
                    railX - 35, yOffset + 1, 13, PanelTheme.CYAN, true);
            }
            if (hiddenBelow > 0) {
                VectorTextRenderer.addText(scrollLayer, "+" + hiddenBelow,
                    railX - 35, yOffset + trackHeight - 20, 13,
                    PanelTheme.CYAN, true);
                VectorTextRenderer.addText(scrollLayer, "V", railX - 5,
                    yOffset + trackHeight - 23, 17, PanelTheme.CYAN, true);
            }
        }

        private function drawChoicePopup(current:Object, state:MenuSelectionState,
            gridHeight:Number):void
        {
            if (!choiceIsOpen) return;
            if (String(current.moduleId) != openChoiceModuleId ||
                String(current.pageId) != openChoicePageId) {
                closeChoice();
                return;
            }
            var controlIndex:int = -1;
            for (var index:int = 0; index < current.controls.length; ++index) {
                if (String(current.controls[index].controlId) ==
                    openChoiceControlId) {
                    controlIndex = index;
                    openChoiceControl = current.controls[index];
                    break;
                }
            }
            var isGridControl:Boolean = MenuSelectionState.isGridRowControl(current, openChoiceControl);
            if ((!isGridControl && (controlIndex < state.firstVisibleRow ||
                controlIndex >= state.firstVisibleRow + int(current.visibleControlRows))) ||
                openChoiceControl.choiceOptions == null ||
                openChoiceControl.choiceOptions.length == 0) {
                closeChoice();
                return;
            }
            choiceCursor = Math.max(0, Math.min(choiceCursor,
                openChoiceControl.choiceOptions.length - 1));
            normalizeChoiceWindow();

            var blocker:Sprite = new Sprite();
            blocker.graphics.beginFill(PanelTheme.BACKGROUND, 0.01);
            blocker.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH,
                PanelLayout.STAGE_HEIGHT);
            blocker.graphics.endFill();
            overlayLayer.addChild(blocker);
            hits.register(blocker, "choiceDismiss", null, 0);

            var visible:int = Math.min(PanelLayout.VISIBLE_CHOICE_OPTIONS,
                openChoiceControl.choiceOptions.length - choiceFirstVisible);
            var popupHeight:Number = visible * PanelLayout.CHOICE_OPTION_HEIGHT;
            var popupWidth:Number = isGridControl ? 280 : PanelLayout.CHOICE_POPUP_WIDTH;
            var popupX:Number = isGridControl ?
                PanelLayout.WORKSPACE_X + 798 :
                PanelLayout.WORKSPACE_X + 625;
            var popupY:Number;
            if (isGridControl) {
                var gridRowIdx:int = gridRowIndexForControl(current, openChoiceControl);
                var anchorY:Number = 105 + gridRowIdx * 34;
                popupY = PanelLayout.ROWS_Y + anchorY + 28;
                if (popupY + popupHeight > PanelLayout.HELP_Y) {
                    popupY = PanelLayout.ROWS_Y + anchorY - popupHeight - 4;
                }
            } else {
                var anchorRow:Number = gridHeight +
                    (controlIndex - state.firstVisibleRow) * PanelLayout.ROW_HEIGHT;
                popupY = PanelLayout.ROWS_Y + anchorRow + 42;
                if (popupY + popupHeight > PanelLayout.HELP_Y) {
                    popupY = PanelLayout.ROWS_Y + anchorRow - popupHeight - 4;
                }
            }
            popupY = Math.max(PanelLayout.SAFE_MARGIN,
                Math.min(PanelLayout.STAGE_HEIGHT - PanelLayout.SAFE_MARGIN -
                    popupHeight, popupY));

            var popup:Sprite = new Sprite();
            popup.x = popupX;
            popup.y = popupY;
            popup.graphics.lineStyle(2, PanelTheme.CYAN);
            popup.graphics.beginFill(PanelTheme.PANEL, 1.0);
            popup.graphics.drawRect(0, 0, popupWidth,
                popupHeight);
            popup.graphics.endFill();
            overlayLayer.addChild(popup);

            for (var visibleIndex:int = 0; visibleIndex < visible; ++visibleIndex) {
                var optionIndex:int = choiceFirstVisible + visibleIndex;
                var option:Object = openChoiceControl.choiceOptions[optionIndex];
                var optionRow:Sprite = new Sprite();
                var focused:Boolean = optionIndex == choiceCursor;
                var selected:Boolean = Number(option.value) ==
                    Number(openChoiceControl.integerValue);
                optionRow.graphics.lineStyle(1,
                    focused ? PanelTheme.GOLD : PanelTheme.ROW_BORDER);
                optionRow.graphics.beginFill(focused ? PanelTheme.ROW_SELECTED :
                    (selected ? PanelTheme.SELECTED_FILL : PanelTheme.WIDGET_FILL));
                optionRow.graphics.drawRect(0, 0,
                    popupWidth, PanelLayout.CHOICE_OPTION_HEIGHT);
                optionRow.graphics.endFill();
                optionRow.y = visibleIndex * PanelLayout.CHOICE_OPTION_HEIGHT;
                optionRow.buttonMode = true;
                optionRow.mouseChildren = false;
                VectorTextRenderer.addText(optionRow,
                    VectorTextRenderer.fit(String(option.label), isGridControl ? 24 : 42), 14, 8, 16,
                    selected ? PanelTheme.CYAN : PanelTheme.TEXT, focused);
                popup.addChild(optionRow);
                hits.register(optionRow, "choiceOption",
                    {"control":openChoiceControl, "option":option}, optionIndex);
            }

            if (openChoiceControl.choiceOptions.length > visible) {
                var hiddenAbove:int = choiceFirstVisible;
                var hiddenBelow:int = Math.max(0,
                    openChoiceControl.choiceOptions.length - choiceFirstVisible - visible);
                if (hiddenAbove > 0) {
                    VectorTextRenderer.addText(popup, "^ +" + hiddenAbove,
                        popupWidth - 58, 6, 13,
                        PanelTheme.CYAN, true);
                }
                if (hiddenBelow > 0) {
                    VectorTextRenderer.addText(popup, "+" + hiddenBelow + " V",
                        popupWidth - 58,
                        popupHeight - 22, 13, PanelTheme.CYAN, true);
                }
            }
        }

        private function drawRecordCollectionPopup(current:Object):void
        {
            if (!recordCollectionIsOpen) return;
            if (current == null || String(current.moduleId) != openRecordModuleId ||
                String(current.pageId) != openRecordPageId) {
                closeRecordCollection();
                return;
            }
            var found:Boolean = false;
            for (var controlIndex:int = 0;
                 controlIndex < current.controls.length; ++controlIndex) {
                if (String(current.controls[controlIndex].controlId) ==
                    openRecordControlId) {
                    openRecordControl = current.controls[controlIndex];
                    found = true;
                    break;
                }
            }
            if (!found || openRecordControl.recordItems == null) {
                closeRecordCollection();
                return;
            }
            var itemCount:int = openRecordControl.recordItems.length;
            if (itemCount > 0) {
                recordCursor = Math.max(0, Math.min(recordCursor, itemCount - 1));
                normalizeRecordWindow();
            } else {
                recordCursor = 0;
                recordFirstVisible = 0;
            }

            var blocker:Sprite = new Sprite();
            blocker.graphics.beginFill(PanelTheme.BACKGROUND, 0.78);
            blocker.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH,
                PanelLayout.STAGE_HEIGHT);
            blocker.graphics.endFill();
            overlayLayer.addChild(blocker);
            hits.register(blocker, "recordDismiss", null, 0);

            var width:Number = 1080;
            var height:Number = 540;
            var dialog:Sprite = new Sprite();
            dialog.x = (PanelLayout.STAGE_WIDTH - width) / 2;
            dialog.y = (PanelLayout.STAGE_HEIGHT - height) / 2;
            dialog.graphics.lineStyle(2, PanelTheme.CYAN);
            dialog.graphics.beginFill(PanelTheme.PANEL, 1.0);
            dialog.graphics.drawRect(0, 0, width, height);
            dialog.graphics.endFill();
            overlayLayer.addChild(dialog);

            VectorTextRenderer.addText(dialog,
                String(openRecordControl.label).toUpperCase(), 28, 20, 23,
                PanelTheme.TEXT, true, width - 56, 32);
            VectorTextRenderer.addText(dialog,
                VectorTextRenderer.fit(String(openRecordControl.description), 92),
                28, 54, 15, PanelTheme.MUTED_TEXT, false, width - 56, 28);

            var listX:Number = 28;
            var listY:Number = 96;
            var listWidth:Number = 390;
            var rowHeight:Number = 48;
            var visible:int = Math.min(8, itemCount - recordFirstVisible);
            if (itemCount == 0) {
                VectorTextRenderer.addText(dialog, "NO RECORDS AVAILABLE", listX,
                    listY + 18, 17, PanelTheme.DIM_TEXT, true);
            }
            for (var visibleIndex:int = 0; visibleIndex < visible;
                 ++visibleIndex) {
                var itemIndex:int = recordFirstVisible + visibleIndex;
                var item:Object = openRecordControl.recordItems[itemIndex];
                var row:Sprite = new Sprite();
                row.x = listX;
                row.y = listY + visibleIndex * rowHeight;
                var focused:Boolean = itemIndex == recordCursor;
                var selected:Boolean = String(item.recordId) ==
                    String(openRecordControl.stringValue);
                var disabled:Boolean = (uint(item.flags) & 1) != 0;
                row.graphics.lineStyle(focused ? 2 : 1,
                    focused ? PanelTheme.GOLD : PanelTheme.ROW_BORDER);
                row.graphics.beginFill(focused ? PanelTheme.ROW_SELECTED :
                    (selected ? PanelTheme.SELECTED_FILL : PanelTheme.WIDGET_FILL));
                row.graphics.drawRect(0, 0, listWidth, rowHeight - 4);
                row.graphics.endFill();
                row.buttonMode = !disabled;
                VectorTextRenderer.addText(row,
                    VectorTextRenderer.fit(String(item.label), 34), 12, 7, 16,
                    disabled ? PanelTheme.DISABLED_TEXT :
                        ((uint(item.flags) & 2) != 0 ? PanelTheme.WARNING :
                            (selected ? PanelTheme.CYAN : PanelTheme.TEXT)), true);
                VectorTextRenderer.addText(row,
                    VectorTextRenderer.fit(String(item.summary), 44), 12, 26,
                    12, PanelTheme.DIM_TEXT);
                dialog.addChild(row);
                hits.register(row, disabled ? "disabled" : "recordItem",
                    {"control":openRecordControl, "item":item}, itemIndex);
            }

            var detailX:Number = 450;
            dialog.graphics.lineStyle(1, PanelTheme.BORDER);
            dialog.graphics.beginFill(PanelTheme.WIDGET_FILL);
            dialog.graphics.drawRect(detailX, listY, width - detailX - 28, 390);
            dialog.graphics.endFill();
            if (itemCount > 0) {
                var detailItem:Object = openRecordControl.recordItems[recordCursor];
                VectorTextRenderer.addText(dialog, String(detailItem.label),
                    detailX + 22, listY + 20, 21,
                    (uint(detailItem.flags) & 2) != 0 ? PanelTheme.WARNING :
                        PanelTheme.TEXT, true, width - detailX - 72, 32);
                VectorTextRenderer.addText(dialog, String(detailItem.summary),
                    detailX + 22, listY + 62, 16, PanelTheme.CYAN, false,
                    width - detailX - 72, 54, true);
                VectorTextRenderer.addText(dialog, String(detailItem.detail),
                    detailX + 22, listY + 132, 16, PanelTheme.MUTED_TEXT, false,
                    width - detailX - 72, 220, true);
            }
            VectorTextRenderer.addText(dialog,
                "ENTER SELECT    ESC CLOSE" +
                    (itemCount > 8 ? "    " + (recordFirstVisible + 1) + "-" +
                        (recordFirstVisible + visible) + " / " + itemCount : ""),
                28, height - 40, 14, PanelTheme.DIM_TEXT, true);
        }

        private function drawFooter(model:Object, state:MenuSelectionState,
            inputMode:String):void
        {
            footerLayer.graphics.clear();
            footerLayer.graphics.lineStyle(1, PanelTheme.BORDER);
            footerLayer.graphics.moveTo(0, 0);
            footerLayer.graphics.lineTo(PanelLayout.FOOTER_WIDTH, 0);
            var prefix:String = inputMode == "mouse" ? "" :
                (inputMode == "controller" ? "A " : "ENTER ");
            var actionX:Number = PanelLayout.FOOTER_ACTIONS_X;
            addFooterButton(prefix + "APPLY", actionX, 0, Boolean(model.dirty), state);
            addFooterButton(prefix + "CANCEL", actionX + 185, 1,
                Boolean(model.dirty), state);
            addFooterButton((inputMode == "controller" ? "B " : "TAB ESC ") + "BACK",
                actionX + 390, 2, true, state);
        }

        private function addFooterButton(label:String, xPosition:Number, actionIndex:int,
            enabled:Boolean, state:MenuSelectionState):void
        {
            var button:Sprite = new Sprite();
            var selected:Boolean = state.focusRegion == PanelLayout.FOCUS_ACTIONS &&
                state.focusedAction == actionIndex;
            button.graphics.lineStyle(selected ? 2 : 1,
                selected ? PanelTheme.CYAN : (enabled ? PanelTheme.BORDER : PanelTheme.BORDER));
            button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL : PanelTheme.BUTTON_FILL);
            button.graphics.drawRect(0, 5, 175, 38);
            button.graphics.endFill();
            button.x = xPosition;
            button.buttonMode = enabled;
            VectorTextRenderer.addText(button, label, 14, 13, 15,
                enabled ? PanelTheme.TEXT : PanelTheme.DISABLED_TEXT, selected);
            footerLayer.addChild(button);
            hits.register(button, enabled ? "action" : "disabled", null, actionIndex);
        }
    }
}
