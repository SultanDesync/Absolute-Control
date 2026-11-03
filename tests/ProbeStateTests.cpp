#include "ProbeState.h"

#include <cassert>

using AbsoluteControlPanelResearch::Advance;
using AbsoluteControlPanelResearch::ProbeEvent;
using AbsoluteControlPanelResearch::ProbePhase;

int main()
{
    auto phase = ProbePhase::Cold;
    phase = Advance(phase, ProbeEvent::PluginLoaded);
    assert(phase == ProbePhase::WaitingForData);

    phase = Advance(phase, ProbeEvent::DataReady);
    assert(phase == ProbePhase::RegistrationDisabled);

    phase = Advance(phase, ProbeEvent::RegistrationEnabled);
    assert(phase == ProbePhase::Registered);

    phase = Advance(phase, ProbeEvent::MenuOpened);
    assert(phase == ProbePhase::Open);

    phase = Advance(phase, ProbeEvent::MenuClosed);
    assert(phase == ProbePhase::Registered);

    phase = Advance(phase, ProbeEvent::RuntimeFault);
    assert(phase == ProbePhase::Faulted);
    assert(Advance(phase, ProbeEvent::DataReady) == ProbePhase::Faulted);

    assert(Advance(ProbePhase::Cold, ProbeEvent::MenuOpened) == ProbePhase::Cold);
    assert(Advance(ProbePhase::RegistrationDisabled, ProbeEvent::RegistrationFailed) ==
           ProbePhase::Faulted);
}
