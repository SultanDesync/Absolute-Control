#pragma once

#include "runtime/ShipBindings.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Absolute Control's settings: one file, and the only one it reads.
    //
    // The first time it starts with AbsoluteHOTAS files present and no file of its own,
    // those files are imported once into that file. They are never written, and never read
    // again. What couldn't be brought across is kept in the file's [Import] section so the
    // menu can show it on any later launch.
    class ControlSettings
    {
    public:
        struct ImportReport
        {
            bool imported{};          // This file began as an import.
            std::string from;         // The files it came from.
            std::string date;         // When, as YYYY-MM-DD.
            std::vector<std::string> dropped;
        };

        static ControlSettings& Instance();

        // walkSpeedFile is the file walk speed used to keep to itself: its [WalkSpeed]
        // section is copied in once, when this file has none. alwaysImport is for test
        // runs, which start from the same HOTAS-style files each time.
        void Load(std::filesystem::path file, const std::vector<std::filesystem::path>& hotasFiles,
            const std::filesystem::path& walkSpeedFile = {}, bool alwaysImport = false);

        [[nodiscard]] IniValues Merged() const;
        [[nodiscard]] std::optional<std::string> Get(std::string_view section, std::string_view key) const;

        // Sets one value. Empty text is a value too: it unbinds.
        void Set(std::string_view section, std::string_view key, std::string_view value);
        [[nodiscard]] bool Save() const;
        // Back to a new install's flight settings. Walk speed is kept. The file stays, so
        // nothing is imported again.
        [[nodiscard]] bool Reset();

        // Tells the control loop to load the settings again.
        void Publish() noexcept { generation_.fetch_add(1, std::memory_order_release); }
        [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_.load(std::memory_order_acquire); }

        [[nodiscard]] ImportReport Report() const;
        // "file", "imported" or "defaults", for the log.
        [[nodiscard]] std::string Origin() const;

    private:
        void SetLocked(std::string_view section, std::string_view key, std::string_view value);
        [[nodiscard]] bool SaveLocked() const;

        mutable std::mutex mutex_;
        IniValues merged_;
        std::vector<SettingEntry> entries_; // In the order they are written.
        std::filesystem::path file_;
        std::string origin_{ "defaults" };
        std::atomic<std::uint64_t> generation_{};
    };
}
