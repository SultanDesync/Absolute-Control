package acp.ui
{
    import flash.display.DisplayObjectContainer;
    import flash.display.Sprite;

    public final class PixelTextRenderer
    {
        private static const GLYPHS:Object = {
            "A":[14,17,17,31,17,17,17], "B":[30,17,17,30,17,17,30], "C":[14,17,16,16,16,17,14],
            "D":[30,17,17,17,17,17,30], "E":[31,16,16,30,16,16,31], "F":[31,16,16,30,16,16,16],
            "G":[14,17,16,23,17,17,14], "H":[17,17,17,31,17,17,17], "I":[31,4,4,4,4,4,31],
            "J":[7,2,2,2,18,18,12], "K":[17,18,20,24,20,18,17], "L":[16,16,16,16,16,16,31],
            "M":[17,27,21,21,17,17,17], "N":[17,25,21,19,17,17], "O":[14,17,17,17,17,17,14],
            "P":[30,17,17,30,16,16,16], "Q":[14,17,17,17,21,18,13], "R":[30,17,17,30,20,18,17],
            "S":[15,16,16,14,1,1,30], "T":[31,4,4,4,4,4,4], "U":[17,17,17,17,17,17,14],
            "V":[17,17,17,17,17,10,4], "W":[17,17,17,17,21,27,17], "X":[17,17,10,4,10,17,17],
            "Y":[17,17,10,4,4,4,4], "Z":[31,1,2,4,8,16,31], "0":[14,17,19,21,25,17,14],
            "1":[4,12,4,4,4,4,14], "2":[14,17,1,2,4,8,31], "3":[30,1,1,14,1,1,30],
            "4":[2,6,10,18,31,2,2], "5":[31,16,16,30,1,1,30], "6":[14,16,16,30,17,17,14],
            "7":[31,1,2,4,8,8,8], "8":[14,17,17,14,17,17,14], "9":[14,17,17,15,1,1,14],
            "-":[0,0,0,31,0,0,0], "+":[0,4,4,31,4,4,0], ".":[0,0,0,0,0,12,12],
            ":":[0,12,12,0,12,12,0], " ":[0,0,0,0,0,0,0], "?":[14,17,1,2,4,0,4]
        };

        public static function addText(parent:DisplayObjectContainer, value:String,
            xPosition:Number, yPosition:Number, size:Number, color:uint):Sprite
        {
            var field:Sprite = createText(value, size, color);
            field.x = xPosition;
            field.y = yPosition;
            parent.addChild(field);
            return field;
        }

        public static function createText(value:String, size:Number, color:uint):Sprite
        {
            var field:Sprite = new Sprite();
            drawText(field, value, size, color);
            field.mouseEnabled = false;
            field.mouseChildren = false;
            return field;
        }

        public static function drawText(target:Sprite, value:String, size:Number, color:uint):void
        {
            var pixel:Number = Math.max(1, Math.floor(size / 7));
            var cursorX:Number = 0;
            var cursorY:Number = 0;
            var normalized:String = value.toUpperCase();
            target.graphics.beginFill(color, 1.0);
            for (var index:int = 0; index < normalized.length; ++index) {
                var character:String = normalized.charAt(index);
                if (character == "\n") {
                    cursorX = 0;
                    cursorY += pixel * 9;
                    continue;
                }
                var rows:Array = GLYPHS[character] as Array;
                if (rows == null) rows = GLYPHS["?"] as Array;
                for (var row:int = 0; row < 7; ++row) {
                    for (var column:int = 0; column < 5; ++column) {
                        if ((int(rows[row]) & (1 << (4 - column))) != 0) {
                            target.graphics.drawRect(cursorX + column * pixel,
                                cursorY + row * pixel, pixel, pixel);
                        }
                    }
                }
                cursorX += pixel * 6;
            }
            target.graphics.endFill();
        }

        public static function fit(value:String, maximum:int):String
        {
            return value.length <= maximum ? value :
                value.substr(0, Math.max(0, maximum - 3)) + "...";
        }
    }
}
