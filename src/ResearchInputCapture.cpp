#include "ResearchInputCapture.h"

#include "EvidenceLog.h"

namespace AbsoluteControlPanelResearch::ResearchInputCapture
{
    namespace
    {
        constexpr auto kCaptureTimeout = std::chrono::seconds(8);
        constexpr std::uint32_t kBounceFrames = 2;

        struct Device
        {
            GUID instanceGuid{};
            std::string productName;
            std::uint32_t axisCount{};
            std::uint32_t buttonCount{};
            LPDIRECTINPUTDEVICE8A handle{};
            DIJOYSTATE2 current{};
            DIJOYSTATE2 previous{};
            bool valid{};
        };

        LPDIRECTINPUT8A g_directInput{};
        std::vector<Device> g_devices;
        bool g_initialized{};
        bool g_capturing{};
        std::chrono::steady_clock::time_point g_captureStarted{};
        std::int32_t g_candidateDevice{ -1 };
        std::int32_t g_candidateButton{ -1 };
        std::uint32_t g_candidateFrames{};

        BOOL CALLBACK CountObjects(
            const DIDEVICEOBJECTINSTANCEA* a_object, VOID* a_context)
        {
            auto* device = static_cast<Device*>(a_context);
            if ((a_object->dwType & DIDFT_AXIS) != 0) {
                ++device->axisCount;
            } else if ((a_object->dwType & DIDFT_BUTTON) != 0) {
                ++device->buttonCount;
            }
            return DIENUM_CONTINUE;
        }

        BOOL CALLBACK EnumerateDevice(
            const DIDEVICEINSTANCEA* a_instance, VOID*)
        {
            Device device;
            device.instanceGuid = a_instance->guidInstance;
            device.productName = a_instance->tszProductName;

            LPDIRECTINPUTDEVICE8A temporary{};
            if (SUCCEEDED(g_directInput->CreateDevice(
                    a_instance->guidInstance, &temporary, nullptr))) {
                temporary->EnumObjects(CountObjects, &device, DIDFT_ALL);
                temporary->Release();
            }
            g_devices.push_back(std::move(device));
            return DIENUM_CONTINUE;
        }

