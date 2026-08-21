package acp.ui
{
    public final class MenuSelectionState
    {
        private var model:Object;

        public var activePageIndex:int = 0;
        public var firstVisibleModule:int = 0;
        public var firstVisibleTab:int = 0;
        public var selectedRow:int = 0;
        public var firstVisibleRow:int = 0;
        public var focusRegion:int = PanelLayout.FOCUS_CONTROLS;
        public var focusedAction:int = 0;
        private var preserveSemanticViewport:Boolean = false;
        private static const PINNED_CONTEXT_FLAG:uint = 1 << 13;

        public static function isGridRowControl(current:Object, control:Object):Boolean
        {
            if (current == null || control == null || current.liveComponents == null) return false;
            if (uint(control.kind) != 3) return false;
            var cId:String = String(control.controlId);
            for (var compIdx:int = 0; compIdx < current.liveComponents.length; ++compIdx) {
                var comp:Object = current.liveComponents[compIdx];
                if (uint(comp.kind) == 2 && comp.columns != null) {
                    for (var colIdx:int = 0; colIdx < comp.columns.length; ++colIdx) {
                        var col:Object = comp.columns[colIdx];
                        var assocId:String = col.associatedControlId != null ? String(col.associatedControlId) : "";
                        if (assocId.length > 0 && cId == assocId) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        public static function isHeaderEmpty(current:Object, headerIndex:int):Boolean
        {
            if (current == null || current.controls == null) return true;
            for (var i:int = headerIndex + 1; i < current.controls.length; ++i) {
                var c:Object = current.controls[i];
                if (uint(c.kind) == 7) break;
                if (!isGridRowControl(current, c)) return false;
            }
            return true;
        }

        public function applyModel(next:Object):void
        {
            var previous:Object = page();
            var previousModuleId:String = previous == null ? "" :
                String(previous.moduleId);
            var previousPageId:String = previous == null ? "" :
                String(previous.pageId);
            var viewportControlId:String = "";
            if (previous != null && previous.controls != null &&
                firstVisibleRow >= 0 &&
                firstVisibleRow < previous.controls.length) {
                viewportControlId =
                    String(previous.controls[firstVisibleRow].controlId);
            }
            model = next;
            activePageIndex = Math.max(0,
                Math.min(int(model.activePage), model.pages.length - 1));
            selectedRow = Math.max(0, int(model.selectedControl));
            focusRegion = Math.max(PanelLayout.FOCUS_MODULES,
                Math.min(PanelLayout.FOCUS_ANCHORS, int(model.focusRegion)));
            focusedAction = Math.max(0, int(model.focusedAction));
            var current:Object = page();
            preserveSemanticViewport = false;
            if (current != null && Boolean(current.compositionEnhanced) &&
                String(current.moduleId) == previousModuleId &&
                String(current.pageId) == previousPageId &&
                viewportControlId.length > 0) {
                var viewportIndex:int = controlIndexById(
                    current, viewportControlId);
                if (viewportIndex >= 0) {
                    firstVisibleRow = viewportIndex;
                    preserveSemanticViewport = true;
                }
            }
            normalize();
        }

        public function page():Object
        {
            return model != null && model.pages != null &&
                activePageIndex < model.pages.length ? model.pages[activePageIndex] : null;
        }

        public function normalize():void
        {
            if (model != null && model.pages != null && model.pages.length > 0) {
                var starts:Array = modulePageStarts();
                var modulePosition:int = activeModulePosition(starts);
                if (modulePosition < firstVisibleModule) firstVisibleModule = modulePosition;
                if (modulePosition > firstVisibleModule + PanelLayout.VISIBLE_MODULES - 1) {
                    firstVisibleModule = modulePosition - PanelLayout.VISIBLE_MODULES + 1;
                }

                var tabs:Array = activeModulePages();
                var tabPosition:int = activeTabPosition(tabs);
                if (tabPosition < firstVisibleTab) firstVisibleTab = tabPosition;
                if (tabPosition > firstVisibleTab + PanelLayout.VISIBLE_TABS - 1) {
                    firstVisibleTab = tabPosition - PanelLayout.VISIBLE_TABS + 1;
                }
            }

            var current:Object = page();
            var anchors:Array = semanticAnchors(current);
            if (focusRegion == PanelLayout.FOCUS_ANCHORS) {
                if (anchors.length == 0) {
                    focusRegion = PanelLayout.FOCUS_CONTROLS;
                    focusedAction = 0;
                } else {
                    focusedAction = Math.min(focusedAction,
                        anchors.length - 1);
                }
            } else {
                focusedAction = Math.min(focusedAction, 2);
            }
            if (current == null || current.controls == null || current.controls.length == 0) {
                selectedRow = 0;
                firstVisibleRow = 0;
                return;
            }
            selectedRow = Math.max(0, Math.min(selectedRow, current.controls.length - 1));
            if (uint(current.controls[selectedRow].kind) == 7) {
                var replacement:int = nextSelectableIndex(current, selectedRow, 1);
                if (replacement < 0) replacement = nextSelectableIndex(
                    current, selectedRow, -1);
                selectedRow = Math.max(0, replacement);
            }
            var visibleRows:int = current.liveComponents != null &&
                current.liveComponents.length > 0 ? 5 : PanelLayout.VISIBLE_ROWS;
            var rows:Array = controlRows(current);
            if (isPinnedControl(current.controls[selectedRow])) return;
            if (rows.length == 0) return;
            var selectedLayout:int = layoutRowForControl(rows, selectedRow);
            if (Boolean(current.compositionEnhanced)) {
                if (preserveSemanticViewport) {
                    preserveSemanticViewport = false;
                    return;
                }
                // Semantic rows can contain an embedded plot, so a fixed row
                // count cannot prove that the selected control is visible.
                // Pin the selected layout row to the viewport start instead.
                firstVisibleRow = int(rows[selectedLayout][0]);
                return;
            }
            var firstLayout:int = layoutRowForControl(rows, firstVisibleRow);
            if (selectedLayout < firstLayout) firstLayout = selectedLayout;
            if (selectedLayout > firstLayout + visibleRows - 1) {
                firstLayout = selectedLayout - visibleRows + 1;
            }
            firstLayout = Math.max(0, Math.min(firstLayout,
                Math.max(0, rows.length - visibleRows)));
            firstVisibleRow = int(rows[firstLayout][0]);
        }

        public function moveControl(direction:int):Object
        {
            var current:Object = page();
            if (current == null || current.controls == null ||
                current.controls.length == 0) return null;
            var next:int = nextSelectableIndex(current, selectedRow, direction);
            if (next < 0) return null;
            selectedRow = next;
            normalize();
            return current.controls[selectedRow];
        }

        public function controlRows(current:Object):Array
        {
            var rows:Array = [];
            if (current == null || current.controls == null) return rows;
            if (Boolean(current.compositionEnhanced) &&
                current.compositionNodes != null) {
                var consumed:Object = {};
                for (var nodeIndex:int = 0;
                     nodeIndex < current.compositionNodes.length; ++nodeIndex) {
                    var node:Object = current.compositionNodes[nodeIndex];
                    if (uint(node.kind) != 11 ||
                        !nodeIsEffective(current, node, 1)) continue;
                    var controlIndex:int = controlIndexById(
                        current, String(node.referenceId));
                    if (controlIndex < 0 ||
                        isPinnedControl(current.controls[controlIndex]) ||
                        !Boolean(current.controls[controlIndex].semanticVisible)) continue;
                    var parent:Object = compositionNodeById(
                        current, String(node.parentNodeId));
                    if (parent != null && uint(parent.kind) == 3) {
                        var parentId:String = String(parent.nodeId);
                        if (Boolean(consumed[parentId])) continue;
                        consumed[parentId] = true;
                        var semanticRow:Array = [];
                        for (var siblingIndex:int = 0;
                             siblingIndex < current.compositionNodes.length;
                             ++siblingIndex) {
                            var sibling:Object =
                                current.compositionNodes[siblingIndex];
                            if (uint(sibling.kind) != 11 ||
                                String(sibling.parentNodeId) != parentId ||
                                !nodeIsEffective(current, sibling, 1)) continue;
                            var siblingControl:int = controlIndexById(
                                current, String(sibling.referenceId));
                            if (siblingControl >= 0 &&
                                !isPinnedControl(current.controls[siblingControl]) && Boolean(
                                current.controls[siblingControl].semanticVisible)) {
                                semanticRow.push(siblingControl);
                            }
                        }
                        for (var rowStart:int = 0;
                             rowStart < semanticRow.length; rowStart += 3) {
                            rows.push(semanticRow.slice(rowStart,
                                Math.min(rowStart + 3, semanticRow.length)));
                        }
                    } else {
                        rows.push([controlIndex]);
                    }
                }
                return rows;
            }
            var index:int = 0;
            while (index < current.controls.length) {
                var control:Object = current.controls[index];
                if (isPinnedControl(control) ||
                    isGridRowControl(current, control) ||
                    (uint(control.kind) == 7 && isHeaderEmpty(current, index))) {
                    ++index;
                    continue;
                }
                var row:Array = [index];
                if (uint(control.kind) == 4 &&
                    (uint(control.flags) & 64) != 0) {
                    while (row.length < 3 && index + row.length <
                        current.controls.length) {
                        var candidate:Object = current.controls[index + row.length];
                        if (isGridRowControl(current, candidate) ||
                            (uint(candidate.kind) == 7 && isHeaderEmpty(current, index + row.length))) break;
                        if (uint(candidate.kind) != 4 ||
                            (uint(candidate.flags) & 64) == 0) break;
                        row.push(index + row.length);
                    }
                }
                rows.push(row);
                index += row.length;
            }
            return rows;
        }

        public function pinnedControls(current:Object):Array
        {
            var result:Array = [];
            if (current == null || current.controls == null) return result;
            for (var index:int = 0; index < current.controls.length; ++index) {
                var control:Object = current.controls[index];
                if (isPinnedControl(control) && Boolean(control.semanticVisible)) {
                    result.push({"index":index, "control":control});
                }
            }
            return result;
        }

        private function isPinnedControl(control:Object):Boolean
        {
            return control != null &&
                (uint(control.flags) & PINNED_CONTEXT_FLAG) != 0;
        }

        private function layoutRowForControl(rows:Array, controlIndex:int):int
        {
            for (var row:int = 0; row < rows.length; ++row) {
                for (var item:int = 0; item < rows[row].length; ++item) {
                    if (int(rows[row][item]) == controlIndex) return row;
                }
            }
            return 0;
        }

        private function nextSelectableIndex(current:Object, start:int,
            direction:int):int
        {
            var ordered:Array = [];
            var pinned:Array = pinnedControls(current);
            for (var pinnedIndex:int = 0; pinnedIndex < pinned.length;
                 ++pinnedIndex) {
                if (Boolean(pinned[pinnedIndex].control.semanticEnabled)) {
                    ordered.push(int(pinned[pinnedIndex].index));
                }
            }
            var rows:Array = controlRows(current);
            for (var row:int = 0; row < rows.length; ++row) {
                for (var item:int = 0; item < rows[row].length; ++item) {
                    var candidateIndex:int = int(rows[row][item]);
                    var candidate:Object = current.controls[candidateIndex];
                    if (uint(candidate.kind) != 7 &&
                        !isGridRowControl(current, candidate) &&
                        Boolean(candidate.semanticVisible) &&
                        Boolean(candidate.semanticEnabled)) {
                        ordered.push(candidateIndex);
                    }
                }
            }
            if (ordered.length == 0) return -1;
            var position:int = ordered.indexOf(start);
            if (position < 0) return direction < 0 ?
                int(ordered[ordered.length - 1]) : int(ordered[0]);
            position += direction < 0 ? -1 : 1;
            if (position >= 0 && position < ordered.length) {
                return int(ordered[position]);
            }
            return -1;
        }

        public function semanticAnchors(current:Object):Array
        {
            var result:Array = [];
            if (current == null || !Boolean(current.compositionEnhanced) ||
                current.compositionNodes == null) return result;
            for (var index:int = 0;
                 index < current.compositionNodes.length; ++index) {
                var anchor:Object = current.compositionNodes[index];
                if (uint(anchor.kind) != 6 ||
                    !nodeIsEffective(current, anchor, 3)) continue;
                var targetId:String = String(anchor.referenceId);
                for (var placementIndex:int = 0;
                     placementIndex < current.compositionNodes.length;
                     ++placementIndex) {
                    var placement:Object =
                        current.compositionNodes[placementIndex];
                    if (uint(placement.kind) != 11 ||
                        !nodeIsEffective(current, placement, 3) ||
                        !nodeDescendsFrom(current, placement, targetId)) continue;
                    var controlIndex:int = controlIndexById(
                        current, String(placement.referenceId));
                    if (controlIndex >= 0 &&
                        Boolean(current.controls[controlIndex].semanticVisible) &&
                        Boolean(current.controls[controlIndex].semanticEnabled)) {
                        result.push({"nodeId":String(anchor.nodeId),
                            "label":String(anchor.label),
                            "controlIndex":controlIndex,
                            "control":current.controls[controlIndex]});
                        break;
                    }
                }
            }
            return result;
        }

        public function anchorTarget(index:int):Object
        {
            var anchors:Array = semanticAnchors(page());
            return index >= 0 && index < anchors.length ? anchors[index] : null;
        }

        public function selectControlIndex(index:int):Object
        {
            var current:Object = page();
            if (current == null || index < 0 ||
                index >= current.controls.length) return null;
            selectedRow = index;
            normalize();
            return current.controls[selectedRow];
        }

        public function selectControlId(controlId:String):Object
        {
            var current:Object = page();
            if (current == null) return null;
            var index:int = controlIndexById(current, controlId);
            if (index < 0) return null;
            selectedRow = index;
            return current.controls[index];
        }

        private function controlIndexById(current:Object, controlId:String):int
        {
            for (var index:int = 0; index < current.controls.length; ++index) {
                if (String(current.controls[index].controlId) == controlId) {
                    return index;
                }
            }
            return -1;
        }

        private function compositionNodeById(current:Object, nodeId:String):Object
        {
            if (nodeId.length == 0 || current.compositionNodes == null) return null;
            for (var index:int = 0;
                 index < current.compositionNodes.length; ++index) {
                if (String(current.compositionNodes[index].nodeId) == nodeId) {
                    return current.compositionNodes[index];
                }
            }
            return null;
        }

        private function nodeIsEffective(current:Object, node:Object,
            requiredFlags:uint):Boolean
        {
            var cursor:Object = node;
            var remaining:int = current.compositionNodes.length + 1;
            while (cursor != null && remaining-- > 0) {
                if ((uint(cursor.stateFlags) & requiredFlags) != requiredFlags) {
                    return false;
                }
                var parentId:String = String(cursor.parentNodeId);
                if (parentId.length == 0) return true;
                cursor = compositionNodeById(current, parentId);
            }
            return false;
        }

        private function nodeDescendsFrom(current:Object, node:Object,
            ancestorId:String):Boolean
        {
            var cursor:Object = node;
            var remaining:int = current.compositionNodes.length + 1;
            while (cursor != null && remaining-- > 0) {
                if (String(cursor.nodeId) == ancestorId) return true;
                var parentId:String = String(cursor.parentNodeId);
                if (parentId.length == 0) return false;
                cursor = compositionNodeById(current, parentId);
            }
            return false;
        }

        public function modulePageStarts():Array
        {
            var result:Array = [];
            if (model == null || model.modules == null) return result;
            for (var i:int = 0; i < model.modules.length; ++i) result.push(i);
            return result;
        }

        public function activeModulePosition(starts:Array):int
        {
            for (var i:int = 0; i < starts.length; ++i) {
                if (String(model.modules[int(starts[i])].moduleId) == currentPageModule()) return i;
            }
            return 0;
        }

        public function activeModulePages():Array
        {
            var result:Array = [];
            if (model == null || model.pages == null) return result;
            for (var i:int = 0; i < model.pages.length; ++i) result.push(i);
            return result;
        }

        public function activeTabPosition(tabs:Array):int
        {
            for (var i:int = 0; i < tabs.length; ++i) {
                if (int(tabs[i]) == activePageIndex) return i;
            }
            return 0;
        }

        public function pageTarget(direction:int):Object
        {
            if (model == null || model.pages.length == 0) return null;
            var tabs:Array = activeModulePages();
            if (tabs.length == 0) return null;
            var tabPosition:int = activeTabPosition(tabs);
            var next:int = int(tabs[(tabPosition + direction + tabs.length) % tabs.length]);
            return model.pages[next];
        }

        public function moduleTarget(direction:int):Object
        {
            if (model == null || model.modules == null || model.modules.length == 0) return null;
            var starts:Array = modulePageStarts();
            if (starts.length == 0) return null;
            var modulePosition:int = activeModulePosition(starts);
            var next:int = int(starts[(modulePosition + direction + starts.length) % starts.length]);
            return model.modules[next];
        }

        public function currentPageModule():String
        {
            var current:Object = page();
            return current == null ? "" : String(current.moduleId);
        }

        public function currentPageId():String
        {
            var current:Object = page();
            return current == null ? "" : String(current.pageId);
        }
    }
}
