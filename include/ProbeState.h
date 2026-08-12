#pragma once

#include <cstdint>
#include <string_view>

namespace AbsoluteControlPanelResearch
{
    enum class ProbePhase : std::uint8_t
    {
        Cold,
        WaitingForData,
        RegistrationDisabled,
        Registered,
        Open,
        Faulted
    };

    enum class ProbeEvent : std::uint8_t
    {
        PluginLoaded,
        DataReady,
        RegistrationEnabled,
        RegistrationFailed,
        MenuOpened,
        MenuClosed,
        RuntimeFault
    };

    [[nodiscard]] constexpr ProbePhase Advance(ProbePhase a_phase, ProbeEvent a_event) noexcept
    {
        switch (a_phase) {
        case ProbePhase::Cold:
            return a_event == ProbeEvent::PluginLoaded ? ProbePhase::WaitingForData : a_phase;
        case ProbePhase::WaitingForData:
            return a_event == ProbeEvent::DataReady ? ProbePhase::RegistrationDisabled :
                   a_event == ProbeEvent::RuntimeFault ? ProbePhase::Faulted : a_phase;
        case ProbePhase::RegistrationDisabled:
            return a_event == ProbeEvent::RegistrationEnabled ? ProbePhase::Registered :
                   a_event == ProbeEvent::RegistrationFailed || a_event == ProbeEvent::RuntimeFault ?
                       ProbePhase::Faulted : a_phase;
        case ProbePhase::Registered:
            return a_event == ProbeEvent::MenuOpened ? ProbePhase::Open :
                   a_event == ProbeEvent::RuntimeFault ? ProbePhase::Faulted : a_phase;
        case ProbePhase::Open:
            return a_event == ProbeEvent::MenuClosed ? ProbePhase::Registered :
                   a_event == ProbeEvent::RuntimeFault ? ProbePhase::Faulted : a_phase;
        case ProbePhase::Faulted:
            return ProbePhase::Faulted;
        }
        return ProbePhase::Faulted;
    }

    [[nodiscard]] constexpr std::string_view PhaseName(ProbePhase a_phase) noexcept
    {
        switch (a_phase) {
        case ProbePhase::Cold: return "cold";
        case ProbePhase::WaitingForData: return "waiting-for-data";
        case ProbePhase::RegistrationDisabled: return "registration-disabled";
        case ProbePhase::Registered: return "registered";
        case ProbePhase::Open: return "open";
        case ProbePhase::Faulted: return "faulted";
        }
        return "unknown";
    }
}
