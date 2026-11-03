#include "ui/PauseMenuPresentation.h"
#include "ui/PauseEntryPolicy.h"
#include "ui/PauseViewportPolicy.h"
#include "EvidenceLog.h"
#include <RE/Starfield.h>
#include <atomic>
#include <cmath>
#include <format>
#include <optional>
#include <vector>

namespace AbsoluteControlPanelResearch::Ui::PauseMenuPresentation
{
    namespace
    {
        using Value = RE::Scaleform::GFx::Value;
        std::atomic_bool g_autoFit{ true }, g_purple{ false }, g_matchFont{ false };
        // The game's UI pass boosts these movie-space colors substantially.
        constexpr std::uint32_t kNormalPurple = 0x705098;
        constexpr std::uint32_t kSelectedPurple = 0x300C50;
        struct TintState
        {
            Value text;
            Value originalTransform;
            std::uint32_t appliedColor{};
            std::uint32_t loggedCycle{};
        };
        TintState& Tint()
        {
            // At most one retained label; avoid GFx releases during DLL teardown.
            static auto* state = new TintState;
            return *state;
        }

        std::optional<double> Number(const Value& object, const char* key)
        {
            Value value;
            if (!object.GetMember(key, &value)) return {};
            const double number = value.IsInt() ? value.GetInt() : value.IsUInt() ? value.GetUInt() :
                value.IsNumber() ? value.GetNumber() : NAN;
            return std::isfinite(number) ? std::optional(number) : std::nullopt;
        }
        bool Boolean(const Value& object, const char* key)
        {
            Value value;
            return object.GetMember(key, &value) && value.IsBoolean() && value.GetBoolean();
        }
        bool Object(const Value& parent, const char* key, Value& out)
        {
            return parent.GetMember(key, &out) && out.IsObject();
        }
        bool Clip(Value& list, int index, Value& clip)
        {
            Value args[]{ Value(index), Value(false) };
            return list.Invoke("GetClipByIndex", &clip, args, 2) && clip.IsObject();
        }

        bool Owned(Value& list, const Value& clip)
        {
            const auto item = Number(clip, "itemIndex");
            if (!item || *item < 0 || *item >= 128 || std::floor(*item) != *item) return false;
            Value row, arg(static_cast<int>(*item));
            if (!list.Invoke("GetDataForEntry", &row, &arg, 1) || !row.IsObject() ||
                !Boolean(row, PauseEntryPolicy::kOwnerMarker)) return false;
            const auto action = Number(row, "uActionType");
            return action && PauseEntryPolicy::ParseAction(*action) == PauseEntryPolicy::kAction;
        }

