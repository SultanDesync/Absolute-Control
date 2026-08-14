#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace AbsoluteControlPanelResearch::Runtime
{
    // A gate lets producer threads queue work without allowing that work to run
    // after its owning service has stopped. Deactivation also waits for callbacks
    // that had already begun, but callbacks themselves never run under a gate lock.
    class CallbackGate final
    {
    public:
        CallbackGate() = default;
        CallbackGate(const CallbackGate&) = delete;
        CallbackGate& operator=(const CallbackGate&) = delete;

        template <class Callback>
        bool TryInvoke(Callback&& a_callback)
        {
            if (!active_.load(std::memory_order_acquire)) {
                return false;
            }

            inFlight_.fetch_add(1, std::memory_order_acq_rel);
            if (!active_.load(std::memory_order_acquire)) {
                ReleaseInvocation();
                return false;
            }

            try {
                std::invoke(std::forward<Callback>(a_callback));
            } catch (...) {
                ReleaseInvocation();
                throw;
            }
            ReleaseInvocation();
            return true;
        }

        void Deactivate() noexcept
        {
            active_.store(false, std::memory_order_release);
            waitCondition_.notify_all();
        }

        void WaitForIdle() noexcept
        {
            std::unique_lock lock{ waitLock_ };
            waitCondition_.wait(lock, [this] {
                return inFlight_.load(std::memory_order_acquire) == 0;
            });
        }

        [[nodiscard]] bool IsActive() const noexcept
        {
            return active_.load(std::memory_order_acquire);
        }

    private:
        void ReleaseInvocation() noexcept
        {
            if (inFlight_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // Pair the transition with the wait mutex so a waiter cannot
                // miss the final notification between checking and sleeping.
                const std::scoped_lock lock{ waitLock_ };
                waitCondition_.notify_all();
            }
        }

        std::atomic_bool active_{ true };
        std::atomic_size_t inFlight_{};
        std::mutex waitLock_;
        std::condition_variable waitCondition_;
    };

    // An explicitly owned, restartable worker. Stop requests cancellation,
    // invalidates queued callbacks, joins without holding worker-shared state,
    // and returns only after already-running callbacks have completed.
    class CooperativeService final
    {
    public:
        using Worker =
            std::function<void(std::stop_token, const std::shared_ptr<CallbackGate>&)>;

        CooperativeService() = default;
        CooperativeService(const CooperativeService&) = delete;
        CooperativeService& operator=(const CooperativeService&) = delete;

        ~CooperativeService()
        {
            Stop();
        }

        [[nodiscard]] bool Start(Worker a_worker) noexcept
        {
            if (!a_worker) {
                return false;
            }
            try {
                const std::scoped_lock lifecycleLock{ lifecycleLock_ };
                StopOwnedWorker();

                auto gate = std::make_shared<CallbackGate>();
                running_.store(true, std::memory_order_release);
                std::jthread worker{
                    [this, gate, task = std::move(a_worker)](
                        std::stop_token a_stopToken) noexcept {
                        try {
                            task(a_stopToken, gate);
                        } catch (...) {
                            // Service boundaries must not unwind through std::jthread.
                        }
                        gate->Deactivate();
                        running_.store(false, std::memory_order_release);
                    }
                };

                {
                    const std::scoped_lock stateLock{ stateLock_ };
                    gate_ = std::move(gate);
                    worker_ = std::move(worker);
                }
                return true;
            } catch (...) {
                running_.store(false, std::memory_order_release);
                return false;
            }
        }

        void Stop() noexcept
        {
            const std::scoped_lock lifecycleLock{ lifecycleLock_ };
            StopOwnedWorker();
        }

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return running_.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::shared_ptr<CallbackGate> Gate() const noexcept
        {
            const std::scoped_lock stateLock{ stateLock_ };
            return gate_;
        }

    private:
        void StopOwnedWorker() noexcept
        {
            std::jthread worker;
            std::shared_ptr<CallbackGate> gate;
            {
                const std::scoped_lock stateLock{ stateLock_ };
                gate = std::move(gate_);
                worker = std::move(worker_);
                running_.store(false, std::memory_order_release);
            }

            if (gate) {
                gate->Deactivate();
            }
            if (worker.joinable()) {
                worker.request_stop();
                worker.join();
            }
            if (gate) {
                gate->WaitForIdle();
            }
        }

        mutable std::mutex lifecycleLock_;
        mutable std::mutex stateLock_;
        std::jthread worker_;
        std::shared_ptr<CallbackGate> gate_;
        std::atomic_bool running_{};
    };

    template <class Rep, class Period>
    [[nodiscard]] bool InterruptibleWait(
        std::stop_token a_stopToken,
        std::chrono::duration<Rep, Period> a_duration) noexcept
    {
        std::mutex lock;
        std::condition_variable_any condition;
        std::unique_lock waitLock{ lock };
        condition.wait_for(waitLock, a_stopToken, a_duration, [] { return false; });
        return !a_stopToken.stop_requested();
    }
}
