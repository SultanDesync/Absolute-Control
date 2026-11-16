#include "ui/FlightPages.h"

#include "AbsoluteControlPanelAPI.h"
#include "LiveComponentsExperimentalAPI.h"
#include "EvidenceLog.h"
#include "ShipActionCatalog.h"
#include "runtime/AxisCapture.h"
#include "runtime/BackendRuntime.h"
#include "runtime/ButtonCapture.h"
#include "runtime/ControlSettings.h"
#include "runtime/FlightAxes.h"
#include "runtime/FlightTelemetry.h"
#include "runtime/ShipBindings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControlPanelResearch::Ui::FlightPages
{
    namespace
    {
        namespace Api = AbsoluteControlPanelApi;
        namespace Live = AbsoluteControlPanelExperimental;
        namespace Rt = AbsoluteControl::Runtime;
        namespace Telemetry = AbsoluteControl::Runtime::FlightTelemetry;

        constexpr std::string_view kModuleId = "absolute.control";

        enum class Kind
        {
            Header,
            Axis,    // A binding recorded by moving an axis.
            Button,  // A binding recorded by pressing a button or hat.
            Toggle,
            Percent, // A percentage, in the file and on the page.
            Action,  // Sets another row from where the lever is right now.
            Text,    // A line to read, kept in the row's target.
            Choice,  // One of the row's choices, saved by its name.
            Reset    // Puts every flight setting back to a new install's.
        };

        struct Row
        {
            Kind kind{};
            std::string id, label, description;
            std::string section, key;
            double fallback{};
            int minimum{}, maximum{}, step{ 1 };
            std::string target; // For an action: the row it sets.
            // For a choice: the name saved in the file, then what the page shows. The first
            // is used when nothing is saved.
            std::vector<std::pair<std::string, std::string>> choices;
        };

        struct Page
        {
            std::string pageId, name, description, group;
            std::vector<Row> rows;
            std::vector<Api::ControlDescriptorV1> controls;
            std::mutex mutex;
            std::map<std::string, std::string> draft; // Row id to the text that Apply will save.
            Rt::AxisCapture axisCapture;
            Rt::ButtonCapture buttonCapture;
        };

        // Every page, so one of them can clear what the others have pending.
        std::vector<Page*> g_pages;

        template <std::size_t N>
        void Copy(char (&destination)[N], std::string_view source) noexcept
        {
            const auto count = (std::min)(source.size(), N - 1);
            std::memcpy(destination, source.data(), count);
            destination[count] = '\0';
        }

        std::string_view Trim(std::string_view text) noexcept
        {
            const auto first = text.find_first_not_of(" \t");
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(" \t") - first + 1);
        }

        bool IsTrue(std::string_view text) noexcept
        {
            std::string value(Trim(text));
            for (char& c : value) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            return value == "true" || value == "1" || value == "yes" || value == "on";
        }

        double Number(std::string_view text, double fallback) noexcept
        {
            text = Trim(text);
            double value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end != text.data() && std::isfinite(value) ? value : fallback;
        }

        // The saved text for a row: what the page has pending, else what the files say.
        std::optional<std::string> Stored(const Page& page, const Row& row)
        {
            if (const auto found = page.draft.find(row.id); found != page.draft.end()) return found->second;
            return Rt::ControlSettings::Instance().Get(row.section, row.key);
        }

        // "VKBsim T-Rudder · Axis 4", and a warning when the device or the control isn't there.
        std::string BindingLabel(std::string_view token, bool axis)
        {
            token = Trim(token);
            if (token.empty() || token == "-1") return "Unbound";
            const auto binding = axis ? Rt::ParseAxisBinding("", token) : Rt::ParseShipBinding("", token);
            if (binding.status != Rt::BindingStatus::Bound) return std::format("{} (can't be read)", token);

            std::string what;
            unsigned number{};
            if (axis && ::sscanf_s(binding.channel.c_str(), "axis/%u", &number) == 1) {
                what = std::format("Axis {}", number + 1);
            } else if (const auto button = Rt::BindingNumberForChannel(binding.channel); button >= 129) {
                constexpr std::array<std::string_view, 4> kDirections{ "up", "right", "down", "left" };
                what = std::format("Hat {} {}", (button - 129) / 4 + 1, kDirections[(button - 129) % 4]);
            } else {
                what = std::format("Button {}", button);
            }

            const auto snapshot = Rt::NativeBackend().Devices().Snapshot();
            for (const auto& description : snapshot.devices) {
                const auto name = Trim(description.device.displayName);
                const bool same = binding.devicePhysicalId ? binding.device == description.device.physical :
                    std::ranges::equal(name, std::string_view(binding.device), [](char a, char b) {
                        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                    });
                if (!same) continue;
                const bool present = std::ranges::any_of(description.channels, [&](const auto& channel) {
                    return binding.channel == channel.identity.channel;
                });
                return present ? std::format("{} · {}", name, what) : std::format("{} · {} (not on this device)", name, what);
            }
            return std::format("{} · {} (not connected)", binding.device, what);
        }

        // Which of a row's choices the saved text names.
        std::size_t ChoiceIndex(const Row& row, std::string_view stored)
        {
            stored = Trim(stored);
            for (std::size_t index = 0; index < row.choices.size(); ++index) {
                if (std::ranges::equal(std::string_view(row.choices[index].first), stored, [](char a, char b) {
                        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                    })) return index;
            }
            return 0;
        }

        const Row* Find(const Page& page, std::string_view id)
        {
            const auto found = std::ranges::find_if(page.rows, [&](const Row& row) { return row.id == id; });
            return found == page.rows.end() ? nullptr : &*found;
        }

        // A row's value as the page shows it: a percentage, or 0 and 1 for a switch.
        double Shown(const Page& page, std::string_view id)
        {
            const auto* row = Find(page, id);
            if (!row) return 0;
            const auto stored = Stored(page, *row);
            switch (row->kind) {
            case Kind::Toggle: return (stored ? IsTrue(*stored) : row->fallback != 0) ? 1 : 0;
            case Kind::Percent: return std::clamp(Number(stored.value_or(""), row->fallback), 0.0, 200.0);
            default: return 0;
            }
        }

        // What the live graphs draw. The graphs are read from the menu's thread without
        // locks, so the page's pending values are copied here whenever they change.
        struct ThrottleView
        {
            std::atomic<float> idle{ 5 }, saturation{ 100 }, reverseAt{}, reverseWidth{}, boostAt{ 95 }, boostWidth{};
            std::atomic<bool> invert{}, reverse{}, boost{};
        };
        struct StickView
        {
            const char* page;
            const char* prefix;
            const char* title;
            std::size_t telemetry;
            std::atomic<float> deadzone{}, saturation{ 100 };
            std::atomic<bool> invert{};
            std::atomic<std::uint64_t> sequence{};
        };
        ThrottleView g_throttle;
        std::atomic<std::uint64_t> g_throttleSequence{};
        std::array<StickView, 7> g_sticks{ {
            { "flight-axes", "pitch", "Pitch", Telemetry::Pitch }, { "flight-axes", "yaw", "Yaw", Telemetry::Yaw },
            { "flight-axes", "roll", "Roll", Telemetry::Roll },
            { "flight-axes", "strafe-side", "Strafe sideways", Telemetry::StrafeLateral },
            { "flight-axes", "strafe-up", "Strafe up and down", Telemetry::StrafeVertical },
            { "reticle", "aim-yaw", "Reticle left and right", Telemetry::AimYaw },
            { "reticle", "aim-pitch", "Reticle up and down", Telemetry::AimPitch },
        } };

        void SyncViews(const Page& page)
        {
            if (page.pageId == "throttle") {
                g_throttle.idle = static_cast<float>(Shown(page, "throttle-idle"));
                g_throttle.saturation = static_cast<float>(Shown(page, "throttle-saturation"));
                g_throttle.invert = Shown(page, "throttle-invert") != 0;
                g_throttle.reverse = Shown(page, "zone-reverse") != 0;
                g_throttle.reverseAt = static_cast<float>(Shown(page, "zone-reverse-at"));
                g_throttle.reverseWidth = static_cast<float>(Shown(page, "zone-reverse-width"));
                g_throttle.boost = Shown(page, "zone-boost") != 0;
                g_throttle.boostAt = static_cast<float>(Shown(page, "zone-boost-at"));
                g_throttle.boostWidth = static_cast<float>(Shown(page, "zone-boost-width"));
            } else {
                for (auto& stick : g_sticks) {
                    if (page.pageId != stick.page) continue;
                    const std::string prefix{ stick.prefix };
                    stick.deadzone = static_cast<float>(Shown(page, prefix + "-deadzone"));
                    stick.saturation = static_cast<float>(Shown(page, prefix + "-saturation"));
                    stick.invert = Shown(page, prefix + "-invert") != 0;
                }
            }
        }

        // Where the lever is, as travel from the bottom, with the page's pending invert.
        std::optional<float> LeverTravel()
        {
            const auto position = Telemetry::Position(Telemetry::Throttle);
            if (!position) return {};
            const float travel = std::clamp((*position + 1.0F) * 50.0F, 0.0F, 100.0F);
            return g_throttle.invert.load(std::memory_order_relaxed) ? 100.0F - travel : travel;
        }

        struct Range
        {
            std::uint32_t bandCount{}, markerCount{};
            Live::RangeBandV1 bands[Live::kMaximumRangeBands]{};
            Live::RangeMarkerV1 markers[Live::kMaximumRangeMarkers]{};

            void Band(Live::RangeBandSemantic semantic, Live::VisualRole role, double from, double to, const char* label) noexcept
            {
                if (bandCount >= Live::kMaximumRangeBands || to <= from) return;
                auto& band = bands[bandCount++];
                band.semantic = semantic;
                band.visualRole = role;
                band.minimumValue = from;
                band.maximumValue = to;
                Copy(band.label, label);
            }
            // A marker with a control can be dragged, and the drag writes that control.
            void Marker(Live::RangeMarkerSemantic semantic, double value, const char* id, const char* label, const char* control = "") noexcept
            {
                if (markerCount >= Live::kMaximumRangeMarkers) return;
                auto& marker = markers[markerCount++];
                marker.semantic = semantic;
                marker.value = value;
                Copy(marker.markerId, id);
                Copy(marker.label, label);
                Copy(marker.controlId, control);
            }
        };

        // The lever's travel, bottom to top, as the throttle maths divides it up.
        Range ThrottleRange() noexcept
        {
            using Band = Live::RangeBandSemantic;
            using Role = Live::VisualRole;
            Range range;
            const bool reverse = g_throttle.reverse.load(std::memory_order_relaxed);
            const bool boost = g_throttle.boost.load(std::memory_order_relaxed);
            const double idle = std::clamp<double>(g_throttle.idle.load(std::memory_order_relaxed), 0, 95);
            const double reverseAt = g_throttle.reverseAt.load(std::memory_order_relaxed);
            const double reverseWidth = g_throttle.reverseWidth.load(std::memory_order_relaxed);
            const double boostAt = g_throttle.boostAt.load(std::memory_order_relaxed);
            const double boostWidth = g_throttle.boostWidth.load(std::memory_order_relaxed);
            const double boostFrom = std::clamp(boostAt + boostWidth, 0.0, 100.0);

            double thrustFrom = idle;
            if (reverse) {
                const double low = std::clamp(reverseAt - reverseWidth, 0.0, 100.0);
                thrustFrom = std::clamp(reverseAt + reverseWidth, low, 100.0);
                range.Band(Band::Reverse, Role::Warning, 0, low, "Reverse");
                range.Band(Band::Dead, Role::Neutral, low, thrustFrom, "Zero thrust");
            } else {
                range.Band(Band::Dead, Role::Neutral, 0, idle, "Idle");
            }
            if (reverse && boost) {
                // With both zones the ramp is fitted between them: half thrust at the detent,
                // full thrust at the boost zone's lower edge.
                const double fullAt = std::clamp(boostAt - boostWidth, thrustFrom, 100.0);
                range.Band(Band::Active, Role::Accent, thrustFrom, fullAt, "Thrust");
                range.Band(Band::Cruise, Role::Positive, fullAt, std::max(fullAt, boostFrom), "Full thrust");
                range.Band(Band::Boost, Role::Critical, std::max(fullAt, boostFrom), 100, "Boost");
            } else if (boost) {
                range.Band(Band::Active, Role::Accent, thrustFrom, std::max(thrustFrom, boostFrom), "Thrust");
                range.Band(Band::Boost, Role::Critical, std::max(thrustFrom, boostFrom), 100, "Boost");
            } else {
                range.Band(Band::Active, Role::Accent, thrustFrom, 100, "Thrust");
            }

            if (reverse) {
                range.Marker(Live::RangeMarkerSemantic::Custom, std::clamp(reverseAt, 0.0, 100.0), "reverse-zone", "Reverse zone", "zone-reverse-at");
                range.Marker(Live::RangeMarkerSemantic::Detent, 50, "half-thrust", "Half thrust");
            } else {
                const double saturation = std::clamp<double>(g_throttle.saturation.load(std::memory_order_relaxed), 5, 100);
                range.Marker(Live::RangeMarkerSemantic::Saturation, std::clamp(idle + saturation * (100.0 - idle) / 100.0, 0.0, 100.0),
                    "full-thrust", "Full thrust");
            }
            if (boost) range.Marker(Live::RangeMarkerSemantic::Custom, std::clamp(boostAt, 0.0, 100.0), "boost-zone", "Boost zone", "zone-boost-at");
            return range;
        }

        // A stick's travel either side of centre.
        Range StickRange(const StickView& stick) noexcept
        {
            using Band = Live::RangeBandSemantic;
            using Role = Live::VisualRole;
            Range range;
            const double dead = std::clamp<double>(stick.deadzone.load(std::memory_order_relaxed), 0, 95);
            const double full = std::clamp<double>(stick.saturation.load(std::memory_order_relaxed), std::max(dead, 5.0), 100);
            range.Band(Band::Cruise, Role::Positive, -100, -full, "Full");
            range.Band(Band::Active, Role::Accent, -full, -dead, "");
            range.Band(Band::Dead, Role::Neutral, -dead, dead, "Deadzone");
            range.Band(Band::Active, Role::Accent, dead, full, "");
            range.Band(Band::Cruise, Role::Positive, full, 100, "Full");
            // Built without allocating: this runs on the menu's thread for every frame.
            char deadzoneControl[Live::kIdentifierCapacity]{}, saturationControl[Live::kIdentifierCapacity]{};
            std::snprintf(deadzoneControl, sizeof(deadzoneControl), "%s-deadzone", stick.prefix);
            std::snprintf(saturationControl, sizeof(saturationControl), "%s-saturation", stick.prefix);
            range.Marker(Live::RangeMarkerSemantic::Center, 0, "centre", "");
            range.Marker(Live::RangeMarkerSemantic::Custom, dead, "deadzone", "Deadzone", deadzoneControl);
            range.Marker(Live::RangeMarkerSemantic::Saturation, full, "full-output", "Full output", saturationControl);
            return range;
        }

        void FillFrame(Live::LiveFrameV1& frame, std::optional<float> value, const Range& range, std::uint64_t sequence) noexcept
        {
            frame.kind = Live::ComponentKind::RangeMeter;
            frame.sequence = sequence;
            frame.monotonicTimestampUs = Rt::BackendRuntime::Now() / 1000;
            frame.flags = value ? Live::kFrameNone : Live::kFrameUnavailable;
            frame.rangeMeter.available = value ? 1 : 0;
            frame.rangeMeter.liveValue = value ? *value : 0;
            if (frame.structSize >= Live::kLiveFrameV1DynamicRangeSize) {
                // The zones follow the page's pending values, so a change shows before Apply.
                frame.dynamicRange.present = 1;
                frame.dynamicRange.bandCount = range.bandCount;
                frame.dynamicRange.markerCount = range.markerCount;
                std::memcpy(frame.dynamicRange.bands, range.bands, sizeof(range.bands));
                std::memcpy(frame.dynamicRange.markers, range.markers, sizeof(range.markers));
            }
        }

        Live::Result __cdecl ReadThrottleFrame(void*, Live::LiveFrameV1* frame) noexcept
        {
            if (!frame || frame->structSize < Live::kLiveFrameV1BaseSize) return Live::Result::InvalidArgument;
            FillFrame(*frame, LeverTravel(), ThrottleRange(), g_throttleSequence.fetch_add(1, std::memory_order_relaxed) + 1);
            return Live::Result::Ok;
        }

        Live::Result __cdecl ReadStickFrame(void* context, Live::LiveFrameV1* frame) noexcept
        {
            if (!context || !frame || frame->structSize < Live::kLiveFrameV1BaseSize) return Live::Result::InvalidArgument;
            auto& stick = *static_cast<StickView*>(context);
            auto position = Telemetry::Position(stick.telemetry);
            if (position) {
                *position = std::clamp(*position * 100.0F, -100.0F, 100.0F);
                if (stick.invert.load(std::memory_order_relaxed)) *position = -*position;
            }
            FillFrame(*frame, position, StickRange(stick), stick.sequence.fetch_add(1, std::memory_order_relaxed) + 1);
            return Live::Result::Ok;
        }
        Api::Result __cdecl ReadValue(void* context, const char* rawId, Api::ValueV1* output) noexcept
        {
            if (!context || !rawId || !output || output->structSize < sizeof(Api::ValueV1)) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                const auto* row = Find(page, rawId);
                if (!row) return Api::Result::NotFound;
                const auto stored = Stored(page, *row);
                switch (row->kind) {
                case Kind::Toggle:
                    output->kind = Api::ValueKind::Boolean;
                    output->booleanValue = (stored ? IsTrue(*stored) : row->fallback != 0) ? 1 : 0;
                    break;
                case Kind::Percent:
                    output->kind = Api::ValueKind::Integer;
                    output->integerValue = std::clamp<std::int64_t>(std::llround(Number(stored.value_or(""), row->fallback)),
                        row->minimum, row->maximum);
                    break;
                case Kind::Axis:
                case Kind::Button:
                    output->kind = Api::ValueKind::String;
                    Copy(output->stringValue, BindingLabel(stored.value_or(""), row->kind == Kind::Axis));
                    break;
                case Kind::Text:
                    output->kind = Api::ValueKind::String;
                    Copy(output->stringValue, row->target);
                    break;
                case Kind::Choice:
                    output->kind = Api::ValueKind::Integer;
                    output->integerValue = static_cast<std::int64_t>(ChoiceIndex(*row, stored.value_or("")));
                    break;
                default:
                    return Api::Result::NotFound;
                }
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl WriteDraft(void* context, const char* rawId, const Api::ValueV1* value) noexcept
        {
            if (!context || !rawId || !value || value->structSize < sizeof(Api::ValueV1)) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                const auto* row = Find(page, rawId);
                if (!row) return Api::Result::NotFound;
                const auto integer = value->integerValue;
                if (row->kind == Kind::Toggle && value->kind == Api::ValueKind::Boolean) {
                    page.draft[row->id] = value->booleanValue ? "true" : "false";
                } else if (row->kind == Kind::Percent && value->kind == Api::ValueKind::Integer &&
                    integer >= row->minimum && integer <= row->maximum) {
                    page.draft[row->id] = std::format("{}", integer);
                } else if (row->kind == Kind::Choice && value->kind == Api::ValueKind::Integer &&
                    integer >= 0 && static_cast<std::size_t>(integer) < row->choices.size()) {
                    page.draft[row->id] = row->choices[static_cast<std::size_t>(integer)].first;
                } else if ((row->kind == Kind::Axis || row->kind == Kind::Button) && value->kind == Api::ValueKind::String) {
                    // Empty clears the binding. Anything else has to be a binding that can be read.
                    const std::string token{ Trim(value->stringValue) };
                    const auto status = token.empty() ? Rt::BindingStatus::Bound :
                        (row->kind == Kind::Axis ? Rt::ParseAxisBinding("", token) : Rt::ParseShipBinding("", token)).status;
                    if (status != Rt::BindingStatus::Bound) return Api::Result::InvalidArgument;
                    page.draft[row->id] = token;
                } else {
                    return Api::Result::InvalidArgument;
                }
                SyncViews(page);
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl ReadChoices(void* context, const char* rawId, Api::ChoiceOptionV1* options,
            std::uint32_t capacity, std::uint32_t* count) noexcept
        {
            if (!context || !rawId || !options || !count) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                const auto* row = Find(page, rawId);
                if (!row || row->kind != Kind::Choice) return Api::Result::NotFound;
                if (row->choices.size() > capacity) return Api::Result::CapacityExceeded;
                for (std::size_t index = 0; index < row->choices.size(); ++index) {
                    options[index].value = static_cast<std::int64_t>(index);
                    Copy(options[index].label, row->choices[index].second);
                }
                *count = static_cast<std::uint32_t>(row->choices.size());
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        // "Set from lever": puts a zone where the lever is right now.
        Api::Result __cdecl InvokeAction(void* context, const char* rawId) noexcept
        {
            if (!context || !rawId) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                const auto* row = Find(page, rawId);
                if (row && row->kind == Kind::Reset) {
                    // The menu has already asked the player to confirm.
                    auto& settings = Rt::ControlSettings::Instance();
                    const bool saved = settings.Reset();
                    settings.Publish();
                    for (auto* other : g_pages) {
                        if (other == &page) continue;
                        const std::scoped_lock otherLock{ other->mutex };
                        other->draft.clear();
                        SyncViews(*other);
                    }
                    EvidenceLog::Event("flight_settings_reset", std::format("saved={}", saved ? 1 : 0));
                    return saved ? Api::Result::Ok : Api::Result::WriteFailure;
                }
                const auto* target = row && row->kind == Kind::Action ? Find(page, row->target) : nullptr;
                if (!target) return Api::Result::NotFound;
                const auto travel = LeverTravel();
                if (!travel) return Api::Result::Rejected; // No lever bound, or it isn't connected.
                const auto percent = std::clamp<double>(std::round(*travel), target->minimum, target->maximum);
                page.draft[target->id] = std::format("{}", std::llround(percent));
                SyncViews(page);
                EvidenceLog::Event("flight_set_from_lever", std::format("control={} travel={:.0f}", target->id, percent));
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl BeginCapture(void* context, const char* rawId) noexcept
        {
            if (!context || !rawId) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                const auto* row = Find(page, rawId);
                if (!row || (row->kind != Kind::Axis && row->kind != Kind::Button)) return Api::Result::NotFound;
                const auto now = Rt::BackendRuntime::Now();
                if (row->kind == Kind::Axis) page.axisCapture.Begin(now);
                else page.buttonCapture.Begin(now);
                EvidenceLog::Event("flight_binding_capture", std::format("started control={}", row->id));
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl PollCapture(void* context, const char* rawId, Api::BindingCaptureV1* output) noexcept
        {
            if (!context || !rawId || !output || output->structSize < sizeof(Api::BindingCaptureV1)) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                const auto* row = Find(page, rawId);
                if (!row || (row->kind != Kind::Axis && row->kind != Kind::Button)) return Api::Result::NotFound;
                const bool axis = row->kind == Kind::Axis;
                const auto& devices = Rt::NativeBackend().Devices();
                const auto now = Rt::BackendRuntime::Now();
                // Both recorders share the same four states.
                const int state = axis ? static_cast<int>(page.axisCapture.Poll(devices, now)) :
                    static_cast<int>(page.buttonCapture.Poll(devices, now));
                const auto& token = axis ? page.axisCapture.Token() : page.buttonCapture.Token();
                const auto finish = [&] { axis ? page.axisCapture.Cancel() : page.buttonCapture.Cancel(); };
                switch (state) {
                case 1:
                    output->state = Api::BindingCaptureState::Capturing;
                    Copy(output->detail, axis ? "Move the axis you want to use." : "Press the button or hat you want to use.");
                    break;
                case 2:
                    output->state = Api::BindingCaptureState::Captured;
                    Copy(output->binding, token);
                    Copy(output->detail, "Recorded. Apply to keep it.");
                    EvidenceLog::Event("flight_binding_capture", std::format("captured control={} token={}", row->id, token));
                    finish();
                    break;
                case 3:
                    output->state = Api::BindingCaptureState::TimedOut;
                    Copy(output->detail, axis ? "No axis moved." : "Nothing was pressed.");
                    EvidenceLog::Event("flight_binding_capture", std::format("timed_out control={}", row->id));
                    finish();
                    break;
                default:
                    output->state = Api::BindingCaptureState::Idle;
                    break;
                }
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl CancelCapture(void* context, const char* rawId) noexcept
        {
            if (!context || !rawId) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                page.axisCapture.Cancel();
                page.buttonCapture.Cancel();
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl Apply(void* context) noexcept
        {
            if (!context) return Api::Result::InvalidArgument;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                if (page.draft.empty()) return Api::Result::Ok;
                auto& settings = Rt::ControlSettings::Instance();
                for (const auto& [id, text] : page.draft) {
                    if (const auto* row = Find(page, id)) settings.Set(row->section, row->key, text);
                }
                const auto count = page.draft.size();
                page.draft.clear();
                SyncViews(page);
                const bool saved = settings.Save();
                // The loop picks the change up even if the file couldn't be written.
                settings.Publish();
                EvidenceLog::Event("flight_settings_applied", std::format("page={} changed={} saved={}", page.pageId, count, saved ? 1 : 0));
                return saved ? Api::Result::Ok : Api::Result::WriteFailure;
            } catch (...) {
                return Api::Result::WriteFailure;
            }
        }

        void __cdecl Cancel(void* context) noexcept
        {
            if (!context) return;
            try {
                auto& page = *static_cast<Page*>(context);
                const std::scoped_lock lock{ page.mutex };
                page.draft.clear();
                SyncViews(page);
            } catch (...) {
            }
        }

        Row Header(std::string id, std::string label, std::string description)
        {
            return { Kind::Header, std::move(id), std::move(label), std::move(description) };
        }

        // The rows every stick axis has: its binding and its shaping. The section is the
        // one the axis has in the settings file.
        void AddAxis(std::vector<Row>& rows, std::string_view id, std::string_view title, std::string_view about,
            std::string_view section, double deadzoneDefault, int sensitivityMaximum = 200)
        {
            const std::string name(id), where(section);
            rows.push_back(Header(name + "-group", std::string(title), std::string(about)));
            rows.push_back({ Kind::Axis, name + "-axis", "Axis", "Move the axis you want to use, or click its bar above. Delete clears it.", where, "Axis" });
            rows.push_back({ Kind::Toggle, name + "-invert", "Invert", "Swap the axis's direction.", where, "Invert" });
            rows.push_back({ Kind::Percent, name + "-sensitivity", "Sensitivity (%)", "Scales the output. 100 is one to one.",
                where, "Sensitivity", 100, 10, sensitivityMaximum, 5 });
            rows.push_back({ Kind::Percent, name + "-deadzone", "Deadzone (%)", "Travel around the centre that does nothing.",
                where, "Deadzone", deadzoneDefault, 0, 50, 1 });
            rows.push_back({ Kind::Percent, name + "-saturation", "Full output at (%)", "Travel at which the output reaches full. Lower it for a shorter throw.",
                where, "FullOutputAt", 100, 5, 100, 5 });
        }

        Row SetFromLever(std::string id, std::string label, std::string description, std::string target)
        {
            Row row{ Kind::Action, std::move(id), std::move(label), std::move(description) };
            row.target = std::move(target);
            return row;
        }

        std::vector<Row> ThrottleRows()
        {
            std::vector<Row> rows;
            rows.push_back(Header("throttle-group", "Lever", "A lever that sets thrust by its position. The graph above shows where it is and how its travel is divided."));
            rows.push_back({ Kind::Axis, "throttle-axis", "Axis", "Move the lever you want to use. Delete clears it and gives the throttle back to the keys.",
                "Throttle", "Axis" });
            rows.push_back({ Kind::Toggle, "throttle-invert", "Invert", "Swap which end of the lever is full thrust.", "Throttle", "Invert" });
            rows.push_back({ Kind::Percent, "throttle-idle", "Idle band (%)", "Travel at the bottom of the lever that stays at zero thrust. Not used with the reverse zone on.",
                "Throttle", "IdleBand", 5, 0, 50, 1 });
            rows.push_back(SetFromLever("throttle-idle-set", "Set idle band from lever", "Rest the lever where thrust should begin, then choose this.", "throttle-idle"));
            rows.push_back({ Kind::Percent, "throttle-saturation", "Full thrust at (%)", "How much of the travel above the idle band it takes to reach full thrust.",
                "Throttle", "FullThrustAt", 100, 5, 100, 5 });

            rows.push_back(Header("reverse-group", "Reverse zone", "The bottom of the lever reverses the ship once it has slowed. With it on, the lever's middle is half thrust."));
            rows.push_back({ Kind::Toggle, "zone-reverse", "Reverse zone", "Pulling the lever below this zone reverses the ship.", "Throttle", "ReverseZone" });
            rows.push_back(SetFromLever("zone-reverse-set", "Set zone from lever", "Hold the lever where the zone should sit, then choose this. You can also drag its marker on the graph.", "zone-reverse-at"));
            rows.push_back({ Kind::Percent, "zone-reverse-at", "Zone centre (%)", "Where the zone sits, as travel from the bottom of the lever.",
                "Throttle", "ReverseZoneCentre", Rt::kDefaultReverseZoneCentre, 0, 50, 1 });
            rows.push_back({ Kind::Percent, "zone-reverse-width", "Zone half-width (%)", "Reverse starts this far below the centre, and thrust this far above it.",
                "Throttle", "ReverseZoneWidth", Rt::kDefaultReverseZoneWidth, 0, 25, 1 });

            rows.push_back(Header("boost-group", "Boost zone", "The top of the lever presses your booster key."));
            rows.push_back({ Kind::Toggle, "zone-boost", "Boost zone", "Pushing the lever past this zone fires the boosters.", "Throttle", "BoostZone" });
            rows.push_back(SetFromLever("zone-boost-set", "Set zone from lever", "Hold the lever where boost should begin to feel right, then choose this. You can also drag its marker on the graph.", "zone-boost-at"));
            rows.push_back({ Kind::Percent, "zone-boost-at", "Zone centre (%)", "Where the zone sits, as travel from the bottom of the lever.",
                "Throttle", "BoostZoneCentre", Rt::kDefaultBoostZoneCentre, 50, 100, 1 });
            rows.push_back({ Kind::Percent, "zone-boost-width", "Zone half-width (%)", "Boosters fire this far above the centre. With the reverse zone on, full thrust is reached this far below it.",
                "Throttle", "BoostZoneWidth", Rt::kDefaultBoostZoneWidth, 0, 25, 1 });
            return rows;
        }

        std::vector<Row> AxisRows()
        {
            std::vector<Row> rows;
            AddAxis(rows, "pitch", "Pitch", "Nose up and down.", "Pitch", 0);
            AddAxis(rows, "yaw", "Yaw", "Nose left and right.", "Yaw", 0);
            AddAxis(rows, "roll", "Roll", "Works together with sideways strafe.", "Roll", 0);
            AddAxis(rows, "strafe-side", "Strafe sideways", "Slide left and right. Your flight mode key is held for you while you strafe.", "StrafeSideways", 5);
            AddAxis(rows, "strafe-up", "Strafe up and down", "Slide up and down.", "StrafeVertical", 5);
            return rows;
        }

        std::vector<Row> ReticleRows()
        {
            std::vector<Row> rows;
            rows.push_back(Header("reticle-about", "Reticle", "What places the aiming reticle. Anything but the mouse also stops a bumped mouse shifting it."));
            Row mode{ Kind::Choice, "reticle-mode", "Reticle follows",
                "The mouse: left to the game. Flight stick: pitch and yaw move the reticle and the ship follows it, like a gamepad stick. "
                "Its own axes: a mini stick moves it while the flight stick steers. Held centred: it stays in the middle. "
                "With AbsoluteZero installed the mouse keeps it.",
                "Reticle", "Mode" };
            mode.choices = { { "Mouse", "The mouse" }, { "FlightStick", "Flight stick" }, { "OwnAxes", "Its own axes" }, { "Centred", "Held centred" } };
            mode.maximum = static_cast<int>(mode.choices.size()) - 1;
            rows.push_back(std::move(mode));
            rows.push_back({ Kind::Percent, "reticle-stick-sensitivity", "Flight stick reach (%)",
                "Used with Flight stick. Above 100 the reticle reaches its edge before the stick reaches its stop.",
                "Reticle", "StickSensitivity", 100, 10, 400, 5 });
            AddAxis(rows, "aim-yaw", "Reticle left and right", "Used with Its own axes.", "AimYaw", 4, 400);
            AddAxis(rows, "aim-pitch", "Reticle up and down", "Used with Its own axes.", "AimPitch", 4, 400);
            return rows;
        }

        std::vector<Row> ButtonRows()
        {
            std::vector<Row> rows;
            rows.push_back(Header("buttons-about", "Ship buttons", "Each button presses the key the game has for that action, so your in-game key bindings still decide what it does."));
            rows.push_back({ Kind::Toggle, "buttons-enabled", "Enabled", "Turn every ship button off without losing the bindings.",
                "ShipButtons", "Enabled", 1.0 });

            struct Group
            {
                ShipActionGroup group;
                std::string_view id, title, about;
            };
            constexpr std::array<Group, 5> kGroups{ {
                { ShipActionGroup::WeaponsCombat, "weapons", "Weapons", "Fire each weapon group." },
                { ShipActionGroup::FlightSystems, "flight", "Flight systems", "Boosters, flight mode, targeting, scanner and repair." },
                { ShipActionGroup::NavigationContext, "systems", "Power and targets", "Move power between systems and pick targets." },
                { ShipActionGroup::Camera, "camera", "Camera", "Point of view and zoom." },
                { ShipActionGroup::CockpitDocking, "cockpit", "Cockpit and docking", "Take off, get up and leave the ship." },
            } };
            for (const auto& group : kGroups) {
                rows.push_back(Header(std::string(group.id) + "-group", std::string(group.title), std::string(group.about)));
                // The game's own order within each group.
                for (const auto actionId : kNativeShipButtonActions) {
                    const auto* action = FindShipAction(actionId);
                    if (!action || action->group != group.group) continue;
                    const auto index = static_cast<std::size_t>(action - kShipActionCatalog.data());
                    const bool targetButton = actionId == "PreviousTargetSystem" || actionId == "NextTargetSystem";
                    if (actionId == "PreviousTargetSystem") {
                        Row choice{ Kind::Choice, "target-cycle", "Cycle target system with",
                            "How you pick a target's system in Targeting Mode. Power buttons: Previous and Next System do both there. "
                            "An axis: push it for one step, then let it centre. Own buttons: the two rows below.",
                            "ShipButtons", "CycleTargetSystemWith" };
                        choice.choices = { { "PowerButtons", "Power buttons" }, { "Roll", "Roll axis" }, { "Yaw", "Yaw axis" },
                            { "StrafeSideways", "Strafe sideways axis" }, { "OwnButtons", "Own buttons" } };
                        // The menu treats a choice as a range of whole numbers, one per option.
                        choice.maximum = static_cast<int>(choice.choices.size()) - 1;
                        rows.push_back(std::move(choice));
                    }
                    rows.push_back({ Kind::Button, "button-" + std::string(action->actionId), std::string(action->displayLabel),
                        targetButton ? "Only used when Cycle target system with is set to Own buttons. Press the button or hat you want to use. Delete clears it." :
                            "Press the button or hat you want to use. Delete clears it.",
                        "ShipButtons", std::string(Rt::ShipSettingKey(index)) });
                }
            }
            return rows;
        }
        Row Line(std::string id, std::string label, std::string description, std::string text)
        {
            Row row{ Kind::Text, std::move(id), std::move(label), std::move(description) };
            row.maximum = 255;
            row.target = std::move(text);
            return row;
        }

        // Where the settings came from, what an import couldn't bring, and the way out.
        std::vector<Row> SettingsRows()
        {
            std::vector<Row> rows;
            const auto report = Rt::ControlSettings::Instance().Report();
            rows.push_back(Header("settings-about", "Settings file", "One file, AbsoluteControl.ini, beside the plugin."));
            if (report.imported) {
                rows.push_back(Line("import-from", "Imported from", "Your AbsoluteHOTAS settings were copied in once. Those files are not read again, so editing them changes nothing.",
                    report.date.empty() ? report.from : std::format("{} on {}", report.from, report.date)));
                rows.push_back(Line("import-result", "Result", "Bindings and tuning for features Absolute Control has were carried over.",
                    report.dropped.empty() ? "Everything was carried over" :
                    std::format("{} not carried over", report.dropped.size())));
                // The menu shows the first of them; all of them are in the file's [Import] section.
                constexpr std::size_t kShown = 12;
                for (std::size_t index = 0; index < report.dropped.size() && index < kShown; ++index) {
                    // The box is narrow, so it shows the setting's name. The whole line is
                    // the description, read with the row selected.
                    const auto& line = report.dropped[index];
                    const auto colon = line.find(": ");
                    rows.push_back(Line(std::format("import-line-{}", index + 1), std::format("Not carried {}", index + 1),
                        line + ".", colon == std::string::npos ? line : line.substr(0, colon)));
                }
            } else {
                rows.push_back(Line("import-from", "Set up", "Nothing was imported from AbsoluteHOTAS.", "In this menu"));
            }
            rows.push_back(Header("fresh-group", "Start fresh", "More reliable than repairing old settings."));
            rows.push_back({ Kind::Reset, "settings-reset", "Reset flight settings", "Clears every binding and setting on the Flight pages, back to a new install. Walk speed is not changed. This can't be undone." });
            return rows;
        }

        Api::ControlDescriptorV1 Describe(const Row& row)
        {
            Api::ControlDescriptorV1 control;
            switch (row.kind) {
            case Kind::Header: control.kind = Api::ControlKind::GroupHeader; break;
            case Kind::Axis:
            case Kind::Button:
                control.kind = Api::ControlKind::InputBinding;
                control.flags = Api::kBindingController | Api::kBindingClearable;
                break;
            case Kind::Toggle: control.kind = Api::ControlKind::Toggle; break;
            case Kind::Percent: control.kind = Api::ControlKind::IntegerSlider; break;
            case Kind::Choice: control.kind = Api::ControlKind::Choice; break;
            case Kind::Action:
                control.kind = Api::ControlKind::Action;
                control.flags = Api::kControlMutatesDraft;
                break;
            case Kind::Text:
                control.kind = Api::ControlKind::TextInput;
                control.flags = Api::kControlReadOnly;
                break;
            case Kind::Reset:
                control.kind = Api::ControlKind::Action;
                control.flags = Api::kControlRequiresConfirmation;
                break;
            }
            Copy(control.controlId, row.id);
            Copy(control.label, row.label);
            Copy(control.description, row.description);
            control.minimumValue = row.minimum;
            control.maximumValue = row.maximum;
            control.stepValue = row.step;
            return control;
        }

        template <class MenuApi>
        void RegisterPage(const MenuApi& api, Page& page)
        {
            page.controls.clear();
            for (const auto& row : page.rows) page.controls.push_back(Describe(row));
            Api::PageDescriptorV1 descriptor;
            Copy(descriptor.moduleId, kModuleId);
            Copy(descriptor.pageId, page.pageId);
            Copy(descriptor.group, page.group);
            Copy(descriptor.displayName, page.name);
            Copy(descriptor.description, page.description);
            descriptor.context = &page;
            descriptor.readValue = &ReadValue;
            descriptor.writeDraft = &WriteDraft;
            descriptor.invokeAction = &InvokeAction;
            descriptor.readChoiceOptions = &ReadChoices;
            descriptor.beginBindingCapture = &BeginCapture;
            descriptor.pollBindingCapture = &PollCapture;
            descriptor.cancelBindingCapture = &CancelCapture;
            descriptor.apply = &Apply;
            descriptor.cancel = &Cancel;
            descriptor.controls = page.controls.data();
            descriptor.controlCount = static_cast<std::uint32_t>(page.controls.size());
            const auto result = api.registerPage(&descriptor);
            EvidenceLog::Event(result == Api::Result::Ok ? "flight_page_registered" : "flight_page_failed",
                std::format("page={} controls={} result={}", page.pageId, page.controls.size(), static_cast<std::uint32_t>(result)));
        }
    }

    void Register() noexcept
    {
        try {
            const auto* api = AbsoluteControlPanel_QueryApi(Api::kAbiVersion);
            if (!api || !api->registerPage) {
                EvidenceLog::Event("flight_page_failed", "menu API unavailable");
                return;
            }
            // The menu keeps pointers into these for as long as the game runs.
            static Page throttle{ "throttle", "Throttle", "Bind the lever and set its zones by feel.", "Flight", ThrottleRows() };
            static Page axes{ "flight-axes", "Flight Axes", "Bind and shape pitch, yaw, roll and strafe.", "Flight", AxisRows() };
            static Page reticle{ "reticle", "Reticle", "Choose what moves the aiming reticle.", "Flight", ReticleRows() };
            static Page buttons{ "ship-buttons", "Ship Buttons", "Bind buttons and hats to ship actions.", "Flight", ButtonRows() };
            static Page settings{ "settings-file", "Settings File", "Where your settings came from, and how to start fresh.", "Setup", SettingsRows() };
            g_pages = { &throttle, &axes, &reticle, &buttons, &settings };
            RegisterPage(*api, throttle);
            RegisterPage(*api, axes);
            RegisterPage(*api, reticle);
            RegisterPage(*api, buttons);
            RegisterPage(*api, settings);
            {
                const std::scoped_lock lock{ throttle.mutex, axes.mutex, reticle.mutex };
                SyncViews(throttle);
                SyncViews(axes);
                SyncViews(reticle);
            }

            // Live graphs: the lever with its zones, pinned above the Throttle page, and a
            // bar for each stick axis on Flight Axes.
            const auto* live = AbsoluteControlPanel_QueryLiveComponentsExperimental(Live::kAbiVersion);
            if (!live || !live->registerLiveChannel) {
                EvidenceLog::Event("flight_graph_failed", "live component API unavailable");
                return;
            }
            const auto addGraph = [&](std::string_view pageId, std::string_view channelId, std::string_view title,
                double minimum, const Range& range, Live::ReadLiveFrameCallback read, void* context, std::uint32_t flags) {
                Live::LiveChannelDescriptorV1 channel;
                Copy(channel.moduleId, kModuleId);
                Copy(channel.pageId, pageId);
                Copy(channel.channelId, channelId);
                Copy(channel.title, title);
                channel.kind = Live::ComponentKind::RangeMeter;
                channel.rangeMeter.minimumValue = minimum;
                channel.rangeMeter.maximumValue = 100;
                channel.rangeMeter.bandCount = range.bandCount;
                channel.rangeMeter.markerCount = range.markerCount;
                std::memcpy(channel.rangeMeter.bands, range.bands, sizeof(range.bands));
                std::memcpy(channel.rangeMeter.markers, range.markers, sizeof(range.markers));
                channel.context = context;
                channel.readLiveFrame = read;
                channel.flags = flags;
                const auto result = live->registerLiveChannel(&channel);
                EvidenceLog::Event(result == Live::Result::Ok ? "flight_graph_registered" : "flight_graph_failed",
                    std::format("page={} graph={} result={}", pageId, channelId, static_cast<std::uint32_t>(result)));
            };
            addGraph("throttle", "lever", "Lever travel", 0, ThrottleRange(), &ReadThrottleFrame, nullptr, Live::kLivePresentationPinned);
            for (auto& stick : g_sticks) {
                addGraph(stick.page, std::string(stick.prefix) + "-bar", stick.title, -100, StickRange(stick), &ReadStickFrame, &stick,
                    Live::kLivePresentationNone);
            }
        } catch (...) {
            EvidenceLog::Event("flight_page_failed", "exception during registration");
        }
    }
}
