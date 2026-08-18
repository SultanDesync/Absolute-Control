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
        private var scrollLayer:Sprite;
        private var helpLayer:Sprite;
        private var footerLayer:Sprite;
        private var overlayLayer:Sprite;
        private var tooltipLayer:Sprite;
        private var statusField:Sprite;
        private var activeGridTiers:Object = {};
        private var gridFocusActive:Boolean = false;
        private var selectedGridColumnId:String = "";
        private var openChoiceModuleId:String = "";
        private var openChoicePageId:String = "";
        private var openChoiceControlId:String = "";
        private var openChoiceControl:Object;
        private var choiceCursor:int = 0;
        private var choiceFirstVisible:int = 0;

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
                rowLayer == null || scrollLayer == null || helpLayer == null ||
                footerLayer == null || overlayLayer == null ||
                tooltipLayer == null) return;

            clearLayer(rowLayer, true);
            clearLayer(scrollLayer, true);
            clearLayer(moduleLayer, false);
            clearLayer(tabLayer, false);
            clearLayer(helpLayer, false);
            clearLayer(footerLayer, false);
            clearLayer(overlayLayer, true);
            clearLayer(tooltipLayer, true);
            hits.resetHits();

            var current:Object = state.page();
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
            var gridHeight:Number = drawLiveComponents(current);
            var visibleRows:int = gridHeight > 0 ? 5 : PanelLayout.VISIBLE_ROWS;
            var controlRows:Array = state.controlRows(current);
            var firstLayoutRow:int = 0;
            for (var rowSearch:int = 0; rowSearch < controlRows.length;
                 ++rowSearch) {
                if (int(controlRows[rowSearch][0]) == state.firstVisibleRow) {
                    firstLayoutRow = rowSearch;
                    break;
                }
            }
            var visible:int = Math.min(visibleRows,
                controlRows.length - firstLayoutRow);
            var lastVisibleControl:int = state.firstVisibleRow - 1;
            for (var i:int = 0; i < visible; ++i) {
                var physicalRow:Array = controlRows[firstLayoutRow + i];
                var firstControlIndex:int = int(physicalRow[0]);
                var firstControl:Object = current.controls[firstControlIndex];
                var rowY:Number = gridHeight + i * PanelLayout.ROW_HEIGHT;
                if (uint(firstControl.kind) == 7) {
                    addGroupHeader(firstControl, rowY);
                } else if (physicalRow.length > 1) {
                    addInlineRow(model, state, current, physicalRow, rowY);
                } else {
                    addRow(model, state, firstControl, firstControlIndex, rowY);
                }
                lastVisibleControl = int(physicalRow[physicalRow.length - 1]);
            }
            current.visibleControlRows = Math.max(0,
                lastVisibleControl - state.firstVisibleRow + 1);
            drawScrollBar(current, state, gridHeight, visibleRows,
                controlRows, firstLayoutRow);
            drawHelp(current, state);
            drawFooter(model, state, inputMode);
            if (Boolean(model.bindingConflictActive)) {
                closeChoice();
                BindingConflictDialog.draw(
                    overlayLayer, hits, model, inputMode == "controller" ?
                        state.focusedAction : dirtyDecisionCursor, inputMode);
            } else if (Boolean(model.dirtyDecisionActive)) {
                closeChoice();
                DirtyDecisionDialog.draw(
                    overlayLayer, hits, model, inputMode == "controller" ?
                        state.focusedAction : dirtyDecisionCursor, inputMode);
            } else {
                drawChoicePopup(current, state, gridHeight);
            }
        }

        public function openChoice(current:Object, control:Object):void
        {
            if (current == null || control == null || control.choiceOptions == null ||
                control.choiceOptions.length == 0 || !Boolean(control.available)) return;
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

        private function drawLiveComponents(current:Object):Number
        {
            if (current.liveComponents == null || current.liveComponents.length == 0) {
                return 0;
            }
            var component:Object = current.liveComponents[0];
            if (uint(component.kind) != 2 || component.columns == null ||
                component.tiers == null) return 0;
            drawSegmentedGrid(component);
            return 318;
        }

        private function drawSegmentedGrid(component:Object):void
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
                VectorTextRenderer.addText(rowLayer,
                    "LIVE " + int(column.currentCount) + "/" + int(column.maximumCount) +
                    "  TARGET " + int(column.targetCount), quickX + 198,
                    rowY + 4, 14,
                    Boolean(component.available) ? PanelTheme.DIM_TEXT : PanelTheme.WARNING);
            }
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
            var width:Number = (PanelLayout.CONTROL_ROW_WIDTH - 28 -
                gap * (indices.length - 1)) / indices.length;
            for (var item:int = 0; item < indices.length; ++item) {
                var index:int = int(indices[item]);
                var control:Object = current.controls[index];
                var button:Sprite = new Sprite();
                var selected:Boolean = state.focusRegion ==
                    PanelLayout.FOCUS_CONTROLS && state.selectedRow == index;
                button.x = 14 + item * (width + gap);
                button.y = 7;
                button.graphics.lineStyle(selected ? 2 : 1,
                    selected ? PanelTheme.CYAN : PanelTheme.BORDER);
                button.graphics.beginFill(selected ? PanelTheme.SELECTED_FILL :
                    PanelTheme.BUTTON_FILL);
                button.graphics.drawRoundRect(0, 0, width, 34, 6, 6);
                button.graphics.endFill();
                button.buttonMode = Boolean(control.available);
                VectorTextRenderer.addText(button,
                    VectorTextRenderer.fit(String(control.label), 30),
                    14, 7, 16, Boolean(control.available) ? PanelTheme.TEXT :
                    PanelTheme.DISABLED_TEXT, selected);
                row.addChild(button);
                hits.register(button, Boolean(control.available) ? "activate" :
                    "disabled", control, index);
            }
            rowLayer.addChild(row);
        }

        private function drawHelp(current:Object, state:MenuSelectionState):void
        {
            helpLayer.graphics.clear();
            helpLayer.graphics.lineStyle(1, PanelTheme.BORDER);
            helpLayer.graphics.beginFill(PanelTheme.HELP_FILL);
            helpLayer.graphics.drawRect(0, 0, PanelLayout.WORKSPACE_WIDTH,
                PanelLayout.HELP_HEIGHT);
            helpLayer.graphics.endFill();
            var description:String = String(current.description);
            if (current.controls != null && current.controls.length > 0) {
                description = String(current.controls[state.selectedRow].description);
                if (uint(current.controls[state.selectedRow].kind) == 5 &&
                    String(current.controls[state.selectedRow].stringValue).length > 0) {
                    description += "  ASSIGNED: " +
                        String(current.controls[state.selectedRow].stringValue);
                }
            }
            VectorTextRenderer.addText(helpLayer, "SELECTED CONTROL", 14, 10, 13,
                PanelTheme.DIM_TEXT, true);
            VectorTextRenderer.addText(helpLayer, description, 14, 34, 16,
                PanelTheme.MUTED_TEXT, false, PanelLayout.WORKSPACE_WIDTH - 28, 60, true);
        }

        private function drawScrollBar(current:Object, state:MenuSelectionState,
            yOffset:Number, visibleRows:int, layoutRows:Array,
            firstLayoutRow:int):void
        {
            if (layoutRows == null || layoutRows.length <= visibleRows) return;
            var trackHeight:Number = visibleRows * PanelLayout.ROW_HEIGHT - 4;
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
            if (controlIndex < state.firstVisibleRow ||
                controlIndex >= state.firstVisibleRow + int(current.visibleControlRows) ||
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
            var anchorRow:Number = gridHeight +
                (controlIndex - state.firstVisibleRow) * PanelLayout.ROW_HEIGHT;
            var popupX:Number = PanelLayout.WORKSPACE_X + 625;
            var popupY:Number = PanelLayout.ROWS_Y + anchorRow + 42;
            if (popupY + popupHeight > PanelLayout.HELP_Y) {
                popupY = PanelLayout.ROWS_Y + anchorRow - popupHeight - 4;
            }
            popupY = Math.max(PanelLayout.SAFE_MARGIN,
                Math.min(PanelLayout.STAGE_HEIGHT - PanelLayout.SAFE_MARGIN -
                    popupHeight, popupY));

            var popup:Sprite = new Sprite();
            popup.x = popupX;
            popup.y = popupY;
            popup.graphics.lineStyle(2, PanelTheme.CYAN);
            popup.graphics.beginFill(PanelTheme.PANEL, 1.0);
            popup.graphics.drawRect(0, 0, PanelLayout.CHOICE_POPUP_WIDTH,
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
                    PanelLayout.CHOICE_POPUP_WIDTH, PanelLayout.CHOICE_OPTION_HEIGHT);
                optionRow.graphics.endFill();
                optionRow.y = visibleIndex * PanelLayout.CHOICE_OPTION_HEIGHT;
                optionRow.buttonMode = true;
                VectorTextRenderer.addText(optionRow,
                    VectorTextRenderer.fit(String(option.label), 42), 14, 8, 16,
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
                        PanelLayout.CHOICE_POPUP_WIDTH - 58, 6, 13,
                        PanelTheme.CYAN, true);
                }
                if (hiddenBelow > 0) {
                    VectorTextRenderer.addText(popup, "+" + hiddenBelow + " V",
                        PanelLayout.CHOICE_POPUP_WIDTH - 58,
                        popupHeight - 22, 13, PanelTheme.CYAN, true);
                }
            }
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
