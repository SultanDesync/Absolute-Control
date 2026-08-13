#pragma once

namespace AbsoluteControlPanelResearch::ResearchInputCapture
{
    struct PollResult
    {
        enum class State
        {
            Idle,
            Capturing,
            Captured,
            TimedOut,
            Fault
        } state{ State::Idle };

        std::string binding;
    };

    bool Initialize() noexcept;
    [[nodiscard]] std::uint32_t DeviceCount() noexcept;
    bool BeginButtonCapture() noexcept;
    [[nodiscard]] PollResult Poll() noexcept;
}