        void MatchFont(RE::Scaleform::GFx::Movie* movie, Value& list, std::uint32_t cycle)
        {
            struct WidthState { std::uint32_t cycle{}; bool ready{}; double field{}, row{}, list{}; };
            static WidthState state;
            const auto count = Number(list, "totalEntryClips");
            Value listBorder;
            if (!count || *count < 1 || *count > 128 || !Object(list, "Border_mc", listBorder)) return;
            struct Row { Value field, border, fill, fillShape; };
            std::vector<Row> rows;
            int ownedIndex = -1;
            for (int i = 0; i < static_cast<int>(*count); ++i) {
                Value clip, text;
                Row row;
                if (!Clip(list, i, clip) || !Object(clip, "Text_mc", text) ||
                    !Object(text, "textField", row.field) || !Object(clip, "Border_mc", row.border) ||
                    !Object(clip, "Fill_mc", row.fill)) return;
                // Writing Fill_mc.width takes its transform away from the authored timeline,
                // freezing its selected/unselected color. Resize only its static inner shape.
                Value childIndex(0);
                if (Number(row.fill, "numChildren") != 1.0 || Number(row.fill, "totalFrames") != 1.0 ||
                    !row.fill.Invoke("getChildAt", &row.fillShape, &childIndex, 1) ||
                    !row.fillShape.IsObject()) return;
                const auto shapeWidth = Number(row.fillShape, "width");
                if (!shapeWidth || *shapeWidth <= 0 || *shapeWidth > 10000) return;
                Value bounds, mask, scrollRect;
                if (!row.fillShape.Invoke("getBounds", &bounds, &row.fill, 1)) return;
                const auto shapeLeft = Number(bounds, "x"), shapeTop = Number(bounds, "y");
                const auto fillScale = Number(row.fill, "scaleX");
                if (!shapeLeft || !shapeTop || std::abs(*shapeLeft) > 0.5 || std::abs(*shapeTop) > 0.5 ||
                    !fillScale || *fillScale <= 0 || Number(row.fill, "rotation") != 0.0 ||
                    Number(row.fillShape, "rotation") != 0.0 ||
                    (row.fill.GetMember("mask", &mask) && mask.IsObject()) ||
                    (row.fill.GetMember("scrollRect", &scrollRect) && scrollRect.IsObject())) return;
                // This adapter only supports the vanilla axis-aligned, unscaled row layout.
                for (const auto* object : { &clip, &text, &row.field }) {
                    if (Number(*object, "scaleX") != 1.0 || Number(*object, "scaleY") != 1.0 ||
                        Number(*object, "rotation") != 0.0) return;
                }
                for (const auto* object : { &row.field, &row.border, &row.fill }) {
                    const auto width = Number(*object, "width");
                    if (!width || *width < 80 || *width > 1000) return;
                }
                if (Owned(list, clip)) ownedIndex = i;
                rows.push_back(row);
            }
            if (ownedIndex < 0) return;
            if (state.cycle != cycle) {
                state = { cycle }; // One bounded measurement attempt per menu open.
                auto& field = rows[ownedIndex].field;
                const auto original = Number(field, "width").value();
                // Let vanilla's existing shrink mode measure at a bounded wider size. Restore
                // immediately in this callback; no Configure/text/format changes or extra clip.
                if (!field.SetMember("width", Value(original * 1.25))) return;
                const auto natural = Number(field, "textWidth");
                if (!field.SetMember("width", Value(original))) {
                    EvidenceLog::Event("pause_width_restore_failed", std::format("cycle={}", cycle));
                    return;
                }
                if (!natural) return;
                const auto delta = PauseViewportPolicy::WidthDelta(original, *natural);
                const auto rowWidth = Number(rows[ownedIndex].border, "width").value();
                const auto listWidth = Number(listBorder, "width");
                const auto left = Number(list, "containerLeft"), top = Number(list, "containerTop");
                Value stage, point, global;
                if (!delta || !listWidth || !left || !top || !Object(list, "stage", stage) ||
                    *listWidth < 80 || *listWidth > 1000) return;
                const auto stageWidth = Number(stage, "stageWidth");
                Value args[]{ Value(*left + *listWidth + *delta), Value(*top) };
                movie->asMovieRoot->CreateObject(&point, "flash.geom.Point", args, 2);
                if (!point.IsObject() || !list.Invoke("localToGlobal", &global, &point, 1)) return;
                const auto right = Number(global, "x");
                // Keep this left-column adapter away from the central logo/other panels.
                if (!stageWidth || !right || *right <= 0 || *right > *stageWidth * 0.40) return;
                state.field = original + *delta;
                state.row = rowWidth + *delta;
                state.list = *listWidth + *delta;
                state.ready = true;
                EvidenceLog::Event("pause_width_fit", std::format(
                    "cycle={} field_before={:.2f} natural_text={:.2f} delta={:.2f} field_target={:.2f} row_target={:.2f}",
                    cycle, original, *natural, *delta, state.field, state.row));
            }
            if (!state.ready) return;
            // Absolute minimum sizes prevent repeated growth. Late subscribers can create
            // clips: size those too without taking ownership of the animated Fill transform.
            auto grow = [](Value& object, double target) {
                const auto width = Number(object, "width");
                return width && *width + 0.5 < target && object.SetMember("width", Value(target));
            };
            bool geometryChanged = false;
            for (auto& row : rows) {
                geometryChanged |= grow(row.field, state.field);
                geometryChanged |= grow(row.border, state.row); // Native hitArea is this Border_mc.
                const auto fillWidth = Number(row.fill, "width").value();
                if (fillWidth + 0.5 < state.row) {
                    const auto shapeWidth = Number(row.fillShape, "width").value();
                    grow(row.fillShape, shapeWidth * state.row / fillWidth);
                }
            }
            geometryChanged |= grow(listBorder, state.list);
            if (geometryChanged) (void)list.Invoke("UpdateContainerRect");
        }

