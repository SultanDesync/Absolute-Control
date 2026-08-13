#include "ProbeConfig.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <string_view>

namespace AbsoluteControlPanelResearch
{
    namespace
    {
        std::string_view Trim(std::string_view a_value) noexcept
        {
            const auto isSpace = [](const unsigned char a_character) {
                return std::isspace(a_character) != 0;
            };
            while (!a_value.empty() && isSpace(static_cast<unsigned char>(a_value.front()))) {
                a_value.remove_prefix(1);
            }
            while (!a_value.empty() && isSpace(static_cast<unsigned char>(a_value.back()))) {
                a_value.remove_suffix(1);
            }
            return a_value;
        }

        bool ParseBool(std::string_view a_value, bool a_fallback) noexcept
        {
            std::string normalized{ Trim(a_value) };
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char a_character) {
                return static_cast<char>(std::tolower(a_character));
            });
            if (normalized == "1" || normalized == "true" || normalized == "yes" ||
                normalized == "on") {
                return true;
            }
            if (normalized == "0" || normalized == "false" || normalized == "no" ||
                normalized == "off") {
                return false;
            }
            return a_fallback;
        }

        std::uint32_t ParseUnsigned(
            std::string_view a_value, std::uint32_t a_fallback) noexcept
        {
            a_value = Trim(a_value);
            int base = 10;
            if (a_value.starts_with("0x") || a_value.starts_with("0X")) {
                base = 16;
                a_value.remove_prefix(2);
            }

            std::uint32_t parsed{};
            const auto [end, error] =
                std::from_chars(a_value.data(), a_value.data() + a_value.size(), parsed, base);
            return error == std::errc{} && end == a_value.data() + a_value.size() ? parsed :
                                                                                   a_fallback;
        }
    }

    ProbeConfig LoadProbeConfig(const std::filesystem::path& a_path) noexcept
    {
        ProbeConfig config;
        std::ifstream stream{ a_path };
        if (!stream) {
            return config;
        }

        std::string line;
        while (std::getline(stream, line)) {
            const auto trimmed = Trim(line);
            if (trimmed.empty() || trimmed.starts_with('#') || trimmed.starts_with(';') ||
                trimmed.starts_with('[')) {
                continue;
            }

            const auto separator = trimmed.find('=');
            if (separator == std::string_view::npos) {
                continue;
            }

            const auto key = Trim(trimmed.substr(0, separator));
            const auto value = Trim(trimmed.substr(separator + 1));
            if (key == "RunId") {
                config.runId.assign(value);
            } else if (key == "EnableRegistration") {
                config.enableRegistration = ParseBool(value, config.enableRegistration);
            } else if (key == "AutoOpen") {
                config.autoOpen = ParseBool(value, config.autoOpen);
            } else if (key == "RequireArm") {
                config.requireArm = ParseBool(value, config.requireArm);
            } else if (key == "AdvanceTitleWithSendInput") {
                config.advanceTitleWithSendInput =
                    ParseBool(value, config.advanceTitleWithSendInput);
            } else if (key == "ArmTimeoutMilliseconds") {
                config.armTimeoutMilliseconds =
                    ParseUnsigned(value, config.armTimeoutMilliseconds);
            } else if (key == "OpenDelayMilliseconds") {
                config.openDelayMilliseconds =
                    ParseUnsigned(value, config.openDelayMilliseconds);
            } else if (key == "VisibleMilliseconds") {
                config.visibleMilliseconds = ParseUnsigned(value, config.visibleMilliseconds);
            } else if (key == "MenuFlags") {
                config.menuFlags = ParseUnsigned(value, config.menuFlags);
            }
        }

        config.openDelayMilliseconds =
            std::clamp(config.openDelayMilliseconds, 1000u, 120000u);
        config.armTimeoutMilliseconds =
            std::clamp(config.armTimeoutMilliseconds, 1000u, 600000u);
        config.visibleMilliseconds = std::clamp(config.visibleMilliseconds, 2000u, 30000u);
        if (config.runId.empty()) {
            config.runId = "manual";
        }
        return config;
    }
}
