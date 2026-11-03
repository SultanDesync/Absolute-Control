#pragma once
#include <cstdint>

namespace AbsoluteControlPanelResearch::ResearchSupport
{
    struct ContinueConfirmationState
    {
        bool readable{};
        bool mainMenuOpen{};
        bool panelVisible{};
        std::uint32_t currentAction{};
        std::uint32_t selectedAction{};
        bool rowDisabled{};
        bool hasConfirmText{};
        bool inputDisabled{};
        bool selectionDisabled{};
        bool hasPrompt{};
    };

    [[nodiscard]] constexpr bool CanConfirmContinue(const ContinueConfirmationState& state)
    {
        return state.readable && state.mainMenuOpen && state.panelVisible &&
            state.currentAction == 1 && state.selectedAction == 1 && !state.rowDisabled &&
            state.hasConfirmText && state.inputDisabled && state.selectionDisabled && state.hasPrompt;
    }
}
