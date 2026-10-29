#pragma once

#include <cstdint>

namespace AbsoluteControl::Runtime
{
    struct DigitalSampleState
    {
        bool down{};
        std::uint64_t presses{}, releases{};

        void Observe(bool nextDown, bool rebase) noexcept
        {
            if (!rebase) {
                if (nextDown && !down) ++presses;
                if (!nextDown && down) ++releases;
            }
            down = nextDown;
        }
    };

    // Native DirectInput adapter policy, kept out of the provider-neutral ABI.
    [[nodiscard]] constexpr bool PovDirectionActive(std::uint32_t hundredths,
        std::uint32_t direction) noexcept
    {
        if (hundredths >= 36000 || direction >= 4) return false;
        const auto target = direction * 9000;
        auto difference = hundredths > target ? hundredths - target : target - hundredths;
        if (difference > 18000) difference = 36000 - difference;
        return difference <= 4500;
    }
}
