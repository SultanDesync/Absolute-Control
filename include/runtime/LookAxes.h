#pragma once

#include "runtime/ShipBindings.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

// Looking around the cockpit. Each of yaw, pitch and roll takes up to two bound axes, such
// as a head tracker and a mini stick, and they add together. Ported from Absolute Head
// Tracking, where a tracker angle, a stick and key presses already mixed the same way.
// Nothing here touches the game or a device.
namespace AbsoluteControl::Runtime
{
    enum class LookAxis : std::size_t { Yaw, Pitch, Roll };
    inline constexpr std::size_t kLookAxes = 3;

    // One bound input. What it does depends on what it is bound to: a head tracker's axis
    // reports an angle, which is used as it is (times the sensitivity); a stick's travel is
    // scaled so full travel reaches the axis limit.
    struct LookInputConfig
    {
        ConfiguredBinding binding;
        bool invert{};
        float sensitivity{ 1.0F };
        float deadzone{ 0.1F }; // A stick's travel that does nothing, 0 to 0.95.
    };

    struct LookAxisConfig
    {
        bool enabled{ true };
        LookInputConfig input, second;
        float minimum{}, centre{}, maximum{}; // Degrees.
    };

    struct LookConfig
    {
        bool enabled{ true };
        std::array<LookAxisConfig, kLookAxes> axes;
        float trackerDeadzone{ 0.0F }; // Degrees of head movement that do nothing.
        float smoothing{ 0.15F };      // 0 is none, 0.99 the most.
        ConfiguredBinding recentreButton, toggleButton;
        std::string recentreKey{ "Ctrl+Alt+C" }, toggleKey{ "Ctrl+Alt+T" };
        float firingCone{ 30.0F };     // Degrees either side of the ship's nose. 0 turns the limit off.
    };

    // Absolute Control's own file:
    //   [Look]  Enabled, TrackerDeadzone, Smoothing, RecentreButton, ToggleButton,
    //           RecentreKey, ToggleKey, FiringCone
    //   [LookYaw] [LookPitch] [LookRoll]  Enabled, Axis, Invert, Sensitivity, Deadzone,
    //           SecondAxis, SecondAxisInvert, SecondAxisSensitivity, SecondAxisDeadzone,
    //           Minimum, Centre, Maximum
    // With nothing set, the axes are bound to OpenTrack, so a running tracker just works.
    [[nodiscard]] LookConfig LoadLookConfig(const IniValues& ini);
    [[nodiscard]] std::vector<SettingEntry> WriteLookConfig(const LookConfig& config);
    // Absolute Head Tracking's AbsoluteHeadTracking.ini, for the one-time import.
    [[nodiscard]] LookConfig LoadHeadTrackingLookConfig(const IniValues& ini);

    // A key chord such as Ctrl+Alt+C, as a virtual key and its modifiers. Nothing if unset
    // or unreadable.
    struct KeyChord
    {
        int key{};
        bool control{}, alt{}, shift{};
    };
    [[nodiscard]] std::optional<KeyChord> ParseKeyChord(std::string_view text);

    // One input's reading this tick. An angle input gives its angle in degrees; a stick gives
    // its travel, -1 to 1. Nothing when unbound, disconnected or stale.
    struct LookReading
    {
        std::optional<double> value;
        bool angle{};
    };

    struct LookFrame
    {
        bool active{};                    // Something is bound and reporting.
        std::array<float, kLookAxes> degrees{}; // Smoothed, limited, signed as the game's camera wants.
        std::array<float, kLookAxes> tracker{}; // The angle inputs alone, before limits, for the menu.
    };

    struct Quaternion
    {
        float w{ 1.0F }, x{}, y{}, z{};
    };
    // The game's camera axes: yaw about Z, pitch about X, roll about Y (view forward is +Y),
    // composed yaw, then roll, then pitch, as Head Tracking did.
    [[nodiscard]] Quaternion LookRotation(const std::array<float, kLookAxes>& degrees) noexcept;

    // Holds what carries over between ticks: where each angle input was centred, and the
    // smoothed output.
    class LookMixer
    {
    public:
        // inputs[axis][0] is Axis, [1] is SecondAxis. seconds since the last tick.
        LookFrame Tick(const LookConfig& config, const std::array<std::array<LookReading, 2>, kLookAxes>& inputs,
            float seconds, bool enabled);
        // The next reading of every angle input becomes its centre.
        void Recentre() noexcept;
        [[nodiscard]] bool Centred() const noexcept;

    private:
        std::array<std::array<std::optional<double>, 2>, kLookAxes> centre_{};
        std::array<float, kLookAxes> smoothed_{};
        bool recentre_{};
    };
}
