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
            selectPage:Function, redraw:Function, send:Function = null):Boolean
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
            if (kind == "radialDemoToggle") {
                shell.toggleRadialDemo(target.payload, stageX, stageY);
                redraw();
                return true;
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
                var page:Object = selection.page();
                sliderWrites.prepare(model, page);
                if (pointer.beginRange(page, target, stageX, stageY,
                    sliderWrites.queue)) {
                    selection.selectControlId(pointer.activeRangeControlId);
                    syncFocus();
                } else if (send != null) {
                    // A click that isn't on a landmark opens the binding the graph belongs to.
                    var binding:Object = bindingFor(page, target.payload.component);
                    if (binding != null) {
                        selection.focusRegion = PanelLayout.FOCUS_CONTROLS;
                        selection.selectControlId(String(binding.controlId));
                        syncFocus();
                        ControlWidgets.activate(binding, send);
                    }
                }
                return true;
            }
            return false;
        }

        // The binding in the section the graph's landmarks belong to: walking up from a
        // landmark's control, the first binding met before the section's heading. A page
        // with a single binding uses that one.
        private function bindingFor(page:Object, component:Object):Object
        {
            if (page == null || page.controls == null) return null;
            var only:Object = null;
            var count:int = 0;
            for each (var candidate:Object in page.controls) {
                if (bindable(candidate)) { only = candidate; ++count; }
            }
            if (component != null && component.markers != null) {
                for each (var marker:Object in component.markers) {
                    var controlId:String = String(marker.controlId);
                    if (controlId.length == 0) continue;
                    var from:int = -1;
                    for (var index:int = 0; index < page.controls.length; ++index) {
                        if (String(page.controls[index].controlId) == controlId) { from = index; break; }
                    }
                    for (var row:int = from; row >= 0; --row) {
                        if (uint(page.controls[row].kind) == 7) break;
                        if (bindable(page.controls[row])) return page.controls[row];
                    }
                }
            }
            return count == 1 ? only : null;
        }

        private function bindable(control:Object):Boolean
        {
            return control != null && uint(control.kind) == 5 &&
                Boolean(control.available) && (uint(control.flags) & 1) == 0;
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
