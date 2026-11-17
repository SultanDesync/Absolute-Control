#pragma once

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // One "key = value" line and the section it belongs to.
    struct SettingEntry
    {
        std::string section, key, value;
    };

    // Calls back with each "key = value" line of INI text and the section it sits in.
    void ForEachIniEntry(std::string_view text,
        const std::function<void(std::string_view section, std::string_view key, std::string_view value)>& entry);

    // INI values by section and key, without regard to case. Later values override earlier ones.
    class IniValues
    {
    public:
        void Merge(std::string_view text);
        void Set(std::string_view section, std::string_view key, std::string_view value);
        [[nodiscard]] std::optional<std::string> Get(std::string_view section, std::string_view key) const;
        // How many keys a section has with something set.
        [[nodiscard]] std::size_t CountSet(std::string_view section) const;

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
    // A head tracker's axes are named instead of numbered: OpenTrack@Yaw is channel axis/yaw.
    [[nodiscard]] ConfiguredBinding ParseAxisBinding(std::string_view actionId, std::string_view token);
    inline constexpr std::array<std::string_view, 3> kNamedAxes{ "yaw", "pitch", "roll" };
    // How an axis channel is written after the @ in a token: 0x32 for axis/2, Yaw for
    // axis/yaw. Nothing for a channel that isn't an axis.
    [[nodiscard]] std::optional<std::string> AxisTokenControl(std::string_view channel);
    // Absolute Control's own file: [ShipButtons] Enabled, and one key per action.
    [[nodiscard]] ShipBindingConfig LoadShipBindings(const IniValues& ini);
    [[nodiscard]] std::vector<SettingEntry> WriteShipBindings(const ShipBindingConfig& config);
    // The key an action is saved under, by catalog index. "Ship Action 1" is saved as what
    // it does.
    [[nodiscard]] std::string_view ShipSettingKey(std::size_t actionIndex) noexcept;
    // AbsoluteHOTAS's files, for the one-time import: [ShipButtons] i...Button keys.
    [[nodiscard]] ShipBindingConfig LoadHotasShipBindings(const IniValues& ini);
}
