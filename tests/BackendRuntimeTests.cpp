#include "runtime/BackendRuntime.h"
#include "runtime/DeviceSamplePolicy.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __LINE__ << ": " << #expression << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;
    namespace D = DeviceDraft;

    template <class Predicate> bool Wait(Predicate predicate)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!predicate()) {
            if (std::chrono::steady_clock::now() >= until) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    DeviceDescription Description()
    {
        DeviceDescription d;
        strcpy_s(d.device.provider, "mock.native");
        strcpy_s(d.device.physical, "device-a");
        strcpy_s(d.device.displayName, "Identical product");
        d.device.providerEpoch = d.device.enumerationRevision = d.device.connectionEpoch = 1;
        d.device.channelCount = 1;
        d.device.validity = D::Validity::Valid;
        D::Channel c;
        strcpy_s(c.identity.provider, d.device.provider);
        strcpy_s(c.identity.physical, d.device.physical);
        strcpy_s(c.identity.channel, "axis/0");
        c.components = 1;
        c.shape = D::Shape::Scalar;
        d.channels.push_back(c);
        return d;
    }

    DeviceFrame Frame(const DeviceDescription& d, std::uint64_t sequence, std::uint64_t now)
    {
        DeviceFrame result;
        auto& f = result.frame;
        strcpy_s(f.provider, d.device.provider);
        strcpy_s(f.physical, d.device.physical);
        f.providerEpoch = d.device.providerEpoch;
        f.enumerationRevision = d.device.enumerationRevision;
        f.connectionEpoch = d.device.connectionEpoch;
        f.sequence = sequence;
        f.sampledAt = f.receivedAt = now;
        f.clock = D::Clock::CoreMonotonicNanoseconds;
        f.validity = d.device.validity;
        f.sampleCount = 1;
        D::Sample s;
        strcpy_s(s.channel, "axis/0");
        s.shape = D::Shape::Scalar;
        s.components = 1;
        s.validity = d.device.validity;
        s.raw[0] = s.value[0] = static_cast<double>(sequence);
        result.samples.push_back(s);
        return result;
    }

    bool StoreContract()
    {
        DeviceStore store;
        std::uint32_t count{};
        CHECK(store.CopyDevices(nullptr, 0, &count) == D::Result::NotReady);
        store.Start();
        auto d = Description();
        CHECK(store.ReplaceInventory(std::span(&d, 1)) == D::Result::Ok);
        CHECK(store.CopyDevices(nullptr, 0, &count) == D::Result::BufferTooSmall && count == 1);
        CHECK(store.CopyDevices(nullptr, 1, &count) == D::Result::InvalidArgument);
        D::Device copy;
        CHECK(store.CopyDevices(&copy, 1, &count) == D::Result::Ok);
        CHECK(store.CopyChannels(copy, nullptr, 0, &count) == D::Result::BufferTooSmall && count == 1);
        D::Frame read;
        D::Sample sample;
        CHECK(store.CopyLatest(copy, read, &sample, 1, &count, 100, 10) == D::Result::NotReady);
        auto input = Frame(d, 1, 100);
        CHECK(store.Publish(input) == D::Result::Ok);
        input.samples[0].value[0] = 99;
        CHECK(store.CopyLatest(copy, read, &sample, 1, &count, 100, 10) == D::Result::Ok);
        CHECK(sample.value[0] == 1 && read.sequence == 1);
        read.sequence = 999;
        CHECK(store.CopyLatest(copy, read, nullptr, 0, &count, 100, 10) == D::Result::BufferTooSmall);
        CHECK(read.sequence == 999); // No partial publication.
        CHECK(store.CopyLatest(copy, read, &sample, 1, &count, 111, 10) == D::Result::Ok);
        CHECK(read.validity == D::Validity::Stale && sample.validity == D::Validity::Stale);
        CHECK(store.History(copy).back().frame.validity == D::Validity::Valid);
        CHECK(store.CopyLatest(copy, read, &sample, 1, &count, 99, 10) == D::Result::Ok);
        CHECK(read.validity == D::Validity::Valid);
        CHECK(store.Publish(Frame(d, 1, 112)) == D::Result::StaleGeneration);
        CHECK(store.Publish(Frame(d, 2, 99)) == D::Result::StaleGeneration);
        auto invalid = Frame(d, 2, 101);
        invalid.samples[0].value[0] = std::numeric_limits<double>::quiet_NaN();
        CHECK(store.Publish(invalid) == D::Result::InvalidArgument);
        auto badInventory = d;
        badInventory.channels.push_back(d.channels.front());
        badInventory.device.channelCount = 2;
        CHECK(store.ReplaceInventory(std::span(&badInventory, 1)) == D::Result::InvalidArgument);
        CHECK(store.History(copy).size() == 1);
        for (std::uint64_t sequence = 2; sequence <= 12; ++sequence)
            CHECK(store.Publish(Frame(d, sequence, 100 + sequence)) == D::Result::Ok);
        CHECK(store.History(copy).size() == DeviceStore::kHistoryLimit);
        CHECK(store.History(copy).front().frame.sequence == 5);

        // Same-count identity replacement is distinct even with the same label.
        auto replacement = d;
        strcpy_s(replacement.device.physical, "device-b");
        strcpy_s(replacement.channels[0].identity.physical, "device-b");
        replacement.device.enumerationRevision = 2;
        CHECK(store.ReplaceInventory(std::span(&replacement, 1)) == D::Result::Ok);
        CHECK(store.CopyLatest(copy, read, &sample, 1, &count, 112, 10) == D::Result::NotFound);
        CHECK(store.CopyDevices(&copy, 1, &count) == D::Result::Ok);
        const auto oldConnection = copy;
        replacement.device.connectionEpoch = 2;
        CHECK(store.ReplaceInventory(std::span(&replacement, 1)) == D::Result::Ok);
        CHECK(store.CopyChannels(oldConnection, nullptr, 0, &count) == D::Result::StaleGeneration);
        CHECK(store.Publish(Frame(replacement, 13, 120)) == D::Result::Ok);
        CHECK(store.CopyDevices(&copy, 1, &count) == D::Result::Ok);
        auto history = store.History(copy);
        history[0].samples[0].value[0] = 999;
        CHECK(store.History(copy)[0].samples[0].value[0] == 13);
        store.Stop();
        CHECK(store.CopyDevices(&copy, 1, &count) == D::Result::NotReady);
        return true;
    }

    bool DigitalPolicy()
    {
        DigitalSampleState state;
        state.Observe(true, true); // Held at initial/reconnected observation.
        CHECK(state.presses == 0 && state.down);
        state.Observe(false, false);
        state.Observe(true, false);
        CHECK(state.presses == 1 && state.releases == 1);
        state.Observe(false, false); // Lost connection releases the observed hold.
        state.Observe(false, false);
        state.Observe(true, true);
        CHECK(state.presses == 1 && state.releases == 2 && state.down);
        state.Observe(false, false);
        CHECK(state.releases == 3);
        CHECK(PovDirectionActive(0, 0) && PovDirectionActive(35999, 0));
        CHECK(PovDirectionActive(4500, 0) && PovDirectionActive(4500, 1));
        CHECK(!PovDirectionActive(4501, 0) && PovDirectionActive(4501, 1));
        CHECK(!PovDirectionActive(0xFFFFFFFFU, 0));
        CHECK(!PovDirectionActive(0x80000000U, 0));
        CHECK(!PovDirectionActive(36000, 0) && !PovDirectionActive(0, 4));
        return true;
    }

    struct Observations
    {
        std::atomic_uint starts{}, stops{}, polls{};
        std::atomic_bool entered{}, unblock{};
        bool failStart{}, failPoll{}, block{};
    };
    class MockProvider final : public InputProvider
    {
    public:
        explicit MockProvider(Observations& observations) : observations_(observations) {}
        bool Start() override { ++observations_.starts; return !observations_.failStart; }
        void Stop() noexcept override { ++observations_.stops; }
        void Poll(DeviceStore& store, std::uint64_t now) override
        {
            observations_.entered = true;
            while (observations_.block && !observations_.unblock) std::this_thread::yield();
            if (observations_.failPoll) throw std::runtime_error("mock provider failure");
            auto description = Description();
            if (store.ReplaceInventory(std::span(&description, 1)) != D::Result::Ok)
                throw std::runtime_error("inventory failure");
            const auto sequence = ++observations_.polls;
            if (store.Publish(Frame(description, sequence, now)) != D::Result::Ok)
                throw std::runtime_error("publish failure");
        }
    private:
        Observations& observations_;
    };

    bool WorkerLifetime()
    {
        // There is no menu object, SFSE, window or SWF in this executable.
        Observations observations;
        BackendRuntime runtime;
        CHECK(runtime.Start(std::make_unique<MockProvider>(observations)));
        CHECK(Wait([&] { return observations.polls >= 3; }));
        CHECK(runtime.Lifecycle() == State::Ready);
        CHECK(!runtime.Start(std::make_unique<MockProvider>(observations)));
        const auto initialSnapshot = runtime.Devices().Snapshot();
        CHECK(initialSnapshot.ready && initialSnapshot.session == 1 && initialSnapshot.devices.size() == 1);
        DeviceChanges changes;
        CHECK(runtime.Devices().CopyChanges(0, changes) == D::Result::Ok);
        CHECK(changes.events.size() == 2 && changes.events[0].kind == DeviceChangeKind::Started &&
            changes.events[1].kind == DeviceChangeKind::InventoryChanged);
        for (int i = 0; i < 100; ++i) {
            std::uint32_t count{}; D::Device device; D::Frame frame; D::Sample sample;
            CHECK(runtime.Devices().CopyDevices(&device, 1, &count) == D::Result::Ok);
            CHECK(runtime.Devices().CopyLatest(device, frame, &sample, 1, &count,
                BackendRuntime::Now(), 1'000'000'000) == D::Result::Ok);
            CHECK(static_cast<double>(frame.sequence) == sample.value[0]);
        }
        runtime.Stop(); runtime.Stop();
        CHECK(observations.starts == 1 && observations.stops == 1);
        const auto stopped = observations.polls.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(observations.polls == stopped);
        CHECK(runtime.Lifecycle() == State::Unavailable);
        CHECK(!runtime.Devices().Snapshot().ready && runtime.Devices().Snapshot().devices.empty());
        CHECK(runtime.Devices().CopyChanges(initialSnapshot.cursor, changes) == D::Result::Ok);
        CHECK(changes.events.size() == 1 && changes.events[0].kind == DeviceChangeKind::Stopped);

        Observations failed;
        failed.failPoll = true;
        CHECK(runtime.Start(std::make_unique<MockProvider>(failed)));
        CHECK(Wait([&] { return runtime.Lifecycle() == State::Degraded; }));
        CHECK(failed.stops == 1 && runtime.Failure() == "mock provider failure");
        CHECK(!runtime.Devices().Snapshot().ready && runtime.Devices().Snapshot().session == 2);
        CHECK(runtime.Devices().CopyChanges(changes.cursor, changes) == D::Result::Ok);
        CHECK(changes.events.size() == 2 && changes.events[0].kind == DeviceChangeKind::Started &&
            changes.events[1].kind == DeviceChangeKind::Stopped);
        runtime.Stop();
        failed.failPoll = false; failed.failStart = true;
        CHECK(runtime.Start(std::make_unique<MockProvider>(failed)));
        CHECK(Wait([&] { return runtime.Lifecycle() == State::Degraded; }));
        CHECK(failed.stops == 2);
        const auto failedStartSnapshot = runtime.Devices().Snapshot();
        CHECK(failedStartSnapshot.session == 2 && failedStartSnapshot.cursor == changes.cursor);
        runtime.Stop();

        Observations blocked;
        blocked.block = true;
        CHECK(runtime.Start(std::make_unique<MockProvider>(blocked)));
        const bool entered = Wait([&] { return blocked.entered.load(); });
        if (!entered) { blocked.unblock = true; runtime.Stop(); CHECK(entered); }
        std::atomic_bool stoppedWorker{};
        std::jthread stop([&] { runtime.Stop(); stoppedWorker = true; });
        const bool stopping = Wait([&] { return runtime.Lifecycle() == State::Stopping; });
        const bool premature = stoppedWorker.load() || blocked.stops != 0;
        blocked.unblock = true;
        stop.join();
        CHECK(stopping && !premature && blocked.stops == 1);
        CHECK(runtime.Devices().Snapshot().session == 3 && !runtime.Devices().Snapshot().ready);
        return true;
    }
}

int main()
{
    if (!StoreContract() || !DigitalPolicy() || !WorkerLifetime()) return 1;
    std::cout << "Native backend: coherent copy-out, generations, bounded history, freshness and worker lifecycle passed.\n";
}
