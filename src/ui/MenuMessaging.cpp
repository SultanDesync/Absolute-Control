#include "ui/MenuMessaging.h"

#include "EvidenceLog.h"
#include "NativeMenuProbe.h"
#include "runtime/ProbeRuntimeState.h"
#include "ui/PauseMenuIntegration.h"

#include <format>

namespace AbsoluteControlPanelResearch::Ui
{
    void QueueNamedMenuMessage(std::string_view a_menuName,
        RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept
    {
        const auto queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) {
            EvidenceLog::Event("menu_message_rejected", "UIMessageQueue unavailable");
            Runtime::Transition(ProbeEvent::RuntimeFault);
            return;
        }

        const auto result =
            queue->AddMessage(RE::BSFixedString(a_menuName.data()), a_type);
        EvidenceLog::Event(
            "named_menu_message_requested",
            std::format(
                "menu={} type={} source={} result={}", a_menuName,
                static_cast<std::uint32_t>(a_type), a_source, result));
    }

    void QueueControlPanelMessage(
        RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept
    {
        if (a_type == RE::UI_MESSAGE_TYPE::kShow) {
            if (!RE::UIMessageQueue::GetSingleton()) {
                QueueNamedMenuMessage(
                    NativeMenuProbe::kMenuName, a_type, a_source);
                return;
            }
            const auto ui = RE::UI::GetSingleton();
            if (ui && ui->IsMenuOpen(
                    RE::BSFixedString(NativeMenuProbe::kMenuName.data()))) {
                EvidenceLog::Event(
                    "control_panel_show_ignored",
                    std::format("source={} reason=already-open", a_source));
                return;
            }
            const bool openedFromPause = a_source == "pause-entry";
            const bool originAccepted =
                PauseMenuIntegration::SetReturnToPauseOnClose(openedFromPause);
            EvidenceLog::Event(
                "control_panel_open_origin",
                std::format(
                    "source={} return_target={} accepted={}", a_source,
                    openedFromPause ? "PauseMenu" : "none", originAccepted));
        }

        QueueNamedMenuMessage(NativeMenuProbe::kMenuName, a_type, a_source);
        EvidenceLog::Event(
            a_type == RE::UI_MESSAGE_TYPE::kShow ? "menu_show_requested" :
                                                   "menu_hide_requested",
            std::format("source={}", a_source));
    }
}
