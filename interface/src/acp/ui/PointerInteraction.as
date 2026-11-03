package acp.ui
{
    import flash.display.Sprite;
    import flash.geom.Point;

    public final class PointerInteraction
    {
        private var hitTargets:Array = [];
        private var draggingSlider:Boolean = false;
        private var draggingModuleId:String = "";
        private var draggingPageId:String = "";
        private var draggingControlId:String = "";
        private var draggingRange:Boolean = false;
        private var draggingRangeChannelId:String = "";
        private var draggingRangeScale:Number = 0;

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
            if (draggingRange) return moveRange(stageX, stageY, current, dispatch);
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
            draggingRange = false;
            draggingModuleId = "";
            draggingPageId = "";
            draggingControlId = "";
            draggingRangeChannelId = "";
            draggingRangeScale = 0;
        }

        public function get isDraggingSlider():Boolean
        {
            return draggingSlider || draggingRange;
        }

        public function get activeRangeControlId():String
        {
            return draggingRange ? draggingControlId : "";
        }

        public function beginRange(current:Object, target:Object,
            stageX:Number, stageY:Number, dispatch:Function):Boolean
        {
            if (current == null || target == null || target.payload == null) return false;
            var view:Sprite = target.view as Sprite;
            var component:Object = target.payload.component;
            if (view == null || component == null || component.markers == null) return false;
            var local:Point = view.globalToLocal(new Point(stageX, stageY));
            var fraction:Number = Math.max(0, Math.min(1,
                local.x / Math.max(1, Number(target.payload.trackWidth))));
            var selection:Object = rangeControlAt(current, component, fraction);
            if (selection == null) return false;
            var control:Object = selection.control;
            var marker:Object = selection.marker;
            draggingSlider = false;
            draggingRange = true;
            draggingModuleId = String(current.moduleId);
            draggingPageId = String(current.pageId);
            draggingRangeChannelId = String(component.channelId);
            draggingControlId = String(control.controlId);
            var controlValue:Number = uint(control.kind) == 1 ?
                Number(control.integerValue) : Number(control.floatValue);
            var componentMinimum:Number = Number(component.minimum);
            var componentMaximum:Number = Number(component.maximum);
            draggingRangeScale = Math.abs(controlValue) > 0.000001 ?
                Number(marker.value) / controlValue :
                ((componentMinimum < 0 && componentMaximum > 0) ?
                    Math.max(Math.abs(componentMinimum),
                        Math.abs(componentMaximum)) : 0);
            writeRangeFromPointer(view, target.payload, control,
                stageX, stageY, dispatch);
            return true;
        }

        private function moveRange(stageX:Number, stageY:Number,
            current:Object, dispatch:Function):Boolean
        {
            if (current == null || String(current.moduleId) != draggingModuleId ||
                String(current.pageId) != draggingPageId) {
                clearSliderDrag();
                return false;
            }
            var control:Object = controlById(current, draggingControlId);
            if (control == null) {
                clearSliderDrag();
                return false;
            }
            for (var i:int = hitTargets.length - 1; i >= 0; --i) {
                var target:Object = hitTargets[i];
                if (String(target.kind) != "rangeMeter" || target.payload == null ||
                    String(target.payload.channelId) != draggingRangeChannelId) continue;
                var view:Sprite = target.view as Sprite;
                if (view == null) {
                    clearSliderDrag();
                    return false;
                }
                writeRangeFromPointer(view, target.payload, control,
                    stageX, stageY, dispatch);
                return true;
            }
            clearSliderDrag();
            return false;
        }

        private function rangeControlAt(current:Object, component:Object,
            fraction:Number):Object
        {
            var minimum:Number = Number(component.minimum);
            var maximum:Number = Number(component.maximum);
            var span:Number = Math.max(0.000001, maximum - minimum);
            var value:Number = minimum + fraction * span;
            var best:Object;
            var bestMarker:Object;
            var bestDistance:Number = Number.MAX_VALUE;
            for each (var marker:Object in component.markers) {
                var control:Object = controlById(current, String(marker.controlId));
                if (!numericControl(control)) continue;
                var distance:Number = Math.abs(Number(marker.value) - value) / span;
                if (distance < bestDistance) {
                    best = control;
                    bestMarker = marker;
                    bestDistance = distance;
                }
            }
            if (best != null && bestDistance <= 0.035) {
                return {"control":best, "marker":bestMarker};
            }
            if (component.bands == null) return null;
            for each (var band:Object in component.bands) {
                if (uint(band.semantic) == 2 || value < Number(band.minimum) ||
                    value > Number(band.maximum)) continue;
                var zoneBest:Object;
                var zoneMarker:Object;
                var zoneDistance:Number = Number.MAX_VALUE;
                for each (marker in component.markers) {
                    control = controlById(current, String(marker.controlId));
                    if (!numericControl(control) ||
                        Number(marker.value) < Number(band.minimum) - span * 0.005 ||
                        Number(marker.value) > Number(band.maximum) + span * 0.005) continue;
                    distance = Math.abs(Number(marker.value) - value);
                    if (distance < zoneDistance) {
                        zoneBest = control;
                        zoneMarker = marker;
                        zoneDistance = distance;
                    }
                }
                if (zoneBest != null) {
                    return {"control":zoneBest, "marker":zoneMarker};
                }
            }
            return null;
        }

        private function controlById(current:Object, controlId:String):Object
        {
            if (current == null || current.controls == null || controlId.length == 0) return null;
            for each (var control:Object in current.controls) {
                if (String(control.controlId) == controlId) return control;
            }
            return null;
        }

        private function numericControl(control:Object):Boolean
        {
            return control != null && Boolean(control.available) &&
                (uint(control.flags) & 1) == 0 &&
                (uint(control.kind) == 1 || uint(control.kind) == 2);
        }

        private function writeRangeFromPointer(view:Sprite, payload:Object,
            control:Object, stageX:Number, stageY:Number,
            dispatch:Function):void
        {
            var local:Point = view.globalToLocal(new Point(stageX, stageY));
            var fraction:Number = Math.max(0, Math.min(1,
                local.x / Math.max(1, Number(payload.trackWidth))));
            var minimum:Number = Number(control.minimum);
            var maximum:Number = Number(control.maximum);
            var step:Number = Math.max(0.000001, Number(control.step));
            var component:Object = payload.component;
            var graphValue:Number = component == null ? 0 :
                Number(component.minimum) + fraction *
                    (Number(component.maximum) - Number(component.minimum));
            var raw:Number = Math.abs(draggingRangeScale) > 0.000001 ?
                graphValue / draggingRangeScale :
                minimum + fraction * (maximum - minimum);
            var snapped:Number = minimum + Math.round((raw - minimum) / step) * step;
            if (uint(control.kind) == 1) {
                var integerValue:int = Math.round(Math.max(minimum,
                    Math.min(maximum, snapped)));
                if (integerValue != int(control.integerValue))
                    dispatch("write", control, false, integerValue, 0);
            } else {
                var floatValue:Number = Math.max(minimum, Math.min(maximum, snapped));
                if (Math.abs(floatValue - Number(control.floatValue)) > step * 0.0001)
                    dispatch("write", control, false, 0, floatValue);
            }
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
