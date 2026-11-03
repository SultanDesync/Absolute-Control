package acp.ui
{
    import flash.display.Sprite;

    public final class ActionConfirmationDialog
    {
        public static function draw(layer:Sprite, hits:PointerInteraction,
            model:Object, cursor:int, inputMode:String):void
        {
            var blocker:Sprite = new Sprite();
            blocker.graphics.beginFill(PanelTheme.BACKGROUND, 0.78);
            blocker.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH,
                PanelLayout.STAGE_HEIGHT);
            blocker.graphics.endFill();
            layer.addChild(blocker);
            hits.register(blocker, "disabled", null, 0);

            var width:Number = 820;
            var height:Number = 310;
            var dialog:Sprite = new Sprite();
            dialog.x = (PanelLayout.STAGE_WIDTH - width) / 2;
            dialog.y = (PanelLayout.STAGE_HEIGHT - height) / 2;
            dialog.graphics.lineStyle(2, PanelTheme.WARNING);
            dialog.graphics.beginFill(PanelTheme.PANEL, 1.0);
            dialog.graphics.drawRect(0, 0, width, height);
            dialog.graphics.endFill();
            layer.addChild(dialog);

            VectorTextRenderer.addText(dialog, "CONFIRM ACTION", 36, 28, 24,
                PanelTheme.WARNING, true, width - 72, 34);
            VectorTextRenderer.addText(dialog,
                String(model.actionConfirmationLabel), 36, 72, 21,
                PanelTheme.TEXT, true, width - 72, 32);
            VectorTextRenderer.addText(dialog,
                String(model.actionConfirmationDetail), 36, 116, 17,
                PanelTheme.MUTED_TEXT, false, width - 72, 62, true);

            var labels:Array = inputMode == "controller" ?
                ["A CONFIRM", "B CANCEL"] :
                (inputMode == "keyboard" ?
                    ["ENTER CONFIRM", "ESC CANCEL"] : ["CONFIRM", "CANCEL"]);
            var commands:Array = ["actionConfirm", "actionCancel"];
            cursor = Math.max(0, Math.min(1, cursor));
            for (var index:int = 0; index < 2; ++index) {
                var button:Sprite = new Sprite();
                var focused:Boolean = index == cursor;
                button.x = 36 + index * 380;
                button.y = 226;
                button.graphics.lineStyle(focused ? 2 : 1,
                    focused ? PanelTheme.CYAN :
                        (index == 0 ? PanelTheme.WARNING : PanelTheme.BORDER));
                button.graphics.beginFill(focused ? PanelTheme.SELECTED_FILL :
                    PanelTheme.BUTTON_FILL);
                button.graphics.drawRect(0, 0, 350, 50);
                button.graphics.endFill();
                button.buttonMode = true;
                VectorTextRenderer.addText(button, String(labels[index]),
                    18, 14, 17,
                    index == 0 ? PanelTheme.WARNING : PanelTheme.TEXT, true);
                dialog.addChild(button);
                hits.register(button, "actionConfirmation", commands[index], index);
            }
        }
    }
}
