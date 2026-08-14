#pragma once

#include "input/PlatformInputServices.h"

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
        void Call(const RE::Scaleform::GFx::FunctionHandler::Params& a_params);
        [[nodiscard]] bool HandleButtonInput(const RE::ButtonEvent& a_event);
        void HandlePointerPhase(Input::PointerPhase a_phase);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
