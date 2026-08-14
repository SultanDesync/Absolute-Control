#pragma once

#include <RE/Starfield.h>

#include <string_view>

namespace AbsoluteControlPanelResearch::Ui
{
    void QueueNamedMenuMessage(std::string_view a_menuName,
        RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept;
    void QueueControlPanelMessage(
        RE::UI_MESSAGE_TYPE a_type, std::string_view a_source) noexcept;
}
