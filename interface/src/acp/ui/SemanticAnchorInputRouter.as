package acp.ui
{
    public final class SemanticAnchorInputRouter
    {
        public function handlePointer(kind:String, target:Object,
            selection:MenuSelectionState, syncFocus:Function,
            send:Function, redraw:Function):Boolean
        {
            if (kind != "anchor") return false;
            selection.focusRegion = PanelLayout.FOCUS_ANCHORS;
            selection.focusedAction = int(target.index);
            var control:Object = selection.selectControlIndex(
                int(target.payload.controlIndex));
            syncFocus();
            if (control != null) send("selectControl", control, false, 0, 0);
            else redraw();
            return true;
        }

        public function activate(selection:MenuSelectionState,
            syncFocus:Function, send:Function):Boolean
        {
            if (selection.focusRegion != PanelLayout.FOCUS_ANCHORS) return false;
            var anchor:Object = selection.anchorTarget(selection.focusedAction);
            if (anchor != null) {
                var control:Object = selection.selectControlIndex(
                    int(anchor.controlIndex));
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                syncFocus();
                if (control != null) send("selectControl", control, false, 0, 0);
            }
            return true;
        }

        public function moveVertical(selection:MenuSelectionState,
            current:Object, direction:int, syncFocus:Function,
            redraw:Function):Boolean
        {
            if (selection.focusRegion != PanelLayout.FOCUS_ANCHORS) return false;
            var anchors:Array = selection.semanticAnchors(current);
            if (anchors.length > 0) {
                selection.focusedAction = (selection.focusedAction + direction +
                    anchors.length) % anchors.length;
                syncFocus();
                redraw();
            }
            return true;
        }

        public function moveHorizontal(selection:MenuSelectionState,
            current:Object, direction:int, hasGrid:Boolean,
            syncFocus:Function, redraw:Function):Boolean
        {
            var target:int = -1;
            var hasAnchors:Boolean = selection.semanticAnchors(current).length > 0;
            if (direction < 0 &&
                selection.focusRegion == PanelLayout.FOCUS_CONTROLS) {
                target = hasGrid ? PanelLayout.FOCUS_GRID :
                    (hasAnchors ? PanelLayout.FOCUS_ANCHORS :
                        PanelLayout.FOCUS_MODULES);
            } else if (direction < 0 &&
                selection.focusRegion == PanelLayout.FOCUS_ANCHORS) {
                target = PanelLayout.FOCUS_MODULES;
            } else if (direction > 0 &&
                selection.focusRegion == PanelLayout.FOCUS_MODULES) {
                target = hasGrid ? PanelLayout.FOCUS_GRID :
                    (hasAnchors ? PanelLayout.FOCUS_ANCHORS :
                        PanelLayout.FOCUS_CONTROLS);
            } else if (direction > 0 &&
                selection.focusRegion == PanelLayout.FOCUS_ANCHORS) {
                target = PanelLayout.FOCUS_CONTROLS;
            }
            if (target < 0) return false;
            selection.focusRegion = target;
            selection.focusedAction = 0;
            syncFocus();
            redraw();
            return true;
        }
    }
}
