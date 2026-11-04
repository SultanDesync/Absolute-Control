#pragma once

#include <cstdint>
#include <optional>

namespace AbsoluteControl::Runtime
{
    // Pilot and flight state from the game's objects, for one build. See the static map,
    // F:\AbsoluteHOTAS-ResearchData\analysis\starfield-flight-model\flight-model-map.md.
    // Address Library covers the singletons, but a game update must re-verify the offsets.
    struct FlightSignals
    {
        std::uint32_t flag{};       // PlayerCharacter+0x37C bit 4: pilot seat.
        std::uint32_t sit{};        // PlayerCharacter+0xFC bits 11-12: 1 sitting down, 2 seated, 3 getting up.
        std::uint32_t traversal{};  // PlayerCharacter+0x10A8 set and its +0x46 clear: the tests the game's ladder input makes.
        std::int32_t tm{};          // Spaceship::TargetingMode+0x54: nonzero while active.
        std::uint32_t active{};     // PlayerControls::Manager+0x1D4: input processed this frame.
        std::uint32_t cam{ 0xFF };  // PlayerCamera current state id, 0xFF with no state.
        std::uint32_t cluster{};    // FlightMovementHandler present, Manager+0x208.
        std::uint32_t engine{};     // cluster+0x7C.
        std::uint32_t boost{};      // cluster+0x7D.
        std::uint32_t drift{};      // cluster+0x7E.
        std::uint32_t mode{};       // cluster+0x94.
        float manual{};             // cluster+0x68.
        float effective{};          // cluster+0x6C.
        float speed{};              // cluster+0x70.
    };

    // Aligned field reads with no call or lock, safe from any thread. Objects can be freed
    // underneath the reader, so a fault returns nothing and the next read tries again.
    [[nodiscard]] std::optional<FlightSignals> ReadFlightSignals();
}
