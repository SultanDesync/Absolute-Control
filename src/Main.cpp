#include "NativeMenuProbe.h"
#include "runtime/BackendRuntime.h"
#include "runtime/ShipControlLoop.h"
#include "runtime/WalkSpeedRuntime.h"
#include "ui/WalkGaugeMenu.h"
#include "ui/WalkSpeedPage.h"

#if defined(ACP_ENABLE_RESEARCH_TOOLS)
#include "EvidenceLog.h"
#include "research/ResearchSupport.h"
#include "research/TestInputProvider.h"
#include "research/TestMenuPage.h"
#endif

namespace
{
    bool g_backendRunning = false;

    [[nodiscard]] bool UseTestInput()
    {
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
        return AbsoluteControlPanelResearch::ResearchSupport::LoadConfig(
            AbsoluteControlPanelResearch::NativeMenuProbe::kConfigPath).useTestInput;
#else
        return false;
#endif
    }

    [[nodiscard]] std::unique_ptr<AbsoluteControl::Runtime::InputProvider> MakeInputProvider()
    {
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
        if (UseTestInput()) return AbsoluteControlPanelResearch::TestInput::MakeProvider();
#endif
        return AbsoluteControl::Runtime::MakeDirectInputProvider();
    }

    [[nodiscard]] std::vector<std::filesystem::path> ShipBindingFiles()
    {
        if (UseTestInput()) return { "Data/SFSE/Plugins/AbsoluteControlTestBindings.ini" };
        return { "Data/SFSE/Plugins/AbsoluteHOTAS.ini", "Data/SFSE/Plugins/AbsoluteHOTAS_Custom.ini" };
    }

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
                } else if (!AbsoluteControl::Runtime::NativeBackend().Start(MakeInputProvider())) {
                    REX::ERROR("AC native device worker failed to start.");
                } else {
                    g_backendRunning = true;
                    REX::INFO("AC native device worker starting independently of presentation.");
                }
            }
            AbsoluteControlPanelResearch::NativeMenuProbe::OnEarlyMenus();
        } else if (a_message->type == SFSE::MessagingInterface::kPostDataLoad) {
            AbsoluteControlPanelResearch::NativeMenuProbe::OnDataReady();
            if (g_backendRunning) {
                // Test runs read fixed settings and save elsewhere. A saved copy in MO2's
                // overwrite folder would hide the fixed file, and real settings stay untouched.
                const bool testInput = UseTestInput();
                AbsoluteControl::Runtime::WalkSpeedRuntime::Install(testInput ?
                    "Data/SFSE/Plugins/AbsoluteControlTestWalkSpeed.ini" :
                    "Data/SFSE/Plugins/AbsoluteControlWalkSpeed.ini", testInput ?
                    "Data/SFSE/Plugins/AbsoluteControlTestWalkSpeed.saved.ini" :
                    "Data/SFSE/Plugins/AbsoluteControlWalkSpeed.ini");
                AbsoluteControlPanelResearch::Ui::WalkSpeedPage::Register();
                AbsoluteControlPanelResearch::Ui::WalkGaugeMenu::Register();
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
                if (testInput) AbsoluteControlPanelResearch::TestMenuPage::Register();
#endif
                AbsoluteControl::Runtime::StartShipControlLoop(ShipBindingFiles());
            }
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
