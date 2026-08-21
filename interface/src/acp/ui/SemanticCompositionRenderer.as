package acp.ui
{
    import flash.display.Sprite;

    public final class SemanticCompositionRenderer
    {
        public static const ROW_HEIGHT:Number = 82;
        public static const EMBEDDED_RANGE_HEIGHT:Number = 132;
        public static const EMBEDDED_PLOT_HEIGHT:Number = 172;
        private static const ANCHORS_PER_ROW:int = 8;
        private static const ANCHOR_HEIGHT:Number = 38;

        private var layer:Sprite;
        private var hits:PointerInteraction;

        public function SemanticCompositionRenderer(target:Sprite,
            pointerHits:PointerInteraction)
        {
            layer = target;
            hits = pointerHits;
        }

        public function drawAnchors(current:Object, state:MenuSelectionState,
            yPosition:Number):Number
        {
            var anchors:Array = state.semanticAnchors(current);
            if (anchors.length == 0) return 0;
            var rows:int = Math.ceil(anchors.length / ANCHORS_PER_ROW);
            var width:Number = (PanelLayout.CONTROL_ROW_WIDTH -
                7 * 8) / ANCHORS_PER_ROW;
            for (var index:int = 0; index < anchors.length; ++index) {
                var anchor:Object = anchors[index];
                var button:Sprite = new Sprite();
                var focused:Boolean = state.focusRegion ==
                    PanelLayout.FOCUS_ANCHORS && state.focusedAction == index;
                button.x = (index % ANCHORS_PER_ROW) * (width + 8);
                button.y = yPosition + int(index / ANCHORS_PER_ROW) *
                    ANCHOR_HEIGHT;
                button.graphics.lineStyle(focused ? 2 : 1,
                    focused ? PanelTheme.GOLD : PanelTheme.BORDER);
                button.graphics.beginFill(focused ? PanelTheme.ROW_SELECTED :
                    PanelTheme.BUTTON_FILL);
                button.graphics.drawRoundRect(0, 0, width,
                    ANCHOR_HEIGHT - 5, 7, 7);
                button.graphics.endFill();
                button.buttonMode = true;
                VectorTextRenderer.addText(button,
                    VectorTextRenderer.fit(String(anchor.label), 17),
                    10, 8, 14, focused ? PanelTheme.GOLD : PanelTheme.CYAN,
                    true, width - 20, 20);
                layer.addChild(button);
                hits.register(button, "anchor", anchor, index);
            }
            return rows * ANCHOR_HEIGHT + 6;
        }

        public function drawFrame(current:Object, control:Object,
            yPosition:Number):Number
        {
            if (current == null || !Boolean(current.compositionEnhanced)) {
                return 0;
            }
            var node:Object = frameNode(current, String(control.controlId));
            if (node == null || !isFirstControlInNode(current, control, node)) {
                return 0;
            }
            var frame:Sprite = new Sprite();
            frame.y = yPosition;
            var enabled:Boolean = nodeIsEffective(current, node, 2);
            var emphasized:Boolean = (uint(node.flags) & 2) != 0;
            var severity:uint = uint(node.severity);
            frame.graphics.lineStyle(emphasized ? 2 : 1,
                enabled ? severityColor(severity) : PanelTheme.BORDER);
            frame.graphics.beginFill(PanelTheme.ROW_EVEN, 0.96);
            frame.graphics.drawRoundRect(0, 0,
                PanelLayout.CONTROL_ROW_WIDTH, ROW_HEIGHT - 5, 8, 8);
            frame.graphics.endFill();
            VectorTextRenderer.addText(frame,
                VectorTextRenderer.fit(String(node.label).toUpperCase(), 38),
                14, 6, 14, enabled ? PanelTheme.TEXT :
                    PanelTheme.DISABLED_TEXT, true, 470, 20);
            var value:String = String(node.value);
            if (value.length > 0) {
                VectorTextRenderer.addText(frame,
                    VectorTextRenderer.fit(value, 42), 720, 6, 13,
                    severityColor(severity), true, 360, 20);
            }
            var source:String = String(node.sourceLabel);
            if (source.length > 0) {
                VectorTextRenderer.addText(frame,
                    VectorTextRenderer.fit(source, 28), 1100, 6, 12,
                    PanelTheme.DIM_TEXT, false, 270, 20);
            }
            layer.addChild(frame);
            return 27;
        }

        public function rowHeight(current:Object, control:Object):Number
        {
            if (current == null || control == null ||
                !Boolean(current.compositionEnhanced)) return ROW_HEIGHT;
            var frame:Object = frameNode(current, String(control.controlId));
            if (frame == null || !isFirstControlInNode(current, control, frame)) {
                return ROW_HEIGHT;
            }
            var component:Object = embeddedLiveComponent(current, frame);
            return ROW_HEIGHT + 27 + (component == null ? 0 :
                (uint(component.kind) == 0 ? EMBEDDED_RANGE_HEIGHT :
                    EMBEDDED_PLOT_HEIGHT) + 6);
        }

        public function liveComponentForControl(current:Object,
            control:Object):Object
        {
            if (current == null || control == null ||
                !Boolean(current.compositionEnhanced)) return null;
            var frame:Object = frameNode(current, String(control.controlId));
            if (frame == null || !isFirstControlInNode(current, control, frame)) {
                return null;
            }
            return embeddedLiveComponent(current, frame);
        }

        public function detailForControl(current:Object, control:Object):String
        {
            if (current == null || control == null ||
                !Boolean(current.compositionEnhanced)) return "";
            var node:Object = frameNode(current, String(control.controlId));
            var detail:String = node == null ? "" : String(node.detail);
            if (current.compositionAssociations != null) {
                for (var index:int = 0;
                     index < current.compositionAssociations.length; ++index) {
                    var association:Object =
                        current.compositionAssociations[index];
                    if (String(association.sourceId) !=
                        String(control.controlId)) continue;
                    var semantic:String = String(association.semanticId);
                    if (semantic.length == 0) continue;
                    if (detail.length > 0) detail += "  ";
                    detail += "LIVE GRAPH: " + semantic.toUpperCase();
                    break;
                }
            }
            return detail;
        }

        private function frameNode(current:Object, controlId:String):Object
        {
            var placement:Object = null;
            for (var index:int = 0;
                 index < current.compositionNodes.length; ++index) {
                var candidate:Object = current.compositionNodes[index];
                if (uint(candidate.kind) == 11 &&
                    String(candidate.referenceId) == controlId) {
                    placement = candidate;
                    break;
                }
            }
            if (placement == null) return null;
            var cursor:Object = placement;
            var section:Object = null;
            var remaining:int = current.compositionNodes.length + 1;
            while (cursor != null && remaining-- > 0) {
                if (uint(cursor.kind) == 2) return cursor;
                if (uint(cursor.kind) == 1) section = cursor;
                cursor = nodeById(current, String(cursor.parentNodeId));
            }
            return section;
        }

        private function isFirstControlInNode(current:Object, control:Object,
            frame:Object):Boolean
        {
            for (var index:int = 0;
                 index < current.compositionNodes.length; ++index) {
                var candidate:Object = current.compositionNodes[index];
                if (uint(candidate.kind) != 11 ||
                    !nodeDescendsFrom(current, candidate,
                        String(frame.nodeId))) continue;
                return String(candidate.referenceId) ==
                    String(control.controlId);
            }
            return false;
        }

        private function embeddedLiveComponent(current:Object,
            frame:Object):Object
        {
            if (current.liveComponents == null) return null;
            var channelId:String = "";
            for (var nodeIndex:int = 0;
                 nodeIndex < current.compositionNodes.length; ++nodeIndex) {
                var node:Object = current.compositionNodes[nodeIndex];
                if (uint(node.kind) == 10 &&
                    nodeDescendsFrom(current, node, String(frame.nodeId))) {
                    channelId = String(node.referenceId);
                    break;
                }
            }
            if (channelId.length == 0) return null;
            for (var componentIndex:int = 0;
                 componentIndex < current.liveComponents.length;
                 ++componentIndex) {
                var component:Object = current.liveComponents[componentIndex];
                if (String(component.channelId) == channelId) return component;
            }
            return null;
        }

        private function nodeDescendsFrom(current:Object, node:Object,
            ancestorId:String):Boolean
        {
            var cursor:Object = node;
            var remaining:int = current.compositionNodes.length + 1;
            while (cursor != null && remaining-- > 0) {
                if (String(cursor.nodeId) == ancestorId) return true;
                var parentId:String = String(cursor.parentNodeId);
                if (parentId.length == 0) return false;
                cursor = nodeById(current, parentId);
            }
            return false;
        }

        private function nodeById(current:Object, nodeId:String):Object
        {
            if (nodeId.length == 0) return null;
            for (var index:int = 0;
                 index < current.compositionNodes.length; ++index) {
                if (String(current.compositionNodes[index].nodeId) == nodeId) {
                    return current.compositionNodes[index];
                }
            }
            return null;
        }

        private function nodeIsEffective(current:Object, node:Object,
            requiredFlag:uint):Boolean
        {
            var cursor:Object = node;
            var remaining:int = current.compositionNodes.length + 1;
            while (cursor != null && remaining-- > 0) {
                if ((uint(cursor.stateFlags) & requiredFlag) == 0) return false;
                var parentId:String = String(cursor.parentNodeId);
                if (parentId.length == 0) return true;
                cursor = nodeById(current, parentId);
            }
            return false;
        }

        private function severityColor(severity:uint):uint
        {
            if (severity == 4) return PanelTheme.ERROR;
            if (severity == 3) return PanelTheme.WARNING;
            if (severity == 2) return PanelTheme.GOLD;
            if (severity == 5) return PanelTheme.DISABLED_TEXT;
            return PanelTheme.CYAN;
        }
    }
}
