#pragma once

#include <cstdint>

namespace AbsoluteControlPanelResearch::Scaleform
{
    // UI-thread-only protocol state. Invocation is not acceptance: a movie must
    // acknowledge the exact in-flight generation before publication completes.
    class BridgeProtocolState final
    {
    public:
        enum class ReadyResult { Accepted, AlreadyReady, Rejected };
        enum class AckResult { Publication, Heartbeat, Rejected };

        [[nodiscard]] ReadyResult Ready(std::uint32_t argumentCount,
            std::uint64_t version) noexcept
        {
            if (failed_ || argumentCount != 1 || version != 1) {
                Fail();
                return ReadyResult::Rejected;
            }
            if (ready_) return ReadyResult::AlreadyReady;
            ready_ = true;
            return ReadyResult::Accepted;
        }

        [[nodiscard]] bool BeginPublication(std::uint64_t generation) noexcept
        {
            if (!IsReady() || pending_ != 0 || generation == 0 ||
                generation <= applied_) return false;
            pending_ = generation;
            acknowledged_ = false;
            return true;
        }

        [[nodiscard]] AckResult Acknowledge(std::uint64_t generation) noexcept
        {
            if (!IsReady()) return AckResult::Rejected;
            if (pending_ != 0) {
                if (generation != pending_) return AckResult::Rejected;
                acknowledged_ = true;
                return AckResult::Publication;
            }
            // A zero heartbeat is allowed only to deliver the first model.
            return generation == applied_ ? AckResult::Heartbeat : AckResult::Rejected;
        }

        [[nodiscard]] bool CompletePublication(bool invoked) noexcept
        {
            if (!IsReady() || pending_ == 0 || !invoked || !acknowledged_) {
                Fail();
                return false;
            }
            applied_ = pending_;
            pending_ = 0;
            acknowledged_ = false;
            return true;
        }

        void Fail() noexcept
        {
            failed_ = true;
            pending_ = 0;
            acknowledged_ = false;
        }

        [[nodiscard]] bool IsReady() const noexcept { return ready_ && !failed_; }
        [[nodiscard]] bool HasAppliedModel() const noexcept
        {
            return IsReady() && applied_ != 0;
        }
        [[nodiscard]] bool Failed() const noexcept { return failed_; }

    private:
        std::uint64_t pending_{};
        std::uint64_t applied_{};
        bool ready_{};
        bool acknowledged_{};
        bool failed_{};
    };
}
