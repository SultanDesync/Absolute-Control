package acp.ui
{
    import flash.ui.Keyboard;

    public final class ModalInputRouter
    {
        public function handleKey(model:Object, keyCode:uint, cursor:int,
            dispatchPage:Function, redraw:Function):Object
        {
            if (model == null) return {"active":false, "handled":false,
                "cursor":cursor};
            if (Boolean(model.actionConfirmationActive)) {
                return handleActionConfirmation(keyCode, cursor,
                    dispatchPage, redraw);
            }
            if (Boolean(model.bindingConflictActive)) {
                return handleBindingConflict(keyCode, cursor,
                    dispatchPage, redraw);
            }
            if (Boolean(model.dirtyDecisionActive)) {
                return handleDirtyDecision(keyCode, cursor,
                    dispatchPage, redraw);
            }
            return {"active":false, "handled":false, "cursor":cursor};
        }

        private function handleActionConfirmation(keyCode:uint, cursor:int,
            dispatchPage:Function, redraw:Function):Object
        {
            var handled:Boolean = true;
            if (keyCode == Keyboard.ESCAPE || keyCode == Keyboard.TAB ||
                keyCode == Keyboard.X) {
                dispatchPage("actionCancel");
            } else if (isDirection(keyCode)) {
                cursor = 1 - Math.min(1, cursor);
                redraw();
            } else if (isAccept(keyCode)) {
                dispatchPage(cursor == 0 ? "actionConfirm" : "actionCancel");
            } else {
                handled = false;
            }
            return {"active":true, "handled":handled, "cursor":cursor};
        }

        private function handleBindingConflict(keyCode:uint, cursor:int,
            dispatchPage:Function, redraw:Function):Object
        {
            var handled:Boolean = true;
            if (keyCode == Keyboard.ESCAPE || keyCode == Keyboard.TAB ||
                keyCode == Keyboard.X) {
                dispatchPage("bindingCancel");
            } else if (isDirection(keyCode)) {
                cursor = 1 - Math.min(1, cursor);
                redraw();
            } else if (isAccept(keyCode)) {
                dispatchPage(cursor == 0 ? "bindingReassign" : "bindingCancel");
            } else {
                handled = false;
            }
            return {"active":true, "handled":handled, "cursor":cursor};
        }

        private function handleDirtyDecision(keyCode:uint, cursor:int,
            dispatchPage:Function, redraw:Function):Object
        {
            var handled:Boolean = true;
            if (keyCode == Keyboard.ESCAPE || keyCode == Keyboard.TAB) {
                dispatchPage("dirtyStay");
            } else if (keyCode == Keyboard.F) {
                dispatchPage("dirtyApply");
            } else if (keyCode == Keyboard.X) {
                dispatchPage("dirtyDiscard");
            } else if (keyCode == Keyboard.LEFT || keyCode == Keyboard.UP ||
                keyCode == Keyboard.A || keyCode == Keyboard.W) {
                cursor = (cursor + 2) % 3;
                redraw();
            } else if (keyCode == Keyboard.RIGHT || keyCode == Keyboard.DOWN ||
                keyCode == Keyboard.D || keyCode == Keyboard.S) {
                cursor = (cursor + 1) % 3;
                redraw();
            } else if (isAccept(keyCode)) {
                dispatchPage(cursor == 0 ? "dirtyApply" :
                    (cursor == 1 ? "dirtyDiscard" : "dirtyStay"));
            } else {
                handled = false;
            }
            return {"active":true, "handled":handled, "cursor":cursor};
        }

        private function isDirection(keyCode:uint):Boolean
        {
            return keyCode == Keyboard.LEFT || keyCode == Keyboard.RIGHT ||
                keyCode == Keyboard.UP || keyCode == Keyboard.DOWN ||
                keyCode == Keyboard.A || keyCode == Keyboard.D ||
                keyCode == Keyboard.W || keyCode == Keyboard.S;
        }

        private function isAccept(keyCode:uint):Boolean
        {
            return keyCode == Keyboard.E || keyCode == Keyboard.ENTER ||
                keyCode == Keyboard.SPACE;
        }
    }
}
