#include "NativeMenuProbe.h"

namespace
{
    void OnSfseMessage(SFSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        if (a_message->type == SFSE::MessagingInterface::kPostDataLoad) {
            AbsoluteControlPanelResearch::NativeMenuProbe::OnDataReady();
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 64 });

    const auto messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(&OnSfseMessage)) {
        REX::CRITICAL("Could not subscribe to SFSE messaging; native-menu probe disabled.");
        return false;
    }

    REX::INFO("Absolute Control Panel loaded; waiting for post-data-load.");
    return true;
}
