#pragma once

#include "input/PlatformInputServices.h"
#include "ui/MenuOpenOrigin.h"

#include <RE/Starfield.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace AbsoluteControlPanelResearch::Scaleform
{
    enum class NativeFunction : std::uintptr_t
    {
        Ready,
        Close,
        Dispatch,
        Compound,
        Focus,
        ModelApplied
    };

    class MenuBridge final
    {
    public:
        MenuBridge();
        ~MenuBridge();
        MenuBridge(const MenuBridge&) = delete;
        MenuBridge& operator=(const MenuBridge&) = delete;

        void Attach(RE::Scaleform::GFx::Movie* a_movie,
            const RE::Scaleform::GFx::Value& a_menuObject);
        void LogMovieState(std::string_view a_phase);
        [[nodiscard]] bool CompleteHandshake(bool invoked);
        void Call(const RE::Scaleform::GFx::FunctionHandler::Params& a_params);
        void OnShown();
        [[nodiscard]] Ui::MenuOpenOrigin OpenOrigin() const noexcept;
        // Returns the displayed session's origin only through its accepted close path.
        [[nodiscard]] Ui::MenuOpenOrigin OnHidden();
        [[nodiscard]] bool HandleButtonInput(const RE::ButtonEvent& a_event);
        void HandlePointerPhase(Input::PointerPhase a_phase);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
