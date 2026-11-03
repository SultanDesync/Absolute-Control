#include "research/PauseMenuDiagnostics.h"
#include "EvidenceLog.h"
#include <RE/Starfield.h>
#include <format>
#include <string>

namespace AbsoluteControlPanelResearch::Research::PauseMenuDiagnostics
{
    namespace
    {
        using Value = RE::Scaleform::GFx::Value;
        std::string Field(const Value& object, const char* name)
        {
            Value value;
            if (!object.GetMember(name, &value)) return "<unavailable>";
            if (value.IsBoolean()) return value.GetBoolean() ? "true" : "false";
            if (value.IsInt()) return std::to_string(value.GetInt());
            if (value.IsUInt()) return std::to_string(value.GetUInt());
            if (value.IsNumber()) return std::to_string(value.GetNumber());
            if (value.IsString()) {
                auto result = std::string(value.GetString()).substr(0, 96);
                for (auto& ch : result) if (ch == '\r' || ch == '\n') ch = ' ';
                return result;
            }
            return "<non-scalar>";
        }
        int Count(const Value& object, const char* name)
        {
            Value value;
            if (!object.GetMember(name, &value)) return -1;
            if (value.IsInt()) return value.GetInt();
            if (value.IsUInt() && value.GetUInt() <= 128) return static_cast<int>(value.GetUInt());
            return -1;
        }
    }

    void Observe(RE::Scaleform::GFx::Movie* movie, std::uint32_t cycle,
        std::string_view phase) noexcept
    {
        // Research-only, bounded per process and sampled on the movie's own UI thread.
        static unsigned snapshots = 0;
        if (snapshots >= 80) return;
        ++snapshots;
        try {
            Value list;
            if (!movie || !movie->asMovieRoot || !movie->asMovieRoot->GetVariable(
                &list, "_root.Menu_mc.MainPanel_mc.MainList_mc") || !list.IsObject()) {
                EvidenceLog::Event("pause_list_observation", std::format(
                    "cycle={} phase={} available=false", cycle, phase));
                return;
            }
            const auto count = Count(list, "entryCount");
            const auto clips = Count(list, "totalEntryClips");
            EvidenceLog::Event("pause_list_observation", std::format(
                "cycle={} phase={} available=true entries={} clips={} selected={} "
                "scroll={} max_scroll={} items_shown={} height={} visible={}",
                cycle, phase, count, clips, Field(list, "selectedIndex"),
                Field(list, "scrollPosition"), Field(list, "maxScrollPosition"),
                Field(list, "itemsShown"), Field(list, "height"), Field(list, "visible")));
            for (int index = 0; index < count && index < 128; ++index) {
                Value arg(index), row;
                // Public accessor: no selectedIndex writes, scrolling, or list rebuilds.
                const bool got = list.Invoke("GetDataForEntry", &row, &arg, 1) && row.IsObject();
                EvidenceLog::Event("pause_list_row", std::format(
                    "cycle={} phase={} index={} readable={} action={} owner={} disabled={} label=[{}]",
                    cycle, phase, index, got, got ? Field(row, "uActionType") : "?",
                    got ? Field(row, "_absoluteControlPanelEntry") : "?",
                    got ? Field(row, "bDisabled") : "?", got ? Field(row, "sActionText") : "?"));
            }
            for (int index = 0; index < clips && index < 128; ++index) {
                Value args[]{ Value(index), Value(false) }, clip, text, field;
                // Explicit false prevents GetClipByIndex from creating missing clips.
                const bool got = list.Invoke("GetClipByIndex", &clip, args, 2) && clip.IsObject();
                const bool gotText = got && clip.GetMember("Text_mc", &text) && text.IsObject() &&
                    text.GetMember("textField", &field) && field.IsObject();
                EvidenceLog::Event("pause_list_clip", std::format(
                    "cycle={} phase={} clip={} readable={} item={} visible={} y={} alpha={} text=[{}]",
                    cycle, phase, index, got, got ? Field(clip, "itemIndex") : "?",
                    got ? Field(clip, "visible") : "?", got ? Field(clip, "y") : "?",
                    got ? Field(clip, "alpha") : "?", gotText ? Field(field, "text") : "?"));
                if (gotText && (index == 0 || index == 11)) {
                    Value format, border, fill;
                    field.GetMember("defaultTextFormat", &format);
                    clip.GetMember("Border_mc", &border);
                    clip.GetMember("Fill_mc", &fill);
                    EvidenceLog::Event("pause_label_metrics", std::format(
                        "cycle={} phase={} clip={} field_width={} text_width={} text_height={} "
                        "font_size={} border_width={} fill_width={} "
                        "text_x={} text_scale={} field_x={} field_scale={} selected={} frame={} fill_children={} fill_frames={}", cycle, phase, index,
                        Field(field, "width"), Field(field, "textWidth"), Field(field, "textHeight"),
                        Field(format, "size"),
                        Field(border, "width"), Field(fill, "width"), Field(text, "x"), Field(text, "scaleX"),
                        Field(field, "x"), Field(field, "scaleX"), Field(clip, "selected"),
                        Field(clip, "currentLabel"), Field(fill, "numChildren"), Field(fill, "totalFrames")));
                }
            }
        } catch (...) {
            EvidenceLog::Event("pause_list_observation_error", std::format("cycle={} phase={}", cycle, phase));
        }
    }

    void Advance(RE::Scaleform::GFx::Movie* movie, std::uint32_t cycle) noexcept
    {
        static std::uint32_t lastCycle = 0, frame = 0;
        if (lastCycle != cycle) { lastCycle = cycle; frame = 0; }
        if (frame >= 600) return;
        ++frame;
        if (frame == 1 || frame == 2 || frame == 10 || frame == 60 || frame == 180 || frame == 600)
            Observe(movie, cycle, std::format("frame_{}", frame));
    }
}
