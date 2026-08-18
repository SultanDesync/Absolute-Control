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

        public function applyModel(next:Object):void
        {
            model = next;
            activePageIndex = Math.max(0,
                Math.min(int(model.activePage), model.pages.length - 1));
            selectedRow = Math.max(0, int(model.selectedControl));
            focusRegion = Math.max(PanelLayout.FOCUS_MODULES,
                Math.min(PanelLayout.FOCUS_GRID, int(model.focusRegion)));
            focusedAction = Math.max(0, Math.min(2, int(model.focusedAction)));
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
            if (rows.length == 0) return;
            var selectedLayout:int = layoutRowForControl(rows, selectedRow);
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
            var index:int = 0;
            while (index < current.controls.length) {
                var control:Object = current.controls[index];
                var row:Array = [index];
                if (uint(control.kind) == 4 &&
                    (uint(control.flags) & 64) != 0) {
                    while (row.length < 3 && index + row.length <
                        current.controls.length) {
                        var candidate:Object = current.controls[index + row.length];
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
            var count:int = current.controls.length;
            if (count == 0) return -1;
            var index:int = start;
            for (var visited:int = 0; visited < count; ++visited) {
                index = (index + (direction < 0 ? -1 : 1) + count) % count;
                if (uint(current.controls[index].kind) != 7) return index;
            }
            return -1;
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
