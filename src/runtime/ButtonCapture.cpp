#include "runtime/ButtonCapture.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <string_view>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        namespace D = DeviceDraft;

        constexpr std::uint64_t kMaximumFrameAgeNanoseconds = 250'000'000;

        std::string_view Trim(std::string_view text) noexcept
        {
            const auto first = text.find_first_not_of(' ');
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(' ') - first + 1);
        }

        bool Number(std::string_view text, std::uint32_t& value) noexcept
        {
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return !text.empty() && error == std::errc{} && end == text.data() + text.size();
        }

        // Names survive a replug better than ids, so use the name unless two devices share it.
        std::string MakeToken(const DeviceSnapshot& snapshot, std::string_view physical, std::uint32_t number)
        {
            std::string_view name;
            for (const auto& description : snapshot.devices) {
                if (physical == description.device.physical) name = Trim(description.device.displayName);
            }
            const auto sameName = std::ranges::count_if(snapshot.devices, [&](const DeviceDescription& description) {
                return Trim(description.device.displayName) == name;
            });
            return name.empty() || sameName > 1 ? std::format("guid:{}@{}", physical, number) : std::format("{}@{}", name, number);
        }
    }

    std::uint32_t BindingNumberForChannel(std::string_view channel) noexcept
    {
        std::uint32_t index{};
        if (channel.starts_with("button/")) {
            return Number(channel.substr(7), index) && index < 128 ? index + 1 : 0;
        }
        if (!channel.starts_with("pov/")) return 0;
        channel.remove_prefix(4);
        const auto slash = channel.find('/');
        if (slash == std::string_view::npos || !Number(channel.substr(0, slash), index) || index > 3) return 0;
        channel.remove_prefix(slash + 1);
        constexpr std::array<std::string_view, 4> kDirections{ "direction/up", "direction/right", "direction/down", "direction/left" };
        const auto direction = std::ranges::find(kDirections, channel);
        if (direction == kDirections.end()) return 0;
        return 129 + index * 4 + static_cast<std::uint32_t>(direction - kDirections.begin());
    }

    void ButtonCapture::Begin(std::uint64_t now)
    {
        state_ = State::Capturing;
        startedAt_ = now;
        armed_.clear();
        token_.clear();
    }

    void ButtonCapture::Cancel() noexcept
    {
        state_ = State::Idle;
    }

    ButtonCapture::State ButtonCapture::Poll(const DeviceStore& devices, std::uint64_t now)
    {
        if (state_ != State::Capturing) return state_;
        if (now - startedAt_ >= kTimeoutNanoseconds) {
            state_ = State::TimedOut;
            return state_;
        }
        const auto snapshot = devices.Snapshot();
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
                if (sample.validity != D::Validity::Valid) continue;
                const auto number = BindingNumberForChannel(sample.channel);
                if (number == 0) continue;
                const bool down = sample.value[0] != 0;
                auto& armed = armed_[Key{ description.device.physical, sample.channel }];
                if (!down) {
                    armed = true;
                } else if (armed) {
                    token_ = MakeToken(snapshot, description.device.physical, number);
                    state_ = State::Captured;
                    return state_;
                }
            }
        }
        return state_;
    }
}
