#pragma once

#include <cstdint>
#include <type_traits>

// Internal R2 design draft. Not exported, installed in the SDK, or frozen.
// No DirectInput layout, usage IDs or device acquisition contract appears here.
namespace AbsoluteControl::Runtime::DeviceDraft
{
    inline constexpr std::uint32_t kDraftVersion = 1;
    inline constexpr std::uint32_t kIdCapacity = 128;
    inline constexpr std::uint32_t kNameCapacity = 128;
    inline constexpr std::uint32_t kValueCapacity = 8;
    inline constexpr std::uint64_t kEnumeration = 1ULL << 0;
    inline constexpr std::uint64_t kChannelDescriptors = 1ULL << 1;
    inline constexpr std::uint64_t kLatestFrames = 1ULL << 2;
    inline constexpr std::uint32_t kCalibrationRangeKnown = 1U << 0;

    enum class Result : std::uint32_t {
        Ok, NotReady, InvalidArgument, NotFound, StaleGeneration,
        BufferTooSmall, Incompatible
    };
    enum class Shape : std::uint32_t { Button, Scalar, Vector, RelativeDelta, Contact, Pov, Pose };
    enum class Validity : std::uint32_t { Unknown, Valid, Disconnected, Stale };
    enum class Clock : std::uint32_t { Unknown, CoreMonotonicNanoseconds };
    enum class SampleOrigin : std::uint32_t { Physical, LegacyPublished };
    enum class Units : std::uint32_t { Unspecified, Boolean, Unitless, Radians, Metres, ProviderDefined };

    struct Identity
    {
        char provider[kIdCapacity]{};
        char physical[kIdCapacity]{};
        char channel[kIdCapacity]{};
    };

    struct Device
    {
        std::uint32_t structSize{ sizeof(Device) };
        char provider[kIdCapacity]{};
        char physical[kIdCapacity]{};
        char displayName[kNameCapacity]{};
        std::uint64_t providerEpoch{};
        std::uint64_t enumerationRevision{};
        std::uint64_t connectionEpoch{};
        Validity validity{ Validity::Unknown };
        std::uint32_t channelCount{};
    };

    struct Channel
    {
        std::uint32_t structSize{ sizeof(Channel) };
        Shape shape{};
        Identity identity{};
        char displayName[kNameCapacity]{};
        Units rawUnits{ Units::Unspecified };
        Units valueUnits{ Units::Unspecified };
        std::uint32_t components{};
    };

    // A copied coherent per-device frame. Zero means unknown for epochs,
    // revisions and timestamps, never evidence of a fresh physical sample.
    struct Frame
    {
        std::uint32_t structSize{ sizeof(Frame) };
        std::uint32_t sampleCount{};
        char provider[kIdCapacity]{};
        char physical[kIdCapacity]{};
        std::uint64_t providerEpoch{};
        std::uint64_t enumerationRevision{};
        std::uint64_t connectionEpoch{};
        std::uint64_t sequence{};
        std::uint64_t sampledAt{};
        std::uint64_t receivedAt{};
        Clock clock{ Clock::Unknown };
        Validity validity{ Validity::Unknown };
    };

    struct Sample
    {
        std::uint32_t structSize{ sizeof(Sample) };
        Shape shape{};
        char channel[kIdCapacity]{};
        Validity validity{ Validity::Unknown };
        SampleOrigin origin{ SampleOrigin::Physical };
        std::uint32_t components{};
        std::uint32_t counterBits{}; // 0=no counters, 32/64=modular source width.
        std::uint32_t flags{};
        std::uint32_t reserved{};
        std::uint64_t pressCount{};
        std::uint64_t releaseCount{};
        std::uint64_t calibrationRevision{}; // 0=unknown; never infer policy parity.
        double raw[kValueCapacity]{};
        double value[kValueCapacity]{};
        double calibrationMinimum[kValueCapacity]{};
        double calibrationMaximum[kValueCapacity]{};
    };

    // Conceptual copy-out ABI: buffers belong to the caller. Calls neither
    // invoke subscriber callbacks nor register ownership/acquire hardware.
    // The service must snapshot under its own synchronization; the caller must
    // not concurrently share a writable output buffer. No function is exported.
    struct Api
    {
        std::uint32_t structSize{ sizeof(Api) };
        std::uint32_t draftVersion{ kDraftVersion };
        std::uint64_t capabilities{};
        Result (*copyDevices)(Device* records, std::uint32_t capacity,
            std::uint32_t* required) noexcept{};
        Result (*copyChannels)(const Device* expectedDevice, Channel* records,
            std::uint32_t capacity, std::uint32_t* required) noexcept{};
        Result (*copyLatest)(const Device* expectedDevice, Frame* frame,
            Sample* records, std::uint32_t capacity, std::uint32_t* required) noexcept{};
    };

    static_assert(std::is_standard_layout_v<Api> && std::is_trivially_copyable_v<Api>);
    static_assert(std::is_standard_layout_v<Device> && std::is_trivially_copyable_v<Device>);
    static_assert(std::is_standard_layout_v<Channel> && std::is_trivially_copyable_v<Channel>);
    static_assert(std::is_standard_layout_v<Frame> && std::is_trivially_copyable_v<Frame>);
    static_assert(std::is_standard_layout_v<Sample> && std::is_trivially_copyable_v<Sample>);
}
