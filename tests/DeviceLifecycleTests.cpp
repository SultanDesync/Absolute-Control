#include "runtime/DeviceStore.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __LINE__ << ": " << #expression << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;
    namespace D = DeviceDraft;

    DeviceDescription Description()
    {
        DeviceDescription d;
        strcpy_s(d.device.provider, "mock.native");
        strcpy_s(d.device.physical, "controller-a");
        strcpy_s(d.device.displayName, "Controller");
        d.device.providerEpoch = d.device.enumerationRevision = d.device.connectionEpoch = 1;
        d.device.validity = D::Validity::Valid;
        d.device.channelCount = 1;
        D::Channel c;
        strcpy_s(c.identity.provider, d.device.provider);
        strcpy_s(c.identity.physical, d.device.physical);
        strcpy_s(c.identity.channel, "button/0");
        c.shape = D::Shape::Button;
        c.components = 1;
        c.rawUnits = c.valueUnits = D::Units::Boolean;
        d.channels.push_back(c);
        return d;
    }

    bool Reconciliation()
    {
        DeviceStore store;
        DeviceChanges changes;
        auto snapshot = store.Snapshot();
        CHECK(!snapshot.ready && snapshot.cursor == 0 && snapshot.session == 0);
        CHECK(store.CopyChanges(0, changes) == D::Result::Ok && changes.events.empty());
        store.Stop();
        store.Start();
        store.Start();
        CHECK(store.Snapshot().cursor == 1 && store.Snapshot().session == 1);
        auto device = Description();
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        snapshot = store.Snapshot();
        CHECK(snapshot.ready && snapshot.cursor == 2 && snapshot.devices.size() == 1);
        CHECK(store.CopyChanges(0, changes) == D::Result::Ok);
        CHECK(changes.cursor == 2 && changes.dropped == 0 && changes.events.size() == 2);
        CHECK(changes.events[0].kind == DeviceChangeKind::Started && changes.events[0].session == 1);
        CHECK(changes.events[1].kind == DeviceChangeKind::InventoryChanged);

        // Identical provider inventory and sample publication must not flood the bus.
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        DeviceFrame frame;
        strcpy_s(frame.frame.provider, device.device.provider);
        strcpy_s(frame.frame.physical, device.device.physical);
        frame.frame.providerEpoch = frame.frame.enumerationRevision = frame.frame.connectionEpoch = 1;
        frame.frame.clock = D::Clock::CoreMonotonicNanoseconds;
        frame.frame.validity = D::Validity::Valid;
        frame.frame.sampleCount = 1;
        D::Sample sample;
        strcpy_s(sample.channel, "button/0");
        sample.shape = D::Shape::Button;
        sample.components = 1;
        sample.validity = D::Validity::Valid;
        frame.samples.push_back(sample);
        for (std::uint64_t i = 1; i <= 1000; ++i) {
            frame.frame.sequence = frame.frame.sampledAt = frame.frame.receivedAt = i;
            CHECK(store.Publish(frame) == D::Result::Ok);
        }
        CHECK(store.CopyChanges(snapshot.cursor, changes) == D::Result::Ok);
        CHECK(changes.cursor == snapshot.cursor && changes.events.empty());

        // Accepted metadata changes notify, but do not erase compatible sample history.
        strcpy_s(device.device.displayName, "Renamed");
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        CHECK(store.Snapshot().cursor == 3 && store.History(device.device).size() == DeviceStore::kHistoryLimit);
        strcpy_s(device.channels[0].displayName, "Trigger");
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        CHECK(store.Snapshot().cursor == 4);

        device.device.validity = D::Validity::Disconnected;
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        device.device.validity = D::Validity::Valid;
        ++device.device.connectionEpoch;
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        CHECK(store.Snapshot().cursor == 6 && store.History(device.device).empty());
        CHECK(snapshot.devices[0].device.connectionEpoch == 1);
        CHECK(std::strcmp(snapshot.devices[0].device.displayName, "Controller") == 0);

        // Rejected stale/invalid inventories publish neither partial state nor events.
        auto invalid = device;
        invalid.device.connectionEpoch = 1;
        CHECK(store.ReplaceInventory(std::span(&invalid, 1)) == D::Result::StaleGeneration);
        invalid = device;
        invalid.channels[0].components = 0;
        CHECK(store.ReplaceInventory(std::span(&invalid, 1)) == D::Result::InvalidArgument);
        CHECK(store.Snapshot().cursor == 6);
        changes.cursor = 999;
        CHECK(store.CopyChanges(7, changes) == D::Result::InvalidArgument && changes.cursor == 999);

        // Same-count replacement, removal and restart are observable.
        strcpy_s(device.device.physical, "controller-b");
        strcpy_s(device.channels[0].identity.physical, "controller-b");
        ++device.device.enumerationRevision;
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        CHECK(store.Snapshot().cursor == 7);
        CHECK(store.ReplaceInventory({}) == D::Result::Ok);
        CHECK(store.Snapshot().cursor == 8 && store.Snapshot().devices.empty());
        store.Stop();
        store.Stop();
        CHECK(!store.Snapshot().ready && store.Snapshot().cursor == 9);
        CHECK(store.CopyChanges(8, changes) == D::Result::Ok);
        CHECK(changes.events.size() == 1 && changes.events[0].kind == DeviceChangeKind::Stopped);
        store.Start();
        CHECK(store.Snapshot().session == 2 && store.Snapshot().cursor == 10);
        CHECK(store.CopyChanges(9, changes) == D::Result::Ok);
        CHECK(changes.events.size() == 1 && changes.events[0].session == 2);
        return true;
    }

    bool Overflow()
    {
        DeviceStore store;
        for (std::size_t i = 0; i < DeviceStore::kChangeLimit; ++i) {
            store.Start();
            store.Stop();
        }
        DeviceChanges changes;
        CHECK(store.CopyChanges(0, changes) == D::Result::Ok);
        CHECK(changes.events.size() == DeviceStore::kChangeLimit);
        CHECK(changes.dropped == DeviceStore::kChangeLimit && changes.cursor == 2 * DeviceStore::kChangeLimit);
        for (std::size_t i = 0; i < changes.events.size(); ++i) {
            CHECK(changes.events[i].sequence == changes.dropped + i + 1);
        }
        CHECK(store.CopyChanges(DeviceStore::kChangeLimit - 1, changes) == D::Result::Ok && changes.dropped == 1);
        CHECK(store.CopyChanges(DeviceStore::kChangeLimit, changes) == D::Result::Ok && changes.dropped == 0);
        // Slow readers recover current state; they do not replay vanished history.
        const auto snapshot = store.Snapshot();
        CHECK(store.CopyChanges(snapshot.cursor, changes) == D::Result::Ok);
        CHECK(changes.events.empty() && changes.dropped == 0);
        return true;
    }

    bool ConcurrentSnapshots()
    {
        DeviceStore store;
        store.Start();
        auto device = Description();
        CHECK(store.ReplaceInventory(std::span(&device, 1)) == D::Result::Ok);
        std::atomic_bool done{}, failed{};
        std::jthread writer([&] {
            for (std::uint64_t revision = 2; revision <= 2000; ++revision) {
                device.device.enumerationRevision = revision;
                if (store.ReplaceInventory(std::span(&device, 1)) != D::Result::Ok) failed = true;
            }
            done = true;
        });
        // A reader's cursor and inventory must describe the same publication.
        do {
            const auto snapshot = store.Snapshot();
            if (snapshot.devices.size() != 1 ||
                snapshot.cursor != snapshot.devices[0].device.enumerationRevision + 1) failed = true;
            DeviceChanges changes;
            if (store.CopyChanges(snapshot.cursor, changes) != D::Result::Ok ||
                changes.cursor < snapshot.cursor) failed = true;
            for (const auto& change : changes.events) {
                if (change.sequence <= snapshot.cursor || change.session != snapshot.session) failed = true;
            }
        } while (!done);
        writer.join();
        CHECK(!failed);
        return true;
    }
}

int main()
{
    if (!Reconciliation() || !Overflow() || !ConcurrentSnapshots()) return 1;
    std::cout << "Device lifecycle: coherent reconciliation, bounded cursors, loss detection and concurrent copies passed.\n";
}
