#include "runtime/CameraLook.h"

#include "EvidenceLog.h"
#include "runtime/AimRestriction.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>

namespace AbsoluteControl::Runtime::CameraLook
{
    namespace
    {
        namespace Log = AbsoluteControlPanelResearch::EvidenceLog;
        using CameraRotationFunction = void (*)(void*, float*);

        // Starfield 1.16.244, as Head Tracking 1.0.0-beta.1 found them. The vtable slot is only
        // replaced if it still holds the expected function.
        constexpr std::uintptr_t kFirstPersonStateVtableRva = 0x4D04360;
        constexpr std::size_t kCameraRotationSlot = 13;
        constexpr std::uintptr_t kCameraRotationRva = 0x1E84A20;
        constexpr std::uintptr_t kPlayerCameraSingletonRva = 0x61DD460;
        // The loop publishes every 8 ms; a longer gap means it stopped, or the player did.
        constexpr std::int64_t kFreshMilliseconds = 250;

        CameraRotationFunction g_original{};
        std::atomic<bool> g_installed{};
        std::array<std::atomic<std::uint32_t>, 4> g_rotation{};
        std::atomic<std::int64_t> g_publishedAt{};

        std::int64_t SteadyMilliseconds()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

#pragma warning(push)
#pragma warning(disable: 4733)
        template <class T> bool SafeRead(std::uintptr_t address, T& value) noexcept
        {
            if (!address) return false;
            __try {
                value = *reinterpret_cast<volatile T*>(address);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        bool SafeCopy(void* destination, const void* source, std::size_t size) noexcept
        {
            if (!destination || !source || !size) return false;
            __try {
                std::memcpy(destination, source, size);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }
#pragma warning(pop)

        bool SwapSlot(std::uintptr_t* slot, std::uintptr_t expected, std::uintptr_t detour)
        {
            std::uintptr_t current{};
            if (!SafeRead(reinterpret_cast<std::uintptr_t>(slot), current) || current != expected) return false;
            DWORD protection{};
            if (!::VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) return false;
            ::InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot), reinterpret_cast<void*>(detour));
            ::FlushInstructionCache(::GetCurrentProcess(), slot, sizeof(*slot));
            ::VirtualProtect(slot, sizeof(*slot), protection, &protection);
            return true;
        }

        void HookedCameraRotation(void* state, float* quaternion)
        {
            const auto original = g_original;
            if (!original) return;
            original(state, quaternion);
            const auto publishedAt = g_publishedAt.load(std::memory_order_acquire);
            const auto now = SteadyMilliseconds();
            if (!state || !quaternion || !publishedAt || now < publishedAt || now - publishedAt > kFreshMilliseconds ||
                !AimRestriction::Ready()) {
                AimRestriction::ClearCameraFrame();
                return;
            }
            // Only the camera the player is looking through, not another first-person state.
            const auto module = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
            std::uintptr_t playerCamera{}, currentState{};
            if (!SafeRead(module + kPlayerCameraSingletonRva, playerCamera) || !playerCamera ||
                !SafeRead(playerCamera + 0x10, currentState) || currentState != reinterpret_cast<std::uintptr_t>(state)) {
                AimRestriction::ClearCameraFrame();
                return;
            }
            std::array<float, 4> native{}, head{};
            if (!SafeCopy(native.data(), quaternion, sizeof(native))) return;
            for (std::size_t index = 0; index < head.size(); ++index) {
                head[index] = std::bit_cast<float>(g_rotation[index].load(std::memory_order_relaxed));
            }
            const std::array<float, 4> result{
                native[0] * head[0] - native[1] * head[1] - native[2] * head[2] - native[3] * head[3],
                native[0] * head[1] + native[1] * head[0] - native[2] * head[3] + native[3] * head[2],
                native[0] * head[2] + native[1] * head[3] + native[2] * head[0] - native[3] * head[1],
                native[0] * head[3] - native[1] * head[2] + native[2] * head[1] + native[3] * head[0],
            };
            if (!SafeCopy(quaternion, result.data(), sizeof(result))) {
                AimRestriction::ClearCameraFrame();
                return;
            }
            AimRestriction::PublishCameraFrame({ head[0], head[1], head[2], head[3] });
        }
    }

    InstallResult Install()
    {
        static std::mutex mutex;
        static std::optional<InstallResult> done;
        std::lock_guard lock(mutex);
        if (done) return *done;
        InstallResult result;
        // The limit first: without it the camera stays the game's own.
        result.aimRestriction = AimRestriction::Install();
        if (result.aimRestriction) {
            const auto module = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
            auto* slot = reinterpret_cast<std::uintptr_t*>(module + kFirstPersonStateVtableRva +
                kCameraRotationSlot * sizeof(std::uintptr_t));
            g_original = reinterpret_cast<CameraRotationFunction>(module + kCameraRotationRva);
            result.camera = SwapSlot(slot, module + kCameraRotationRva, reinterpret_cast<std::uintptr_t>(&HookedCameraRotation));
            if (!result.camera) g_original = nullptr;
        }
        g_installed.store(result.camera && result.aimRestriction, std::memory_order_release);
        Log::Event("camera_look_install", std::format("camera={} aim_restriction={}",
            result.camera ? 1 : 0, result.aimRestriction ? 1 : 0));
        done = result;
        return result;
    }

    bool Ready() noexcept
    {
        return g_installed.load(std::memory_order_acquire);
    }

    void Publish(const Quaternion& rotation)
    {
        const float norm = std::sqrt(rotation.w * rotation.w + rotation.x * rotation.x +
            rotation.y * rotation.y + rotation.z * rotation.z);
        if (!std::isfinite(norm) || norm < 1e-6F) {
            Clear();
            return;
        }
        const std::array<float, 4> normal{ rotation.w / norm, rotation.x / norm, rotation.y / norm, rotation.z / norm };
        for (std::size_t index = 0; index < normal.size(); ++index) {
            g_rotation[index].store(std::bit_cast<std::uint32_t>(normal[index]), std::memory_order_relaxed);
        }
        g_publishedAt.store(SteadyMilliseconds(), std::memory_order_release);
    }

    void Clear()
    {
        g_publishedAt.store(0, std::memory_order_release);
        AimRestriction::ClearCameraFrame();
    }
}
