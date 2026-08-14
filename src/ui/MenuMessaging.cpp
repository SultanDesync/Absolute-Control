#include "ui/MenuMessaging.h"

#include "EvidenceLog.h"
#include "NativeMenuProbe.h"
#include "runtime/ProbeRuntimeState.h"

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
        QueueNamedMenuMessage(NativeMenuProbe::kMenuName, a_type, a_source);
        EvidenceLog::Event(
            a_type == RE::UI_MESSAGE_TYPE::kShow ? "menu_show_requested" :
                                                   "menu_hide_requested",
            std::format("source={}", a_source));
    }
}
