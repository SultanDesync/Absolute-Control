#include "runtime/ProbeRuntimeState.h"

#include <atomic>
#include <utility>

namespace AbsoluteControlPanelResearch::Runtime
{
    namespace
    {
        std::atomic<ProbePhase> g_phase{ ProbePhase::Cold };
        ProbeConfig g_config;
    }

    void Transition(ProbeEvent a_event) noexcept
    {
        auto current = g_phase.load(std::memory_order_acquire);
        while (!g_phase.compare_exchange_weak(
            current, Advance(current, a_event), std::memory_order_acq_rel)) {
        }
    }

    ProbePhase Phase() noexcept
    {
        return g_phase.load(std::memory_order_acquire);
    }

    void SetConfig(ProbeConfig a_config) noexcept
    {
        g_config = std::move(a_config);
    }

    const ProbeConfig& Config() noexcept
    {
        return g_config;
    }
}
