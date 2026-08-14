package acp.ui
{
    public final class SliderWriteCoordinator
    {
        private var pending:Object;
        private var preparedModel:Object;
        private var preparedPage:Object;

        public function prepare(model:Object, page:Object):void
        {
            preparedModel = model;
            preparedPage = page;
        }

        public function queue(command:String, control:Object,
            booleanValue:Boolean, integerValue:Number, floatValue:Number):void
        {
            if (preparedModel == null || preparedPage == null || control == null) return;
            pending = {
                "command":command,
                "moduleId":String(preparedPage.moduleId),
                "pageId":String(preparedPage.pageId),
                "controlId":String(control.controlId),
                "valueKind":uint(control.valueKind),
                "booleanValue":booleanValue,
                "integerValue":integerValue,
                "floatValue":floatValue,
                "expectedGeneration":Number(preparedModel.generation)
            };
        }

        public function isStale(next:Object):Boolean
        {
            return pending != null && (next == null ||
                Number(pending.expectedGeneration) != Number(next.generation));
        }

        public function flush(model:Object, page:Object, dispatch:Function,
            onStale:Function):void
        {
            if (pending == null) return;
            var write:Object = pending;
            pending = null;
            if (model == null || page == null ||
                Number(write.expectedGeneration) != Number(model.generation) ||
                String(write.moduleId) != String(page.moduleId) ||
                String(write.pageId) != String(page.pageId)) {
                if (onStale != null) onStale();
                return;
            }
            dispatch(String(write.command), String(write.moduleId),
                String(write.pageId), String(write.controlId),
                uint(write.valueKind), Boolean(write.booleanValue),
                Number(write.integerValue), Number(write.floatValue),
                Number(write.expectedGeneration));
        }

        public function clear():void
        {
            pending = null;
            preparedModel = null;
            preparedPage = null;
        }
    }
}
