package acp.ui
{
    import flash.display.Sprite;
    import flash.ui.Keyboard;

    public final class LiveSurfaceInputRouter
    {
        public function handlePointer(kind:String, target:Object,
            stageX:Number, stageY:Number, shell:MenuShellRenderer,
            pointer:PointerInteraction, sliderWrites:SliderWriteCoordinator,
            model:Object, selection:MenuSelectionState, syncFocus:Function,
            selectPage:Function, redraw:Function):Boolean
        {
            if (kind == "rangeGuidance") {
                shell.showThrottleGuidance(); redraw(); return true;
            }
            if (kind == "guidanceDismiss") {
                shell.hideGuidance(); redraw(); return true;
            }
            if (kind == "guidanceOpenThrottle") {
                shell.hideGuidance();
                openThrottle(model, selection, syncFocus, selectPage, redraw);
                return true;
            }
            if (kind == "liveDisclosure") {
                shell.toggleLiveDisclosure(target.payload); redraw(); return true;
            }
            if (kind == "slider") {
                var current:Object = selection.page();
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                selection.selectedRow = int(target.index);
                syncFocus();
                pointer.beginSlider(current, target.payload);
                sliderWrites.prepare(model, current);
                ControlWidgets.writeSliderFromPointer(target.view as Sprite,
                    target.payload, stageX, stageY, sliderWrites.queue);
                return true;
            }
            if (kind == "rangeMeter") {
                sliderWrites.prepare(model, selection.page());
                if (pointer.beginRange(selection.page(), target, stageX, stageY,
                    sliderWrites.queue)) {
                    selection.selectControlId(pointer.activeRangeControlId);
                    syncFocus();
                }
                return true;
            }
            return false;
        }

        public function handleKey(keyCode:uint, shell:MenuShellRenderer,
            model:Object, selection:MenuSelectionState, syncFocus:Function,
            selectPage:Function, redraw:Function):Boolean
        {
            if (!shell.guidanceIsOpen) return false;
            if (keyCode == Keyboard.ENTER) {
                shell.hideGuidance();
                openThrottle(model, selection, syncFocus, selectPage, redraw);
            } else if (keyCode == Keyboard.ESCAPE) {
                shell.hideGuidance(); redraw();
            }
            return true;
        }

        private function openThrottle(model:Object,
            selection:MenuSelectionState, syncFocus:Function,
            selectPage:Function, redraw:Function):void
        {
            if (model == null || model.pages == null) return;
            for each (var target:Object in model.pages) {
                if (String(target.moduleId) != "absolute.hotas" ||
                    String(target.pageId) != "hotas-throttle") continue;
                selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                selection.focusedAction = 0;
                syncFocus(); selectPage(target); return;
            }
            redraw();
        }
    }
}
