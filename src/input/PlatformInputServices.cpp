#include "input/PlatformInputServices.h"

#include "EvidenceLog.h"
#include "runtime/CooperativeService.h"

#include <SFSE/SFSE.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <memory>

namespace AbsoluteControlPanelResearch::Input
{
    namespace
    {
        struct PlatformServices
        {
            Runtime::CooperativeService hotkey;
            Runtime::CooperativeService pointer;
        };

        PlatformServices& Services() noexcept
        {
            // Starfield/SFSE does not unload plugins during a running process and
            // exposes no unload message. Keep ownership explicit but process-lived
            // so no jthread join can occur under the loader lock.
            static auto* services = new PlatformServices{};
            return *services;
        }

        template <class Callback>
        void QueueIfActive(const SFSE::TaskInterface* a_taskInterface,
            const std::shared_ptr<Runtime::CallbackGate>& a_gate,
            Callback&& a_callback)
        {
            a_taskInterface->AddTask([
                gate = a_gate, callback = std::forward<Callback>(a_callback)]() mutable {
                if (gate) {
                    (void)gate->TryInvoke(std::move(callback));
                }
            });
        }
    }

    void StartOpenHotkey(std::uint32_t a_virtualKey, OpenRequest a_requestOpen) noexcept
    {
        StopOpenHotkey();
        if (a_virtualKey == 0) {
            EvidenceLog::Event("open_hotkey_disabled");
            return;
        }
        const auto taskInterface = SFSE::GetTaskInterface();
        if (!taskInterface) {
            EvidenceLog::Event("open_hotkey_failed", "SFSE task interface unavailable");
            return;
        }

        EvidenceLog::Event(
            "open_hotkey_registered", std::format("virtual_key=0x{:02X}", a_virtualKey));
        const bool started = Services().hotkey.Start(
            [taskInterface, a_virtualKey, a_requestOpen](std::stop_token a_stopToken,
                const std::shared_ptr<Runtime::CallbackGate>& a_gate) {
            bool wasDown = false;
            while (!a_stopToken.stop_requested()) {
                DWORD foregroundProcess{};
                const auto foreground = ::GetForegroundWindow();
                if (foreground) {
                    ::GetWindowThreadProcessId(foreground, &foregroundProcess);
                }
                const bool focused = foregroundProcess == ::GetCurrentProcessId();
                const auto keyState = focused ?
                    ::GetAsyncKeyState(static_cast<int>(a_virtualKey)) : 0;
                const bool down = (keyState & 0x8000) != 0;
                const bool pressed = focused &&
                    (((keyState & 0x0001) != 0) || (down && !wasDown));
                if (pressed && a_requestOpen) {
                    QueueIfActive(taskInterface, a_gate, a_requestOpen);
                }
                wasDown = down;
                if (!Runtime::InterruptibleWait(
                        a_stopToken, std::chrono::milliseconds(20))) {
                    break;
                }
            }
        });
        if (!started) {
            EvidenceLog::Event("open_hotkey_failed", "worker could not start");
        }
    }

    void StartPointerPolling(PointerDispatch a_dispatch) noexcept
    {
        StopPointerPolling();
        const auto taskInterface = SFSE::GetTaskInterface();
        if (!taskInterface) {
            EvidenceLog::Event(
                "pointer_input_failed", "SFSE task interface unavailable");
            return;
        }

        EvidenceLog::Event("pointer_input_registered", "button=left");
        const bool started = Services().pointer.Start(
            [taskInterface, a_dispatch](std::stop_token a_stopToken,
                const std::shared_ptr<Runtime::CallbackGate>& a_gate) {
            bool wasDown = false;
            bool hasPointerPosition = false;
            POINT lastPointerPosition{};
            auto movePending = std::make_shared<std::atomic_bool>(false);
            const auto queuePointerPhase =
                [taskInterface, a_gate, movePending, a_dispatch](PointerPhase a_phase) {
                    QueueIfActive(taskInterface, a_gate,
                        [a_phase, movePending, a_dispatch]() {
                        const auto clearMovePending = [&]() {
                            if (a_phase == PointerPhase::Move) {
                                movePending->store(false, std::memory_order_release);
                            }
                        };
                        if (a_dispatch) {
                            a_dispatch(a_phase);
                        }
                        clearMovePending();
                    });
                };
            while (!a_stopToken.stop_requested()) {
                DWORD foregroundProcess{};
                const auto foreground = ::GetForegroundWindow();
                if (foreground) {
                    ::GetWindowThreadProcessId(foreground, &foregroundProcess);
                }
                const bool focused = foregroundProcess == ::GetCurrentProcessId();
                const auto buttonState = focused ? ::GetAsyncKeyState(VK_LBUTTON) : 0;
                const bool down = (buttonState & 0x8000) != 0;
                const bool pressed = focused && down && !wasDown;
                const bool clickPulse = focused && !down &&
                    (buttonState & 0x0001) != 0;
                POINT pointerPosition{};
                const bool pointerAvailable = focused &&
                    ::GetCursorPos(&pointerPosition) != FALSE;
                const bool moved = pointerAvailable && hasPointerPosition &&
                    (pointerPosition.x != lastPointerPosition.x ||
                        pointerPosition.y != lastPointerPosition.y);
                if (pointerAvailable) {
                    lastPointerPosition = pointerPosition;
                    hasPointerPosition = true;
                } else {
                    hasPointerPosition = false;
                }
                if (pressed || clickPulse) {
                    queuePointerPhase(PointerPhase::Down);
                } else if (moved &&
                           !movePending->exchange(true, std::memory_order_acq_rel)) {
                    queuePointerPhase(PointerPhase::Move);
                }
                if ((wasDown && !down) || clickPulse) {
                    queuePointerPhase(PointerPhase::Up);
                }
                wasDown = down;
                if (!Runtime::InterruptibleWait(
                        a_stopToken, std::chrono::milliseconds(16))) {
                    break;
                }
            }
        });
        if (!started) {
            EvidenceLog::Event("pointer_input_failed", "worker could not start");
        }
    }

    void StopOpenHotkey() noexcept
    {
        Services().hotkey.Stop();
    }

    void StopPointerPolling() noexcept
    {
        Services().pointer.Stop();
    }

    void StopPlatformInputServices() noexcept
    {
        StopOpenHotkey();
        StopPointerPolling();
    }

    bool ResolvePointerStage(POINT& a_point, double& a_stageX, double& a_stageY,
        std::string_view a_failureEvent) noexcept
    {
        const auto window = ::GetForegroundWindow();
        RECT client{};
        if (!window || !::GetClientRect(window, &client) ||
            !::GetCursorPos(&a_point) || !::ScreenToClient(window, &a_point)) {
            EvidenceLog::Event(a_failureEvent, "window geometry unavailable");
            return false;
        }
        const auto width = static_cast<double>(client.right - client.left);
        const auto height = static_cast<double>(client.bottom - client.top);
        const auto scale = (std::min)(width / 1920.0, height / 1080.0);
        if (!std::isfinite(scale) || scale <= 0.0) {
            EvidenceLog::Event(a_failureEvent, "invalid viewport scale");
            return false;
        }
        const auto offsetX = (width - 1920.0 * scale) * 0.5;
        const auto offsetY = (height - 1080.0 * scale) * 0.5;
        a_stageX = (static_cast<double>(a_point.x) - offsetX) / scale;
        a_stageY = (static_cast<double>(a_point.y) - offsetY) / scale;
        return true;
    }
}