        void Fit(RE::Scaleform::GFx::Movie* movie, Value& list, int count, std::uint32_t cycle)
        {
            const auto current = Number(list, "containerHeight");
            const auto border = Number(list, "borderHeight");
            const auto top = Number(list, "containerTop");
            Value first, second, stage;
            if (!current || !border || !top || *border <= 0 || !Clip(list, 0, first) ||
                !Clip(list, 1, second) || !Object(list, "stage", stage)) return;
            const auto firstY = Number(first, "y"), secondY = Number(second, "y");
            const auto rowHeight = Number(first, "clipHeight");
            const auto stageHeight = Number(stage, "stageHeight");
            if (!firstY || !secondY || !rowHeight || !stageHeight || *stageHeight <= 0) return;
            const auto pitch = *secondY - *firstY;
            if (pitch < *rowHeight - 1) return; // Unsupported overlapping/custom row layout.
            if (count * pitch + 1.0 <= *current + 0.5) return;

            Value origin, unit, globalOrigin, globalUnit;
            Value originArgs[]{ Value(0.0), Value(*top) };
            Value unitArgs[]{ Value(0.0), Value(*top + 100.0) };
            movie->asMovieRoot->CreateObject(&origin, "flash.geom.Point", originArgs, 2);
            movie->asMovieRoot->CreateObject(&unit, "flash.geom.Point", unitArgs, 2);
            if (!origin.IsObject() || !unit.IsObject() ||
                !list.Invoke("localToGlobal", &globalOrigin, &origin, 1) ||
                !list.Invoke("localToGlobal", &globalUnit, &unit, 1)) return;
            const auto y = Number(globalOrigin, "y"), endY = Number(globalUnit, "y");
            const auto x = Number(globalOrigin, "x"), endX = Number(globalUnit, "x");
            if (!y || !endY || !x || !endX || std::abs(*endX - *x) > 0.5) return;
            const auto scale = (*endY - *y) / 100.0;
            if (scale < 0.01 || scale > 10.0) return;
            // Leave the lower 12% of the stage for prompts/footer. No text scaling.
            const auto safeMaximum = (*stageHeight * 0.88 - *y) / scale;
            const auto target = PauseViewportPolicy::Target(count, pitch, *current, safeMaximum);
            if (!target || *target <= *current + 0.5) return;
            const auto borderTarget = *border * (*target / *current);
            const bool set = list.SetMember("borderHeight", Value(borderTarget));
            EvidenceLog::Event("pause_viewport_fit", std::format(
                "cycle={} entries={} before={:.2f} requested={:.2f} safe_max={:.2f} pitch={:.2f} applied={}",
                cycle, count, *current, *target, safeMaximum, pitch, set));
        }

        bool IsOurTransform(const Value& transform, std::uint32_t color)
        {
            return Number(transform, "redMultiplier") == 0.0 &&
                Number(transform, "greenMultiplier") == 0.0 &&
                Number(transform, "blueMultiplier") == 0.0 &&
                Number(transform, "redOffset") == double((color >> 16) & 255) &&
                Number(transform, "greenOffset") == double((color >> 8) & 255) &&
                Number(transform, "blueOffset") == double(color & 255);
        }

