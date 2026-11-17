#pragma once

#include "runtime/DeviceStore.h"
#include "runtime/HeadlessCore.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Owned C++ implementation boundary within AC, not a retained DLL callback.
    class InputProvider
    {
    public:
        virtual ~InputProvider() = default;
        virtual bool Start() = 0;
        virtual void Poll(DeviceStore& store, std::uint64_t now) = 0;
        virtual void Stop() noexcept = 0;
    };

    class BackendRuntime final
    {
    public:
        ~BackendRuntime();
        [[nodiscard]] bool Start(std::unique_ptr<InputProvider> provider);
        void Stop();
        [[nodiscard]] State Lifecycle() const { return state_.load(); }
        [[nodiscard]] const DeviceStore& Devices() const { return devices_; }
        [[nodiscard]] std::string Failure() const;
        [[nodiscard]] static std::uint64_t Now();
    private:
        std::mutex lifecycle_;
        std::jthread worker_;
        std::atomic<State> state_{ State::Unavailable };
        DeviceStore devices_;
        mutable std::mutex failureMutex_;
        std::string failure_;
    };

    [[nodiscard]] BackendRuntime& NativeBackend();
    [[nodiscard]] std::unique_ptr<InputProvider> MakeDirectInputProvider();

    // A head tracker, through OpenTrack's freetrack output. It is always listed, so a
    // binding to it resolves before the tracker starts; it reports valid only while tracking.
    namespace OpenTrackChannels
    {
        inline constexpr std::string_view kProvider = "absolute.control.opentrack";
        inline constexpr std::string_view kPhysical = "opentrack";
        inline constexpr std::string_view kDisplayName = "OpenTrack";
        inline constexpr std::string_view kYaw = "axis/yaw";
        inline constexpr std::string_view kPitch = "axis/pitch";
        inline constexpr std::string_view kRoll = "axis/roll";
    }
    [[nodiscard]] std::unique_ptr<InputProvider> MakeOpenTrackProvider();

    // Runs several sources on the backend's one thread. The first is the main one: if it
    // fails to start, so does the whole. Any other that fails, at start or later, is
    // dropped on its own and the rest carry on.
    [[nodiscard]] std::unique_ptr<InputProvider> MakeCombinedProvider(
        std::vector<std::unique_ptr<InputProvider>> providers);
}
