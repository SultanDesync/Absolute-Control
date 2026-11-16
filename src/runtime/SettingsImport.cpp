#include "runtime/SettingsImport.h"

#include "ShipActionCatalog.h"
#include "runtime/FlightAxes.h"
#include "runtime/ShipOutputs.h"

#include <array>
#include <format>
#include <string_view>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        std::string_view Trim(std::string_view text)
        {
            constexpr std::string_view kSpace = " \t\r\n";
            const auto first = text.find_first_not_of(kSpace);
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(kSpace) - first + 1);
        }

        // A binding that was set but is written in a form that can't be read any more, such
        // as a device picked by its position in a list.
        bool Unreadable(const ConfiguredBinding& binding)
        {
            const auto token = Trim(binding.token);
            return !token.empty() && token != "-1" && binding.status != BindingStatus::Bound;
        }
    }

    ImportedSettings ImportHotasSettings(const IniValues& hotas)
    {
        ImportedSettings result;
        const auto flight = LoadHotasFlightConfig(hotas);
        const auto buttons = LoadHotasShipBindings(hotas);

        result.entries = WriteFlightConfig(flight);
        auto buttonEntries = WriteShipBindings(buttons);
        const auto sync = hotas.Get("General", "bSyncShipOutputsFromControlMap");
        // Kept beside Enabled, ahead of the bindings.
        buttonEntries.insert(buttonEntries.begin() + 1,
            { "ShipButtons", "FollowGameRebinds", sync && Trim(*sync) == "false" ? "false" : "true" });
        // AbsoluteHOTAS switched the power buttons over in Targeting Mode; this is the nearest.
        buttonEntries.insert(buttonEntries.begin() + 2, { "ShipButtons", "CycleTargetSystemWith", "PowerButtons" });
        result.entries.insert(result.entries.end(), std::make_move_iterator(buttonEntries.begin()),
            std::make_move_iterator(buttonEntries.end()));

        // A key forced for one action, where the player didn't want the game's own binding.
        for (std::size_t index = 0; index < kShipActionCatalog.size(); ++index) {
            const auto text = hotas.Get("ShipButtonOutputs", kShipActionCatalog[index].legacyOutputIniKey);
            if (text && ParseShipOutput(*text)) {
                result.entries.push_back({ "ShipButtonKeys", std::string(ShipSettingKey(index)), std::string(Trim(*text)) });
            }
        }

        for (const auto& name : flight.ignored) {
            // "Section/Key" as the HOTAS file has it, so it can be found there.
            const auto slash = name.find('/');
            result.dropped.push_back(slash == std::string::npos ? std::format("{}: not in Absolute Control", name) :
                std::format("[{}] {}: this feature isn't in Absolute Control", name.substr(0, slash), name.substr(slash + 1)));
        }
        const std::array<std::pair<std::string_view, const ConfiguredBinding*>, 7> axes{ {
            { "Throttle", &flight.throttle }, { "Pitch", &flight.pitch }, { "Yaw", &flight.yaw }, { "Roll", &flight.roll },
            { "Strafe sideways", &flight.strafeLateral }, { "Strafe up and down", &flight.strafeVertical },
            { "Reverse", &flight.reverse },
        } };
        for (const auto& [name, binding] : axes) {
            if (Unreadable(*binding)) {
                result.dropped.push_back(std::format("{} axis \"{}\": bind it again in the menu", name, Trim(binding->token)));
            }
        }
        for (std::size_t index = 0; index < kShipActionCatalog.size() && index < buttons.bindings.size(); ++index) {
            if (Unreadable(buttons.bindings[index])) {
                result.dropped.push_back(std::format("{} button \"{}\": bind it again in the menu",
                    kShipActionCatalog[index].displayLabel, Trim(buttons.bindings[index].token)));
            }
        }
        return result;
    }
}
