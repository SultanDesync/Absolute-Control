#include "runtime/AxisCapture.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        namespace D = DeviceDraft;

        constexpr std::uint64_t kMaximumFrameAgeNanoseconds = 250'000'000;

        [[nodiscard]] std::string_view Trim(std::string_view text) noexcept
        {
            const auto first = text.find_first_not_of(' ');
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(' ') - first + 1);
        }

        [[nodiscard]] std::optional<std::uint32_t> AxisNumber(std::string_view channel) noexcept
        {
            if (!channel.starts_with("axis/")) return std::nullopt;
            channel.remove_prefix(5);
            std::uint32_t axis{};
            const auto [end, error] = std::from_chars(channel.data(), channel.data() + channel.size(), axis);
            if (error != std::errc{} || end != channel.data() + channel.size() || axis > 7) return std::nullopt;
            return axis;
        }

        [[nodiscard]] bool IsUsableAxis(const D::Sample& sample) noexcept
        {
            return sample.shape == D::Shape::Scalar && sample.validity == D::Validity::Valid &&
                (sample.flags & D::kCalibrationRangeKnown) && AxisNumber(sample.channel);
        }

        // Names survive a replug better than ids, so use the name unless two devices share it.
        [[nodiscard]] std::string MakeToken(const DeviceSnapshot& snapshot, std::string_view physical, std::uint32_t axis)
        {
            std::string_view name;
            for (const auto& description : snapshot.devices) {
                if (physical == description.device.physical) name = Trim(description.device.displayName);
            }
            const auto sameName = std::ranges::count_if(snapshot.devices, [&](const DeviceDescription& description) {
                return Trim(description.device.displayName) == name;
            });
            return name.empty() || sameName > 1 ?
                std::format("guid:{}@0x{:02X}", physical, 0x30 + axis) :
                std::format("{}@0x{:02X}", name, 0x30 + axis);
        }
    }

    void AxisCapture::Begin(std::uint64_t now)
    {
        state_ = State::Capturing;
        startedAt_ = now;
        start_.clear();
        moving_.reset();
        token_.clear();
    }

    void AxisCapture::Cancel() noexcept
    {
        state_ = State::Idle;
        moving_.reset();
    }

    AxisCapture::State AxisCapture::Poll(const DeviceStore& devices, std::uint64_t now)
    {
        if (state_ != State::Capturing) return state_;
        if (now - startedAt_ >= kTimeoutNanoseconds) {
            state_ = State::TimedOut;
            return state_;
        }

        const auto snapshot = devices.Snapshot();
        std::optional<Key> biggest;
        double biggestMove{};
        for (const auto& description : snapshot.devices) {
            buffer_.resize(description.channels.size());
            if (buffer_.empty()) continue;
            D::Frame frame;
            std::uint32_t required{};
            const auto result = devices.CopyLatest(description.device, frame, buffer_.data(),
                static_cast<std::uint32_t>(buffer_.size()), &required, now, kMaximumFrameAgeNanoseconds);
            if (result != D::Result::Ok || frame.validity != D::Validity::Valid) continue;
            const auto count = (std::min)(static_cast<std::size_t>(required), buffer_.size());
            for (std::size_t index = 0; index < count; ++index) {
                const auto& sample = buffer_[index];
                if (!IsUsableAxis(sample)) continue;
                Key key{ description.device.physical, sample.channel };
                // Where each axis rests when first seen is its starting point, not zero.
                const auto [entry, added] = start_.try_emplace(key, sample.value[0]);
                const auto move = std::abs(sample.value[0] - entry->second);
                if (move >= kMovement && move > biggestMove) {
                    biggest = std::move(key);
                    biggestMove = move;
                }
            }
        }

        if (!biggest) {
            moving_.reset();
        } else if (biggest != moving_) {
            moving_ = std::move(biggest);
            movingSince_ = now;
        } else if (now - movingSince_ >= kHoldNanoseconds) {
            token_ = MakeToken(snapshot, moving_->first, *AxisNumber(moving_->second));
            state_ = State::Captured;
        }
        return state_;
    }
}
