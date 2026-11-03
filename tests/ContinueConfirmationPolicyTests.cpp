#include "research/ContinueConfirmationPolicy.h"
#include <initializer_list>
using namespace AbsoluteControlPanelResearch::ResearchSupport;

int main()
{
    const ContinueConfirmationState ready{ true, true, true, 1, 1, false, true, true, true, true };
    if (!CanConfirmContinue(ready) || CanConfirmContinue({})) return 1;
    for (auto field : { &ContinueConfirmationState::readable, &ContinueConfirmationState::mainMenuOpen,
        &ContinueConfirmationState::panelVisible, &ContinueConfirmationState::hasConfirmText,
        &ContinueConfirmationState::inputDisabled, &ContinueConfirmationState::selectionDisabled,
        &ContinueConfirmationState::hasPrompt }) {
        auto state = ready;
        state.*field = false;
        if (CanConfirmContinue(state)) return 2;
    }
    auto state = ready;
    state.rowDisabled = true;
    if (CanConfirmContinue(state)) return 3;
    state = ready; state.currentAction = 2;
    if (CanConfirmContinue(state)) return 4;
    state = ready; state.selectedAction = 2;
    if (CanConfirmContinue(state)) return 5;
    return 0;
}
