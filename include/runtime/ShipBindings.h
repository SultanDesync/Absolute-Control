#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Read-only view of the existing HOTAS INI files. Later files override earlier ones.
    class IniValues
    {
    public:
        void Merge(std::string_view text);
        [[nodiscard]] std::optional<std::string> Get(std::string_view section, std::string_view key) const;

    private:
        std::map<std::string, std::string> values_;
    };

    enum class BindingStatus { Unbound, Bound, DeviceMissing, DeviceAmbiguous, ChannelMissing, Unsupported, Invalid };
    [[nodiscard]] std::string_view ToString(BindingStatus status) noexcept;

    struct ConfiguredBinding
    {
        std::string actionId;
        std::string token; // The saved text, kept for reports.
        BindingStatus status{ BindingStatus::Unbound }; // Bound here means the token parsed.
        bool devicePhysicalId{}; // device is an uppercase GUID rather than a display name.
        std::string device;
        std::string channel;
    };

    struct ShipBindingConfig
    {
        bool enabled{ true };
        std::vector<ConfiguredBinding> bindings; // One per catalog action, in catalog order.
    };

    // Reads Device Name@N, {GUID}@button:N and guid:{GUID}@N. Old #index@N and bare N
    // tokens depend on enumeration order or a default device, so they stay Unsupported.
    [[nodiscard]] ConfiguredBinding ParseShipBinding(std::string_view actionId, std::string_view token);
    // The same forms for an axis: Device Name@0x32, {GUID}@axis:0x32 and guid:{GUID}@0x32.
    [[nodiscard]] ConfiguredBinding ParseAxisBinding(std::string_view actionId, std::string_view token);
    [[nodiscard]] ShipBindingConfig LoadShipBindings(const IniValues& ini);
}
