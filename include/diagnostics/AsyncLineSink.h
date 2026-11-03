#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace AbsoluteControlPanelResearch::Diagnostics
{
    struct AsyncLineSinkStatistics
    {
        std::uint64_t accepted{};
        std::uint64_t written{};
        std::uint64_t dropped{};
        std::uint64_t ioFailures{};
        std::size_t queued{};
        bool running{};
    };

    // A bounded, single-writer JSONL sink. Producers drop the newest record when
    // capacity is exhausted; they never perform file I/O or wait for disk. The
    // dropped count is observable through Statistics(). Flush and Shutdown are
    // explicit synchronization points intended for tests and controlled teardown.
    class AsyncLineSink final
    {
    public:
        AsyncLineSink();
        AsyncLineSink(const AsyncLineSink&) = delete;
        AsyncLineSink& operator=(const AsyncLineSink&) = delete;
        ~AsyncLineSink();

        [[nodiscard]] bool Start(
            const std::filesystem::path& a_path, std::size_t a_capacity) noexcept;
        [[nodiscard]] bool Enqueue(std::string a_line) noexcept;
        [[nodiscard]] bool Flush(
            std::chrono::milliseconds a_timeout = std::chrono::seconds(5)) noexcept;
        [[nodiscard]] bool Shutdown(
            std::chrono::milliseconds a_timeout = std::chrono::seconds(5)) noexcept;

        [[nodiscard]] AsyncLineSinkStatistics Statistics() const noexcept;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
}
