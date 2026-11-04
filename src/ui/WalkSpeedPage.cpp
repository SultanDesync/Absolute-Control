#include "ui/WalkSpeedPage.h"

#include "AbsoluteControlPanelAPI.h"
#include "EvidenceLog.h"
#include "runtime/BackendRuntime.h"
#include "runtime/WalkSpeedRuntime.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControlPanelResearch::Ui::WalkSpeedPage
{
    namespace
    {
        namespace Api = AbsoluteControlPanelApi;
        namespace Walk = AbsoluteControl::Runtime;

        constexpr std::string_view kModuleId = "absolute.control";
        constexpr std::string_view kPageId = "walk-speed";

        struct AxisOption
        {
            std::string label;
            std::string token;
        };

        std::mutex g_mutex;
        Walk::WalkSpeedSettings g_draft;

        template <std::size_t N>
        void Copy(char (&destination)[N], std::string_view source) noexcept
        {
            const auto count = (std::min)(source.size(), N - 1);
            std::memcpy(destination, source.data(), count);
            destination[count] = '\0';
        }

        Api::ControlDescriptorV1 Control(Api::ControlKind kind, std::uint32_t flags, std::string_view id,
            std::string_view label, std::string_view description, double minimum = 0, double maximum = 0,
            double step = 0) noexcept
        {
            Api::ControlDescriptorV1 result;
            result.kind = kind;
            result.flags = flags;
            Copy(result.controlId, id);
            Copy(result.label, label);
            Copy(result.description, description);
            result.minimumValue = minimum;
            result.maximumValue = maximum;
            result.stepValue = step;
            return result;
        }

        const std::array kControls{
            Control(Api::ControlKind::GroupHeader, Api::kControlNone, "walk-about", "Analog walk speed",
                "Movement keys act like a stick held at this speed."),
            Control(Api::ControlKind::Toggle, Api::kControlNone, "walk-enabled", "Enabled",
                "On foot only. At full speed the game's own key handling runs, so running and sprinting are unchanged."),
            Control(Api::ControlKind::Toggle, Api::kControlNone, "walk-gauge", "Show speed gauge",
                "Fades in on screen when the speed changes, and out again once it holds steady."),
            Control(Api::ControlKind::TextInput, Api::kControlReadOnly, "walk-status", "Current speed",
                "The speed now, and which control set it. The one moved last is in charge.", 0, 255, 1),
            Control(Api::ControlKind::GroupHeader, Api::kControlNone, "walk-axis-group", "Axis",
                "A controller axis that sets the speed across its full travel."),
            Control(Api::ControlKind::Choice, Api::kControlNone, "walk-axis", "Speed axis",
                "Connected controller axes.", 0, 255, 1),
            Control(Api::ControlKind::Toggle, Api::kControlNone, "walk-invert", "Invert axis",
                "Swap which end of the axis is full speed."),
            Control(Api::ControlKind::GroupHeader, Api::kControlNone, "walk-adjust-group", "Wheel and keys",
                "Nudge a saved speed without an axis. Moving the axis takes over again."),
            Control(Api::ControlKind::Choice, Api::kControlNone, "walk-wheel", "Adjust with wheel",
                "Hold this and scroll. Shift also sprints while moving, and Ctrl also sneaks.", 0, 3, 1),
            Control(Api::ControlKind::Choice, Api::kControlNone, "walk-keys", "Speed keys",
                "Tap for one step, or hold to sweep. The game still sees these keys.", 0, 3, 1),
            Control(Api::ControlKind::IntegerSlider, Api::kControlNone, "walk-step", "Step size (%)",
                "How far one wheel notch or key tap moves the speed.", 1, 25, 1)
        };

        std::string Trimmed(const char* text)
        {
            std::string_view view{ text };
            const auto first = view.find_first_not_of(' ');
            if (first == std::string_view::npos) return {};
            return std::string(view.substr(first, view.find_last_not_of(' ') - first + 1));
        }

        // Rebuilt on each read, so it follows controllers being plugged in. Index 0 is no axis.
        std::vector<AxisOption> AxisOptions(const std::string& current)
        {
            std::vector<AxisOption> options{ { "None", "" } };
            const auto snapshot = Walk::NativeBackend().Devices().Snapshot();
            for (const auto& description : snapshot.devices) {
                const auto name = Trimmed(description.device.displayName);
                for (const auto& channel : description.channels) {
                    std::uint32_t axis{};
                    if (channel.shape != Walk::DeviceDraft::Shape::Scalar ||
                        ::sscanf_s(channel.identity.channel, "axis/%u", &axis) != 1 || axis > 7) continue;
                    options.push_back({ std::format("{} · {}", name, channel.displayName),
                        std::format("{}@0x{:02X}", name, 0x30 + axis) });
                }
            }
            if (!current.empty() && std::ranges::none_of(options, [&](const AxisOption& option) {
                    return option.token == current; })) {
                options.push_back({ std::format("{} (not connected)", current), current });
            }
            return options;
        }

        Api::Result __cdecl ReadValue(void*, const char* rawId, Api::ValueV1* output) noexcept
        {
            if (!rawId || !output || output->structSize < sizeof(Api::ValueV1)) return Api::Result::InvalidArgument;
            try {
                const std::string_view id{ rawId };
                const std::scoped_lock lock{ g_mutex };
                if (id == "walk-enabled" || id == "walk-invert" || id == "walk-gauge") {
                    output->kind = Api::ValueKind::Boolean;
                    output->booleanValue = (id == "walk-enabled" ? g_draft.enabled :
                        id == "walk-invert" ? g_draft.invertAxis : g_draft.showGauge) ? 1 : 0;
                } else if (id == "walk-wheel") {
                    output->kind = Api::ValueKind::Integer;
                    output->integerValue = static_cast<std::int64_t>(g_draft.wheelModifier);
                } else if (id == "walk-keys") {
                    output->kind = Api::ValueKind::Integer;
                    output->integerValue = static_cast<std::int64_t>(g_draft.speedKeys);
                } else if (id == "walk-step") {
                    output->kind = Api::ValueKind::Integer;
                    output->integerValue = std::lround(g_draft.step * 100.0);
                } else if (id == "walk-axis") {
                    const auto options = AxisOptions(g_draft.axis);
                    const auto found = std::ranges::find(options, g_draft.axis, &AxisOption::token);
                    output->kind = Api::ValueKind::Integer;
                    output->integerValue = found - options.begin();
                } else if (id == "walk-status") {
                    const auto status = Walk::WalkSpeedRuntime::CurrentStatus();
                    output->kind = Api::ValueKind::String;
                    Copy(output->stringValue, std::format("{:.0f}% · {}", status.speed * 100.0,
                        status.source == Walk::WalkSpeedSource::Axis ? "axis" : "wheel and keys"));
                } else {
                    return Api::Result::NotFound;
                }
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl WriteDraft(void*, const char* rawId, const Api::ValueV1* value) noexcept
        {
            if (!rawId || !value || value->structSize < sizeof(Api::ValueV1)) return Api::Result::InvalidArgument;
            try {
                const std::string_view id{ rawId };
                const std::scoped_lock lock{ g_mutex };
                const auto integer = value->integerValue;
                if ((id == "walk-enabled" || id == "walk-invert" || id == "walk-gauge") && value->kind == Api::ValueKind::Boolean) {
                    (id == "walk-enabled" ? g_draft.enabled : id == "walk-invert" ? g_draft.invertAxis : g_draft.showGauge) =
                        value->booleanValue != 0;
                } else if (id == "walk-wheel" && value->kind == Api::ValueKind::Integer && integer >= 0 && integer <= 3) {
                    g_draft.wheelModifier = static_cast<Walk::WheelModifier>(integer);
                } else if (id == "walk-keys" && value->kind == Api::ValueKind::Integer && integer >= 0 && integer <= 3) {
                    g_draft.speedKeys = static_cast<Walk::SpeedKeys>(integer);
                } else if (id == "walk-step" && value->kind == Api::ValueKind::Integer && integer >= 1 && integer <= 25) {
                    g_draft.step = static_cast<double>(integer) / 100.0;
                } else if (id == "walk-axis" && value->kind == Api::ValueKind::Integer) {
                    const auto options = AxisOptions(g_draft.axis);
                    if (integer < 0 || integer >= static_cast<std::int64_t>(options.size())) return Api::Result::InvalidArgument;
                    g_draft.axis = options[static_cast<std::size_t>(integer)].token;
                } else {
                    return Api::Result::InvalidArgument;
                }
                return Api::Result::Ok;
            } catch (...) {
                return Api::Result::Rejected;
            }
        }

        void Fill(Api::ChoiceOptionV1* options, std::span<const std::string_view> labels) noexcept
        {
            for (std::size_t index = 0; index < labels.size(); ++index) {
                options[index].value = static_cast<std::int64_t>(index);
                Copy(options[index].label, labels[index]);
            }
        }

        Api::Result __cdecl ReadChoices(void*, const char* rawId, Api::ChoiceOptionV1* options,
            std::uint32_t capacity, std::uint32_t* outputCount) noexcept
        {
            if (!rawId || !options || !outputCount) return Api::Result::InvalidArgument;
            try {
                const std::string_view id{ rawId };
                if (id == "walk-wheel" || id == "walk-keys") {
                    static constexpr std::array<std::string_view, 4> kWheel{ "Off", "Alt + wheel", "Shift + wheel", "Ctrl + wheel" };
                    static constexpr std::array<std::string_view, 4> kKeys{ "Off", "Numpad + / -", "] / [", "Page Up / Page Down" };
                    *outputCount = 4;
                    if (capacity < 4) return Api::Result::CapacityExceeded;
                    Fill(options, id == "walk-wheel" ? std::span<const std::string_view>{ kWheel } : std::span<const std::string_view>{ kKeys });
                    return Api::Result::Ok;
                }
                if (id != "walk-axis") return Api::Result::NotFound;
                const std::scoped_lock lock{ g_mutex };
                const auto axes = AxisOptions(g_draft.axis);
                *outputCount = static_cast<std::uint32_t>(axes.size());
                if (capacity < axes.size()) return Api::Result::CapacityExceeded;
                for (std::size_t index = 0; index < axes.size(); ++index) {
                    options[index].value = static_cast<std::int64_t>(index);
                    Copy(options[index].label, axes[index].label);
                }
                return Api::Result::Ok;
            } catch (...) {
                *outputCount = 0;
                return Api::Result::Rejected;
            }
        }

        Api::Result __cdecl Apply(void*) noexcept
        {
            try {
                const std::scoped_lock lock{ g_mutex };
                return Walk::WalkSpeedRuntime::ApplySettings(g_draft) ? Api::Result::Ok : Api::Result::WriteFailure;
            } catch (...) {
                return Api::Result::WriteFailure;
            }
        }

        void __cdecl Cancel(void*) noexcept
        {
            try {
                const std::scoped_lock lock{ g_mutex };
                g_draft = Walk::WalkSpeedRuntime::Settings();
            } catch (...) {
            }
        }
    }

    void Register() noexcept
    {
        try {
            const auto* api = AbsoluteControlPanel_QueryApi(Api::kAbiVersion);
            if (!api || !api->registerPage) {
                EvidenceLog::Event("walk_page_failed", "menu API unavailable");
                return;
            }
            {
                const std::scoped_lock lock{ g_mutex };
                g_draft = Walk::WalkSpeedRuntime::Settings();
            }
            Api::PageDescriptorV1 page;
            Copy(page.moduleId, kModuleId);
            Copy(page.pageId, kPageId);
            Copy(page.displayName, "Walk Speed");
            Copy(page.description, "Set on-foot movement speed with an axis, the wheel or keys.");
            page.readValue = &ReadValue;
            page.writeDraft = &WriteDraft;
            page.readChoiceOptions = &ReadChoices;
            page.apply = &Apply;
            page.cancel = &Cancel;
            page.controls = kControls.data();
            page.controlCount = static_cast<std::uint32_t>(kControls.size());
            const auto result = api->registerPage(&page);
            EvidenceLog::Event(result == Api::Result::Ok ? "walk_page_registered" : "walk_page_failed",
                std::format("result={}", static_cast<std::uint32_t>(result)));
        } catch (...) {
            EvidenceLog::Event("walk_page_failed", "exception during registration");
        }
    }
}
