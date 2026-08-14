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
            var enabled:Boolean = Boolean(control.available);
            var widget:Sprite = new Sprite();
            widget.x = 480;
            widget.y = 8;
            widget.buttonMode = enabled;

            if (kind == 0) {
                drawToggle(widget, control, enabled);
                registerHit(widget, "activate", control, 0);
            } else if (kind == 1 || kind == 2) {
                drawSlider(widget, control, enabled);
                registerHit(widget, "slider", control, 0);
            } else if (kind == 3) {
                drawChoice(widget, control, enabled);
                registerHit(widget, "activate", control, 0);
            } else if (kind == 4) {
                drawAction(widget, enabled);
                registerHit(widget, "activate", control, 0);
            } else {
                drawBinding(widget, control, enabled, capturing);
                registerHit(widget, "activate", control, 0);
            }
            row.addChild(widget);
        }

        public static function activate(control:Object, dispatch:Function):void
        {
            if (!Boolean(control.available)) return;
            if (uint(control.kind) == 0) {
                dispatch("write", control, !Boolean(control.booleanValue), 0, 0);
            } else if (uint(control.kind) == 4) {
                dispatch("invoke", control, false, 0, 0);
            } else if (uint(control.kind) == 5) {
                dispatch("beginBindingCapture", control, false, 0, 0);
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
                Math.min(1, local.x / PanelLayout.SLIDER_TRACK_WIDTH));
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
                return String(int(control.integerValue));
            }
            if (uint(control.kind) == 2) return String(Number(control.floatValue));
            if (uint(control.kind) == 4) return "RUN";
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
            PixelTextRenderer.addText(widget, on ? "ON" : "OFF", 470, 10, 11,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
        }

        private static function drawSlider(widget:Sprite, control:Object, enabled:Boolean):void
        {
            var minimum:Number = Number(control.minimum);
            var maximum:Number = Number(control.maximum);
            var value:Number = uint(control.kind) == 1 ?
                Number(control.integerValue) : Number(control.floatValue);
            var fraction:Number = maximum > minimum ?
                Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum))) : 0;
            widget.graphics.beginFill(PanelTheme.ROW_BORDER);
            widget.graphics.drawRect(0, 14, PanelLayout.SLIDER_TRACK_WIDTH, 6);
            widget.graphics.endFill();
            widget.graphics.beginFill(enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.drawRect(0, 14, PanelLayout.SLIDER_TRACK_WIDTH * fraction, 6);
            widget.graphics.drawCircle(PanelLayout.SLIDER_TRACK_WIDTH * fraction, 17, 8);
            widget.graphics.endFill();
            PixelTextRenderer.addText(widget, displayValue(control), 470, 9, 13,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
        }

        private static function drawChoice(widget:Sprite, control:Object, enabled:Boolean):void
        {
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(230, 0, 393, 34);
            widget.graphics.endFill();
            PixelTextRenderer.addText(widget,
                PixelTextRenderer.fit(displayValue(control), 24), 246, 10, 12,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
            PixelTextRenderer.addText(widget, "V", 595, 10, 12, PanelTheme.DIM_TEXT);
        }

        private static function drawAction(widget:Sprite, enabled:Boolean):void
        {
            widget.graphics.lineStyle(1,
                enabled ? PanelTheme.CYAN : PanelTheme.DISABLED_WIDGET);
            widget.graphics.beginFill(PanelTheme.WIDGET_FILL);
            widget.graphics.drawRect(430, 0, 193, 34);
            widget.graphics.endFill();
            PixelTextRenderer.addText(widget, "RUN", 502, 10, 12,
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
            PixelTextRenderer.addText(widget,
                PixelTextRenderer.fit(capturing ? "PRESS KEY OR CHORD" :
                    displayValue(control), 31), 160, 10, 12,
                enabled ? PanelTheme.TEXT : PanelTheme.TOGGLE_KNOB_OFF);
        }
    }
}
