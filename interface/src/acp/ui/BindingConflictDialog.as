package acp.ui
{
    import flash.display.Sprite;

    public final class BindingConflictDialog
    {
        public static function draw(layer:Sprite, hits:PointerInteraction,
            model:Object, cursor:int, inputMode:String):void
        {
            var blocker:Sprite = new Sprite();
            blocker.graphics.beginFill(PanelTheme.BACKGROUND, 0.80);
            blocker.graphics.drawRect(0, 0, PanelLayout.STAGE_WIDTH,
                PanelLayout.STAGE_HEIGHT);
            blocker.graphics.endFill();
            layer.addChild(blocker);
            hits.register(blocker, "disabled", null, 0);

            var width:Number = 760;
            var height:Number = 300;
            var dialog:Sprite = new Sprite();
            dialog.x = (PanelLayout.STAGE_WIDTH - width) / 2;
            dialog.y = (PanelLayout.STAGE_HEIGHT - height) / 2;
            dialog.graphics.lineStyle(2, PanelTheme.WARNING);
            dialog.graphics.beginFill(PanelTheme.PANEL, 1.0);
            dialog.graphics.drawRect(0, 0, width, height);
            dialog.graphics.endFill();
            layer.addChild(dialog);

            VectorTextRenderer.addText(dialog, "BINDING ALREADY IN USE",
                36, 30, 25, PanelTheme.TEXT, true, width - 72, 34);
            VectorTextRenderer.addText(dialog,
                String(model.bindingConflictDetail), 36, 82, 18,
                PanelTheme.MUTED_TEXT, false, width - 72, 70, true);
            VectorTextRenderer.addText(dialog,
                "Reassign removes it from the conflicting profile and adds it to this one. Apply saves the change.",
                36, 152, 16, PanelTheme.WARNING, false,
                width - 72, 48, true);
            if (String(model.error).length > 0) {
                VectorTextRenderer.addText(dialog, String(model.error),
                    36, 196, 14, PanelTheme.ERROR, true,
                    width - 72, 24);
            }

            var labels:Array = inputMode == "controller" ?
                ["A REASSIGN", "B CANCEL"] :
                (inputMode == "keyboard" ?
                    ["ENTER REASSIGN", "ESC CANCEL"] :
                    ["REASSIGN", "CANCEL"]);
            var commands:Array = ["bindingReassign", "bindingCancel"];
            cursor = Math.max(0, Math.min(1, cursor));
            for (var index:int = 0; index < 2; ++index) {
                var button:Sprite = new Sprite();
                var focused:Boolean = index == cursor;
                button.x = 110 + index * 280;
                button.y = 224;
                button.graphics.lineStyle(focused ? 2 : 1,
                    focused ? PanelTheme.CYAN : PanelTheme.BORDER);
                button.graphics.beginFill(focused ? PanelTheme.SELECTED_FILL :
                    PanelTheme.BUTTON_FILL);
                button.graphics.drawRect(0, 0, 260, 48);
                button.graphics.endFill();
                button.buttonMode = true;
                VectorTextRenderer.addText(button, String(labels[index]),
                    18, 13, 17, index == 0 ? PanelTheme.WARNING :
                    PanelTheme.TEXT, true);
                dialog.addChild(button);
                hits.register(button, "bindingConflict", commands[index], index);
            }
        }
    }
}
