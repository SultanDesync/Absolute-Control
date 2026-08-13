#include "EvidenceLog.h"

#include <SFSE/Logger.h>

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
            if (const auto logDirectory = SFSE::log::log_directory()) {
                g_path = *logDirectory / "AbsoluteControlPanelResearch.evidence.jsonl";
                std::filesystem::create_directories(g_path.parent_path());
            }
        } catch (...) {
            g_path.clear();
        }
    }

    void Event(std::string_view a_event, std::string_view a_detail) noexcept
    {
        try {
            const std::scoped_lock lock{ g_lock };
            REX::INFO("[probe:{}] {} {}", g_runId, a_event, a_detail);
            if (g_path.empty()) {
                return;
            }

            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
            std::ofstream stream{ g_path, std::ios::app };
            stream << "{\"timestamp_ms\":" << milliseconds << ",\"run_id\":\""
                   << Escape(g_runId) << "\",\"event\":\"" << Escape(a_event)
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
