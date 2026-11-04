#pragma once

#include "runtime/DeviceStore.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Records an axis binding by watching which axis moves, so only axes that really work can
    // be picked. An axis counts once it has stayed away from where it started, which rules out
    // a resting wobble or a single noisy reading. If several qualify, the biggest move wins.
    class AxisCapture
    {
    public:
        enum class State { Idle, Capturing, Captured, TimedOut };

        // On a -1 to 1 axis this is about 12% of travel, the same as HOTAS's flight axis capture.
        static constexpr double kMovement = 0.25;
        static constexpr std::uint64_t kHoldNanoseconds = 80'000'000;
        static constexpr std::uint64_t kTimeoutNanoseconds = 8'000'000'000;

        void Begin(std::uint64_t now);
        void Cancel() noexcept;
        [[nodiscard]] State Poll(const DeviceStore& devices, std::uint64_t now);
        // A binding token, set once Poll returns Captured.
        [[nodiscard]] const std::string& Token() const noexcept { return token_; }

    private:
        using Key = std::pair<std::string, std::string>; // Device physical id and channel.

        State state_{ State::Idle };
        std::uint64_t startedAt_{};
        std::uint64_t movingSince_{};
        std::map<Key, double> start_;
        std::optional<Key> moving_;
        std::vector<DeviceDraft::Sample> buffer_;
        std::string token_;
    };
}
