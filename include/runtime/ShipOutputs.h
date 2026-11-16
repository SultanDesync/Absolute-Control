#pragma once

#include "ShipActionCatalog.h"
#include "runtime/ControlMapReader.h"
#include "runtime/ShipBindings.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    struct ResolvedShipOutput
    {
        ShipActionOutputSpec output;
        KeyboardResolutionSource source{ KeyboardResolutionSource::VanillaFallback };
    };

    // "key:0x13" (a scan code), "mouse:1" to "mouse:4", or "none". Nothing for anything else.
    [[nodiscard]] std::optional<ShipActionOutputSpec> ParseShipOutput(std::string_view text);

    // The key or mouse button each catalog action presses, in catalog order. An action's
    // output is the game's default, then the player's own rebind (unless [ShipButtons]
    // FollowGameRebinds is false), then an explicit [ShipButtonKeys] value under the
    // action's setting key. Context actions always press the game's fixed key.
    [[nodiscard]] std::vector<ResolvedShipOutput> ResolveShipOutputs(const IniValues& ini,
        std::span<const ControlMap::Record> records);

    // Keys and mouse buttons held on behalf of owners. Several actions can share a key
    // (Space is take-off, flight mode and autopilot), so a key goes down with its first
    // owner and up with its last. It only tracks and reports: the emitter does the pressing.
    class HeldOutputs
    {
    public:
        using Emit = std::function<void(const ShipActionOutputSpec& output, bool down)>;

        explicit HeldOutputs(Emit emit);

        void Set(const ShipActionOutputSpec& output, std::uint32_t owner, bool held);
        void ReleaseAll();
        [[nodiscard]] bool Any() const noexcept { return !held_.empty(); }

    private:
        struct Held
        {
            ShipActionOutputSpec output;
            std::vector<std::uint32_t> owners;
        };

        Emit emit_;
        std::vector<Held> held_;
    };
}
