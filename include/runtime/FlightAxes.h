#pragma once

#include "runtime/ShipBindings.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Flight axis maths carried over from AbsoluteHOTAS, so existing settings fly the same.
// Nothing here touches the game or a device: it turns axis positions into the values the
// flight output writes.
namespace AbsoluteControl::Runtime
{
    struct AxisShape
    {
        bool invert{};
        float sensitivity{ 1.0F };
        float saturation{ 1.0F }; // Stick travel that gives full output, 0.05 to 1.
        float deadzone{};         // Stick travel that gives none, 0 to 0.95.
    };

    // Position -1 to 1 in, output -1 to 1: invert, deadzone, a straight ramp up to the
    // saturation point, then sensitivity as a final gain.
    [[nodiscard]] float ShapeAxis(float position, const AxisShape& shape) noexcept;

    // Zone positions are in the INI's units: 0 to 65535 along the lever's travel.
    struct ThrottleConfig
    {
        bool invert{};
        float sensitivity{ 1.0F };
        float saturation{ 1.0F };
        bool unipolar{ true };        // Whole travel is forward thrust.
        float idlePlateau{ 0.05F };   // Bottom share of the travel that stays at zero.
        bool unipolarReverse{};       // The bottom of the travel is a reverse zone.
        long reverseZoneCenter{ 3000 };
        long reverseZoneDeadzone{ 3000 };
        bool boostZone{};             // The top of the travel fires the boosters.
        long boostZoneCenter{ 62000 };
        long boostZoneDeadzone{ 2000 };
        long detentCenter{ 32768 };
        long detentDeadzone{ 500 };
    };

    struct ThrottleSample
    {
        float throttle{}; // 0 to 1.
        bool reverse{};   // The lever is in its reverse zone.
        bool boost{};     // The lever is past its boost zone.
    };

    [[nodiscard]] ThrottleSample EvaluateThrottle(const ThrottleConfig& config, float position) noexcept;

    // A separate axis, such as a toe brake, that holds reverse while pressed.
    struct ReverseAxisConfig
    {
        bool enabled{ true };
        bool invert{};
        float sensitivity{ 1.0F };
        float saturation{ 1.0F };
        float deadzone{ 0.05F };
        float activation{ 0.05F };
    };

    [[nodiscard]] bool ReverseAxisHeld(const ReverseAxisConfig& config, float position) noexcept;

    // What moves the game's aim point, which places the reticle. The mouse moves the same
    // value, so anything but Mouse also stops stray mouse movement shifting the reticle.
    enum class ReticleMode
    {
        Mouse,       // Left to the game.
        FlightStick, // Pitch and yaw move it and the ship follows: aim-driven steering.
        OwnAxes,     // Its own pair of axes, such as a mini stick. The flight stick steers directly.
        Centred      // Held at the centre.
    };

    [[nodiscard]] std::string_view ReticleModeName(ReticleMode mode) noexcept;
    [[nodiscard]] ReticleMode ParseReticleMode(std::string_view text) noexcept;

    struct FlightConfig
    {
        bool enabled{ true };
        ConfiguredBinding throttle, pitch, yaw, roll, strafeLateral, strafeVertical, reverse;
        AxisShape pitchShape, yawShape, rollShape, strafeLateralShape, strafeVerticalShape;
        ThrottleConfig throttleConfig;
        ReverseAxisConfig reverseAxis;
        bool rollEnabled{ true };
        bool holdForBoost{ true };       // The boost key takes the throttle while it's held.
        int throttleBurstMilliseconds{ 250 };
        float reverseGateVelocity{ 5.0F };
        ReticleMode reticle{ ReticleMode::Mouse };
        float aimSensitivity{ 1.0F };    // The flight stick's reach when it moves the reticle.
        ConfiguredBinding aimYaw, aimPitch;
        AxisShape aimYawShape, aimPitchShape;
        std::vector<std::string> ignored; // Settings in the file for features this build lacks.
    };

    // Absolute Control's own file. Positions, widths and sensitivities are percentages:
    //   [General]  Enabled
    //   [Throttle] Axis, Invert, Sensitivity, FullThrustAt, IdleBand, ReverseZone,
    //              ReverseZoneCentre, ReverseZoneWidth, BoostZone, BoostZoneCentre,
    //              BoostZoneWidth, HoldForBoost, ReverseBelowSpeed, ReverseAxis,
    //              ReverseAxisInvert, Centred, CentreDetent, CentreDetentWidth
    //   [Pitch] [Yaw] [Roll] [StrafeSideways] [StrafeVertical]
    //              Axis, Invert, Sensitivity, Deadzone, FullOutputAt; [Roll] also Enabled
    //   [Reticle]  Mode (Mouse, FlightStick, OwnAxes, Centred), StickSensitivity
    //   [AimYaw] [AimPitch]  Axis, Invert, Sensitivity, Deadzone, FullOutputAt
    [[nodiscard]] FlightConfig LoadFlightConfig(const IniValues& ini);
    // Every setting above, so a written file reads back as the same configuration.
    [[nodiscard]] std::vector<SettingEntry> WriteFlightConfig(const FlightConfig& config);
    // AbsoluteHOTAS's files, for the one-time import. Settings for features this build
    // lacks are listed in the result's `ignored`.
    [[nodiscard]] FlightConfig LoadHotasFlightConfig(const IniValues& ini);

    // Where a new setup starts, as percentages of the lever's travel.
    inline constexpr float kDefaultReverseZoneCentre = 7.0F;
    inline constexpr float kDefaultReverseZoneWidth = 3.0F;
    inline constexpr float kDefaultBoostZoneCentre = 95.0F;
    inline constexpr float kDefaultBoostZoneWidth = 2.0F;

    // Axis positions off the bus, -1 to 1. Nothing for an axis that is unbound or missing.
    struct FlightInputs
    {
        std::optional<float> throttle, pitch, yaw, roll, strafeLateral, strafeVertical, reverse, aimYaw, aimPitch;
    };

    struct FlightFrame
    {
        bool throttleBound{};
        float throttle{};
        bool reverse{};
        bool boost{};
        float pitch{}, yaw{};
        bool pitchYawBound{};
        // Roll and sideways strafe share one lane in the game: strafe has it while deflected.
        float lateral{};
        bool lateralActive{};
        float vertical{};
        bool verticalActive{};
        // Past the shared lane the game keeps them apart, which lets both work at once.
        float splitRoll{}, splitStrafe{};
        bool splitActive{};
        bool strafeModifier{}; // Either strafe axis is deflected: hold the flight mode key.
        float aimYaw{}, aimPitch{}; // The reticle's own axes, shaped.
    };

    [[nodiscard]] FlightFrame ComputeFlightFrame(const FlightConfig& config, const FlightInputs& inputs) noexcept;
}
