package acp.ui
{
    public final class BridgeCommandDispatcher
    {
        private var bridge:Object;

        public function BridgeCommandDispatcher(nativeBridge:Object)
        {
            bridge = nativeBridge;
        }

        public function send(model:Object, current:Object, command:String,
            control:Object, booleanValue:Boolean, integerValue:Number,
            floatValue:Number, controlIdentity:Boolean = true):void
        {
            if (model == null || bridge == null || bridge.dispatch == null ||
                (current == null && command != "close")) return;
            var moduleId:String = "";
            var pageId:String = "";
            var controlId:String = "";
            var valueKind:uint = 3;
            if (controlIdentity && current != null) {
                moduleId = String(current.moduleId);
                pageId = String(current.pageId);
                if (control != null) {
                    controlId = String(control.controlId);
                    valueKind = uint(control.valueKind);
                }
            } else if (control != null) {
                moduleId = String(control.moduleId);
                pageId = String(control.pageId);
            }
            dispatchFlat(command, moduleId, pageId, controlId, valueKind,
                booleanValue, integerValue, floatValue, Number(model.generation));
        }

        public function dispatchFlat(command:String, moduleId:String,
            pageId:String, controlId:String, valueKind:uint,
            booleanValue:Boolean, integerValue:Number, floatValue:Number,
            expectedGeneration:Number):void
        {
            if (bridge == null || bridge.dispatch == null) return;
            bridge.dispatch(1, command, moduleId, pageId, controlId, valueKind,
                booleanValue, integerValue, floatValue, "", expectedGeneration);
        }

        public function sendCompound(model:Object, current:Object,
            component:Object, operationKind:uint, columnId:String,
            tierId:String, count:uint):void
        {
            if (model == null || current == null || component == null ||
                bridge == null || bridge.compound == null) return;
            bridge.compound(1, String(current.moduleId), String(current.pageId),
                String(component.channelId), String(component.controlId), columnId,
                tierId, operationKind, count, Number(model.generation));
        }

        public function selectGridColumn(model:Object, current:Object,
            component:Object, columnId:String):void
        {
            if (model == null || current == null || component == null ||
                bridge == null || bridge.dispatch == null) return;
            bridge.dispatch(1, "selectGridColumn", String(current.moduleId),
                String(current.pageId), columnId, 3, false, 0, 0,
                String(component.channelId), Number(model.generation));
        }
    }
}
