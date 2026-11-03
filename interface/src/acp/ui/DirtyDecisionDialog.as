package acp.ui
{
    import flash.display.Sprite;

    public final class DirtyDecisionDialog
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

            var width:Number = 860;
            var height:Number = 330;
            var dialog:Sprite = new Sprite();
            dialog.x = (PanelLayout.STAGE_WIDTH - width) / 2;
            dialog.y = (PanelLayout.STAGE_HEIGHT - height) / 2;
            dialog.graphics.lineStyle(2, PanelTheme.GOLD);
            dialog.graphics.beginFill(PanelTheme.PANEL, 1.0);
            dialog.graphics.drawRect(0, 0, width, height);
            dialog.graphics.endFill();
            layer.addChild(dialog);

            var closing:Boolean = Boolean(model.dirtyDecisionClosesMenu);
            VectorTextRenderer.addText(dialog,
                closing ? "SAVE CHANGES BEFORE CLOSING?" :
                    "SAVE CHANGES BEFORE LEAVING THIS PAGE?",
                38, 32, 25, PanelTheme.TEXT, true, width - 76, 34);
            VectorTextRenderer.addText(dialog,
                "The active module owns this draft. Apply persists and reloads it; " +
                "Discard restores the last saved values; Stay returns to editing.",
                38, 88, 18, PanelTheme.MUTED_TEXT, false,
                width - 76, 58, true);
            if (String(model.error).length > 0) {
                VectorTextRenderer.addText(dialog, String(model.error), 38, 154,
                    16, PanelTheme.ERROR, true, width - 76, 42, true);
            }

            var labels:Array = ["APPLY", "DISCARD", "STAY"];
            var commands:Array = ["dirtyApply", "dirtyDiscard", "dirtyStay"];
            var prompts:Array = inputMode == "controller" ?
                ["A APPLY", "A DISCARD", "B STAY"] :
                (inputMode == "keyboard" ?
                    ["F " + labels[0], "X " + labels[1],
                        "ESC " + labels[2]] : labels);
            cursor = Math.max(0, Math.min(2, cursor));
            for (var index:int = 0; index < labels.length; ++index) {
                var button:Sprite = new Sprite();
                var focused:Boolean = index == cursor;
                button.x = 38 + index * 264;
                button.y = 242;
                button.graphics.lineStyle(focused ? 2 : 1,
                    focused ? PanelTheme.CYAN :
                        (index == 1 ? PanelTheme.WARNING : PanelTheme.BORDER));
                button.graphics.beginFill(focused ? PanelTheme.SELECTED_FILL :
                    PanelTheme.BUTTON_FILL);
                button.graphics.drawRect(0, 0, 230, 50);
                button.graphics.endFill();
                button.buttonMode = true;
                VectorTextRenderer.addText(button, String(prompts[index]),
                    18, 14, 17,
                    index == 1 ? PanelTheme.WARNING : PanelTheme.TEXT, true);
                dialog.addChild(button);
                hits.register(button, "dirtyDecision", commands[index], index);
            }
        }
    }
}
