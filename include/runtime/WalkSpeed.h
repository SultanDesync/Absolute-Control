#pragma once

#include "runtime/ShipBindings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace AbsoluteControl::Runtime
{
    enum class WheelModifier : std::uint32_t { Off, Alt, Shift, Ctrl };
    enum class SpeedKeys : std::uint32_t { Off, Numpad, Brackets, PageKeys };
    enum class WalkSpeedSource { Virtual, Axis };

    struct WalkSpeedSettings
    {
        bool enabled{};
        std::string axis; // A binding token such as Device Name@0x32. Empty means no axis.
        bool invertAxis{};
        bool showGauge{ true };
        WheelModifier wheelModifier{ WheelModifier::Alt };
        SpeedKeys speedKeys{ SpeedKeys::Numpad };
        double step{ 0.05 };
        double speed{ 1.0 }; // The saved virtual axis.
    };

    [[nodiscard]] WalkSpeedSettings ParseWalkSpeedSettings(const IniValues& ini);
    [[nodiscard]] std::string FormatWalkSpeedSettings(const WalkSpeedSettings& settings);

    // At full speed the game's own key handling runs untouched, so it plays exactly as vanilla.
    inline constexpr double kFullSpeed = 0.995;

    [[nodiscard]] inline double ClampSpeed(double speed) noexcept
    {
        return std::isfinite(speed) ? std::clamp(speed, 0.0, 1.0) : 1.0;
    }

    // Bus axes run -1 to 1. Walk speed runs 0 to 1 across the axis's full travel.
    [[nodiscard]] inline double AxisToSpeed(double value, bool invert) noexcept
    {
        return ClampSpeed(((invert ? -value : value) + 1.0) / 2.0);
    }

    // Whichever moved last sets the speed, so a parked axis can't overwrite scrolling.
    // The axis takes charge when it first appears, and again once it moves past kAxisPickup.
    class WalkSpeedSelector
    {
    public:
        static constexpr double kAxisPickup = 0.02;

        explicit WalkSpeedSelector(double savedSpeed = 1.0) noexcept : virtual_(ClampSpeed(savedSpeed)) {}

        void Nudge(double delta) noexcept
        {
            virtual_ = ClampSpeed(Speed() + delta);
            source_ = WalkSpeedSource::Virtual;
        }

        void SetVirtual(double speed) noexcept
        {
            virtual_ = ClampSpeed(speed);
            source_ = WalkSpeedSource::Virtual;
        }

        void ObserveAxis(std::optional<double> axisSpeed) noexcept
        {
            if (!axisSpeed) {
                // Losing the axis keeps the current speed rather than jumping.
                if (source_ == WalkSpeedSource::Axis) virtual_ = axis_;
                source_ = WalkSpeedSource::Virtual;
                seen_ = false;
                return;
            }
            const auto speed = ClampSpeed(*axisSpeed);
            if (!seen_ || std::abs(speed - parked_) >= kAxisPickup) {
                source_ = WalkSpeedSource::Axis;
                parked_ = speed;
                seen_ = true;
            }
            if (source_ == WalkSpeedSource::Axis) {
                axis_ = speed;
                parked_ = speed;
            }
        }

        [[nodiscard]] double Speed() const noexcept
        {
            return source_ == WalkSpeedSource::Axis ? axis_ : virtual_;
        }
        [[nodiscard]] WalkSpeedSource Source() const noexcept { return source_; }
        [[nodiscard]] double Virtual() const noexcept { return virtual_; }

    private:
        double virtual_{ 1.0 };
        double axis_{ 1.0 };
        double parked_{};
        bool seen_{};
        WalkSpeedSource source_{ WalkSpeedSource::Virtual };
    };

    struct MoveKeys
    {
        bool forward{};
        bool back{};
        bool left{};
        bool right{};
    };

    struct StickVector
    {
        float x{};
        float y{};
    };

    // Opposing keys cancel and diagonals don't gain speed, as on a real stick.
    [[nodiscard]] inline StickVector WalkStick(const MoveKeys& keys, double speed) noexcept
    {
        float x = (keys.right ? 1.0F : 0.0F) - (keys.left ? 1.0F : 0.0F);
        float y = (keys.forward ? 1.0F : 0.0F) - (keys.back ? 1.0F : 0.0F);
        const auto length = std::sqrt(x * x + y * y);
        if (length > 1.0F) {
            x /= length;
            y /= length;
        }
        const auto scale = static_cast<float>(ClampSpeed(speed));
        return { x * scale, y * scale };
    }

    // A tap moves one step. After a short hold, the key sweeps through the range.
    class SpeedKeyRepeat
    {
    public:
        static constexpr std::uint64_t kHoldDelayNanoseconds = 250'000'000;
        static constexpr double kSweepPerSecond = 0.5;

        [[nodiscard]] double Update(bool down, std::uint64_t now, double step) noexcept
        {
            double delta{};
            if (down && !down_) {
                delta = step;
                pressedAt_ = now;
            } else if (down && now - pressedAt_ >= kHoldDelayNanoseconds) {
                delta = kSweepPerSecond * static_cast<double>(now - last_) / 1e9;
            }
            down_ = down;
            last_ = now;
            return delta;
        }

    private:
        bool down_{};
        std::uint64_t pressedAt_{};
        std::uint64_t last_{};
    };

    // The on-screen gauge fades in when the speed changes, holds while the speed stays put,
    // then fades out. Small wobble from a parked axis doesn't wake it.
    class SpeedGauge
    {
    public:
        static constexpr double kChangeThreshold = 0.01;
        static constexpr std::uint64_t kFadeInNanoseconds = 150'000'000;
        static constexpr std::uint64_t kHoldNanoseconds = 2'000'000'000;
        static constexpr std::uint64_t kFadeOutNanoseconds = 500'000'000;

        // While not allowed, such as in a menu, it stays hidden and speed changes don't count.
        void Update(double speed, bool allowed, std::uint64_t now) noexcept
        {
            const auto elapsed = static_cast<double>(now - last_);
            last_ = now;
            if (!allowed || !seen_) {
                anchor_ = speed;
                seen_ = true;
                awake_ = false;
                alpha_ = 0;
                return;
            }
            if (std::abs(speed - anchor_) >= kChangeThreshold) {
                anchor_ = speed;
                changedAt_ = now;
                awake_ = true;
            }
            if (!awake_) return;
            if (now - changedAt_ < kHoldNanoseconds) {
                alpha_ = (std::min)(1.0, alpha_ + elapsed / static_cast<double>(kFadeInNanoseconds));
            } else {
                alpha_ = (std::max)(0.0, alpha_ - elapsed / static_cast<double>(kFadeOutNanoseconds));
                awake_ = alpha_ > 0;
            }
        }

        [[nodiscard]] double Alpha() const noexcept { return alpha_; }
        [[nodiscard]] bool Visible() const noexcept { return awake_; }

    private:
        double anchor_{};
        double alpha_{};
        std::uint64_t changedAt_{};
        std::uint64_t last_{};
        bool seen_{};
        bool awake_{};
    };
}
