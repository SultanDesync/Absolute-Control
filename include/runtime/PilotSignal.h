#pragma once

#include <cstdint>

namespace AbsoluteControl::Runtime
{
    struct PilotObservation
    {
        bool seated{};      // The player's pilot seat flag.
        bool targeting{};   // Spaceship::TargetingMode is active.
        bool inputActive{}; // PlayerControls is processing input, so no menu or load has it.
    };

    // Targeting Mode clears the seat flag while the player stays seated, so piloting is
    // either one. Leaving Targeting Mode can show both clear for about one game update, so
    // only a clear lasting longer than that counts as leaving the seat.
    class PilotSignal
    {
    public:
        static constexpr std::uint64_t kExitDebounceNanoseconds = 100'000'000;

        void Update(const PilotObservation& observation, std::uint64_t now) noexcept
        {
            if (observation.seated || observation.targeting) {
                piloting_ = true;
                lastPiloting_ = now;
            } else if (piloting_ && now - lastPiloting_ >= kExitDebounceNanoseconds) {
                piloting_ = false;
            }
            inputActive_ = observation.inputActive;
        }

        [[nodiscard]] bool Piloting() const noexcept { return piloting_; }
        [[nodiscard]] bool ButtonsLive() const noexcept { return piloting_ && inputActive_; }

    private:
        bool piloting_{};
        bool inputActive_{};
        std::uint64_t lastPiloting_{};
    };
}
