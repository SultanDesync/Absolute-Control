package
{
    import flash.display.MovieClip;

    public class AbsoluteControlPanelMenu extends MovieClip
    {
        public var BGSCodeObj:Object;

        public function AbsoluteControlPanelMenu()
        {
            super();
        }

        public function onCodeObjCreate():void
        {
            // The first executable SWF will call ready with an explicit bridge version and will
            // render only a title, lifecycle status, close command, and focus diagnostics.
            if (BGSCodeObj != null && BGSCodeObj.ready != null) {
                BGSCodeObj.ready(1);
            }
        }

        public function applySnapshot(snapshot:Object):void
        {
            // Gate R2: immutable C++ -> UI snapshot. The UI must never infer domain policy.
        }

        private function dispatchCommand(command:String, payload:Object = null):void
        {
            if (BGSCodeObj != null && BGSCodeObj.dispatchCommand != null) {
                BGSCodeObj.dispatchCommand(1, command, payload);
            }
        }
    }
}
