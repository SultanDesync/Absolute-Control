package acp.ui
{
    import flash.display.Sprite;
    import flash.geom.Point;

    public final class ControlWidgets
    {
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
                drawBinding(widget, control, enabled, capturing);
                registerHit(widget, "activate", control, 0);
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

        public static function writeSliderFromPointer(view:Sprite, control:Object,
            stageX:Number, stageY:Number, dispatch:Function):void
        {
            if (!Boolean(control.available) || (uint(control.flags) & 1) != 0) return;
            var local:Point = view.globalToLocal(new Point(stageX, stageY));
            var fraction:Number = Math.max(0,
                Math.min(1, (local.x - PanelLayout.SLIDER_TRACK_X) /
                    PanelLayout.SLIDER_TRACK_WIDTH));
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
            return String(control.stringValue);
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

        private static function drawBinding(widget:Sprite, control:Object,
            enabled:Boolean, capturing:Boolean):void
        {
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(capturing ? PanelTheme.CAPTURE_FILL : PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(145, 0, 478, 34);
            widget.graphics.endFill();
            VectorTextRenderer.addText(widget,
                VectorTextRenderer.fit(capturing ? "PRESS KEY OR CHORD" :
                    displayValue(control), 38), 160, 6, 16,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
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
