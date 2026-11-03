package acp.ui
{
    import flash.display.DisplayObjectContainer;
    import flash.display.Sprite;
    import flash.text.AntiAliasType;
    import flash.text.GridFitType;
    import flash.text.TextField;
    import flash.text.TextFieldAutoSize;
    import flash.text.TextFormat;

    public final class VectorTextRenderer
    {
        [Embed(source="FontAssets/Roboto-Regular.ttf", fontName="AbsoluteControlBody",
            mimeType="application/x-font", embedAsCFF="false", advancedAntiAliasing="true")]
        private static const BodyFont:Class;

        [Embed(source="FontAssets/Roboto-Bold.ttf", fontName="AbsoluteControlBody",
            fontWeight="bold", mimeType="application/x-font", embedAsCFF="false",
            advancedAntiAliasing="true")]
        private static const BoldFont:Class;

        public static function addText(parent:DisplayObjectContainer, value:String,
            xPosition:Number, yPosition:Number, size:Number, color:uint,
            bold:Boolean = false, maximumWidth:Number = 0,
            maximumHeight:Number = 0, wrap:Boolean = false):Sprite
        {
            var field:Sprite = createText(value, size, color, bold,
                maximumWidth, maximumHeight, wrap);
            field.x = xPosition;
            field.y = yPosition;
            parent.addChild(field);
            return field;
        }

        public static function createText(value:String, size:Number, color:uint,
            bold:Boolean = false, maximumWidth:Number = 0,
            maximumHeight:Number = 0, wrap:Boolean = false):Sprite
        {
            var field:Sprite = new Sprite();
            drawText(field, value, size, color, bold,
                maximumWidth, maximumHeight, wrap);
            field.mouseEnabled = false;
            field.mouseChildren = false;
            return field;
        }

        public static function drawText(target:Sprite, value:String, size:Number,
            color:uint, bold:Boolean = false, maximumWidth:Number = 0,
            maximumHeight:Number = 0, wrap:Boolean = false):void
        {
            while (target.numChildren > 0) target.removeChildAt(0);
            target.graphics.clear();

            var format:TextFormat = new TextFormat();
            format.font = "AbsoluteControlBody";
            format.size = size;
            format.color = color;
            format.bold = bold;
            format.leading = 2;

            var text:TextField = new TextField();
            text.defaultTextFormat = format;
            text.embedFonts = true;
            text.selectable = false;
            text.mouseEnabled = false;
            text.multiline = wrap;
            text.wordWrap = wrap;
            text.antiAliasType = AntiAliasType.ADVANCED;
            text.gridFitType = GridFitType.PIXEL;
            text.sharpness = 50;
            text.thickness = 0;
            text.x = -2;
            text.y = -2;

            if (maximumWidth > 0) {
                text.autoSize = TextFieldAutoSize.NONE;
                text.width = maximumWidth + 4;
                text.height = maximumHeight > 0 ? maximumHeight + 4 : size + 10;
            } else {
                text.autoSize = TextFieldAutoSize.LEFT;
            }

            text.text = value;
            text.setTextFormat(format);
            target.addChild(text);
        }

        public static function fit(value:String, maximum:int):String
        {
            return value.length <= maximum ? value :
                value.substr(0, Math.max(0, maximum - 3)) + "...";
        }
    }
}
