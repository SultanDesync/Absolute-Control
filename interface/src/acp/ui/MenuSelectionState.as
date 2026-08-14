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
                Math.min(PanelLayout.FOCUS_ACTIONS, int(model.focusRegion)));
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
            if (selectedRow < firstVisibleRow) firstVisibleRow = selectedRow;
            if (selectedRow > firstVisibleRow + PanelLayout.VISIBLE_ROWS - 1) {
                firstVisibleRow = selectedRow - PanelLayout.VISIBLE_ROWS + 1;
            }
        }

        public function modulePageStarts():Array
        {
            var result:Array = [];
            if (model == null || model.pages == null) return result;
            for (var i:int = 0; i < model.pages.length; ++i) {
                var moduleId:String = String(model.pages[i].moduleId);
                var known:Boolean = false;
                for (var found:int = 0; found < result.length; ++found) {
                    if (String(model.pages[int(result[found])].moduleId) == moduleId) {
                        known = true;
                        break;
                    }
                }
                if (!known) result.push(i);
            }
            return result;
        }

        public function activeModulePosition(starts:Array):int
        {
            for (var i:int = 0; i < starts.length; ++i) {
                if (String(model.pages[int(starts[i])].moduleId) == currentPageModule()) return i;
            }
            return 0;
        }

        public function activeModulePages():Array
        {
            var result:Array = [];
            if (model == null || model.pages == null) return result;
            var moduleId:String = currentPageModule();
            for (var i:int = 0; i < model.pages.length; ++i) {
                if (String(model.pages[i].moduleId) == moduleId) result.push(i);
            }
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
            if (model == null || model.pages.length == 0) return null;
            var starts:Array = modulePageStarts();
            if (starts.length == 0) return null;
            var modulePosition:int = activeModulePosition(starts);
            var next:int = int(starts[(modulePosition + direction + starts.length) % starts.length]);
            return model.pages[next];
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
