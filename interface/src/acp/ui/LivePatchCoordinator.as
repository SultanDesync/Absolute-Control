package acp.ui
{
    public final class LivePatchCoordinator
    {
        public static function apply(model:Object, visiblePage:Object,
            moduleId:String, pageId:String, next:Array):Object
        {
            if (model == null || next == null || model.pages == null) return null;
            var current:Object = null;
            for (var pageIndex:int = 0; pageIndex < model.pages.length;
                 ++pageIndex) {
                var candidate:Object = model.pages[pageIndex];
                if (String(candidate.moduleId) == moduleId &&
                    String(candidate.pageId) == pageId) {
                    current = candidate;
                    break;
                }
            }
            if (current == null || current !== visiblePage ||
                current.liveComponents == null) return null;

            for (var patchIndex:int = 0; patchIndex < next.length;
                 ++patchIndex) {
                var patch:Object = next[patchIndex];
                var existing:Object = null;
                var existingIndex:int = -1;
                for (var componentIndex:int = 0;
                     componentIndex < current.liveComponents.length;
                     ++componentIndex) {
                    if (String(current.liveComponents[componentIndex].channelId) ==
                        String(patch.channelId)) {
                        existing = current.liveComponents[componentIndex];
                        existingIndex = componentIndex;
                        break;
                    }
                }
                if (existing == null ||
                    Number(patch.sequence) <= Number(existing.sequence)) continue;
                existing.available = patch.available;
                existing.error = patch.error;
                existing.sequence = patch.sequence;
                existing.timestampUs = patch.timestampUs;
                existing.frameFlags = patch.frameFlags;
                if (uint(patch.kind) == 0) {
                    existing.liveAvailable = patch.liveAvailable;
                    existing.liveValue = patch.liveValue;
                    existing.bands = patch.bands;
                    existing.markers = patch.markers;
                } else if (uint(patch.kind) == 1) {
                    existing.availableMask = patch.availableMask;
                    if (existing.samples == null) existing.samples = [];
                    if (patch.samples != null && patch.samples.length > 0) {
                        existing.samples.push(
                            patch.samples[patch.samples.length - 1]);
                        var capacity:int = Math.max(1,
                            int(existing.historyCapacity));
                        while (existing.samples.length > capacity) {
                            existing.samples.shift();
                        }
                    }
                } else {
                    current.liveComponents[existingIndex] = patch;
                }
            }
            return current;
        }
    }
}
