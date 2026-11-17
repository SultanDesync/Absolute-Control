#include "EvidenceLog.h"
#include "runtime/BackendRuntime.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <numbers>
#include <optional>
#include <string_view>

// A head tracker as one more device on the bus. OpenTrack's "freetrack 2.0 Enhanced"
// output shares the head's angles through a named memory block; this reads them and
// publishes yaw, pitch and roll as axes, so they bind like any joystick axis.
// Ported from Absolute Head Tracking's OpenTrack.cpp.
namespace AbsoluteControl::Runtime
{
    namespace
    {
        namespace D = DeviceDraft;
        namespace Log = AbsoluteControlPanelResearch::EvidenceLog;

        struct FreeTrackData
        {
            std::int32_t dataId;
            std::int32_t cameraWidth;
            std::int32_t cameraHeight;
            float yaw, pitch, roll;
            float x, y, z;
            float rawYaw, rawPitch, rawRoll;
            float rawX, rawY, rawZ;
            float points[8];
        };
        struct FreeTrackHeap
        {
            FreeTrackData data;
            std::int32_t gameId;
            std::array<std::uint8_t, 8> table;
            std::int32_t gameId2;
        };
        static_assert(sizeof(FreeTrackData) == 92);
        static_assert(sizeof(FreeTrackHeap) == 108);

        // A frame counter that stops moving means OpenTrack has stopped tracking.
        constexpr std::uint64_t kStaleNanoseconds = 500'000'000;
        constexpr std::uint64_t kRetryNanoseconds = 1'000'000'000;

        template <std::size_t N> void Copy(char (&output)[N], std::string_view input)
        {
            std::fill(std::begin(output), std::end(output), '\0');
            std::memcpy(output, input.data(), std::min(input.size(), N - 1));
        }

        constexpr std::array<std::string_view, 3> kChannels{ OpenTrackChannels::kYaw,
            OpenTrackChannels::kPitch, OpenTrackChannels::kRoll };
        constexpr std::array<std::string_view, 3> kChannelNames{ "Yaw", "Pitch", "Roll" };

        class OpenTrackProvider final : public InputProvider
        {
        public:
            bool Start() override
            {
                static std::uint64_t nextEpoch = 1;
                epoch_ = nextEpoch++;
                device_.providerEpoch = epoch_;
                device_.enumerationRevision = 1;
                device_.validity = D::Validity::Disconnected;
                device_.channelCount = static_cast<std::uint32_t>(kChannels.size());
                Copy(device_.provider, OpenTrackChannels::kProvider);
                Copy(device_.physical, OpenTrackChannels::kPhysical);
                Copy(device_.displayName, OpenTrackChannels::kDisplayName);
                return true;
            }

            void Stop() noexcept override
            {
                if (view_) ::UnmapViewOfFile(view_);
                if (mapping_) ::CloseHandle(mapping_);
                if (mutex_) ::CloseHandle(mutex_);
                view_ = nullptr;
                mapping_ = mutex_ = nullptr;
            }

