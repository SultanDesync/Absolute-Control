#include "EvidenceLog.h"

#include <SFSE/Logger.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>

namespace AbsoluteControlPanelResearch::EvidenceLog
{
    namespace
    {
        std::mutex g_lock;
        std::string g_runId{ "uninitialized" };
        std::filesystem::path g_path;
        std::atomic<std::uint64_t> g_sequence{};
        std::chrono::steady_clock::time_point g_startedAt =
            std::chrono::steady_clock::now();
        constexpr std::uintmax_t kMaximumEvidenceBytes = 8u * 1024u * 1024u;

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
    }

    void Initialize(std::string_view a_runId) noexcept
    {
        try {
            const std::scoped_lock lock{ g_lock };
            g_runId.assign(a_runId);
            g_sequence.store(0, std::memory_order_release);
            g_startedAt = std::chrono::steady_clock::now();
            g_path = std::filesystem::path{ "Data" } / "SFSE" / "Plugins" /
                     "AbsoluteControlPanel.evidence.jsonl";
            std::error_code directoryError;
            std::filesystem::create_directories(g_path.parent_path(), directoryError);
            if (directoryError) {
                if (const auto logDirectory = SFSE::log::log_directory()) {
                    g_path = *logDirectory /
                             "AbsoluteControlPanel.evidence.jsonl";
                    directoryError.clear();
                    std::filesystem::create_directories(
                        g_path.parent_path(), directoryError);
                }
            }
            if (!directoryError && !g_path.empty()) {
                std::error_code error;
                if (std::filesystem::exists(g_path, error) && !error &&
                    std::filesystem::file_size(g_path, error) > kMaximumEvidenceBytes &&
                    !error) {
                    const auto previous = g_path.parent_path() /
                                          "AbsoluteControlPanel.evidence.previous.jsonl";
                    std::filesystem::remove(previous, error);
                    error.clear();
                    std::filesystem::rename(g_path, previous, error);
                    if (error) {
                        std::ofstream truncate{ g_path, std::ios::trunc };
                    }
                }
            } else {
                g_path.clear();
            }
        } catch (...) {
            g_path.clear();
        }
    }

    void Event(std::string_view a_event, std::string_view a_detail) noexcept
    {
        try {
            const std::scoped_lock lock{ g_lock };
            const auto sequence =
                g_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
            const auto threadId = ::GetCurrentThreadId();
            const auto monotonicMicroseconds =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - g_startedAt)
                    .count();
            REX::INFO(
                "[probe:{} seq:{} tid:{}] {} {}", g_runId, sequence, threadId,
                a_event, a_detail);
            if (g_path.empty()) {
                return;
            }

            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
            std::ofstream stream{ g_path, std::ios::app };
            stream << "{\"timestamp_ms\":" << milliseconds
                   << ",\"monotonic_us\":" << monotonicMicroseconds
                   << ",\"sequence\":" << sequence << ",\"thread_id\":"
                   << threadId << ",\"run_id\":\"" << Escape(g_runId)
                   << "\",\"event\":\"" << Escape(a_event)
                   << "\",\"detail\":\"" << Escape(a_detail) << "\"}\n";
        } catch (...) {
            REX::ERROR("Could not append native-menu probe evidence.");
        }
    }

    std::filesystem::path Path() noexcept
    {
        const std::scoped_lock lock{ g_lock };
        return g_path;
    }
}
