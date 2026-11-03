#include "ui/MenuAudioIntegration.h"

#include "EvidenceLog.h"

#include <RE/Starfield.h>

#include <format>

namespace AbsoluteControlPanelResearch::Ui
{
    namespace
    {
        // Starfield's MenuAudioHandler maps PauseMenu open/close to mode 2.
        // The native functions own a ref-counted mode list: Acquire appends one
        // mode and Release removes one matching mode.
        constexpr std::uint32_t kPauseMenuAudioMode = 2;
        constexpr REL::ID kAcquireMenuAudioMode{ 82727 };
        constexpr REL::ID kReleaseMenuAudioMode{ 82728 };
    }

    PauseMenuAudioLease::~PauseMenuAudioLease()
    {
        Release("lease-destructor");
    }

    void PauseMenuAudioLease::Acquire(std::string_view a_source) noexcept
    {
        if (acquired_) {
            EvidenceLog::Event(
                "menu_audio_lease_reused",
                std::format("mode={} source={}", kPauseMenuAudioMode, a_source));
            return;
        }

        using func_t = void (*)(std::uint32_t);
        static REL::Relocation<func_t> acquire{ kAcquireMenuAudioMode };
        acquire(kPauseMenuAudioMode);
        acquired_ = true;
        EvidenceLog::Event(
            "menu_audio_lease_acquired",
            std::format("mode={} source={}", kPauseMenuAudioMode, a_source));
    }

    void PauseMenuAudioLease::Release(std::string_view a_source) noexcept
    {
        if (!acquired_) {
            return;
        }

        using func_t = void (*)(std::uint32_t);
        static REL::Relocation<func_t> release{ kReleaseMenuAudioMode };
        release(kPauseMenuAudioMode);
        acquired_ = false;
        EvidenceLog::Event(
            "menu_audio_lease_released",
            std::format("mode={} source={}", kPauseMenuAudioMode, a_source));
    }
}
