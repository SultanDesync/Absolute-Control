#include "runtime/ControlSettings.h"

#include "runtime/SettingsImport.h"
#include "runtime/LookAxes.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        bool SameText(std::string_view left, std::string_view right) noexcept
        {
            return std::ranges::equal(left, right, [](char a, char b) {
                const auto lower = [](char value) {
                    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
                };
                return lower(a) == lower(b);
            });
        }

        std::optional<std::string> ReadText(const std::filesystem::path& file)
        {
            std::ifstream stream{ file, std::ios::binary };
            if (!stream) return {};
            return std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        }
    }

    ControlSettings& ControlSettings::Instance()
    {
        static ControlSettings instance;
        return instance;
    }

    void ControlSettings::SetLocked(std::string_view section, std::string_view key, std::string_view value)
    {
        merged_.Set(section, key, value);
        const auto found = std::ranges::find_if(entries_, [&](const SettingEntry& entry) {
            return SameText(entry.section, section) && SameText(entry.key, key);
        });
        if (found != entries_.end()) {
            found->value = std::string(value);
            return;
        }
        // A new key joins the rest of its section, so the file stays grouped.
        const auto last = std::find_if(entries_.rbegin(), entries_.rend(), [&](const SettingEntry& entry) {
            return SameText(entry.section, section);
        });
        SettingEntry entry{ std::string(section), std::string(key), std::string(value) };
        if (last == entries_.rend()) entries_.push_back(std::move(entry));
        else entries_.insert(last.base(), std::move(entry));
    }

    void ControlSettings::Load(std::filesystem::path file, const std::vector<std::filesystem::path>& hotasFiles,
        const std::filesystem::path& walkSpeedFile, bool alwaysImport, const std::filesystem::path& headTrackingFile)
    {
        const std::scoped_lock lock{ mutex_ };
        merged_ = {};
        entries_.clear();
        file_ = std::move(file);
        origin_ = "defaults";

        // Walk speed kept its own file before this one existed.
        const auto importWalkSpeed = [&] {
            const bool present = std::ranges::any_of(entries_, [](const SettingEntry& entry) {
                return SameText(entry.section, "WalkSpeed");
            });
            const auto text = present ? std::nullopt : ReadText(walkSpeedFile);
            if (!text) return false;
            bool copied{};
            ForEachIniEntry(*text, [&](std::string_view section, std::string_view key, std::string_view value) {
                if (!SameText(section, "WalkSpeed")) return;
                SetLocked(section, key, value);
                copied = true;
            });
            return copied;
        };
        // Head tracking was its own mod. Its settings come across once, when this file has no
        // look settings yet; after that its file is never read.
        const auto importHeadTracking = [&] {
            const bool present = std::ranges::any_of(entries_, [](const SettingEntry& entry) {
                return SameText(entry.section, "Look") || SameText(entry.section, "LookYaw");
            });
            const auto text = present ? std::nullopt : ReadText(headTrackingFile);
            if (!text) return false;
            IniValues headTracking;
            headTracking.Merge(*text);
            for (const auto& entry : WriteLookConfig(LoadHeadTrackingLookConfig(headTracking))) {
                SetLocked(entry.section, entry.key, entry.value);
            }
            SetLocked("Import", "HeadTracking", headTrackingFile.filename().string());
            return true;
        };

        if (!alwaysImport) {
            if (const auto text = ReadText(file_)) {
                ForEachIniEntry(*text, [this](std::string_view section, std::string_view key, std::string_view value) {
                    SetLocked(section, key, value);
                });
                origin_ = "file";
                const bool walk = importWalkSpeed();
                if (importHeadTracking() || walk) static_cast<void>(SaveLocked());
                return;
            }
        }

        IniValues hotas;
        std::string from;
        for (const auto& source : hotasFiles) {
            if (const auto text = ReadText(source)) {
                hotas.Merge(*text);
                if (!from.empty()) from += ", ";
                from += source.filename().string();
            }
        }
        if (from.empty()) {
            // Nothing from HOTAS. The file is only written if walk speed or head tracking had
            // settings to keep.
            const bool walk = importWalkSpeed();
            if (importHeadTracking() || walk) static_cast<void>(SaveLocked());
            return;
        }

        const auto imported = ImportHotasSettings(hotas);
        for (const auto& entry : imported.entries) SetLocked(entry.section, entry.key, entry.value);
        SetLocked("Import", "From", from);
        SetLocked("Import", "Date", std::format("{:%F}", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())));
        SetLocked("Import", "NotCarried", std::to_string(imported.dropped.size()));
        for (std::size_t index = 0; index < imported.dropped.size(); ++index) {
            SetLocked("Import", std::format("NotCarried{}", index + 1), imported.dropped[index]);
        }
        origin_ = "imported";
        importWalkSpeed();
        importHeadTracking();
        // Written straight away, so the import happens once even if nothing is changed.
        static_cast<void>(SaveLocked());
    }

    IniValues ControlSettings::Merged() const
    {
        const std::scoped_lock lock{ mutex_ };
        return merged_;
    }

    std::optional<std::string> ControlSettings::Get(std::string_view section, std::string_view key) const
    {
        const std::scoped_lock lock{ mutex_ };
        return merged_.Get(section, key);
    }

    void ControlSettings::Set(std::string_view section, std::string_view key, std::string_view value)
    {
        const std::scoped_lock lock{ mutex_ };
        SetLocked(section, key, value);
    }

    bool ControlSettings::Save() const
    {
        const std::scoped_lock lock{ mutex_ };
        return SaveLocked();
    }

    bool ControlSettings::SaveLocked() const
    {
        if (file_.empty()) return false;
        std::string text = "; Absolute Control settings. This is the only settings file Absolute Control reads.\n"
            "; Change these in the game's menu: Absolute Control, then Throttle, Flight Axes or Ship Buttons.\n"
            "; If something seems broken after an update or an import, delete this file and set up again\n"
            "; from the menu. A fresh setup is more reliable than a repaired one.\n";
        std::string_view section;
        for (const auto& entry : entries_) {
            if (!SameText(entry.section, section)) {
                section = entry.section;
                text += std::format("\n[{}]\n", section);
            }
            text += std::format("{} = {}\n", entry.key, entry.value);
        }

        // Written beside the real file and swapped in, so a failed write leaves the old one.
        auto temporary = file_;
        temporary += ".tmp";
        {
            std::ofstream stream{ temporary, std::ios::binary | std::ios::trunc };
            if (!stream) return false;
            stream << text;
            if (!stream.flush()) return false;
        }
        std::error_code error;
        std::filesystem::rename(temporary, file_, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }

    bool ControlSettings::Reset()
    {
        const std::scoped_lock lock{ mutex_ };
        std::vector<SettingEntry> kept;
        for (auto& entry : entries_) {
            if (SameText(entry.section, "WalkSpeed")) kept.push_back(std::move(entry));
        }
        merged_ = {};
        entries_.clear();
        for (const auto& entry : kept) SetLocked(entry.section, entry.key, entry.value);
        origin_ = "defaults";
        return SaveLocked();
    }

    ControlSettings::ImportReport ControlSettings::Report() const
    {
        const std::scoped_lock lock{ mutex_ };
        ImportReport report;
        const auto from = merged_.Get("Import", "From");
        if (!from) return report;
        report.imported = true;
        report.from = *from;
        report.date = merged_.Get("Import", "Date").value_or("");
        for (std::size_t index = 1; index <= 64; ++index) {
            const auto line = merged_.Get("Import", std::format("NotCarried{}", index));
            if (!line) break;
            report.dropped.push_back(*line);
        }
        return report;
    }

    std::string ControlSettings::Origin() const
    {
        const std::scoped_lock lock{ mutex_ };
        return origin_;
    }
}
