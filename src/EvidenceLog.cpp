#include "EvidenceLog.h"

#include <SFSE/Logger.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace AbsoluteControlPanelResearch::EvidenceLog
{
    namespace
    {
        constexpr std::uintmax_t kMaximumEvidenceBytes = 8u * 1024u * 1024u;

        struct Context
        {
            std::string runId;
            std::filesystem::path path;
            Level minimumLevel{ Level::Info };
            std::chrono::steady_clock::time_point startedAt =
                std::chrono::steady_clock::now();
            std::atomic<std::uint64_t> sequence{};
            Diagnostics::AsyncLineSink sink;
        };

        struct ProcessState
        {
            std::mutex lock;
            std::shared_ptr<Context> current;
            // A timed-out sink remains owned instead of forcing a join on a game
            // thread. Process termination reclaims these rare retired contexts.
            std::vector<std::shared_ptr<Context>> retired;
        };

        ProcessState& State() noexcept
        {
            // SFSE exposes no reliable unload notification. An intentional
            // process-lifetime owner avoids joining a worker from static
            // destruction while the Windows loader lock is held.
            static auto* state = new ProcessState{};
            return *state;
        }

        std::string Escape(std::string_view a_value)
        {
            std::string escaped;
            escaped.reserve(a_value.size());
            for (const char character : a_value) {
                switch (character) {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\r': escaped += "\\r"; break;
                case '\n': escaped += "\\n"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += character; break;
                }
            }
            return escaped;
        }

        std::string_view LevelName(Level a_level) noexcept
        {
            switch (a_level) {
            case Level::Trace: return "trace";
            case Level::Info: return "info";
            case Level::Warning: return "warning";
            case Level::Error: return "error";
            }
            return "info";
        }

        std::shared_ptr<Context> Current() noexcept
        {
            const std::scoped_lock lock{ State().lock };
            return State().current;
        }

        std::filesystem::path ResolvePath(const Options& a_options) noexcept
        {
            if (!a_options.pathOverride.empty()) {
                return a_options.pathOverride;
            }
            auto path = std::filesystem::path{};
            if (const auto logDirectory = SFSE::log::log_directory()) {
                path = *logDirectory / "AbsoluteControlPanel.evidence.jsonl";
            } else {
                path = std::filesystem::path{ "Data" } / "SFSE" / "Plugins" /
                       "AbsoluteControlPanel.evidence.jsonl";
            }
            std::error_code directoryError;
            std::filesystem::create_directories(path.parent_path(), directoryError);
            return directoryError ? std::filesystem::path{} : path;
        }

        void RotateIfNeeded(const std::filesystem::path& a_path) noexcept
        {
            if (a_path.empty()) {
                return;
            }
            std::error_code error;
            if (!std::filesystem::exists(a_path, error) || error ||
                std::filesystem::file_size(a_path, error) <= kMaximumEvidenceBytes || error) {
                return;
            }
            const auto previous = a_path.parent_path() /
                                  "AbsoluteControlPanel.evidence.previous.jsonl";
            std::filesystem::remove(previous, error);
            error.clear();
            std::filesystem::rename(a_path, previous, error);
            if (error) {
                std::ofstream truncate{ a_path, std::ios::trunc };
            }
        }
    }

    void Initialize(std::string_view a_runId, Options a_options) noexcept
    {
        try {
            auto context = std::make_shared<Context>();
            context->runId.assign(a_runId.empty() ? "manual" : a_runId);
            context->minimumLevel = a_options.minimumLevel;
            context->startedAt = std::chrono::steady_clock::now();
            context->path = ResolvePath(a_options);
            RotateIfNeeded(context->path);
            if (!context->path.empty() &&
                !context->sink.Start(context->path, a_options.queueCapacity)) {
                context->path.clear();
            }

            std::shared_ptr<Context> previous;
            {
                const std::scoped_lock lock{ State().lock };
                previous = std::exchange(State().current, context);
            }
            if (previous && !previous->sink.Shutdown(std::chrono::seconds(2))) {
                const std::scoped_lock lock{ State().lock };
                State().retired.push_back(std::move(previous));
            }
        } catch (...) {
            REX::ERROR("Could not initialize native-menu evidence sink.");
        }
    }

    void Event(std::string_view a_event, std::string_view a_detail, Level a_level) noexcept
    {
        try {
            const auto context = Current();
            if (!context || a_level < context->minimumLevel) {
                return;
            }

            const auto sequence =
                context->sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
            const auto threadId = ::GetCurrentThreadId();
            const auto monotonicMicroseconds =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - context->startedAt)
                    .count();
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            auto line = std::format(
                "{{\"timestamp_ms\":{},\"monotonic_us\":{},\"sequence\":{},"
                "\"thread_id\":{},\"run_id\":\"{}\",\"level\":\"{}\","
                "\"event\":\"{}\",\"detail\":\"{}\"}}",
                milliseconds, monotonicMicroseconds, sequence, threadId,
                Escape(context->runId), LevelName(a_level), Escape(a_event),
                Escape(a_detail));
            (void)context->sink.Enqueue(std::move(line));
        } catch (...) {
            // Evidence production is best effort and must never fault game code.
        }
    }

    void Trace(std::string_view a_event, std::string_view a_detail) noexcept
    {
        Event(a_event, a_detail, Level::Trace);
    }

    bool Flush(std::chrono::milliseconds a_timeout) noexcept
    {
        const auto context = Current();
        return !context || context->sink.Flush(a_timeout);
    }

    bool Shutdown(std::chrono::milliseconds a_timeout) noexcept
    {
        std::shared_ptr<Context> context;
        {
            const std::scoped_lock lock{ State().lock };
            context = std::exchange(State().current, {});
        }
        if (!context || context->sink.Shutdown(a_timeout)) {
            return true;
        }
        const std::scoped_lock lock{ State().lock };
        State().retired.push_back(std::move(context));
        return false;
    }

    Diagnostics::AsyncLineSinkStatistics Statistics() noexcept
    {
        const auto context = Current();
        return context ? context->sink.Statistics() :
                         Diagnostics::AsyncLineSinkStatistics{};
    }

    std::filesystem::path Path() noexcept
    {
        const auto context = Current();
        return context ? context->path : std::filesystem::path{};
    }
}
