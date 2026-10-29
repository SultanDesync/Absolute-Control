#include "runtime/BackendRuntime.h"

#include <chrono>
#include <condition_variable>
#include <exception>

namespace AbsoluteControl::Runtime
{
    BackendRuntime::~BackendRuntime() { Stop(); }

    std::uint64_t BackendRuntime::Now()
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    bool BackendRuntime::Start(std::unique_ptr<InputProvider> provider)
    {
        if (!provider) return false;
        std::lock_guard lock(lifecycle_);
        if (worker_.joinable()) return false;
        { std::lock_guard failureLock(failureMutex_); failure_.clear(); }
        state_ = State::Starting;
        try {
            worker_ = std::jthread([this, input = std::move(provider)](std::stop_token stop) {
                bool failed = false;
                try {
                    if (!input->Start()) {
                        failed = true;
                        std::lock_guard failureLock(failureMutex_);
                        failure_ = "input provider initialization failed";
                    }
                    else {
                        devices_.Start();
                        // Publish the first inventory before advertising readiness.
                        input->Poll(devices_, Now());
                        State starting = State::Starting;
                        state_.compare_exchange_strong(starting, State::Ready);
                        std::mutex waitMutex;
                        std::condition_variable_any condition;
                        std::unique_lock waitLock(waitMutex);
                        while (!stop.stop_requested()) {
                            condition.wait_for(waitLock, stop, std::chrono::milliseconds(8), [] { return false; });
                            if (!stop.stop_requested()) input->Poll(devices_, Now());
                        }
                    }
                } catch (const std::exception& error) {
                    failed = true;
                    std::lock_guard failureLock(failureMutex_); failure_ = error.what();
                } catch (...) {
                    failed = true;
                    std::lock_guard failureLock(failureMutex_); failure_ = "input provider threw an unknown exception";
                }
                input->Stop(); // Also required after partial Start or failed Poll.
                devices_.Stop();
                state_ = failed ? State::Degraded : State::Unavailable;
            });
            return true;
        } catch (...) {
            devices_.Stop();
            state_ = State::Degraded;
            return false;
        }
    }

    void BackendRuntime::Stop()
    {
        std::lock_guard lock(lifecycle_);
        if (worker_.joinable()) {
            state_ = State::Stopping;
            worker_.request_stop();
            worker_.join();
        }
        devices_.Stop();
        state_ = State::Unavailable;
    }

    BackendRuntime& NativeBackend()
    {
        // SFSE has no process-exit notification. Never join a worker from DLL
        // detach under the loader lock. This instance has process lifetime;
        // explicit Stop is available before teardown, hot unload unsupported.
        static auto* runtime = new BackendRuntime;
        return *runtime;
    }

    std::string BackendRuntime::Failure() const
    {
        std::lock_guard lock(failureMutex_);
        return failure_;
    }
}
