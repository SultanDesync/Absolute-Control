#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdint>
#include <string_view>

namespace AbsoluteControlPanelResearch::Input
{
    enum class PointerPhase : std::uint8_t
    {
        Down,
        Move,
        Up
    };

    using OpenRequest = void (*)() noexcept;
    using PointerDispatch = void (*)(PointerPhase) noexcept;

    void StartOpenHotkey(std::uint32_t a_virtualKey, OpenRequest a_requestOpen) noexcept;
    void StartPointerPolling(PointerDispatch a_dispatch) noexcept;
    void StopOpenHotkey() noexcept;
    void StopPointerPolling() noexcept;
    void StopPlatformInputServices() noexcept;

    [[nodiscard]] bool ResolvePointerStage(
        POINT& a_point, double& a_stageX, double& a_stageY,
        std::string_view a_failureEvent) noexcept;
}
