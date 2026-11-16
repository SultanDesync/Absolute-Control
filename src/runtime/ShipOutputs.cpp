#include "runtime/ShipOutputs.h"

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        std::string TrimLower(std::string_view text)
        {
            constexpr std::string_view kSpace = " \t\r\n";
            const auto first = text.find_first_not_of(kSpace);
            if (first == std::string_view::npos) return {};
            std::string result(text.substr(first, text.find_last_not_of(kSpace) - first + 1));
            for (char& value : result) {
                if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
            }
            return result;
        }

        std::optional<std::uint16_t> ParseCode(std::string_view text)
        {
            int base = 10;
            if (text.starts_with("0x")) {
                text.remove_prefix(2);
                base = 16;
            }
            std::uint16_t value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            if (text.empty() || error != std::errc{} || end != text.data() + text.size()) return {};
            return value;
        }

        bool SameOutput(const ShipActionOutputSpec& left, const ShipActionOutputSpec& right) noexcept
        {
            return left.kind == right.kind && left.code == right.code && left.extended == right.extended;
        }
    }

    std::optional<ShipActionOutputSpec> ParseShipOutput(std::string_view text)
    {
        const auto normalized = TrimLower(text);
        const std::string_view value{ normalized };
        if (value == "none") return ShipActionOutputSpec{};
        if (value.starts_with("key:")) {
            const auto code = ParseCode(value.substr(4));
            if (!code || *code == 0) return {};
            // The arrow keys' scan codes, which the numpad shares without the extended flag.
            const bool extended = *code == 0x48 || *code == 0x50 || *code == 0x4B || *code == 0x4D;
            return ShipActionOutputSpec{ ShipActionOutputKind::Keyboard, *code, extended };
        }
        if (value.starts_with("mouse:")) {
            const auto code = ParseCode(value.substr(6));
            if (!code || *code < 1 || *code > 4) return {};
            return ShipActionOutputSpec{ ShipActionOutputKind::Mouse, *code, false };
        }
        return {};
    }

    std::vector<ResolvedShipOutput> ResolveShipOutputs(const IniValues& ini,
        std::span<const ControlMap::Record> records)
    {
        const auto sync = ini.Get("General", "bSyncShipOutputsFromControlMap");
        const bool followRebinds = !sync || TrimLower(*sync) != "false";

        std::vector<ResolvedShipOutput> outputs;
        outputs.reserve(kShipActionCatalog.size());
        for (const auto& action : kShipActionCatalog) {
            if (action.recommendedMethod == ShipControlMethod::Context) {
                outputs.push_back({ action.vanillaOutput, KeyboardResolutionSource::FixedContext });
                continue;
            }
            ResolvedShipOutput resolved{ action.vanillaOutput, KeyboardResolutionSource::VanillaFallback };
            if (followRebinds && ControlMap::HasPrimary(records, action.controlMapContext, action.controlMapAction)) {
                resolved.output = ControlMap::ResolveBinding(records, action.controlMapContext,
                    action.controlMapAction, action.vanillaOutput);
                resolved.source = KeyboardResolutionSource::ControlMapCustom;
            }
            if (const auto text = ini.Get("ShipButtonOutputs", action.legacyOutputIniKey)) {
                if (const auto parsed = ParseShipOutput(*text)) {
                    resolved.output = *parsed;
                    resolved.source = KeyboardResolutionSource::LegacyManualOverride;
                }
            }
            outputs.push_back(resolved);
        }
        return outputs;
    }

    HeldOutputs::HeldOutputs(Emit emit) : emit_(std::move(emit)) {}

    void HeldOutputs::Set(const ShipActionOutputSpec& output, std::uint32_t owner, bool held)
    {
        if (output.kind == ShipActionOutputKind::None) return;
        const auto found = std::ranges::find_if(held_, [&](const Held& entry) {
            return SameOutput(entry.output, output);
        });
        if (held) {
            if (found == held_.end()) {
                held_.push_back({ output, { owner } });
                emit_(output, true);
            } else if (std::ranges::find(found->owners, owner) == found->owners.end()) {
                found->owners.push_back(owner);
            }
            return;
        }
        if (found == held_.end()) return;
        std::erase(found->owners, owner);
        if (found->owners.empty()) {
            const auto released = found->output;
            held_.erase(found);
            emit_(released, false);
        }
    }

    void HeldOutputs::ReleaseAll()
    {
        const auto held = std::exchange(held_, {});
        for (const auto& entry : held) emit_(entry.output, false);
    }
}
