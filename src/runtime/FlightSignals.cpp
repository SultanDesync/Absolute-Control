#include "runtime/FlightSignals.h"

namespace AbsoluteControl::Runtime
{
    namespace
    {
        struct Roots
        {
            std::uintptr_t player{};
            std::uintptr_t targetingMode{};
            std::uintptr_t manager{};
            std::uintptr_t camera{};
        };

        template <class T>
        T ReadField(const char* base, std::ptrdiff_t offset)
        {
            return *reinterpret_cast<const volatile T*>(base + offset);
        }

        bool ReadGuarded(const Roots& roots, FlightSignals& signals)
        {
            __try {
                if (const auto* player = *reinterpret_cast<const char* const*>(roots.player)) {
                    signals.flag = (ReadField<std::uint32_t>(player, 0x37C) & 0x10) ? 1 : 0;
                    signals.sit = (ReadField<std::uint32_t>(player, 0xFC) >> 11) & 3;
                    if (const auto* traversal = ReadField<const char*>(player, 0x10A8)) {
                        signals.traversal = ReadField<std::uint8_t>(traversal, 0x46) == 0 ? 1 : 0;
                    }
                }
                if (const auto* targeting = *reinterpret_cast<const char* const*>(roots.targetingMode)) {
                    signals.tm = ReadField<std::int32_t>(targeting, 0x54);
                }
                if (const auto* camera = *reinterpret_cast<const char* const*>(roots.camera)) {
                    if (const auto* state = ReadField<const char*>(camera, 0x10)) {
                        signals.cam = ReadField<std::uint8_t>(state, 0x50);
                    }
                }
                if (const auto* manager = *reinterpret_cast<const char* const*>(roots.manager)) {
                    signals.active = ReadField<std::uint8_t>(manager, 0x1D4);
                    if (const auto* cluster = ReadField<const char*>(manager, 0x208)) {
                        signals.cluster = 1;
                        signals.manual = ReadField<float>(cluster, 0x68);
                        signals.effective = ReadField<float>(cluster, 0x6C);
                        signals.speed = ReadField<float>(cluster, 0x70);
                        signals.engine = ReadField<std::uint8_t>(cluster, 0x7C);
                        signals.boost = ReadField<std::uint8_t>(cluster, 0x7D);
                        signals.drift = ReadField<std::uint8_t>(cluster, 0x7E);
                        signals.mode = ReadField<std::uint32_t>(cluster, 0x94);
                    }
                }
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }
    }

    std::optional<FlightSignals> ReadFlightSignals()
    {
        static const Roots roots{ REL::ID(922868).address(), REL::ID(937526).address(),
            REL::ID(938464).address(), REL::ID(937788).address() };
        FlightSignals signals{};
        if (!ReadGuarded(roots, signals)) return {};
        return signals;
    }
}
