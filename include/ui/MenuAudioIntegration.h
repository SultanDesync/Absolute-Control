#pragma once

#include <string_view>

namespace AbsoluteControlPanelResearch::Ui
{
    class PauseMenuAudioLease final
    {
    public:
        PauseMenuAudioLease() = default;
        ~PauseMenuAudioLease();

        PauseMenuAudioLease(const PauseMenuAudioLease&) = delete;
        PauseMenuAudioLease(PauseMenuAudioLease&&) = delete;
        PauseMenuAudioLease& operator=(const PauseMenuAudioLease&) = delete;
        PauseMenuAudioLease& operator=(PauseMenuAudioLease&&) = delete;

        void Acquire(std::string_view a_source) noexcept;
        void Release(std::string_view a_source) noexcept;

        [[nodiscard]] bool IsAcquired() const noexcept { return acquired_; }

    private:
        bool acquired_{};
    };
}
