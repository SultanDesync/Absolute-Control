#include "runtime/ControlSettings.h"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        std::string Lower(std::string_view text)
        {
            std::string result(text);
            for (char& value : result) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return result;
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

    void ControlSettings::Load(const std::vector<std::filesystem::path>& baseFiles, std::filesystem::path ownFile)
    {
        const std::scoped_lock lock{ mutex_ };
        merged_ = {};
        own_.clear();
        sources_.clear();
        ownFile_ = std::move(ownFile);
        const auto note = [&](const std::filesystem::path& file) {
            if (!sources_.empty()) sources_ += ',';
            sources_ += file.filename().string();
        };
        for (const auto& file : baseFiles) {
            if (const auto text = ReadText(file)) {
                merged_.Merge(*text);
                note(file);
            }
        }
        if (const auto text = ReadText(ownFile_)) {
            ForEachIniEntry(*text, [&](std::string_view section, std::string_view key, std::string_view value) {
                own_[Lower(section) + '\n' + Lower(key)] = { std::string(section), std::string(key), std::string(value) };
                merged_.Set(section, key, value);
            });
            note(ownFile_);
        }
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
        own_[Lower(section) + '\n' + Lower(key)] = { std::string(section), std::string(key), std::string(value) };
        merged_.Set(section, key, value);
    }

    bool ControlSettings::Save() const
    {
        const std::scoped_lock lock{ mutex_ };
        if (ownFile_.empty()) return false;
        std::string text = "; Absolute Control settings changed in the menu.\n"
            "; These override AbsoluteHOTAS.ini and AbsoluteHOTAS_Custom.ini, which are never written.\n";
        std::string section;
        // The map is ordered by section, then key, so each section is written once.
        for (const auto& [id, entry] : own_) {
            if (Lower(entry.section) != Lower(section)) {
                section = entry.section;
                text += "\n[" + section + "]\n";
            }
            text += entry.key + " = " + entry.value + '\n';
        }

        // Written beside the real file and swapped in, so a failed write leaves the old one.
        auto temporary = ownFile_;
        temporary += ".tmp";
        {
            std::ofstream stream{ temporary, std::ios::binary | std::ios::trunc };
            if (!stream) return false;
            stream << text;
            if (!stream.flush()) return false;
        }
        std::error_code error;
        std::filesystem::rename(temporary, ownFile_, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }

    std::string ControlSettings::Sources() const
    {
        const std::scoped_lock lock{ mutex_ };
        return sources_;
    }
}
