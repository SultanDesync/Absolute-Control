#pragma once

#include "runtime/DeviceStore.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Records a button binding from the first button or hat direction pressed. Anything
    // already held when recording starts has to be let go first, so the button that opened
    // the recording can't bind itself.
    class ButtonCapture
    {
    public:
        enum class State { Idle, Capturing, Captured, TimedOut };

        static constexpr std::uint64_t kTimeoutNanoseconds = 8'000'000'000;

        void Begin(std::uint64_t now);
        void Cancel() noexcept;
        [[nodiscard]] State Poll(const DeviceStore& devices, std::uint64_t now);
        // A binding token in the form saved bindings use, set once Poll returns Captured.
        [[nodiscard]] const std::string& Token() const noexcept { return token_; }

    private:
        using Key = std::pair<std::string, std::string>; // Device physical id and channel.

        State state_{ State::Idle };
        std::uint64_t startedAt_{};
        std::map<Key, bool> armed_; // Seen released since recording started.
        std::vector<DeviceDraft::Sample> buffer_;
        std::string token_;
    };

    // The number a saved binding uses for a channel: 1 to 128 for buttons, 129 to 144 for
    // the four directions of up to four hats. Zero for anything else.
    [[nodiscard]] std::uint32_t BindingNumberForChannel(std::string_view channel) noexcept;
}
