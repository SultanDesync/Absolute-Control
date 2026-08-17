#include "NativeMenuProbe.h"

#if defined(ACP_ENABLE_RESEARCH_TOOLS)
#include "EvidenceLog.h"
#include "research/ResearchSupport.h"
#endif

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

#if defined(ACP_ENABLE_RESEARCH_TOOLS)
    const auto earlyResearchConfig =
        AbsoluteControlPanelResearch::ResearchSupport::LoadConfig(
            AbsoluteControlPanelResearch::NativeMenuProbe::kConfigPath);
    AbsoluteControlPanelResearch::EvidenceLog::Initialize(
        earlyResearchConfig.runId, {
            .minimumLevel = AbsoluteControlPanelResearch::EvidenceLog::Level::Trace,
            .queueCapacity = 16384
        });
    AbsoluteControlPanelResearch::ResearchSupport::StartTitleAdvance(
        earlyResearchConfig);
#endif

    const auto messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(&OnSfseMessage)) {
        REX::CRITICAL("Could not subscribe to SFSE messaging; native-menu probe disabled.");
        return false;
    }

    REX::INFO("Absolute Control Panel loaded; waiting for post-data-load.");
    return true;
}
