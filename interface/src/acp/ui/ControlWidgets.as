package acp.ui
{
    import flash.display.Sprite;
    import flash.geom.Point;
    import flash.utils.Dictionary;

    public final class ControlWidgets
    {
        private static var compactSliderMetrics:Dictionary =
            new Dictionary(true);
        public static function draw(row:Sprite, control:Object, capturing:Boolean,
            registerHit:Function):void
        {
            var kind:uint = uint(control.kind);
            var enabled:Boolean = Boolean(control.available) &&
                (uint(control.flags) & 1) == 0;
            var widget:Sprite = new Sprite();
            widget.x = 480;
            widget.y = 8;
            widget.buttonMode = enabled;

            if (kind == 0) {
                drawToggle(widget, control, enabled);
                registerHit(widget, "activate", control, 0);
            } else if (kind == 1 || kind == 2) {
                var sliderHit:Sprite = drawSlider(widget, control, enabled);
                registerHit(sliderHit, "slider", control, 0);
            } else if (kind == 3) {
                drawChoice(widget, control, enabled);
                registerHit(widget, "choice", control, 0);
            } else if (kind == 4) {
                drawAction(widget, enabled);
            } else if (kind == 5) {
                var bindingHits:Object = drawBinding(widget, control, enabled,
                    capturing);
                registerHit(bindingHits.record, "activate", control, 0);
                if (bindingHits.clear != null)
                    registerHit(bindingHits.clear, "clearBinding", control, 0);
            } else if (kind == 8) {
                drawRecordCollection(widget, control, enabled);
                registerHit(widget, "recordCollection", control, 0);
            } else {
                drawTextInput(widget, control, enabled, capturing);
                registerHit(widget, "activate", control, 0);
            }
            row.addChild(widget);
        }

        public static function activate(control:Object, dispatch:Function):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            if (uint(control.kind) == 0) {
                dispatch("write", control, !Boolean(control.booleanValue), 0, 0);
            } else if (uint(control.kind) == 4) {
                dispatch("invoke", control, false, 0, 0);
            } else if (uint(control.kind) == 5) {
                dispatch("beginBindingCapture", control, false, 0, 0);
            } else if (uint(control.kind) == 6) {
                dispatch("beginTextCapture", control, false, 0, 0);
            } else if (uint(control.kind) == 1 || uint(control.kind) == 2 ||
                uint(control.kind) == 3) {
                adjust(control, 1, dispatch);
            }
        }

        public static function adjust(control:Object, direction:int, dispatch:Function):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            if (uint(control.kind) == 1 || uint(control.kind) == 3) {
                dispatch("write", control, false,
                    int(Math.max(Number(control.minimum),
                        Math.min(Number(control.maximum), Number(control.integerValue) +
                            direction * Math.max(1, Number(control.step))))), 0);
            } else if (uint(control.kind) == 2) {
                dispatch("write", control, false, 0,
                    Math.max(Number(control.minimum),
                        Math.min(Number(control.maximum), Number(control.floatValue) +
                            direction * Number(control.step))));
            }
        }

        public static function canClearBinding(control:Object):Boolean
        {
            if (control == null || uint(control.kind) != 5 ||
                !Boolean(control.available) || (uint(control.flags) & 1) != 0 ||
                (uint(control.flags) & 4096) == 0) return false;
            var value:String = String(control.stringValue).toLowerCase();
            return value.length > 0 && value != "unbound" &&
                value != "(unbound)" && value != "none";
        }

        public static function clearBinding(control:Object,
            dispatch:Function):Boolean
        {
            if (!canClearBinding(control)) return false;
            dispatch("write", control, false, 0, 0, true, "");
            return true;
        }

        public static function writeSliderFromPointer(view:Sprite, control:Object,
            stageX:Number, stageY:Number, dispatch:Function):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            var local:Point = view.globalToLocal(new Point(stageX, stageY));
            var metrics:Object = compactSliderMetrics[view];
            var trackX:Number = metrics == null ?
                PanelLayout.SLIDER_TRACK_X : Number(metrics.trackX);
            var trackWidth:Number = metrics == null ?
                PanelLayout.SLIDER_TRACK_WIDTH : Number(metrics.trackWidth);
            var fraction:Number = Math.max(0,
                Math.min(1, (local.x - trackX) /
                    Math.max(1, trackWidth)));
            var minimum:Number = Number(control.minimum);
            var maximum:Number = Number(control.maximum);
            var raw:Number = minimum + fraction * (maximum - minimum);
            var step:Number = Math.max(0.000001, Number(control.step));
            var snapped:Number = minimum + Math.round((raw - minimum) / step) * step;
            if (uint(control.kind) == 1) {
                var integerValue:int = Math.round(Math.max(minimum, Math.min(maximum, snapped)));
                if (integerValue != int(control.integerValue)) {
                    dispatch("write", control, false, integerValue, 0);
                }
            } else {
                var floatValue:Number = Math.max(minimum, Math.min(maximum, snapped));
                if (Math.abs(floatValue - Number(control.floatValue)) > step * 0.0001) {
                    dispatch("write", control, false, 0, floatValue);
                }
            }
        }

        public static function displayValue(control:Object):String
        {
            if (uint(control.kind) == 0) return Boolean(control.booleanValue) ? "ON" : "OFF";
            if (uint(control.kind) == 1 || uint(control.kind) == 3) {
                if (uint(control.kind) == 3 && control.choiceOptions != null) {
                    for (var index:int = 0;
                         index < control.choiceOptions.length; ++index) {
                        if (Number(control.choiceOptions[index].value) ==
                            Number(control.integerValue)) {
                            return String(control.choiceOptions[index].label);
                        }
                    }
                }
                return String(int(control.integerValue));
            }
            if (uint(control.kind) == 2) return String(Number(control.floatValue));
            if (uint(control.kind) == 4) return "";
            if (uint(control.kind) == 8 && control.recordItems != null) {
                for (var recordIndex:int = 0;
                     recordIndex < control.recordItems.length; ++recordIndex) {
                    if (String(control.recordItems[recordIndex].recordId) ==
                        String(control.stringValue)) {
                        return String(control.recordItems[recordIndex].label);
                    }
                }
                return control.recordItems.length == 0 ? "NO RECORDS" :
                    "SELECT RECORD";
            }
            return String(control.stringValue);
        }

        public static function drawSliderOnly(widget:Sprite,
            control:Object):Sprite
        {
            var enabled:Boolean = Boolean(control.available) &&
                (uint(control.flags) & 1) == 0;
            return drawSlider(widget, control, enabled);
        }

        public static function drawCompactSliderOnly(widget:Sprite,
            control:Object, width:Number):Sprite
        {
            var enabled:Boolean = Boolean(control.available) &&
                (uint(control.flags) & 1) == 0;
            var minimum:Number = Number(control.minimum);
            var maximum:Number = Number(control.maximum);
            var value:Number = uint(control.kind) == 1 ?
                Number(control.integerValue) : Number(control.floatValue);
            var fraction:Number = maximum > minimum ? Math.max(0,
                Math.min(1, (value - minimum) / (maximum - minimum))) : 0;
            var valueWidth:Number = 62;
            var railWidth:Number = Math.max(78, width - valueWidth - 14);
            var valueX:Number = railWidth + 14;
            var thumbX:Number = fraction * railWidth;
            var railColor:uint = enabled ? PanelTheme.BORDER :
                PanelTheme.DISABLED_WIDGET;
            var controlColor:uint = enabled ? PanelTheme.CYAN :
                PanelTheme.DISABLED_WIDGET;

            widget.graphics.beginFill(railColor);
            widget.graphics.drawRoundRect(0, 14, railWidth, 5, 5, 5);
            widget.graphics.endFill();
            if (fraction > 0) {
                widget.graphics.beginFill(controlColor);
                widget.graphics.drawRoundRect(0, 14,
                    railWidth * fraction, 5, 5, 5);
                widget.graphics.endFill();
            }
            widget.graphics.lineStyle(2, controlColor);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL);
            widget.graphics.drawCircle(thumbX, 16.5, 8);
            widget.graphics.endFill();
            widget.graphics.lineStyle(1, enabled ? PanelTheme.BORDER :
                PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL);
            widget.graphics.drawRoundRect(valueX, 0, valueWidth, 33, 5, 5);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget,
                compactDisplayValue(control), valueX + 6, 7, 14,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF,
                true, valueWidth - 12, 18);

            var hit:Sprite = new Sprite();
            hit.graphics.beginFill(0xFFFFFF, 0.0);
            hit.graphics.drawRect(-10, 0, railWidth + 20, 34);
            hit.graphics.endFill();
            hit.buttonMode = enabled;
            widget.addChild(hit);
            compactSliderMetrics[hit] = {
                "trackX":0, "trackWidth":railWidth
            };
            return hit;
        }

        public static function drawCompactToggleOnly(widget:Sprite,
            control:Object):Sprite
        {
            var enabled:Boolean = Boolean(control.available) &&
                (uint(control.flags) & 1) == 0;
            var on:Boolean = Boolean(control.booleanValue);
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(on ? PanelTheme.TOGGLE_ON :
                PanelTheme.TOGGLE_OFF);
            widget.graphics.drawRoundRect(0, 0, 58, 26, 13, 13);
            widget.graphics.endFill();
            widget.graphics.beginFill(on ? PanelTheme.TEXT :
                PanelTheme.TOGGLE_KNOB_OFF);
            widget.graphics.drawCircle(on ? 44 : 14, 13, 9);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget, on ? "ON" : "OFF", 68, 4,
                13, enabled ? PanelTheme.TEXT :
                    PanelTheme.TOGGLE_KNOB_OFF, true, 34, 18);
            var hit:Sprite = new Sprite();
            hit.graphics.beginFill(0xFFFFFF, 0.0);
            hit.graphics.drawRect(-4, -4, 108, 34);
            hit.graphics.endFill();
            hit.buttonMode = enabled;
            widget.addChild(hit);
            return hit;
        }

        private static function compactDisplayValue(control:Object):String
        {
            if (uint(control.kind) == 1)
                return String(int(control.integerValue));
            var value:Number = Number(control.floatValue);
            var step:Number = Math.abs(Number(control.step));
            if (step >= 1) return String(int(Math.round(value)));
            if (step >= 0.1) return value.toFixed(1);
            return value.toFixed(2);
        }

        private static function drawToggle(widget:Sprite, control:Object, enabled:Boolean):void
        {
            var on:Boolean = Boolean(control.booleanValue);
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(on ? PanelTheme.TOGGLE_ON : PanelTheme.TOGGLE_OFF);
            widget.graphics.drawRoundRect(535, 2, 88, 30, 15, 15);
            widget.graphics.endFill();
            widget.graphics.beginFill(on ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
            widget.graphics.drawCircle(on ? 607 : 551, 17, 11);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget, on ? "ON" : "OFF", 470, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
        }

        private static function drawSlider(widget:Sprite, control:Object,
            enabled:Boolean):Sprite
        {
            var minimum:Number = Number(control.minimum);
            var maximum:Number = Number(control.maximum);
            var value:Number = uint(control.kind) == 1 ?
                Number(control.integerValue) : Number(control.floatValue);
            var fraction:Number = maximum > minimum ?
                Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum))) : 0;
            var railColor:uint = enabled ? PanelTheme.BORDER :
                PanelTheme.DISABLED_WIDGET;
            var controlColor:uint = enabled ? PanelTheme.CYAN :
                PanelTheme.DISABLED_WIDGET;
            var thumbX:Number = PanelLayout.SLIDER_TRACK_X +
                PanelLayout.SLIDER_TRACK_WIDTH * fraction;

            // Keep the rail inset by more than the thumb radius. The previous
            // zero-inset circle was clipped at the minimum and exposed the
            // rail/fill join through the thumb.
            widget.graphics.beginFill(railColor);
            widget.graphics.drawRoundRect(PanelLayout.SLIDER_TRACK_X, 14,
                PanelLayout.SLIDER_TRACK_WIDTH, 6, 6, 6);
            widget.graphics.endFill();
            if (fraction > 0) {
                widget.graphics.beginFill(controlColor);
                widget.graphics.drawRoundRect(PanelLayout.SLIDER_TRACK_X, 14,
                    PanelLayout.SLIDER_TRACK_WIDTH * fraction, 6, 6, 6);
                widget.graphics.endFill();
            }

            // An opaque thumb body covers every rail edge before the cyan
            // outline and center marker are drawn, avoiding Scaleform seams.
            widget.graphics.lineStyle(2, controlColor, 1.0);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL, 1.0);
            widget.graphics.drawCircle(thumbX, 17,
                PanelLayout.SLIDER_THUMB_RADIUS);
            widget.graphics.endFill();
            widget.graphics.lineStyle();
            widget.graphics.beginFill(controlColor, 1.0);
            widget.graphics.drawCircle(thumbX, 17, 3);
            widget.graphics.endFill();

            widget.graphics.lineStyle(1, enabled ? PanelTheme.BORDER :
                PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL, 1.0);
            widget.graphics.drawRoundRect(PanelLayout.SLIDER_VALUE_X, 1,
                PanelLayout.SLIDER_VALUE_WIDTH, 32, 6, 6);
            widget.graphics.endFill();
            widget.graphics.lineStyle();
            VectorTextRenderer.addText(widget,
                VectorTextRenderer.fit(displayValue(control), 8),
                PanelLayout.SLIDER_VALUE_X + 16, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF, true);

            var hit:Sprite = new Sprite();
            hit.graphics.beginFill(0xFFFFFF, 0.0);
            hit.graphics.drawRect(
                PanelLayout.SLIDER_TRACK_X - PanelLayout.SLIDER_THUMB_RADIUS - 4,
                0,
                PanelLayout.SLIDER_TRACK_WIDTH +
                    (PanelLayout.SLIDER_THUMB_RADIUS + 4) * 2,
                34);
            hit.graphics.endFill();
            hit.buttonMode = enabled;
            widget.addChild(hit);
            return hit;
        }

        private static function drawChoice(widget:Sprite, control:Object, enabled:Boolean):void
        {
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(145, 0, PanelLayout.CHOICE_POPUP_WIDTH, 34);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget,
                VectorTextRenderer.fit(displayValue(control), 38), 160, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
            VectorTextRenderer.addText(widget, "V", 595, 6, 16, PanelTheme.DIM_TEXT);
        }

        private static function drawAction(widget:Sprite, enabled:Boolean):void
        {
            VectorTextRenderer.addText(widget, ">",
                PanelLayout.CONTROL_ROW_WIDTH - widget.x - 34, 4, 22,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
        }

        private static function drawRecordCollection(widget:Sprite,
            control:Object, enabled:Boolean):void
        {
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(145, 0, 478, 34);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget,
                VectorTextRenderer.fit(displayValue(control), 34), 160, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
            VectorTextRenderer.addText(widget, "DETAILS >", 525, 7, 13,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET, true);
        }

        private static function drawBinding(widget:Sprite, control:Object,
            enabled:Boolean, capturing:Boolean):Object
        {
            var fieldX:Number = 145;
            var fieldWidth:Number = 478;
            var clearWidth:Number = 92;
            var gap:Number = 6;
            var recordWidth:Number = fieldWidth - clearWidth - gap;
            var clearX:Number = fieldX + recordWidth + gap;
            var clearable:Boolean = (uint(control.flags) & 4096) != 0;
            var clearEnabled:Boolean = !capturing && canClearBinding(control);

            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(capturing ? PanelTheme.CAPTURE_FILL : PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(fieldX, 0,
                clearable ? recordWidth : fieldWidth, 34);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget,
                VectorTextRenderer.fit(capturing ? "RECORDING INPUT..." :
                    displayValue(control), clearable ? 29 : 38), 160, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);

            if (clearable) {
                widget.graphics.lineStyle(1, clearEnabled ? PanelTheme.GOLD :
                    PanelTheme.DISABLED_WIDGET);
                widget.graphics.beginFill(PanelTheme.BUTTON_FILL);
                widget.graphics.drawRect(clearX, 0, clearWidth, 34);
                widget.graphics.endFill();
                VectorTextRenderer.addText(widget, "CLEAR", clearX + 17, 7,
                    14, clearEnabled ? PanelTheme.GOLD :
                        PanelTheme.DISABLED_TEXT, true, clearWidth - 24, 18);
            }

            var recordHit:Sprite = new Sprite();
            recordHit.graphics.beginFill(0xFFFFFF, 0.0);
            recordHit.graphics.drawRect(fieldX, 0,
                clearable ? recordWidth : fieldWidth, 34);
            recordHit.graphics.endFill();
            recordHit.buttonMode = enabled;
            widget.addChild(recordHit);

            var clearHit:Sprite = null;
            if (clearable) {
                clearHit = new Sprite();
                clearHit.graphics.beginFill(0xFFFFFF, 0.0);
                clearHit.graphics.drawRect(clearX, 0, clearWidth, 34);
                clearHit.graphics.endFill();
                clearHit.buttonMode = clearEnabled;
                widget.addChild(clearHit);
            }
            return { "record":recordHit,
                "clear":clearEnabled ? clearHit : null };
        }

        private static function drawTextInput(widget:Sprite, control:Object,
            enabled:Boolean, capturing:Boolean):void
        {
            widget.graphics.lineStyle(1,
                capturing ? PanelTheme.GOLD :
                    (enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET));
            widget.graphics.beginFill(capturing ? PanelTheme.CAPTURE_FILL :
                PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(145, 0, 478, 34);
            widget.graphics.endFill();
            var text:String = String(control.stringValue);
            VectorTextRenderer.addText(widget,
                VectorTextRenderer.fit((capturing ? "EDIT: " : "") + text, 38),
                160, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
        }
    }
}
