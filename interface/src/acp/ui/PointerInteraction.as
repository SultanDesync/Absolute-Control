package acp.ui
{
    import flash.display.Sprite;

    public final class PointerInteraction
    {
        private var hitTargets:Array = [];
        private var draggingSlider:Boolean = false;
        private var draggingModuleId:String = "";
        private var draggingPageId:String = "";
        private var draggingControlId:String = "";

        public function resetHits():void
        {
            hitTargets = [];
        }

        public function register(view:Sprite, kind:String, payload:Object, index:int):void
        {
            hitTargets.push({ "view":view, "kind":kind, "payload":payload, "index":index });
        }

        public function hit(stageX:Number, stageY:Number):Object
        {
            for (var i:int = hitTargets.length - 1; i >= 0; --i) {
                var target:Object = hitTargets[i];
                var view:Sprite = target.view as Sprite;
                if (view != null && view.hitTestPoint(stageX, stageY, false)) return target;
            }
            return null;
        }

        public function beginSlider(current:Object, control:Object):void
        {
            draggingSlider = true;
            draggingModuleId = current == null ? "" : String(current.moduleId);
            draggingPageId = current == null ? "" : String(current.pageId);
            draggingControlId = String(control.controlId);
        }

        public function moveSlider(stageX:Number, stageY:Number, current:Object,
            dispatch:Function):Boolean
        {
            if (!draggingSlider) return false;
            if (current == null || String(current.moduleId) != draggingModuleId ||
                String(current.pageId) != draggingPageId) {
                clearSliderDrag();
                return false;
            }
            for (var i:int = hitTargets.length - 1; i >= 0; --i) {
                var target:Object = hitTargets[i];
                if (String(target.kind) != "slider" || target.payload == null ||
                    String(target.payload.controlId) != draggingControlId) continue;
                var view:Sprite = target.view as Sprite;
                if (view == null) {
                    clearSliderDrag();
                    return false;
                }
                ControlWidgets.writeSliderFromPointer(view, target.payload,
                    stageX, stageY, dispatch);
                return true;
            }
            clearSliderDrag();
            return false;
        }

        public function clearSliderDrag():void
        {
            draggingSlider = false;
            draggingModuleId = "";
            draggingPageId = "";
            draggingControlId = "";
        }

        public function get isDraggingSlider():Boolean
        {
            return draggingSlider;
        }

        public function wheelRegion(stageX:Number, stageY:Number):String
        {
            if (stageX >= PanelLayout.SIDEBAR_X &&
                stageX <= PanelLayout.SIDEBAR_X + PanelLayout.SIDEBAR_WIDTH &&
                stageY >= PanelLayout.SIDEBAR_Y && stageY < PanelLayout.FOOTER_Y) {
                return "modules";
            }
            if (stageX >= PanelLayout.WORKSPACE_X &&
                stageX <= PanelLayout.WORKSPACE_X + PanelLayout.WORKSPACE_WIDTH &&
                stageY >= PanelLayout.TABS_Y && stageY <= PanelLayout.TABS_Y + 48) {
                return "tabs";
            }
            if (stageX >= PanelLayout.WORKSPACE_X &&
                stageX <= PanelLayout.WORKSPACE_X + PanelLayout.WORKSPACE_WIDTH &&
                stageY >= PanelLayout.ROWS_Y && stageY < PanelLayout.ROWS_BOTTOM) {
                return "rows";
            }
            return "";
        }
    }
}
