package acp.ui
{
    import flash.ui.Keyboard;

    public final class ChoiceInputRouter
    {
        private var shell:MenuShellRenderer;

        public function ChoiceInputRouter(renderer:MenuShellRenderer)
        {
            shell = renderer;
        }

        public function handlePointer(kind:String, target:Object,
            current:Object, dispatch:Function):Boolean
        {
            if (kind == "choiceDismiss") {
                shell.closeChoice();
                return true;
            }
            if (kind == "recordDismiss") {
                shell.closeRecordCollection();
                return true;
            }
            if (kind == "choice") {
                shell.openChoice(current, target.payload);
                return true;
            }
            if (kind == "choiceOption") {
                var selected:Object = target.payload;
                shell.closeChoice();
                dispatch("write", selected.control, false,
                    Number(selected.option.value), 0);
                return true;
            }
            if (kind == "recordCollection") {
                shell.openRecordCollection(current, target.payload);
                return true;
            }
            if (kind == "recordItem") {
                var record:Object = target.payload;
                if ((uint(record.item.flags) & 1) != 0) return true;
                shell.closeRecordCollection();
                dispatch("write", record.control, false, 0, 0, true,
                    String(record.item.recordId));
                return true;
            }
            return false;
        }

        public function activate(control:Object, current:Object,
            dispatch:Function):Boolean
        {
            if (control == null ||
                (uint(control.kind) != 3 && uint(control.kind) != 8)) return false;
            if (uint(control.kind) == 8) {
                shell.openRecordCollection(current, control);
            } else {
                shell.openChoice(current, control);
            }
            return true;
        }

        // -1 means no selector is open, 0 means open but unhandled, and 1
        // means the key was consumed.
        public function handleKey(keyCode:uint, dispatch:Function):int
        {
            if (shell.recordCollectionIsOpen) {
                if (keyCode == Keyboard.ESCAPE || keyCode == Keyboard.TAB) {
                    shell.closeRecordCollection();
                } else if (keyCode == Keyboard.W || keyCode == Keyboard.UP) {
                    shell.moveRecordCollection(-1);
                } else if (keyCode == Keyboard.S || keyCode == Keyboard.DOWN) {
                    shell.moveRecordCollection(1);
                } else if (keyCode == Keyboard.PAGE_UP) {
                    shell.moveRecordCollection(-8);
                } else if (keyCode == Keyboard.PAGE_DOWN) {
                    shell.moveRecordCollection(8);
                } else if (keyCode == Keyboard.E || keyCode == Keyboard.ENTER ||
                    keyCode == Keyboard.SPACE) {
                    var record:Object = shell.selectedRecordItem();
                    if (record == null || (uint(record.item.flags) & 1) != 0) return 0;
                    shell.closeRecordCollection();
                    dispatch("write", record.control, false, 0, 0, true,
                        String(record.item.recordId));
                } else {
                    return 0;
                }
                return 1;
            }
            if (!shell.choiceIsOpen) return -1;
            if (keyCode == Keyboard.ESCAPE || keyCode == Keyboard.TAB) {
                shell.closeChoice();
            } else if (keyCode == Keyboard.W || keyCode == Keyboard.UP) {
                shell.moveChoice(-1);
            } else if (keyCode == Keyboard.S || keyCode == Keyboard.DOWN) {
                shell.moveChoice(1);
            } else if (keyCode == Keyboard.PAGE_UP) {
                shell.moveChoice(-PanelLayout.VISIBLE_CHOICE_OPTIONS);
            } else if (keyCode == Keyboard.PAGE_DOWN) {
                shell.moveChoice(PanelLayout.VISIBLE_CHOICE_OPTIONS);
            } else if (keyCode == Keyboard.E || keyCode == Keyboard.ENTER ||
                keyCode == Keyboard.SPACE) {
                var selected:Object = shell.selectedChoice();
                if (selected == null) return 0;
                shell.closeChoice();
                dispatch("write", selected.control, false,
                    Number(selected.option.value), 0);
            } else {
                return 0;
            }
            return 1;
        }

        public function handleWheel(direction:int):Boolean
        {
            if (shell.recordCollectionIsOpen) {
                shell.moveRecordCollection(direction);
                return true;
            }
            if (!shell.choiceIsOpen) return false;
            shell.moveChoice(direction);
            return true;
        }
    }
}
