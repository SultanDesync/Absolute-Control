#pragma once

#include "ShipActionCatalog.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Reads Starfield's ControlMap_Custom.txt, the player's key and mouse rebinds. Despite the
// extension it is binary, and it only holds differences from the game's own ControlMap.txt.
//
//   File    = Sections, back to back
//   Section = 0x03 | lenHi | lenLo | records   (big-endian length, counting these 3 bytes)
//   Record  = context\0 action\0 device(u32 LE) token(u16 LE) flag(u8) pad(u8)
//             device 1 is keyboard and mouse; anything else is skipped
//             token   keyboard: a Windows virtual-key code in the low byte; mouse: 0 to 3;
//                     0x02FF: explicitly unbound
//             flag    0 is the primary slot, anything else an alternate
//
// A rebind's primary and alternate can sit in different sections, so every section is read.
namespace AbsoluteControl::Runtime::ControlMap
{
    inline constexpr std::uint16_t kUnboundToken = 0x02FF;

    using Output = ShipActionOutputSpec;

    struct Record
    {
        std::string context;
        std::string action;
        std::uint32_t device{};
        std::uint16_t token{};
        std::uint8_t flag{};

        [[nodiscard]] bool IsPrimary() const noexcept { return flag == 0; }
        [[nodiscard]] bool IsUnbound() const noexcept { return token == kUnboundToken; }
    };

    struct File
    {
        std::vector<Record> records; // Keyboard and mouse records from every section.
        bool valid{};                // False if the bytes were malformed or the file is absent.
    };

    [[nodiscard]] File Parse(std::span<const std::uint8_t> bytes);
    // A missing file is the normal "nothing rebound" state: not valid, no records.
    [[nodiscard]] File ReadFile(const std::filesystem::path& path);

    // A scan code for the player's keyboard layout, or a mouse button 1 to 4. None for the
    // unbound token or a key the layout doesn't have.
    [[nodiscard]] Output TokenToOutput(std::uint16_t token);

    [[nodiscard]] bool HasPrimary(std::span<const Record> records, std::string_view context,
        std::string_view action);

    // What to press for one action:
    //   primary rebound                      -> the new key
    //   primary unbound, an alternate bound  -> the alternate
    //   primary unbound, no alternate        -> None
    //   no primary record                    -> vanilla, which still works in game
    // Alternates that exist only in the game's own ControlMap.txt can't be seen from here.
    [[nodiscard]] Output ResolveBinding(std::span<const Record> records, std::string_view context,
        std::string_view action, Output vanilla);
}
