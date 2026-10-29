#include "runtime/BackendRuntime.h"
#include "runtime/DeviceSamplePolicy.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <format>
#include <numbers>
#include <stdexcept>
#include <string>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        using Microsoft::WRL::ComPtr;
        namespace D = DeviceDraft;
        constexpr auto kProvider = "absolute.control.directinput";
        std::atomic_uint64_t nextEpoch{ 1 };

        template <std::size_t N> void Copy(char (&output)[N], std::string_view input)
        {
            if (input.size() >= N) throw std::runtime_error("device identifier exceeds capacity");
            std::fill(std::begin(output), std::end(output), '\0');
            std::memcpy(output, input.data(), input.size());
        }
        std::string Guid(const GUID& value)
        {
            return std::format("{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
                value.Data1, value.Data2, value.Data3, value.Data4[0], value.Data4[1], value.Data4[2],
                value.Data4[3], value.Data4[4], value.Data4[5], value.Data4[6], value.Data4[7]);
        }
        std::string Name(const wchar_t* text)
        {
            const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (size <= 0 || size > static_cast<int>(D::kNameCapacity)) return "DirectInput controller";
            std::string result(static_cast<std::size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
            result.pop_back();
            return result;
        }
        BOOL CALLBACK FindWindow(HWND window, LPARAM output)
        {
            DWORD process{};
            GetWindowThreadProcessId(window, &process);
            if (process == GetCurrentProcessId() && GetWindow(window, GW_OWNER) == nullptr && IsWindowVisible(window)) {
                *reinterpret_cast<HWND*>(output) = window;
                return FALSE;
            }
            return TRUE;
        }
        struct Enumerated
        {
            std::vector<DIDEVICEINSTANCEW> devices;
            bool failed{};
        };
        BOOL CALLBACK Enumerate(const DIDEVICEINSTANCEW* device, void* context) noexcept
        {
            auto& output = *static_cast<Enumerated*>(context);
            try {
                if (output.devices.size() >= DeviceStore::kDeviceLimit) {
                    output.failed = true;
                    return DIENUM_STOP;
                }
                output.devices.push_back(*device);
                return DIENUM_CONTINUE;
            } catch (...) { output.failed = true; return DIENUM_STOP; }
        }
        enum class Kind { Axis, Button, Pov, Direction };
        struct Control
        {
            D::Channel descriptor;
            Kind kind{};
            DWORD offset{};
            std::uint32_t direction{};
            LONG minimum{}, maximum{};
            bool rangeKnown{};
            DigitalSampleState digital;
        };
        struct Controller
        {
            GUID guid{};
            D::Device descriptor;
            ComPtr<IDirectInputDevice8W> device;
            std::vector<Control> controls;
            bool connected{}, objectFailure{};
        };
        void AddControl(Controller& controller, Kind kind, DWORD offset, std::uint32_t ordinal,
            std::uint32_t direction = 0)
        {
            Control c;
            c.kind = kind; c.offset = offset; c.direction = direction;
            auto& d = c.descriptor;
            Copy(d.identity.provider, kProvider);
            Copy(d.identity.physical, controller.descriptor.physical);
            d.components = 1;
            if (kind == Kind::Axis) {
                d.shape = D::Shape::Scalar;
                d.rawUnits = D::Units::ProviderDefined;
                Copy(d.identity.channel, std::format("axis/{}", ordinal));
                Copy(d.displayName, std::format("Axis {}", ordinal + 1));
                DIPROPRANGE range{};
                range.diph.dwSize = sizeof(range);
                range.diph.dwHeaderSize = sizeof(range.diph);
                range.diph.dwObj = offset;
                range.diph.dwHow = DIPH_BYOFFSET;
                c.rangeKnown = SUCCEEDED(controller.device->GetProperty(DIPROP_RANGE, &range.diph)) && range.lMin < range.lMax;
                c.minimum = range.lMin; c.maximum = range.lMax;
                d.valueUnits = c.rangeKnown ? D::Units::Unitless : D::Units::ProviderDefined;
            } else if (kind == Kind::Pov) {
                d.shape = D::Shape::Pov;
                d.rawUnits = D::Units::ProviderDefined;
                d.valueUnits = D::Units::Radians;
                Copy(d.identity.channel, std::format("pov/{}", ordinal));
                Copy(d.displayName, std::format("POV {}", ordinal + 1));
            } else {
                d.shape = D::Shape::Button;
                d.rawUnits = d.valueUnits = D::Units::Boolean;
                if (kind == Kind::Button) {
                    Copy(d.identity.channel, std::format("button/{}", ordinal));
                    Copy(d.displayName, std::format("Button {}", ordinal + 1));
                } else {
                    constexpr std::array names{ "up", "right", "down", "left" };
                    Copy(d.identity.channel, std::format("pov/{}/direction/{}", ordinal, names[direction]));
                    Copy(d.displayName, std::format("POV {} {}", ordinal + 1, names[direction]));
                }
            }
            controller.controls.push_back(std::move(c));
        }
        BOOL CALLBACK Objects(const DIDEVICEOBJECTINSTANCEW* object, void* context) noexcept
        {
            auto& c = *static_cast<Controller*>(context);
            try {
                const std::array<DWORD, 8> axes{ DIJOFS_X, DIJOFS_Y, DIJOFS_Z,
                    DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ, DIJOFS_SLIDER(0), DIJOFS_SLIDER(1) };
                const auto axis = std::find(axes.begin(), axes.end(), object->dwOfs);
                if ((object->dwType & DIDFT_AXIS) && axis != axes.end()) {
                    AddControl(c, Kind::Axis, object->dwOfs, static_cast<std::uint32_t>(axis - axes.begin()));
                } else if ((object->dwType & DIDFT_BUTTON) && object->dwOfs >= DIJOFS_BUTTON(0) && object->dwOfs <= DIJOFS_BUTTON(127)) {
                    AddControl(c, Kind::Button, object->dwOfs, object->dwOfs - DIJOFS_BUTTON(0));
                } else if ((object->dwType & DIDFT_POV) && object->dwOfs >= DIJOFS_POV(0) && object->dwOfs <= DIJOFS_POV(3) &&
                    (object->dwOfs - DIJOFS_POV(0)) % sizeof(DWORD) == 0) {
                    const auto index = static_cast<std::uint32_t>((object->dwOfs - DIJOFS_POV(0)) / sizeof(DWORD));
                    AddControl(c, Kind::Pov, object->dwOfs, index);
                    for (std::uint32_t direction = 0; direction < 4; ++direction) AddControl(c, Kind::Direction, object->dwOfs, index, direction);
                }
                return DIENUM_CONTINUE;
            } catch (...) { c.objectFailure = true; return DIENUM_STOP; }
        }

        class DirectInputProvider final : public InputProvider
        {
        public:
            bool Start() override
            {
                // Unsupported old suite cohort: AC never delegates or races its poller.
                if (GetModuleHandleW(L"AbsoluteHOTAS.dll"))
                    throw std::runtime_error("native AC devices require an adapted suite; AbsoluteHOTAS.dll is loaded");
                if (GetModuleHandleW(L"AbsoluteControlPanel.dll") && GetModuleHandleW(L"AbsoluteControlPanelResearchDev.dll"))
                    throw std::runtime_error("only one Absolute Control host may be loaded");
                epoch_ = nextEpoch.fetch_add(1);
                return SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
                    IID_IDirectInput8W, reinterpret_cast<void**>(input_.GetAddressOf()), nullptr));
            }
            void Stop() noexcept override
            {
                for (auto& c : controllers_) if (c.device) c.device->Unacquire();
                controllers_.clear(); input_.Reset();
            }
            void Poll(DeviceStore& store, std::uint64_t now) override
            {
                if (now >= nextEnumeration_) {
                    nextEnumeration_ = now + 1'000'000'000;
                    Refresh();
                }
                ++sequence_;
                std::vector<DeviceFrame> frames;
                for (auto& c : controllers_) {
                    DIJOYSTATE2 state{};
                    HRESULT status = DIERR_NOTINITIALIZED;
                    if (c.device) {
                        c.device->Poll();
                        status = c.device->GetDeviceState(sizeof(state), &state);
                        if (FAILED(status) && SUCCEEDED(c.device->Acquire())) {
                            c.device->Poll();
                            status = c.device->GetDeviceState(sizeof(state), &state);
                        }
                    }
                    const bool connected = SUCCEEDED(status);
                    const bool baseline = connected && !c.connected;
                    if (connected != c.connected) inventoryDirty_ = true;
                    if (baseline) ++c.descriptor.connectionEpoch;
                    c.descriptor.validity = connected ? D::Validity::Valid : D::Validity::Disconnected;
                    DeviceFrame output;
                    auto& f = output.frame;
                    Copy(f.provider, kProvider); Copy(f.physical, c.descriptor.physical);
                    f.providerEpoch = epoch_; f.enumerationRevision = revision_;
                    f.connectionEpoch = c.descriptor.connectionEpoch;
                    f.sequence = sequence_;
                    f.sampledAt = f.receivedAt = BackendRuntime::Now();
                    f.clock = D::Clock::CoreMonotonicNanoseconds;
                    f.validity = c.descriptor.validity;
                    for (auto& control : c.controls) {
                        D::Sample sample;
                        Copy(sample.channel, control.descriptor.identity.channel);
                        sample.shape = control.descriptor.shape;
                        sample.components = 1; sample.validity = f.validity;
                        const auto bytes = reinterpret_cast<const unsigned char*>(&state);
                        if (control.kind == Kind::Axis) {
                            LONG raw{}; std::memcpy(&raw, bytes + control.offset, sizeof(raw));
                            sample.raw[0] = connected ? raw : 0;
                            sample.value[0] = sample.raw[0];
                            if (control.rangeKnown) {
                                sample.flags = D::kCalibrationRangeKnown;
                                sample.calibrationMinimum[0] = control.minimum;
                                sample.calibrationMaximum[0] = control.maximum;
                                if (connected) sample.value[0] = std::clamp(
                                    (static_cast<double>(raw) - control.minimum) /
                                    (static_cast<double>(control.maximum) - control.minimum) * 2.0 - 1.0, -1.0, 1.0);
                            }
                        } else {
                            DWORD pov{};
                            if (control.kind == Kind::Pov || control.kind == Kind::Direction)
                                std::memcpy(&pov, bytes + control.offset, sizeof(pov));
                            const bool centered = !connected || LOWORD(pov) == 0xFFFF || pov >= 36000;
                            if (control.kind == Kind::Pov) {
                                sample.raw[0] = centered ? -1 : static_cast<double>(pov);
                                sample.value[0] = centered ? -1 : pov * std::numbers::pi / 18000.0;
                            } else {
                                bool down = connected && (bytes[control.offset] & 0x80U) != 0;
                                if (control.kind == Kind::Direction) {
                                    down = !centered && PovDirectionActive(pov, control.direction);
                                }
                                control.digital.Observe(down, baseline);
                                sample.counterBits = 64;
                                sample.pressCount = control.digital.presses; sample.releaseCount = control.digital.releases;
                                sample.raw[0] = sample.value[0] = down ? 1 : 0;
                            }
                        }
                        output.samples.push_back(sample);
                    }
                    c.connected = connected;
                    f.sampleCount = static_cast<std::uint32_t>(output.samples.size());
                    frames.push_back(std::move(output));
                }
                if (inventoryDirty_) {
                    std::vector<DeviceDescription> inventory;
                    for (auto& c : controllers_) {
                        c.descriptor.enumerationRevision = revision_;
                        DeviceDescription description{ c.descriptor, {} };
                        for (const auto& control : c.controls) description.channels.push_back(control.descriptor);
                        inventory.push_back(std::move(description));
                    }
                    if (store.ReplaceInventory(inventory) != D::Result::Ok) throw std::runtime_error("native device inventory rejected");
                    inventoryDirty_ = false;
                }
                for (const auto& frame : frames) {
                    if (store.Publish(frame) != D::Result::Ok) throw std::runtime_error("native device frame rejected");
                }
            }
        private:
            void Open(Controller& c)
            {
                inventoryDirty_ = true;
                if (c.device) c.device->Unacquire();
                c.device.Reset();
                if (FAILED(input_->CreateDevice(c.guid, c.device.GetAddressOf(), nullptr))) return;
                HWND window{};
                EnumWindows(&FindWindow, reinterpret_cast<LPARAM>(&window));
                if (!window || FAILED(c.device->SetDataFormat(&c_dfDIJoystick2)) ||
                    FAILED(c.device->SetCooperativeLevel(window, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
                    c.device.Reset(); return;
                }
                // Rebuild descriptors on a new connection; identity is tied to GUID + object.
                c.controls.clear(); c.objectFailure = false;
                if (FAILED(c.device->EnumObjects(&Objects, &c, DIDFT_ALL)) || c.objectFailure)
                    throw std::runtime_error("DirectInput channel enumeration failed");
                std::sort(c.controls.begin(), c.controls.end(), [](const auto& a, const auto& b) {
                    return std::strcmp(a.descriptor.identity.channel, b.descriptor.identity.channel) < 0;
                });
                c.descriptor.channelCount = static_cast<std::uint32_t>(c.controls.size());
                c.device->Acquire();
            }
            void Refresh()
            {
                Enumerated found;
                if (FAILED(input_->EnumDevices(DI8DEVCLASS_GAMECTRL, &Enumerate, &found, DIEDFL_ATTACHEDONLY)) || found.failed)
                    throw std::runtime_error("DirectInput inventory failed or exceeded 16 devices");
                std::sort(found.devices.begin(), found.devices.end(), [](const auto& a, const auto& b) {
                    return Guid(a.guidInstance) < Guid(b.guidInstance);
                });
                std::vector<std::string> ids;
                for (const auto& instance : found.devices) ids.push_back(Guid(instance.guidInstance));
                if (ids != attached_) { ++revision_; attached_ = ids; inventoryDirty_ = true; }
                for (auto& c : controllers_) {
                    if (std::find(ids.begin(), ids.end(), c.descriptor.physical) == ids.end() && c.device) {
                        c.device->Unacquire(); c.device.Reset();
                    }
                }
                for (const auto& instance : found.devices) {
                    const auto id = Guid(instance.guidInstance);
                    auto foundController = std::find_if(controllers_.begin(), controllers_.end(), [&](const auto& c) {
                        return id == c.descriptor.physical;
                    });
                    if (foundController == controllers_.end()) {
                        if (controllers_.size() == DeviceStore::kDeviceLimit)
                            throw std::runtime_error("DirectInput retained device capacity exceeded");
                        Controller c;
                        c.guid = instance.guidInstance;
                        Copy(c.descriptor.provider, kProvider); Copy(c.descriptor.physical, id);
                        Copy(c.descriptor.displayName, Name(instance.tszInstanceName));
                        c.descriptor.providerEpoch = epoch_;
                        c.descriptor.validity = D::Validity::Disconnected;
                        controllers_.push_back(std::move(c));
                        foundController = controllers_.end() - 1;
                    }
                    if (!foundController->connected || !foundController->device) Open(*foundController);
                }
            }
            ComPtr<IDirectInput8W> input_;
            std::vector<Controller> controllers_;
            std::vector<std::string> attached_;
            std::uint64_t epoch_{}, revision_{ 1 }, sequence_{}, nextEnumeration_{};
            bool inventoryDirty_{ true };
        };
    }

    std::unique_ptr<InputProvider> MakeDirectInputProvider() { return std::make_unique<DirectInputProvider>(); }
}