        bool OpenDevice(Device& a_device) noexcept
        {
            if (FAILED(g_directInput->CreateDevice(
                    a_device.instanceGuid, &a_device.handle, nullptr))) {
                return false;
            }
            if (FAILED(a_device.handle->SetDataFormat(&c_dfDIJoystick2))) {
                a_device.handle->Release();
                a_device.handle = nullptr;
                return false;
            }

            if (const auto window = ::GetForegroundWindow(); window) {
                a_device.handle->SetCooperativeLevel(
                    window, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
            }
            a_device.handle->Acquire();
            return true;
        }

        void PollDevices() noexcept
        {
            for (auto& device : g_devices) {
                device.previous = device.current;
                device.valid = false;
                if (!device.handle) {
                    continue;
                }
                device.handle->Poll();
                auto result = device.handle->GetDeviceState(
                    sizeof(DIJOYSTATE2), &device.current);
                if (FAILED(result)) {
                    device.handle->Acquire();
                    result = device.handle->GetDeviceState(
                        sizeof(DIJOYSTATE2), &device.current);
                }
                device.valid = SUCCEEDED(result);
            }
        }

        [[nodiscard]] bool HasDuplicateName(std::size_t a_index) noexcept
        {
            for (std::size_t index = 0; index < g_devices.size(); ++index) {
                if (index != a_index &&
                    g_devices[index].productName == g_devices[a_index].productName) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::string FormatBinding(
            std::size_t a_deviceIndex, std::uint32_t a_button)
        {
            if (HasDuplicateName(a_deviceIndex)) {
                return std::format("#{}@{}", a_deviceIndex, a_button);
            }
            return std::format(
                "{}@{}", g_devices[a_deviceIndex].productName, a_button);
        }

        void ResetCandidate() noexcept
        {
            g_candidateDevice = -1;
            g_candidateButton = -1;
            g_candidateFrames = 0;
        }
    }

    bool Initialize() noexcept
    {
        if (g_initialized) {
            return g_directInput != nullptr;
        }
        g_initialized = true;

        const auto result = ::DirectInput8Create(
            ::GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A,
            reinterpret_cast<void**>(&g_directInput), nullptr);
        if (FAILED(result) || !g_directInput) {
            EvidenceLog::Event(
                "input_enumeration_failed",
                std::format("stage=create_direct_input hresult=0x{:08X}", result));
            return false;
        }

        g_directInput->EnumDevices(
            DI8DEVCLASS_GAMECTRL, EnumerateDevice, nullptr, DIEDFL_ATTACHEDONLY);
        std::uint32_t opened{};
        for (std::size_t index = 0; index < g_devices.size(); ++index) {
            auto& device = g_devices[index];
            if (OpenDevice(device)) {
                ++opened;
            }
            EvidenceLog::Event(
                "input_device_enumerated",
                std::format(
                    "index={} name={} axes={} buttons={} opened={}", index,
                    device.productName, device.axisCount, device.buttonCount,
                    device.handle != nullptr));
        }
        PollDevices();
        EvidenceLog::Event(
            "input_enumeration_completed",
            std::format("devices={} opened={}", g_devices.size(), opened));
        return opened > 0;
    }

    std::uint32_t DeviceCount() noexcept
    {
        return static_cast<std::uint32_t>(g_devices.size());
    }

    bool BeginButtonCapture() noexcept
    {
        if (!Initialize() || g_devices.empty()) {
            EvidenceLog::Event("binding_capture_rejected", "no input devices available");
            return false;
        }
        PollDevices();
        g_capturing = true;
        g_captureStarted = std::chrono::steady_clock::now();
        ResetCandidate();
        EvidenceLog::Event(
            "binding_capture_started",
            std::format("kind=button devices={}", g_devices.size()));
        return true;
    }

    PollResult Poll() noexcept
    {
        if (!g_capturing) {
            return {};
        }
        PollDevices();

        if (g_candidateButton >= 0) {
            const auto deviceIndex = static_cast<std::size_t>(g_candidateDevice);
            const auto buttonIndex = static_cast<std::size_t>(g_candidateButton - 1);
            const bool held = deviceIndex < g_devices.size() &&
                              g_devices[deviceIndex].valid && buttonIndex < 128 &&
                              (g_devices[deviceIndex].current.rgbButtons[buttonIndex] &
                               0x80) != 0;
            if (held && ++g_candidateFrames >= kBounceFrames) {
                const auto binding = FormatBinding(
                    deviceIndex, static_cast<std::uint32_t>(g_candidateButton));
                g_capturing = false;
                ResetCandidate();
                return { PollResult::State::Captured, binding };
            }
            if (!held) {
                ResetCandidate();
            }
        }

        if (g_candidateButton < 0) {
            for (std::size_t deviceIndex = 0;
                 deviceIndex < g_devices.size() && g_candidateButton < 0;
                 ++deviceIndex) {
                const auto& device = g_devices[deviceIndex];
                if (!device.valid) {
                    continue;
                }
                for (std::size_t buttonIndex = 0; buttonIndex < 128; ++buttonIndex) {
                    const bool down =
                        (device.current.rgbButtons[buttonIndex] & 0x80) != 0;
                    const bool wasDown =
                        (device.previous.rgbButtons[buttonIndex] & 0x80) != 0;
                    if (down && !wasDown) {
                        g_candidateDevice = static_cast<std::int32_t>(deviceIndex);
                        g_candidateButton = static_cast<std::int32_t>(buttonIndex + 1);
                        g_candidateFrames = 1;
                        break;
                    }
                }
            }
        }

        if (std::chrono::steady_clock::now() - g_captureStarted >= kCaptureTimeout) {
            g_capturing = false;
            ResetCandidate();
            return { PollResult::State::TimedOut, {} };
        }
        return { PollResult::State::Capturing, {} };
    }
}