            void Poll(DeviceStore& store, std::uint64_t now) override
            {
                std::array<float, 3> radians{};
                const bool fresh = Read(radians, now);
                const auto validity = fresh ? D::Validity::Valid : D::Validity::Disconnected;
                if (!listed_ || validity != device_.validity) {
                    if (fresh) ++device_.connectionEpoch;
                    device_.validity = validity;
                    DeviceDescription description{ device_, {} };
                    for (std::size_t index = 0; index < kChannels.size(); ++index) {
                        D::Channel channel;
                        channel.shape = D::Shape::Scalar;
                        Copy(channel.identity.provider, OpenTrackChannels::kProvider);
                        Copy(channel.identity.physical, OpenTrackChannels::kPhysical);
                        Copy(channel.identity.channel, kChannels[index]);
                        Copy(channel.displayName, kChannelNames[index]);
                        // Raw values are the head's angle. Bound to a look axis, that angle
                        // is used as it is, where a stick's travel is scaled to the limit.
                        channel.rawUnits = D::Units::Radians;
                        channel.valueUnits = D::Units::Unitless;
                        channel.components = 1;
                        description.channels.push_back(channel);
                    }
                    if (store.ReplaceProviderInventory(OpenTrackChannels::kProvider,
                        std::span<const DeviceDescription>(&description, 1)) != D::Result::Ok) return;
                    listed_ = true;
                    Log::Event("opentrack_state", std::format("tracking={}", fresh ? 1 : 0));
                }

                DeviceFrame output;
                auto& f = output.frame;
                Copy(f.provider, OpenTrackChannels::kProvider);
                Copy(f.physical, OpenTrackChannels::kPhysical);
                f.providerEpoch = device_.providerEpoch;
                f.enumerationRevision = device_.enumerationRevision;
                f.connectionEpoch = device_.connectionEpoch;
                f.sequence = ++sequence_;
                f.sampledAt = f.receivedAt = BackendRuntime::Now();
                f.clock = D::Clock::CoreMonotonicNanoseconds;
                f.validity = validity;
                for (std::size_t index = 0; index < kChannels.size(); ++index) {
                    D::Sample sample;
                    Copy(sample.channel, kChannels[index]);
                    sample.shape = D::Shape::Scalar;
                    sample.components = 1;
                    sample.validity = validity;
                    sample.flags = D::kCalibrationRangeKnown;
                    sample.calibrationMinimum[0] = -std::numbers::pi;
                    sample.calibrationMaximum[0] = std::numbers::pi;
                    sample.raw[0] = fresh ? radians[index] : 0.0;
                    sample.value[0] = std::clamp(sample.raw[0] / std::numbers::pi, -1.0, 1.0);
                    output.samples.push_back(sample);
                }
                f.sampleCount = static_cast<std::uint32_t>(output.samples.size());
                (void)store.Publish(output);
            }

        private:
            bool Open(std::uint64_t now)
            {
                if (view_) return true;
                if (now < nextAttempt_) return false;
                nextAttempt_ = now + kRetryNanoseconds;
                mapping_ = ::OpenFileMappingW(FILE_MAP_READ, FALSE, L"FT_SharedMem");
                if (!mapping_) return false;
                view_ = static_cast<const FreeTrackHeap*>(::MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, sizeof(FreeTrackHeap)));
                if (!view_) {
                    ::CloseHandle(mapping_);
                    mapping_ = nullptr;
                    return false;
                }
                // The protocol's mutex name has this typo in it.
                mutex_ = ::OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, L"FT_Mutext");
                lastId_.reset();
                Log::Event("opentrack_connected", "transport=freetrack_shared_memory");
                return true;
            }

            bool Read(std::array<float, 3>& radians, std::uint64_t now)
            {
                if (!Open(now)) return false;
                FreeTrackData frame{};
                if (mutex_) {
                    const DWORD wait = ::WaitForSingleObject(mutex_, 2);
                    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return lastFresh_;
                }
                std::memcpy(&frame, &view_->data, sizeof(frame));
                if (mutex_) ::ReleaseMutex(mutex_);
                if (!std::isfinite(frame.yaw) || !std::isfinite(frame.pitch) || !std::isfinite(frame.roll)) return false;
                if (!lastId_ || frame.dataId != *lastId_) {
                    lastId_ = frame.dataId;
                    lastFrameAt_ = now;
                }
                lastFresh_ = lastFrameAt_ && now - lastFrameAt_ <= kStaleNanoseconds;
                radians = { frame.yaw, frame.pitch, frame.roll };
                return lastFresh_;
            }

            D::Device device_;
            std::uint64_t epoch_{};
            std::uint64_t sequence_{};
            bool listed_{};
            HANDLE mapping_{};
            HANDLE mutex_{};
            const FreeTrackHeap* view_{};
            std::uint64_t nextAttempt_{};
            std::optional<std::int32_t> lastId_;
            std::uint64_t lastFrameAt_{};
            bool lastFresh_{};
        };
    }

    std::unique_ptr<InputProvider> MakeOpenTrackProvider()
    {
        return std::make_unique<OpenTrackProvider>();
    }
}
