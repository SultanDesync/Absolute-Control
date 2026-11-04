#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AbsoluteControl::Runtime
{
    struct ButtonSample
    {
        bool valid{};
        bool down{};
        std::uint64_t presses{};
    };

    // Turns sampled buttons into held and released intents. A closed gate or a lost device
    // releases what is held, and a button still down when things recover must be released
    // and pressed again, so it can't fire under a new context.
    class ButtonRouter
    {
    public:
        explicit ButtonRouter(std::size_t count) : states_(count) {}

        template <class Sink>
        void Update(bool gateOpen, std::span<const ButtonSample> samples, Sink&& setHeld)
        {
            for (std::size_t index = 0; index < states_.size() && index < samples.size(); ++index) {
                auto& state = states_[index];
                const auto& sample = samples[index];
                if (!gateOpen || !sample.valid) {
                    if (state.held) {
                        state.held = false;
                        setHeld(index, false);
                    }
                    state.armed = false;
                    continue;
                }
                if (!state.armed) {
                    state.armed = !sample.down;
                    state.presses = sample.presses;
                    continue;
                }
                const bool tapped = sample.presses != state.presses;
                state.presses = sample.presses;
                if (sample.down != state.held) {
                    state.held = sample.down;
                    setHeld(index, sample.down);
                } else if (tapped && !sample.down) {
                    // Pressed and released between two samples.
                    setHeld(index, true);
                    setHeld(index, false);
                }
            }
        }

    private:
        struct State
        {
            bool armed{};
            bool held{};
            std::uint64_t presses{};
        };
        std::vector<State> states_;
    };
}
