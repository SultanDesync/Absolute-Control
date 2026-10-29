#pragma once

#include "runtime/DeviceStore.h"
#include "runtime/HeadlessCore.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <string>

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
}
