#include "runtime/FlightOutput.h"

#include "EvidenceLog.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace AbsoluteControl::Runtime::FlightOutput
{
    namespace
    {
        namespace Log = AbsoluteControlPanelResearch::EvidenceLog;

        // Flight handler ("cluster") fields.
        constexpr std::ptrdiff_t kThrottleTarget = 0x68;
        constexpr std::ptrdiff_t kThrottleEffective = 0x6C;
        constexpr std::ptrdiff_t kVelocity = 0x70;
        // The steering block reads its object through r13: Manager + 0x20 on every flight logged.
        constexpr std::ptrdiff_t kSourceFromManager = 0x20;
        constexpr std::ptrdiff_t kSourceReverseIntent = 0x3C;
        constexpr std::ptrdiff_t kSourceAimYaw = 0x4C;
        constexpr std::ptrdiff_t kSourceAimPitch = 0x50;

        constexpr float kThrottleMoved = 0.015F;
        constexpr float kAimScale = 200.0F;
        constexpr float kAimLimit = 400.0F;
        constexpr float kAimChaseRate = 60.0F;

        // Read by the patched code through absolute addresses, so these never move.
        volatile std::uint8_t g_lateralActive{};
        volatile std::uint8_t g_verticalActive{};
        volatile std::uint8_t g_yawActive{};
        volatile std::uint8_t g_pitchActive{};
        volatile std::uint32_t g_lateralBits{};
        volatile std::uint32_t g_verticalBits{};
        volatile std::uint32_t g_yawBits{};
        volatile std::uint32_t g_pitchBits{};
        volatile std::uintptr_t g_capturedSource{};
        volatile std::uintptr_t g_capturedCluster{};
        volatile bool g_holdStores{};          // Every held store except effective throttle.
        volatile bool g_holdEffectiveStores{}; // Effective throttle, +0x6C.

        // Past the shared lane the flight handler turns its inputs into six thruster
        // outputs, where roll and sideways strafe are separate. Writing both there is what
        // lets a ship roll and strafe at once.
        using ThrusterOutput = void (*)(void* handler, const float* input, float* output);
        constexpr std::size_t kOutputStrafe = 0;
        constexpr std::size_t kOutputRoll = 5;
        ThrusterOutput g_thrusterOriginal{};
        std::atomic<std::uintptr_t> g_activeHandler{};
        std::atomic<bool> g_splitActive{};
        std::atomic<std::uint32_t> g_splitStrafeBits{};
        std::atomic<std::uint32_t> g_splitRollBits{};
        std::uintptr_t g_flightHandlerVtable{};

        bool g_reverseOwnsVertical{};
        bool g_sharedGate{}; // Another mod owns the steering block; ours sits just after it.
        bool g_installed{};
        InstallResult g_result;

        // Tick state.
        bool g_applied{};
        bool g_throttleCommanded{};
        float g_lastThrottle{};
        float g_burstValue{};
        std::uint64_t g_burstUntil{};
        std::uint64_t g_lastAimTime{};
        bool g_sourceChecked{};
        bool g_splitLogged{};
        std::uintptr_t g_lastSource{};

        struct Text
        {
            std::uintptr_t module{};
            std::uintptr_t start{};
            std::size_t size{};
        };

        std::optional<Text> FindText()
        {
            const auto module = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
            if (!module) return {};
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
            const auto* section = IMAGE_FIRST_SECTION(nt);
            for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
                if (std::memcmp(section[index].Name, ".text", 5) == 0) {
                    return Text{ module, module + section[index].VirtualAddress, section[index].Misc.VirtualSize };
                }
            }
            return {};
        }

        // A page within reach of a 5-byte jump from the patched instruction.
        std::uint8_t* AllocateNear(std::uintptr_t target)
        {
            const std::uintptr_t first = target > 0x70000000 ? target - 0x70000000 : 0x10000;
            for (std::uintptr_t address = first; address < target + 0x70000000; address += 0x10000) {
                if (auto* memory = static_cast<std::uint8_t*>(::VirtualAlloc(reinterpret_cast<void*>(address), 4096,
                    MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))) return memory;
            }
            return nullptr;
        }

        bool PatchJump(std::uintptr_t target, const std::uint8_t* destination)
        {
            DWORD protection{};
            if (!::VirtualProtect(reinterpret_cast<void*>(target), 5, PAGE_EXECUTE_READWRITE, &protection)) return false;
            auto* code = reinterpret_cast<std::uint8_t*>(target);
            const auto offset = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(destination) - (target + 5));
            code[0] = 0xE9;
            std::memcpy(code + 1, &offset, 4);
            ::VirtualProtect(reinterpret_cast<void*>(target), 5, protection, &protection);
            ::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(target), 5);
            return true;
        }

        class Assembler
        {
        public:
            explicit Assembler(std::uint8_t* memory) : memory_(memory) {}

            void Bytes(std::initializer_list<std::uint8_t> bytes)
            {
                for (const auto byte : bytes) memory_[size_++] = byte;
            }
            void Copy(const void* source, std::size_t count)
            {
                std::memcpy(memory_ + size_, source, count);
                size_ += count;
            }
            // mov rcx, address
            void LoadRcx(const volatile void* address)
            {
                Bytes({ 0x48, 0xB9 });
                const auto value = reinterpret_cast<std::uintptr_t>(address);
                Copy(&value, 8);
            }
            // mov rax, address
            void LoadRax(const volatile void* address)
            {
                Bytes({ 0x48, 0xB8 });
                const auto value = reinterpret_cast<std::uintptr_t>(address);
                Copy(&value, 8);
            }
            // A short jump whose distance is filled in by Land.
            [[nodiscard]] std::size_t ShortJump(std::uint8_t opcode)
            {
                Bytes({ opcode, 0 });
                return size_ - 1;
            }
            void Land(std::size_t jump) { memory_[jump] = static_cast<std::uint8_t>(size_ - jump - 1); }
            void JumpTo(std::uintptr_t address)
            {
                Bytes({ 0xE9 });
                const auto offset = static_cast<std::int32_t>(address - (reinterpret_cast<std::uintptr_t>(memory_) + size_ + 4));
                Copy(&offset, 4);
            }

        private:
            std::uint8_t* memory_;
            std::size_t size_{};
        };

        // Over "lea rdx,[rax+58]; vmovss [rdx],xmm0": notes the objects in r13 and rax, then
        // stores our roll or strafe value in place of the game's while the gate is active.
        bool InstallLateralGate(std::uintptr_t target)
        {
            auto* memory = AllocateNear(target);
            if (!memory) return false;
            Assembler code{ memory };
            code.Bytes({ 0x48, 0x8D, 0x50, 0x58 }); // lea rdx,[rax+58]
            code.Bytes({ 0x51, 0x9C });             // push rcx; pushfq
            code.LoadRcx(&g_capturedSource);
            code.Bytes({ 0x4C, 0x89, 0x29 });       // mov [rcx],r13
            code.LoadRcx(&g_capturedCluster);
            code.Bytes({ 0x48, 0x89, 0x01 });       // mov [rcx],rax
            code.LoadRcx(&g_lateralActive);
            code.Bytes({ 0x80, 0x39, 0x00 });       // cmp byte ptr [rcx],0
            const auto inactive = code.ShortJump(0x74);
            code.LoadRcx(&g_lateralBits);
            code.Bytes({ 0x8B, 0x09, 0x89, 0x0A }); // mov ecx,[rcx]; mov [rdx],ecx
            const auto done = code.ShortJump(0xEB);
            code.Land(inactive);
            code.Bytes({ 0x9D, 0x59 });             // popfq; pop rcx
            code.Bytes({ 0xC5, 0xFA, 0x11, 0x02 }); // vmovss [rdx],xmm0
            code.JumpTo(target + 8);
            code.Land(done);
            code.Bytes({ 0x9D, 0x59 });
            code.JumpTo(target + 8);
            return PatchJump(target, memory);
        }

        // Over one 5-byte "vmovss [reg+disp],xmm": stores our value while the gate is active,
        // otherwise replays the game's instruction. storeOpcode is 0x48 for rax, 0x4A for rdx.
        bool InstallStoreGate(std::uintptr_t target, std::uint8_t storeOpcode, std::uint8_t displacement,
            const volatile std::uint32_t* bits, const volatile std::uint8_t* active)
        {
            auto* memory = AllocateNear(target);
            if (!memory) return false;
            Assembler code{ memory };
            code.Bytes({ 0x51, 0x9C });
            code.LoadRcx(active);
            code.Bytes({ 0x80, 0x39, 0x00 });
            const auto inactive = code.ShortJump(0x74);
            code.LoadRcx(bits);
            code.Bytes({ 0x8B, 0x09, 0x89, storeOpcode, displacement }); // mov ecx,[rcx]; mov [reg+disp],ecx
            const auto done = code.ShortJump(0xEB);
            code.Land(inactive);
            code.Bytes({ 0x9D, 0x59 });
            code.Copy(reinterpret_cast<const void*>(target), 5);
            code.JumpTo(target + 5);
            code.Land(done);
            code.Bytes({ 0x9D, 0x59 });
            code.JumpTo(target + 5);
            return PatchJump(target, memory);
        }

        // The block as another mod leaves it: the first instruction intact, then a jump and
        // eighteen bytes of padding where the four stores were.
        const std::uint8_t* FindSharedBlock(const Text& text)
        {
            constexpr std::array<std::uint8_t, 8> kHead{ 0x49, 0x8B, 0x85, 0xE8, 0x01, 0x00, 0x00, 0xE9 };
            const auto* begin = reinterpret_cast<const std::uint8_t*>(text.start);
            const auto* end = begin + text.size - 64;
            const std::uint8_t* match{};
            for (auto at = std::search(begin, end, kHead.begin(), kHead.end()); at != end;
                at = std::search(at + 1, end, kHead.begin(), kHead.end())) {
                if (!std::all_of(at + 12, at + 30, [](std::uint8_t byte) { return byte == 0x90; })) continue;
                if (match) return nullptr; // Not unique: don't guess.
                match = at;
            }
            return match;
        }

        // Straight after the block the game loads a constant with
        // "vmovss xmm2,[rip+disp]", with the flight handler still in rax. A gate there sets
        // roll or strafe and vertical strafe after the other mod has stored the game's values.
        bool InstallGateAfterSharedBlock(const Text& text)
        {
            const auto* block = FindSharedBlock(text);
            if (!block) return false;
            const auto* next = block + 30;
            if (next[0] != 0xC5 || next[1] != 0xFA || next[2] != 0x10 || next[3] != 0x15 || next[8] != 0x80 || next[9] != 0x3D) return false;
            std::int32_t displacement{};
            std::memcpy(&displacement, next + 4, 4);
            const auto target = reinterpret_cast<std::uintptr_t>(next);
            const auto constant = target + 8 + displacement;

            auto* memory = AllocateNear(target);
            if (!memory) return false;
            Assembler code{ memory };
            code.Bytes({ 0x51, 0x9C });                   // push rcx; pushfq
            code.LoadRcx(&g_lateralActive);
            code.Bytes({ 0x80, 0x39, 0x00 });             // cmp byte ptr [rcx],0
            const auto noLateral = code.ShortJump(0x74);
            code.LoadRcx(&g_lateralBits);
            code.Bytes({ 0x8B, 0x09, 0x89, 0x48, 0x58 }); // mov ecx,[rcx]; mov [rax+58],ecx
            code.Land(noLateral);
            code.LoadRcx(&g_verticalActive);
            code.Bytes({ 0x80, 0x39, 0x00 });
            const auto noVertical = code.ShortJump(0x74);
            code.LoadRcx(&g_verticalBits);
            code.Bytes({ 0x8B, 0x09, 0x89, 0x48, 0x5C }); // mov ecx,[rcx]; mov [rax+5C],ecx
            code.Land(noVertical);
            code.LoadRcx(reinterpret_cast<const void*>(constant));
            code.Bytes({ 0xC5, 0xFA, 0x10, 0x11 });       // vmovss xmm2,[rcx]: the game's own load
            code.Bytes({ 0x9D, 0x59 });                   // popfq; pop rcx
            code.JumpTo(target + 8);
            return PatchJump(target, memory);
        }

        bool InstallSteeringGates(const Text& text)
        {
            constexpr std::array<std::uint8_t, 30> kBlock{
                0x49, 0x8B, 0x85, 0xE8, 0x01, 0x00, 0x00, // mov rax,[r13+1E8]
                0x48, 0x8D, 0x50, 0x58,                   // lea rdx,[rax+58]
                0xC5, 0xFA, 0x11, 0x02,                   // vmovss [rdx],xmm0     roll or strafe
                0xC5, 0xFA, 0x11, 0x52, 0x04,             // vmovss [rdx+04],xmm2  vertical strafe
                0xC5, 0xFA, 0x11, 0x58, 0x60,             // vmovss [rax+60],xmm3  yaw
                0xC5, 0xFA, 0x11, 0x48, 0x64              // vmovss [rax+64],xmm1  pitch
            };
            const auto* begin = reinterpret_cast<const std::uint8_t*>(text.start);
            const auto* end = begin + text.size;
            const auto found = std::search(begin, end, kBlock.begin(), kBlock.end());
            if (found == end) {
                // AbsoluteZero replaces the block from its second instruction on with a jump
                // and padding, and writes the four lanes itself. Roll and both strafes can
                // still be set just after it.
                g_sharedGate = InstallGateAfterSharedBlock(text);
                Log::Event("flight_gates", std::format("installed=0 reason=block_not_found shared={}", g_sharedGate ? 1 : 0));
                return false;
            }
            // More than one match would mean the signature no longer identifies the block.
            if (std::search(found + 1, end, kBlock.begin(), kBlock.end()) != end) {
                Log::Event("flight_gates", "installed=0 reason=block_not_unique");
                return false;
            }
            const auto block = reinterpret_cast<std::uintptr_t>(found);
            // Pitch, yaw and vertical first: the lateral gate is the one another mod may hold.
            const bool ok = InstallStoreGate(block + 25, 0x48, 0x64, &g_pitchBits, &g_pitchActive) &&
                InstallStoreGate(block + 20, 0x48, 0x60, &g_yawBits, &g_yawActive) &&
                InstallStoreGate(block + 15, 0x4A, 0x04, &g_verticalBits, &g_verticalActive) &&
                InstallLateralGate(block + 7);
            Log::Event("flight_gates", std::format("installed={} block=0x{:X}", ok ? 1 : 0, block - text.module));
            return ok;
        }

        // Over one 5-byte game store: skips it while the flag is set, otherwise replays it.
        bool InstallStoreHold(std::uintptr_t target, const volatile bool* hold)
        {
            auto* memory = AllocateNear(target);
            if (!memory) return false;
            Assembler code{ memory };
            // Registers and flags reach the game's code exactly as they arrived.
            code.Bytes({ 0x50, 0x9C });       // push rax; pushfq
            code.LoadRax(hold);
            code.Bytes({ 0x80, 0x38, 0x00 }); // cmp byte ptr [rax],0
            const auto replay = code.ShortJump(0x74);
            code.Bytes({ 0x9D, 0x58 });       // popfq; pop rax
            code.JumpTo(target + 5);
            code.Land(replay);
            code.Bytes({ 0x9D, 0x58 });
            code.Copy(reinterpret_cast<const void*>(target), 5);
            code.JumpTo(target + 5);
            return PatchJump(target, memory);
        }

        bool HasEffectiveThrottleNearby(const std::uint8_t* at, const Text& text)
        {
            const auto* first = reinterpret_cast<const std::uint8_t*>(text.start);
            const auto* last = first + text.size - 2;
            for (int offset = -64; offset < 64; ++offset) {
                const auto* byte = at + offset;
                if (byte < first || byte >= last) continue;
                if ((byte[0] == 0x47 || byte[0] == 0x4F) && byte[1] == 0x6C) return true;
            }
            return false;
        }

        // The stores AbsoluteHOTAS holds back on this build, matched the same way so the
        // same instructions are hooked: "C5 ?? op ?? disp" inside the flight code's window.
        int InstallStoreHolds(const Text& text)
        {
            struct Pattern
            {
                std::uint8_t opcode;
                std::uint8_t displacement;
            };
            constexpr std::array<Pattern, 10> kPatterns{ {
                { 0x11, 0x64 }, { 0x58, 0x64 }, { 0x11, 0x60 }, { 0x58, 0x60 }, { 0x11, 0x58 },
                { 0x11, 0x5C }, { 0x11, 0x68 }, { 0x11, 0x6C }, { 0x29, 0x50 }, { 0x29, 0x60 },
            } };
            constexpr std::uintptr_t kWindowStart = 0x12B0000;
            constexpr std::uintptr_t kWindowEnd = 0x12C0000;
            if (text.module + kWindowEnd + 16 > text.start + text.size || text.module + kWindowStart < text.start) return 0;

            // Every address is collected before anything is patched, so a hook's own jump
            // can't be matched as a store.
            struct Match
            {
                std::uintptr_t address;
                Pattern pattern;
            };
            std::vector<Match> matches;
            for (std::uintptr_t offset = kWindowStart; offset <= kWindowEnd; ++offset) {
                const auto* byte = reinterpret_cast<const std::uint8_t*>(text.module + offset);
                if (byte[0] != 0xC5) continue;
                // Only "[register + 8-bit offset]" is exactly five bytes long. Anything else
                // would be cut in half by the patch.
                if ((byte[3] & 0xC0) != 0x40 || (byte[3] & 0x07) == 0x04) continue;
                for (const auto& pattern : kPatterns) {
                    if (byte[2] != pattern.opcode || byte[4] != pattern.displacement) continue;
                    // A throttle target store is only trusted beside an effective throttle one.
                    if (pattern.displacement == 0x68 && !HasEffectiveThrottleNearby(byte, text)) continue;
                    if (!matches.empty() && text.module + offset < matches.back().address + 5) continue;
                    matches.push_back({ text.module + offset, pattern });
                }
            }
            int hooks{};
            for (const auto& match : matches) {
                const bool effective = match.pattern.opcode == 0x11 && match.pattern.displacement == 0x6C;
                if (!InstallStoreHold(match.address, effective ? &g_holdEffectiveStores : &g_holdStores)) continue;
                ++hooks;
                Log::Event("flight_store_hook", std::format("op=0x{:02X} disp=0x{:02X} at=0x{:X}",
                    match.pattern.opcode, match.pattern.displacement, match.address - text.module));
            }
            return hooks;
        }

        bool ReadFloat(std::uintptr_t address, float& value) noexcept
        {
            __try {
                value = *reinterpret_cast<const volatile float*>(address);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        void WriteFloat(std::uintptr_t address, float value) noexcept
        {
            __try {
                *reinterpret_cast<volatile float*>(address) = value;
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // Target always; effective too when forced or when it would otherwise stay above.
        void WriteThrottle(std::uintptr_t cluster, float throttle, bool forceEffective) noexcept
        {
            __try {
                *reinterpret_cast<volatile float*>(cluster + kThrottleTarget) = throttle;
                const float effective = *reinterpret_cast<const volatile float*>(cluster + kThrottleEffective);
                if (forceEffective || throttle <= effective) {
                    *reinterpret_cast<volatile float*>(cluster + kThrottleEffective) = throttle;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        void SetReverse(bool active, std::uintptr_t source) noexcept
        {
            if (active) {
                // Reverse takes the vertical strafe lane and the game's own slow-down intent.
                g_reverseOwnsVertical = true;
                g_verticalBits = std::bit_cast<std::uint32_t>(-1.0F);
                g_verticalActive = 1;
                if (source) WriteFloat(source + kSourceReverseIntent, -1.0F);
            } else if (g_reverseOwnsVertical) {
                // Only on the way out: this lane is shared, and zeroing it every tick on
                // foot cancels sprint.
                g_verticalActive = 0;
                g_reverseOwnsVertical = false;
                if (source) WriteFloat(source + kSourceReverseIntent, 0.0F);
            }
        }

        // Moves the game's aim point toward where the stick points, over a few frames.
        void SteerAim(std::uintptr_t source, float yaw, float pitch, float sensitivity, std::uint64_t now) noexcept
        {
            const float size = std::sqrt(yaw * yaw + pitch * pitch);
            if (size > 1.0F) {
                yaw /= size;
                pitch /= size;
            }
            const float seconds = std::clamp(static_cast<float>(now - g_lastAimTime) / 1e9F, 0.0001F, 0.1F);
            g_lastAimTime = now;
            const float blend = 1.0F - std::exp(-kAimChaseRate * seconds);
            float currentYaw{}, currentPitch{};
            if (!ReadFloat(source + kSourceAimYaw, currentYaw) || !ReadFloat(source + kSourceAimPitch, currentPitch)) return;
            const float targetYaw = yaw * sensitivity * kAimScale;
            const float targetPitch = pitch * sensitivity * kAimScale;
            WriteFloat(source + kSourceAimYaw, std::clamp(currentYaw + blend * (targetYaw - currentYaw), -kAimLimit, kAimLimit));
            WriteFloat(source + kSourceAimPitch, std::clamp(currentPitch + blend * (targetPitch - currentPitch), -kAimLimit, kAimLimit));
        }
    }

    namespace
    {
        template <class T>
        bool ReadValue(std::uintptr_t address, T& value) noexcept
        {
            __try {
                std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        // Runs on the game's thread, after the handler has worked out its own outputs.
        void HookedThrusterOutput(void* handler, const float* input, float* output)
        {
            g_thrusterOriginal(handler, input, output);
            if (!g_splitActive.load(std::memory_order_acquire) ||
                reinterpret_cast<std::uintptr_t>(handler) != g_activeHandler.load(std::memory_order_acquire)) return;
            output[kOutputStrafe] = std::bit_cast<float>(g_splitStrafeBits.load(std::memory_order_relaxed));
            output[kOutputRoll] = std::bit_cast<float>(g_splitRollBits.load(std::memory_order_relaxed));
        }

        // Swaps one entry of the flight handler's function table, and only if it holds
        // exactly the function this was written against.
        bool InstallSplitHook(std::uintptr_t module)
        {
            const auto vtable = REL::ID(433532).address();
            const auto original = REL::ID(82208).address();
            auto* slot = reinterpret_cast<std::uintptr_t*>(vtable + 0x10);
            std::uintptr_t current{};
            if (!ReadValue(reinterpret_cast<std::uintptr_t>(slot), current) || current != original) {
                Log::Event("flight_split", std::format("installed=0 reason=unexpected_slot vtable=0x{:X} function=0x{:X} slot=0x{:X}",
                    vtable - module, original - module, current > module ? current - module : 0));
                return false;
            }
            DWORD protection{};
            if (!::VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) {
                Log::Event("flight_split", "installed=0 reason=protect_failed");
                return false;
            }
            g_thrusterOriginal = reinterpret_cast<ThrusterOutput>(original);
            g_flightHandlerVtable = vtable;
            ::InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot), reinterpret_cast<void*>(&HookedThrusterOutput));
            ::VirtualProtect(slot, sizeof(*slot), protection, &protection);
            Log::Event("flight_split", std::format("installed=1 vtable=0x{:X} function=0x{:X}", vtable - module, original - module));
            return true;
        }

        // The flight handler the game has selected right now, or zero.
        std::uintptr_t SelectedHandler(std::uintptr_t cluster) noexcept
        {
            std::uint32_t count{}, index{};
            std::uintptr_t handlers{}, handler{}, vtable{};
            if (!g_flightHandlerVtable || !ReadValue(cluster + 0x80, count) || count == 0 || count >= 4096 ||
                !ReadValue(cluster + 0x94, index) || index >= count || !ReadValue(cluster + 0x88, handlers) || !handlers ||
                !ReadValue(handlers + index * sizeof(void*), handler) || !handler ||
                !ReadValue(handler, vtable) || vtable != g_flightHandlerVtable) return 0;
            return handler;
        }
    }

    InstallResult Install()
    {
        if (g_installed) return g_result;
        g_installed = true;
        const auto text = FindText();
        if (!text) {
            Log::Event("flight_output", "installed=0 reason=no_text_section");
            return g_result;
        }
        g_result.steeringGates = InstallSteeringGates(*text);
        g_result.sharedGate = g_sharedGate;
        g_result.storeHooks = InstallStoreHolds(*text);
        g_result.splitHook = InstallSplitHook(text->module);
        Log::Event("flight_output", std::format("installed=1 gates={} shared={} store_hooks={} split={}",
            g_result.steeringGates ? 1 : 0, g_result.sharedGate ? 1 : 0, g_result.storeHooks, g_result.splitHook ? 1 : 0));
        return g_result;
    }

    void Apply(const FlightConfig& config, const FlightFrame& frame, const Target& target,
        bool boostHeld, bool mouseSteeringOwned, std::uint64_t now)
    {
        float current{};
        if (!target.cluster || !ReadFloat(target.cluster + kThrottleTarget, current) || !std::isfinite(current)) {
            Suspend();
            return;
        }
        g_applied = true;
        // The steering object is only trusted from the game's own steering code, and only
        // while it belongs to the flight handler in use. Until that code has run, nothing
        // is written through it.
        const std::uintptr_t captured = g_capturedSource;
        std::uintptr_t source = captured && g_capturedCluster == target.cluster ? captured : 0;
        // Without our own gate in the block nothing is captured. The object sat at
        // Manager + 0x20 on every flight that did capture it.
        if (g_sharedGate && target.manager) source = target.manager + kSourceFromManager;
        g_lastSource = source;
        if (!g_sourceChecked && source && captured) {
            // For a later build: whether it could be worked out from the manager instead.
            g_sourceChecked = true;
            Log::Event("flight_source", std::format("manager_offset_match={}",
                target.manager && captured == target.manager + kSourceFromManager ? 1 : 0));
        }

        // Steering lanes.
        g_lateralBits = std::bit_cast<std::uint32_t>(frame.lateral);
        g_lateralActive = frame.lateralActive ? 1 : 0;
        if (g_thrusterOriginal) {
            const auto handler = SelectedHandler(target.cluster);
            g_activeHandler.store(handler, std::memory_order_release);
            g_splitStrafeBits.store(std::bit_cast<std::uint32_t>(frame.splitStrafe), std::memory_order_relaxed);
            g_splitRollBits.store(std::bit_cast<std::uint32_t>(frame.splitRoll), std::memory_order_relaxed);
            const bool split = handler && frame.splitActive;
            if (split != g_splitActive.load(std::memory_order_relaxed)) {
                g_splitActive.store(split, std::memory_order_release);
                if (split && !g_splitLogged) {
                    g_splitLogged = true;
                    Log::Event("flight_split_active", std::format("roll={:.2f} strafe={:.2f}", frame.splitRoll, frame.splitStrafe));
                }
            }
        }
        if (!g_reverseOwnsVertical) {
            g_verticalBits = std::bit_cast<std::uint32_t>(frame.vertical);
            g_verticalActive = frame.verticalActive ? 1 : 0;
        }
        const bool steer = frame.pitchYawBound && !mouseSteeringOwned;
        const bool throughAim = steer && config.steerThroughAim && source;
        g_yawBits = std::bit_cast<std::uint32_t>(frame.yaw);
        g_pitchBits = std::bit_cast<std::uint32_t>(frame.pitch);
        g_yawActive = steer && !throughAim ? 1 : 0;
        g_pitchActive = steer && !throughAim ? 1 : 0;
        if (throughAim) SteerAim(source, frame.yaw, frame.pitch, config.aimSensitivity, now);
        else g_lastAimTime = now;

        // Throttle.
        if (config.holdForBoost && boostHeld) {
            // The boosters own the throttle while they burn; the lever takes over again after.
            SetReverse(false, source);
            g_holdStores = false;
            g_holdEffectiveStores = false;
            g_burstUntil = 0;
            g_throttleCommanded = false;
            return;
        }
        if (frame.reverse) {
            g_holdStores = false;
            g_holdEffectiveStores = false;
            float velocity{};
            const bool slow = ReadFloat(target.cluster + kVelocity, velocity) &&
                (velocity < 0.0F || velocity <= config.reverseGateVelocity);
            SetReverse(slow, source);
            WriteThrottle(target.cluster, slow ? -1.0F : 0.0F, false);
            g_throttleCommanded = false;
            return;
        }
        SetReverse(false, source);
        g_holdStores = frame.throttleBound;
        g_holdEffectiveStores = frame.throttleBound;
        if (!frame.throttleBound) {
            g_throttleCommanded = false;
            return;
        }
        if (!g_throttleCommanded || std::abs(frame.throttle - g_lastThrottle) > kThrottleMoved) {
            // Written for a short burst after each move; the held stores keep it there.
            g_throttleCommanded = true;
            g_lastThrottle = frame.throttle;
            g_burstValue = frame.throttle;
            g_burstUntil = now + static_cast<std::uint64_t>(std::max(config.throttleBurstMilliseconds, 0)) * 1'000'000ULL;
            WriteThrottle(target.cluster, g_burstValue, true);
        } else if (now < g_burstUntil) {
            WriteThrottle(target.cluster, g_burstValue, true);
        }
    }

    void Suspend()
    {
        if (!g_applied) return;
        g_applied = false;
        g_splitActive.store(false, std::memory_order_release);
        g_lateralActive = 0;
        g_yawActive = 0;
        g_pitchActive = 0;
        if (g_reverseOwnsVertical) SetReverse(false, g_lastSource);
        g_verticalActive = 0;
        g_holdStores = false;
        g_holdEffectiveStores = false;
        g_burstUntil = 0;
        g_throttleCommanded = false;
    }
}
