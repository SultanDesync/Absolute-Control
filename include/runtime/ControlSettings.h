#pragma once

#include "runtime/ShipBindings.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Flight and button settings. The AbsoluteHOTAS files are read and never written; what
    // the menu changes is kept in Absolute Control's own file, which is read last and so
    // wins. An existing HOTAS setup therefore works as it is, and stays as it was.
    class ControlSettings
    {
    public:
        static ControlSettings& Instance();

        void Load(const std::vector<std::filesystem::path>& baseFiles, std::filesystem::path ownFile);

        [[nodiscard]] IniValues Merged() const;
        [[nodiscard]] std::optional<std::string> Get(std::string_view section, std::string_view key) const;

        // Sets one value over everything loaded. Empty text is a value too: it unbinds.
        void Set(std::string_view section, std::string_view key, std::string_view value);
        // Writes every value set so far to Absolute Control's own file.
        [[nodiscard]] bool Save() const;

        // Tells the control loop to load the settings again.
        void Publish() noexcept { generation_.fetch_add(1, std::memory_order_release); }
        [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_.load(std::memory_order_acquire); }

        // The files that were found, for the log.
        [[nodiscard]] std::string Sources() const;

    private:
        struct Entry
        {
            std::string section, key, value;
        };

        mutable std::mutex mutex_;
        IniValues merged_;
        std::map<std::string, Entry> own_; // Keyed without regard to case; the entry keeps it.
        std::filesystem::path ownFile_;
        std::string sources_;
        std::atomic<std::uint64_t> generation_{};
    };
}