        void Style(RE::Scaleform::GFx::Movie* movie, Value& list, bool enabled, std::uint32_t cycle)
        {
            auto& tint = Tint();
            if (tint.text.IsObject()) {
                Value transform, color;
                if (Object(tint.text, "transform", transform) && Object(transform, "colorTransform", color) &&
                    IsOurTransform(color, tint.appliedColor))
                    (void)transform.SetMember("colorTransform", tint.originalTransform);
                tint.text = Value();
                tint.originalTransform = Value();
            }
            if (!enabled) return;
            const auto clips = Number(list, "totalEntryClips");
            if (!clips || *clips < 0 || *clips > 128) return;
            for (int index = 0; index < static_cast<int>(*clips); ++index) {
                Value clip, text, field;
                if (!Clip(list, index, clip) || !Object(clip, "Text_mc", text) ||
                    !Object(text, "textField", field)) continue;
                const auto item = Number(clip, "itemIndex");
                Value row, arg(item ? static_cast<int>(*item) : -1);
                bool owned = enabled && item && *item >= 0 && *item < 128 &&
                    list.Invoke("GetDataForEntry", &row, &arg, 1) && row.IsObject() &&
                    Boolean(row, PauseEntryPolicy::kOwnerMarker);
                const auto action = owned ? Number(row, "uActionType") : std::nullopt;
                owned = owned && action && PauseEntryPolicy::ParseAction(*action) == PauseEntryPolicy::kAction;
                if (owned) {
                    // textColor alone is overridden by vanilla Text_mc's selection transform.
                    // Tint only the glyph container, never the row's Fill/Border/spinner.
                    const auto wanted = Boolean(clip, "selected") ? kSelectedPurple : kNormalPurple;
                    Value transform, original, purple;
                    if (!Object(text, "transform", transform) ||
                        !Object(transform, "colorTransform", original)) break;
                    const auto alpha = Number(original, "alphaMultiplier");
                    const auto alphaOffset = Number(original, "alphaOffset");
                    if (!alpha || !alphaOffset) break;
                    Value args[]{ Value(0.0), Value(0.0), Value(0.0), Value(*alpha),
                        Value(double((wanted >> 16) & 255)), Value(double((wanted >> 8) & 255)),
                        Value(double(wanted & 255)), Value(*alphaOffset) };
                    movie->asMovieRoot->CreateObject(&purple, "flash.geom.ColorTransform", args, 8);
                    if (purple.IsObject() && transform.SetMember("colorTransform", purple)) {
                        tint.text = text;
                        tint.originalTransform = original;
                        tint.appliedColor = wanted;
                        if (tint.loggedCycle != cycle) {
                            EvidenceLog::Event("pause_entry_styled", std::format(
                                "cycle={} item={} color=0x{:06X}", cycle, static_cast<int>(*item), wanted));
                            tint.loggedCycle = cycle;
                        }
                    }
                    break;
                }
            }
        }
    }

    void Configure(bool autoFit, bool purpleEntry, bool matchFont) noexcept
    {
        g_autoFit = autoFit;
        g_purple = purpleEntry;
        g_matchFont = matchFont;
    }

    void Advance(RE::Scaleform::GFx::Movie* movie, std::uint32_t cycle) noexcept
    {
        // Called only by this movie's enterFrame, never by a worker or a foreign movie.
        // Observe final/live entry count; do not delay injection or rebuild another owner's rows.
        try {
            Value list;
            if (!movie || !movie->asMovieRoot || !movie->asMovieRoot->GetVariable(
                &list, "_root.Menu_mc.MainPanel_mc.MainList_mc") || !list.IsObject()) return;
            const auto count = Number(list, "entryCount");
            if (!count || *count <= 0 || *count > 128 || std::floor(*count) != *count) return;
            if (g_autoFit) Fit(movie, list, static_cast<int>(*count), cycle);
            if (g_matchFont) MatchFont(movie, list, cycle);
            Style(movie, list, g_purple, cycle);
        } catch (...) {
            // Unknown menu replacements fail optional: preserve their own layout and navigation.
        }
    }
}
