#include "research/TestInputProvider.h"

#include "runtime/DeviceSamplePolicy.h"

#include <array>
#include <atomic>
#include <cstring>
#include <format>
#include <stdexcept>

namespace AbsoluteControlPanelResearch::TestInput
{
    namespace
    {
        namespace D = AbsoluteControl::Runtime::DeviceDraft;
        using AbsoluteControl::Runtime::BackendRuntime;
        using AbsoluteControl::Runtime::DeviceDescription;
        using AbsoluteControl::Runtime::DeviceFrame;
        using AbsoluteControl::Runtime::DeviceStore;
        using AbsoluteControl::Runtime::DigitalSampleState;

        constexpr auto kProvider = "absolute.control.testinput";
        constexpr auto kPhysical = "{00000000-0000-0000-0000-00000000AC01}";
        constexpr auto kName = "Absolute Test Stick";

        std::array<std::atomic_bool, kButtonCount> g_buttons{};

        template <std::size_t N> void Copy(char (&output)[N], const std::string& input)
        {
            if (input.size() >= N) throw std::runtime_error("test input identifier exceeds capacity");
            std::memcpy(output, input.c_str(), input.size() + 1);
        }

        class Provider final : public AbsoluteControl::Runtime::InputProvider
        {
        public:
            bool Start() override
            {
                auto& device = description_.device;
                Copy(device.provider, kProvider);
                Copy(device.physical, kPhysical);
                Copy(device.displayName, kName);
                device.providerEpoch = device.enumerationRevision = device.connectionEpoch = 1;
                device.validity = D::Validity::Valid;
                device.channelCount = kButtonCount;
                for (std::uint32_t index = 0; index < kButtonCount; ++index) {
                    D::Channel channel;
                    channel.shape = D::Shape::Button;
                    Copy(channel.identity.provider, kProvider);
                    Copy(channel.identity.physical, kPhysical);
                    Copy(channel.identity.channel, std::format("button/{}", index));
                    Copy(channel.displayName, std::format("Button {}", index + 1));
                    channel.rawUnits = channel.valueUnits = D::Units::Boolean;
                    channel.components = 1;
                    description_.channels.push_back(channel);
                }
                return true;
            }

            void Stop() noexcept override {}

            void Poll(DeviceStore& store, std::uint64_t) override
            {
                if (!published_) {
                    if (store.ReplaceInventory(std::span(&description_, 1)) != D::Result::Ok)
                        throw std::runtime_error("test input inventory rejected");
                    published_ = true;
                }
                DeviceFrame output;
                auto& frame = output.frame;
                Copy(frame.provider, kProvider);
                Copy(frame.physical, kPhysical);
                frame.providerEpoch = frame.enumerationRevision = frame.connectionEpoch = 1;
                frame.sequence = ++sequence_;
                frame.sampledAt = frame.receivedAt = BackendRuntime::Now();
                frame.clock = D::Clock::CoreMonotonicNanoseconds;
                frame.validity = D::Validity::Valid;
                for (std::uint32_t index = 0; index < kButtonCount; ++index) {
                    const bool down = g_buttons[index].load(std::memory_order_acquire);
                    states_[index].Observe(down, false);
                    D::Sample sample;
                    Copy(sample.channel, std::format("button/{}", index));
                    sample.shape = D::Shape::Button;
                    sample.validity = D::Validity::Valid;
                    sample.components = 1;
                    sample.counterBits = 64;
                    sample.pressCount = states_[index].presses;
                    sample.releaseCount = states_[index].releases;
                    sample.raw[0] = sample.value[0] = down ? 1 : 0;
                    output.samples.push_back(sample);
                }
                frame.sampleCount = kButtonCount;
                if (store.Publish(output) != D::Result::Ok) throw std::runtime_error("test input frame rejected");
            }

        private:
            DeviceDescription description_;
            std::array<DigitalSampleState, kButtonCount> states_{};
            std::uint64_t sequence_{};
            bool published_{};
        };
    }

    std::unique_ptr<AbsoluteControl::Runtime::InputProvider> MakeProvider()
    {
        return std::make_unique<Provider>();
    }

    bool SetButton(std::uint32_t button, bool down) noexcept
    {
        if (button < 1 || button > kButtonCount) return false;
        g_buttons[button - 1].store(down, std::memory_order_release);
        return true;
    }
}
