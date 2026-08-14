#include "diagnostics/AsyncLineSink.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace AbsoluteControlPanelResearch::Diagnostics
{
    struct AsyncLineSink::State
    {
        struct Record
        {
            std::uint64_t ticket{};
            std::string line;
        };

        mutable std::mutex lifecycleLock;
        mutable std::mutex queueLock;
        std::condition_variable_any queueChanged;
        std::condition_variable flushed;
        std::deque<Record> queue;
        std::filesystem::path path;
        std::size_t capacity{};
        std::uint64_t accepted{};
        std::uint64_t written{};
        std::uint64_t dropped{};
        std::uint64_t ioFailures{};
        bool accepting{};
        bool running{};
        std::jthread worker;

        void Run(std::stop_token a_stopToken) noexcept;
    };

    void AsyncLineSink::State::Run(std::stop_token a_stopToken) noexcept
    {
        std::ofstream stream;
        try {
            stream.open(path, std::ios::app);
        } catch (...) {
            // The queue is still drained so producers and Flush never deadlock.
        }

        bool finished = false;
        while (!finished) {
            std::deque<Record> batch;
            {
                std::unique_lock lock{ queueLock };
                queueChanged.wait(lock, a_stopToken, [this] {
                    return !queue.empty();
                });
                if (queue.empty() && a_stopToken.stop_requested()) {
                    finished = true;
                    continue;
                }
                batch.swap(queue);
            }

            bool ioFailed = !stream.is_open();
            for (const auto& record : batch) {
                if (!ioFailed) {
                    stream << record.line << '\n';
                    ioFailed = !stream.good();
                }
            }
            // Flush every bounded batch. This makes normal-operation loss no
            // larger than the configured outstanding-record capacity.
            if (!ioFailed) {
                stream.flush();
                ioFailed = !stream.good();
            }

            {
                const std::scoped_lock lock{ queueLock };
                if (ioFailed) {
                    ioFailures += batch.size();
                }
                if (!batch.empty()) {
                    written = (std::max)(written, batch.back().ticket);
                }
            }
            flushed.notify_all();
        }

        if (stream.is_open()) {
            stream.flush();
        }
        {
            const std::scoped_lock lock{ queueLock };
            running = false;
        }
        flushed.notify_all();
    }

    AsyncLineSink::AsyncLineSink() : state_(std::make_unique<State>()) {}

    AsyncLineSink::~AsyncLineSink()
    {
        if (!Shutdown(std::chrono::seconds(5))) {
            // Local/test owners retain the conventional jthread guarantee. The
            // plugin's process-lifetime EvidenceLog owner is intentionally not a
            // static object, so this path never runs under the DLL loader lock.
            state_->worker.request_stop();
            state_->queueChanged.notify_all();
        }
    }

    bool AsyncLineSink::Start(
        const std::filesystem::path& a_path, std::size_t a_capacity) noexcept
    {
        if (a_path.empty() || a_capacity == 0) {
            return false;
        }
        try {
            const std::scoped_lock lifecycleLock{ state_->lifecycleLock };

            std::jthread oldWorker;
            {
                const std::scoped_lock queueLock{ state_->queueLock };
                state_->accepting = false;
                oldWorker = std::move(state_->worker);
            }
            if (oldWorker.joinable()) {
                oldWorker.request_stop();
                state_->queueChanged.notify_all();
                oldWorker.join();
            }

            {
                const std::scoped_lock queueLock{ state_->queueLock };
                state_->queue.clear();
                state_->path = a_path;
                state_->capacity = a_capacity;
                state_->accepted = 0;
                state_->written = 0;
                state_->dropped = 0;
                state_->ioFailures = 0;
                state_->accepting = true;
                state_->running = true;
            }
            state_->worker = std::jthread{
                [state = state_.get()](std::stop_token a_stopToken) {
                    state->Run(a_stopToken);
                }
            };
            return true;
        } catch (...) {
            const std::scoped_lock queueLock{ state_->queueLock };
            state_->accepting = false;
            state_->running = false;
            return false;
        }
    }

    bool AsyncLineSink::Enqueue(std::string a_line) noexcept
    {
        try {
            {
                const std::scoped_lock lock{ state_->queueLock };
                const auto outstanding = state_->accepted - state_->written;
                if (!state_->accepting || outstanding >= state_->capacity) {
                    ++state_->dropped;
                    return false;
                }
                const auto ticket = ++state_->accepted;
                state_->queue.push_back(State::Record{ ticket, std::move(a_line) });
            }
            state_->queueChanged.notify_one();
            return true;
        } catch (...) {
            const std::scoped_lock lock{ state_->queueLock };
            ++state_->dropped;
            return false;
        }
    }

    bool AsyncLineSink::Flush(std::chrono::milliseconds a_timeout) noexcept
    {
        try {
            std::unique_lock lock{ state_->queueLock };
            const auto target = state_->accepted;
            state_->queueChanged.notify_one();
            const bool completed = state_->flushed.wait_for(lock, a_timeout, [&] {
                return state_->written >= target || !state_->running;
            });
            return completed && state_->written >= target && state_->ioFailures == 0;
        } catch (...) {
            return false;
        }
    }

    bool AsyncLineSink::Shutdown(std::chrono::milliseconds a_timeout) noexcept
    {
        const std::scoped_lock lifecycleLock{ state_->lifecycleLock };
        {
            const std::scoped_lock queueLock{ state_->queueLock };
            state_->accepting = false;
            if (state_->worker.joinable()) {
                state_->worker.request_stop();
            }
        }
        state_->queueChanged.notify_all();

        {
            std::unique_lock queueLock{ state_->queueLock };
            if (!state_->flushed.wait_for(queueLock, a_timeout, [this] {
                    return !state_->running;
                })) {
                return false;
            }
        }

        std::jthread worker = std::move(state_->worker);
        if (worker.joinable()) {
            worker.join();
        }
        return true;
    }

    AsyncLineSinkStatistics AsyncLineSink::Statistics() const noexcept
    {
        const std::scoped_lock lock{ state_->queueLock };
        return {
            .accepted = state_->accepted,
            .written = state_->written,
            .dropped = state_->dropped,
            .ioFailures = state_->ioFailures,
            .queued = state_->queue.size(),
            .running = state_->running
        };
    }
}
