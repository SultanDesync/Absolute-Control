package
{
    import acp.ui.VectorTextRenderer;
    import flash.display.MovieClip;
    import flash.display.Shape;
    import flash.events.Event;

    // The walk speed gauge: a slim vertical bar filled up to the current speed, styled after
    // the HUD's health bar and oxygen dial. The plugin owns the fade. This reads the speed and
    // opacity each frame through BGSCodeObj, so all of its work happens on the movie's own thread.
    [SWF(width="1920", height="1080", frameRate="60", backgroundColor="#000000")]
    public class AbsoluteWalkGauge extends MovieClip
    {
        private static const BAR_LEFT:Number = 595;
        private static const BAR_WIDTH:Number = 10;
        private static const TOP:Number = 320;
        private static const HEIGHT:Number = 180;
        private static const LABEL_X:Number = BAR_LEFT + BAR_WIDTH + 10;
        private static const LABEL_SIZE:Number = 16;

        // Sampled from the vanilla HUD: gauge fill, small label text, dial ticks, plate.
        private static const FILL_COLOR:uint = 0xF8F8F8;
        private static const LABEL_COLOR:uint = 0xC9C9C9;
        private static const TRACK_LINE_COLOR:uint = 0xA0A0A8;
        private static const TRACK_COLOR:uint = 0x182028;
        private static const TRACK_ALPHA:Number = 0.6;
        private static const SHADOW_COLOR:uint = 0x000000;

        // GameMenuBase needs this object to exist. The plugin adds speed() and alpha() to it.
        public var BGSCodeObj:Object = {};

        private var fill:Shape = new Shape();

        public function AbsoluteWalkGauge()
        {
            super();
            alpha = 0;
            mouseEnabled = false;
            mouseChildren = false;

            var track:Shape = new Shape();
            track.graphics.lineStyle(1, TRACK_LINE_COLOR, 1);
            track.graphics.beginFill(TRACK_COLOR, TRACK_ALPHA);
            track.graphics.drawRect(BAR_LEFT, TOP, BAR_WIDTH, HEIGHT);
            track.graphics.endFill();
            addChild(track);

            fill.graphics.beginFill(FILL_COLOR, 1);
            fill.graphics.drawRect(0, -HEIGHT, BAR_WIDTH, HEIGHT);
            fill.graphics.endFill();
            fill.x = BAR_LEFT;
            fill.y = TOP + HEIGHT;
            fill.scaleY = 0;
            addChild(fill);

            addLabel("100 %", TOP);
            addLabel("0 %", TOP + HEIGHT);
            addEventListener(Event.ENTER_FRAME, onEnterFrame);
        }

        public function onCodeObjCreate():void
        {
        }

        private function addLabel(text:String, centerY:Number):void
        {
            var y:Number = centerY - LABEL_SIZE * 0.65;
            VectorTextRenderer.addText(this, text, LABEL_X + 1, y + 1, LABEL_SIZE, SHADOW_COLOR);
            VectorTextRenderer.addText(this, text, LABEL_X, y, LABEL_SIZE, LABEL_COLOR);
        }

        private function onEnterFrame(event:Event):void
        {
            if (BGSCodeObj == null || BGSCodeObj.speed == null || BGSCodeObj.alpha == null) return;
            fill.scaleY = Math.max(0, Math.min(1, Number(BGSCodeObj.speed())));
            alpha = Math.max(0, Math.min(1, Number(BGSCodeObj.alpha())));
            visible = alpha > 0;
        }
    }
}
