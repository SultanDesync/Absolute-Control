#pragma once

namespace AbsoluteControlPanelResearch::Ui
{
    // Origin of the Show request claimed by a displayed menu session.
    enum class MenuOpenOrigin
    {
        None,
        PauseMenu,
        MainMenu,
        Unclaimed
    };
}
