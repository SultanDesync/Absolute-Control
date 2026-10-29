#include "NativeMenuProbe.h"
#include "runtime/BackendRuntime.h"

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

        if (a_message->type == SFSE::MessagingInterface::kPostPostLoad) {
            // Runtime lifetime does not depend on menu relocation validation,
            // factory registration, configuration or Scaleform availability.
            static bool backendStarted = false;
            if (!backendStarted) {
                backendStarted = true;
                if (GetModuleHandleW(L"AbsoluteControlPanel.dll") && GetModuleHandleW(L"AbsoluteControlPanelResearchDev.dll")) {
                    REX::ERROR("AC native device backend unavailable: both product and research hosts are loaded.");
                } else if (GetModuleHandleW(L"AbsoluteHOTAS.dll")) {
                    REX::ERROR("AC native device backend unavailable: unadapted AbsoluteHOTAS.dll loaded. Use the native development cohort; no legacy fallback is selected.");
                } else if (!AbsoluteControl::Runtime::NativeBackend().Start(
                        AbsoluteControl::Runtime::MakeDirectInputProvider())) {
                    REX::ERROR("AC native device worker failed to start.");
                } else {
                    REX::INFO("AC native device worker starting independently of presentation.");
                }
            }
            AbsoluteControlPanelResearch::NativeMenuProbe::OnEarlyMenus();
        } else if (a_message->type == SFSE::MessagingInterface::kPostDataLoad) {
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

    REX::INFO("Absolute Control Panel loaded; early menu attachment at post-post-load, host at post-data-load.");
    return true;
}
