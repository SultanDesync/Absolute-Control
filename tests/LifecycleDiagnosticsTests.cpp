#include "diagnostics/AsyncLineSink.h"
#include "runtime/CooperativeService.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#define CHECK(expression)            \
    do {                             \
        if (!(expression)) return 1; \
    } while (false)

namespace
{
    using namespace std::chrono_literals;
    using AbsoluteControlPanelResearch::Diagnostics::AsyncLineSink;
    using AbsoluteControlPanelResearch::Runtime::CooperativeService;
    using AbsoluteControlPanelResearch::Runtime::InterruptibleWait;

    std::string ReadProjectFile(const std::filesystem::path& a_relativePath)
    {
        auto root = std::filesystem::current_path();
        for (std::size_t depth{}; depth < 8 &&
             !std::filesystem::exists(root / a_relativePath); ++depth) {
            root = root.parent_path();
        }
        std::ifstream stream{ root / a_relativePath, std::ios::binary };
        return { std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>() };
    }

    template <class Predicate>
    bool WaitUntil(Predicate&& a_predicate, std::chrono::milliseconds a_timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + a_timeout;
        while (!a_predicate()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(1ms);
        }
        return true;
    }

    int TestServiceStartStopRestart()
    {
        CooperativeService service;
        std::atomic_uint32_t callbacks{};
        const auto worker = [&callbacks](std::stop_token a_stopToken,
                                const std::shared_ptr<
                                    AbsoluteControlPanelResearch::Runtime::CallbackGate>&
                                    a_gate) {
            while (!a_stopToken.stop_requested()) {
                (void)a_gate->TryInvoke([&callbacks] {
                    callbacks.fetch_add(1, std::memory_order_acq_rel);
                });
                if (!InterruptibleWait(a_stopToken, 1ms)) {
                    break;
                }
            }
        };

        CHECK(service.Start(worker));
        CHECK(WaitUntil([&] { return callbacks.load() >= 3; }, 1s));
        const auto firstGate = service.Gate();
        service.Stop();
        const auto stoppedAt = callbacks.load();
        std::this_thread::sleep_for(20ms);
        CHECK(callbacks.load() == stoppedAt);
        CHECK(firstGate != nullptr);
        CHECK(!firstGate->TryInvoke([&] { ++callbacks; }));

        CHECK(service.Start(worker));
        CHECK(WaitUntil([&] { return callbacks.load() > stoppedAt; }, 1s));
        service.Stop();
        const auto restartedStop = callbacks.load();
        std::this_thread::sleep_for(20ms);
        CHECK(callbacks.load() == restartedStop);

        // Stop is deliberately idempotent.
        service.Stop();
        CHECK(!service.IsRunning());
        return 0;
    }

    int TestStopWaitsForInFlightCallback()
    {
        CooperativeService service;
        std::atomic_bool entered{};
        std::atomic_bool release{};
        std::atomic_bool stopped{};
        CHECK(service.Start([&](std::stop_token,
                                const std::shared_ptr<
                                    AbsoluteControlPanelResearch::Runtime::CallbackGate>&
                                    a_gate) {
            (void)a_gate->TryInvoke([&] {
                entered.store(true, std::memory_order_release);
                while (!release.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            });
        }));
        CHECK(WaitUntil([&] { return entered.load(); }, 1s));

        std::thread stopper{ [&] {
            service.Stop();
            stopped.store(true, std::memory_order_release);
        } };
        std::this_thread::sleep_for(20ms);
        CHECK(!stopped.load(std::memory_order_acquire));
        release.store(true, std::memory_order_release);
        stopper.join();
        CHECK(stopped.load(std::memory_order_acquire));
        return 0;
    }

    int TestBoundedSinkConcurrencyFlushAndRestart()
    {
        const auto nonce = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto path = std::filesystem::temp_directory_path() /
            ("AbsoluteControlPanel.AsyncLineSink." + std::to_string(nonce) + ".jsonl");
        std::error_code error;
        std::filesystem::remove(path, error);

        AsyncLineSink sink;
        constexpr std::size_t kCapacity = 8;
        constexpr std::size_t kWriterCount = 8;
        constexpr std::size_t kRecordsPerWriter = 2000;
        CHECK(sink.Start(path, kCapacity));

        std::vector<std::thread> writers;
        writers.reserve(kWriterCount);
        for (std::size_t writer = 0; writer < kWriterCount; ++writer) {
            writers.emplace_back([&, writer] {
                for (std::size_t record = 0; record < kRecordsPerWriter; ++record) {
                    (void)sink.Enqueue(
                        "{\"writer\":" + std::to_string(writer) +
                        ",\"record\":" + std::to_string(record) + "}");
                }
            });
        }
        for (auto& writer : writers) {
            writer.join();
        }

        CHECK(sink.Flush(5s));
        const auto stats = sink.Statistics();
        CHECK(stats.accepted + stats.dropped ==
              kWriterCount * kRecordsPerWriter);
        CHECK(stats.dropped > 0);
        CHECK(stats.written == stats.accepted);
        CHECK(stats.queued == 0);
        CHECK(stats.ioFailures == 0);
        CHECK(sink.Shutdown(5s));
        CHECK(!sink.Enqueue("{\"after_stop\":true}"));

        std::ifstream stream{ path };
        std::size_t lines{};
        std::string line;
        while (std::getline(stream, line)) {
            ++lines;
        }
        CHECK(lines == stats.written);

        CHECK(sink.Start(path, 4));
        CHECK(sink.Enqueue("{\"restart\":true}"));
        CHECK(sink.Flush(5s));
        CHECK(sink.Statistics().written == 1);
        CHECK(sink.Shutdown(5s));

        std::filesystem::remove(path, error);
        return 0;
    }

    int TestSourceBoundaries()
    {
        const auto platform = ReadProjectFile("src/input/PlatformInputServices.cpp");
        const auto research = ReadProjectFile("src/research/ResearchSupport.cpp");
        const auto evidence = ReadProjectFile("src/EvidenceLog.cpp");
        const auto sink = ReadProjectFile("src/diagnostics/AsyncLineSink.cpp");
        CHECK(!platform.empty() && !research.empty() && !evidence.empty() && !sink.empty());
        CHECK(platform.find(".detach()") == std::string::npos);
        CHECK(research.find(".detach()") == std::string::npos);
        CHECK(platform.find("CooperativeService") != std::string::npos);
        CHECK(research.find("CooperativeService") != std::string::npos);
        CHECK(research.find("ReleaseActiveKeys") != std::string::npos);
        CHECK(evidence.find("std::ofstream stream") == std::string::npos);
        CHECK(evidence.find("\\\"level\\\"") != std::string::npos);
        CHECK(sink.find("outstanding >= state_->capacity") != std::string::npos);
        return 0;
    }
}

int main()
{
    if (TestServiceStartStopRestart() != 0) return 1;
    if (TestStopWaitsForInFlightCallback() != 0) return 1;
    if (TestBoundedSinkConcurrencyFlushAndRestart() != 0) return 1;
    if (TestSourceBoundaries() != 0) return 1;
    return 0;
}
