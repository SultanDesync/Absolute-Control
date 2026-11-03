#include "NativeMenuProbe.h"
#include "ui/PauseMenuPresentation.h"

#include "EvidenceLog.h"
#include "ControlPanelModule.h"
#include "MenuApiHost.h"
#include "ProbeConfig.h"
#include "input/PlatformInputServices.h"
#include "interop/OSFUICompatibility.h"
#include "runtime/ProbeRuntimeState.h"
#include "runtime/RuntimeCompatibility.h"
#include "ui/ControlPanelMenu.h"
#include "ui/PauseMenuIntegration.h"

#if defined(ACP_ENABLE_RESEARCH_TOOLS)
#include "research/ResearchSupport.h"
#endif

#include <format>

namespace AbsoluteControlPanelResearch::NativeMenuProbe
{
    void OnEarlyMenus() noexcept
    {
        static bool initialized = false;
        if (initialized) return;
        initialized = true;
        Runtime::Transition(ProbeEvent::PluginLoaded);

        Runtime::SetConfig(LoadProbeConfig(
            kCustomConfigPath, LoadProbeConfig(kConfigPath)));
        const auto& config = Runtime::Config();

#if defined(ACP_ENABLE_RESEARCH_TOOLS)
        const auto researchConfig = ResearchSupport::LoadConfig(kConfigPath);
        EvidenceLog::Initialize(researchConfig.runId, {
            .minimumLevel = EvidenceLog::Level::Trace,
            .queueCapacity = 16384
        });
#else
        EvidenceLog::Initialize("manual", {
            .minimumLevel = EvidenceLog::Level::Info,
            .queueCapacity = 4096
        });
#endif
        EvidenceLog::Event(
            "config_loaded",
            std::format(
                "path={} registration={} pause_entry={} main_entry={} hotkey=0x{:02X} flags=0x{:08X}",
                kConfigPath.string(), config.enableRegistration,
                config.enablePauseMenuEntry, config.enableMainMenuEntry, config.openHotkey, config.menuFlags));
        // Install only the validated menu-boundary observer before MainMenu exists.
        // Factory, providers, input services and runtime readiness stay data-gated.
        if (config.enableRegistration && Runtime::ValidateMenuRelocations()) {
            Ui::PauseMenuPresentation::Configure(config.pauseMenuAutoFit, config.pauseMenuPurpleEntry, config.pauseMenuMatchFont);
            const bool installed = Ui::PauseMenuIntegration::InstallLifecycleHook(
                false, config.enableMainMenuEntry);
            EvidenceLog::Event("early_menu_attachment", std::format("installed={} activation_ready=false", installed));
        }
    }

    void OnDataReady() noexcept
    {
        OnEarlyMenus();
        Runtime::Transition(ProbeEvent::DataReady);
        const auto& config = Runtime::Config();
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
        const auto researchConfig = ResearchSupport::LoadConfig(kConfigPath);
#endif
        Interop::OSFUICompatibility::LogStartupState();

        if (!config.enableRegistration) {
            MenuApiHost::MarkRuntimeRejected();
            EvidenceLog::Event(
                "registration_disabled", "configuration requested fail-closed");
            return;
        }

        if (!Runtime::ValidateMenuRelocations()) {
            MenuApiHost::MarkRuntimeRejected();
            Runtime::Transition(ProbeEvent::RegistrationFailed);
            EvidenceLog::Event(
                "registration_failed", "native menu compatibility gate rejected runtime");
            return;
        }

        Ui::PauseMenuPresentation::Configure(config.pauseMenuAutoFit, config.pauseMenuPurpleEntry, config.pauseMenuMatchFont);
        if (!Ui::PauseMenuIntegration::InstallLifecycleHook(
                config.enablePauseMenuEntry, config.enableMainMenuEntry)) {
            EvidenceLog::Event(
                "pause_entry_unavailable",
                "optional lifecycle callsite unavailable; retaining native host, "
                "provider page opens and configured recovery hotkey");
        }

        if (!Ui::ControlPanelMenu::Register(config.menuFlags)) {
            MenuApiHost::MarkRuntimeRejected();
            Runtime::Transition(ProbeEvent::RegistrationFailed);
            EvidenceLog::Event(
                "registration_failed", "UI manager did not retain registration");
            return;
        }

        // The API remains discoverable-but-NotReady until runtime validation,
        // retention of the menu factory (whose
        // CreateMenu path owns the native Scaleform bridge) have all succeeded.
        MenuApiHost::SetOpenRequestWakeCallback(
            &Ui::ControlPanelMenu::RequestOpenFromProvider);
        MenuApiHost::MarkRuntimeReady();

        const bool controlModuleReady = ControlPanelModule::Register(
            config, kCustomConfigPath, {
                .setPauseMenuEntry = &Ui::PauseMenuIntegration::SetEntryEnabled,
                .setRecoveryHotkey = [](std::uint32_t hotkey) noexcept {
                    Input::StartOpenHotkey(hotkey,
                        &Ui::ControlPanelMenu::RequestOpenFromHotkey);
                }
            });
        if (!controlModuleReady) {
            EvidenceLog::Event("control_module_registration_failed",
                "release host settings and registry page unavailable");
        }

#if defined(ACP_ENABLE_RESEARCH_TOOLS)
        EvidenceLog::Event(
            "research_config_loaded",
            std::format(
                "auto_open={} require_arm={} advance_title={} run_id={}",
                researchConfig.autoOpen, researchConfig.requireArm,
                researchConfig.advanceTitleWithSendInput, researchConfig.runId));
#endif

        Runtime::Transition(ProbeEvent::RegistrationEnabled);
        Ui::PauseMenuIntegration::MarkHostReady();
        EvidenceLog::Event("registration_succeeded", kMenuName);
        Input::StartOpenHotkey(
            config.openHotkey, &Ui::ControlPanelMenu::RequestOpenFromHotkey);
        Input::StartPointerPolling(&Ui::ControlPanelMenu::DispatchPointerPhase);
        Ui::PauseMenuIntegration::LogRegistration(config.enablePauseMenuEntry);
        Ui::PauseMenuIntegration::AttachExistingMainMenu();
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
        ResearchSupport::Start(researchConfig);
#endif
    }

    ProbePhase Phase() noexcept
    {
        return Runtime::Phase();
    }

    bool IsRegistrationEnabled() noexcept
    {
        return Runtime::Config().enableRegistration;
    }
}
