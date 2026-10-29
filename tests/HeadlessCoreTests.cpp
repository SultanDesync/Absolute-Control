#include "runtime/HeadlessCore.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __LINE__ << ": " << #expression << '\n'; return false; } } while (false)

namespace
{
    using namespace AbsoluteControl::Runtime;

    // Mock hardware/backend code owns release semantics. The core only requests
    // revocation; it cannot write an output or claim that cleanup occurred.
    struct MockWriter
    {
        Token owner{};
        Token lease{};
        bool held{};
        unsigned releases{};

        bool Release(HeadlessCore& core, const std::string& resource)
        {
            const auto claim = core.Ownership(resource);
            if (!claim || claim->owner != owner || claim->lease != lease ||
                !claim->revoking) return false;
            held = false;
            ++releases;
            return core.AcknowledgeNeutralized(owner, lease) == Result::Ok;
        }
    };

    bool TestRegistrationOrdersAndMenuFailure()
    {
        std::array<int, 3> order{ 0, 1, 2 };
        do {
            HeadlessCore core;
            std::array<Token, 3> tokens{};
            constexpr std::array ids{ "device.mock", "subscriber.mock", "menu.mock" };
            // Subscribers can retry discovery before a provider is registered.
            Service discovered;
            CHECK(core.Discover(ids[0], 1, 1, discovered) == Result::NotFound);
            for (auto index : order) {
                CHECK(core.Register(ids[index], 1, 3,
                    index == 0 ? Role::Provider : Role::Subscriber, tokens[index]) == Result::Ok);
            }
            CHECK(core.Discover(ids[0], 1, 1, discovered) == Result::NotReady);
            CHECK(discovered.token == tokens[0]);
            CHECK(core.DeclareResource("device.physical-1", tokens[0]) == Result::Ok);
            CHECK(core.Start() == Result::Ok);
            CHECK(core.Discover(ids[0], 2, 1, discovered) == Result::Incompatible);
            CHECK(core.Discover(ids[0], 1, 4, discovered) == Result::Incompatible);
            CHECK(core.SetHealth(tokens[0], State::Ready) == Result::Ok);
            CHECK(core.SetHealth(tokens[1], State::Ready) == Result::Ok);
            Token lease{};
            CHECK(core.Acquire("device.physical-1", tokens[0], 10, lease) == Result::Ok);
            CHECK(core.SetHealth(tokens[2], State::Degraded) == Result::Ok);
            CHECK(core.Retire(tokens[2]) == Result::Ok);
            CHECK(core.Lifecycle() == State::Ready);
            CHECK(core.Discover(ids[0], 1, 3, discovered) == Result::Ok);
            CHECK(!core.Ownership("device.physical-1")->revoking);
            // Snapshot edits cannot mutate registry state.
            auto snapshot = core.Services();
            snapshot.front().id = "foreign";
            CHECK(core.Discover(ids[0], 1, 1, discovered) == Result::Ok);
            core.Stop();
            CHECK(core.Lifecycle() == State::Stopping);
            CHECK(core.AcknowledgeNeutralized(tokens[0], lease) == Result::Ok);
            CHECK(core.Lifecycle() == State::Unavailable);
        } while (std::next_permutation(order.begin(), order.end()));

        HeadlessCore late;
        CHECK(late.Start() == Result::Ok); // No menu or provider required.
        Token provider{};
        CHECK(late.Register("late", 1, 1, Role::Provider, provider) == Result::Ok);
        Service found;
        CHECK(late.Discover("late", 1, 1, found) == Result::NotReady);
        CHECK(late.SetHealth(provider, State::Ready) == Result::Ok);
        CHECK(late.Discover("late", 1, 1, found) == Result::Ok);
        CHECK(late.DeclareResource("late.poller", provider) == Result::NotReady);
        late.Stop();
        CHECK(late.Lifecycle() == State::Unavailable);
        return true;
    }

    bool TestOwnershipAndCleanup()
    {
        HeadlessCore core;
        Token legacy{}, replacement{}, shadow{};
        CHECK(core.Register("legacy.hotas", 1, 1, Role::Provider, legacy) == Result::Ok);
        CHECK(core.Register("core.device", 1, 1, Role::Provider, replacement) == Result::Ok);
        CHECK(core.Register("shadow", 1, 1, Role::Shadow, shadow) == Result::Ok);
        CHECK(core.DeclareResource("device.poller", legacy) == Result::Ok);
        CHECK(core.DeclareResource("capture.suite", legacy) == Result::Ok);
        CHECK(core.DeclareResource("output.key", legacy) == Result::Ok);
        CHECK(core.DeclareResource("shadow.poller", shadow) == Result::Forbidden);
        CHECK(core.DeclareResource("device.poller", replacement) == Result::Duplicate);
        const auto startupPlan = core.Authority();
        CHECK(startupPlan.size() == 3);
        CHECK(startupPlan.front().selectedOwner == legacy && !startupPlan.front().claim);
        CHECK(core.Start() == Result::Ok);
        Token unused{ 999 };
        CHECK(core.Acquire("device.poller", legacy, 10, unused) == Result::NotReady);
        CHECK(unused == 0);
        CHECK(core.SetHealth(legacy, State::Ready) == Result::Ok);
        CHECK(core.SetHealth(replacement, State::Ready) == Result::Ok);
        CHECK(core.SetHealth(shadow, State::Ready) == Result::Ok);
        CHECK(core.Acquire("device.poller", shadow, 10, unused) == Result::Forbidden);
        CHECK(core.Acquire("device.poller", replacement, 10, unused) == Result::Forbidden);
        MockWriter writer{ legacy };
        CHECK(core.Acquire("output.key", legacy, 10, writer.lease) == Result::Ok);
        writer.held = true;
        Token deviceLease{}, captureLease{};
        CHECK(core.Acquire("device.poller", legacy, 10, deviceLease) == Result::Ok);
        CHECK(core.Acquire("capture.suite", legacy, 10, captureLease) == Result::Ok);
        CHECK(deviceLease != writer.lease && captureLease != deviceLease);
        CHECK(core.Acquire("capture.suite", legacy, 10, unused) == Result::Busy);
        CHECK(core.Retire(legacy) == Result::Busy);
        CHECK(core.AcknowledgeNeutralized(legacy, writer.lease) == Result::Invalid);
        CHECK(core.SetHealth(legacy, State::Degraded) == Result::Ok);
        CHECK(core.Ownership("device.poller")->revoking);
        CHECK(core.Ownership("capture.suite")->revoking);
        CHECK(writer.held); // A core state change is not physical neutralization.
        CHECK(core.Acquire("output.key", replacement, 10, unused) == Result::Forbidden);
        CHECK(core.Renew(legacy, writer.lease, 10) == Result::NotReady);
        CHECK(core.AcknowledgeNeutralized(replacement, writer.lease) == Result::Stale);
        core.Stop();
        core.Stop();
        CHECK(core.Start() == Result::NotReady);
        CHECK(writer.Release(core, "output.key"));
        CHECK(!writer.held && writer.releases == 1);
        CHECK(core.Lifecycle() == State::Stopping);
        CHECK(core.AcknowledgeNeutralized(legacy, deviceLease) == Result::Ok);
        CHECK(core.AcknowledgeNeutralized(legacy, captureLease) == Result::Ok);
        CHECK(core.Lifecycle() == State::Unavailable);
        CHECK(core.Authority().empty());
        CHECK(core.DeclareResource("output.key", replacement) == Result::Ok);
        CHECK(core.Start() == Result::Ok);
        CHECK(core.SetHealth(replacement, State::Ready) == Result::Ok);
        Token successor{};
        CHECK(core.Acquire("output.key", replacement, 10, successor) == Result::Ok);
        CHECK(successor != writer.lease);
        CHECK(core.AcknowledgeNeutralized(legacy, writer.lease) == Result::Stale);
        CHECK(core.Revoke(legacy, writer.lease) == Result::Stale);
        CHECK(core.Ownership("output.key")->lease == successor);
        core.Stop();
        CHECK(core.AcknowledgeNeutralized(replacement, successor) == Result::Ok);
        return true;
    }

    bool TestExpiryAndHistory()
    {
        HeadlessCore core;
        Token owner{}, lease{};
        CHECK(core.Register("writer", 1, 1, Role::Provider, owner) == Result::Ok);
        CHECK(core.DeclareResource("action.look", owner) == Result::Ok);
        CHECK(core.Start() == Result::Ok);
        CHECK(core.SetHealth(owner, State::Ready) == Result::Ok);
        CHECK(core.Acquire("action.look", owner, 0, lease) == Result::Invalid);
        CHECK(core.Acquire("action.look", owner, 10, lease) == Result::Ok);
        CHECK(core.AdvanceTo(9) == Result::Ok);
        CHECK(core.Renew(owner, lease, 5) == Result::Ok);
        CHECK(core.AdvanceTo(10) == Result::Ok);
        CHECK(!core.Ownership("action.look")->revoking);
        CHECK(core.AdvanceTo(14) == Result::Ok);
        CHECK(core.Ownership("action.look")->revoking);
        CHECK(core.AdvanceTo(13) == Result::Invalid);
        CHECK(core.Renew(owner, lease, 1) == Result::NotReady);
        Token second{};
        CHECK(core.Acquire("action.look", owner, 1, second) == Result::Busy);
        const auto history = core.History();
        CHECK(history.dropped == 0);
        CHECK(history.events.back().kind == EventKind::Revoked);
        CHECK(history.events.back().reason == Reason::Expired);
        CHECK(history.events.back().time == 14);
        CHECK(history.events.back().owner == owner && history.events.back().lease == lease);
        CHECK(core.AcknowledgeNeutralized(owner, lease) == Result::Ok);
        CHECK(core.Acquire("action.look", owner, 1, second) == Result::Ok);
        CHECK(core.Renew(owner, lease, 1) == Result::Stale);
        CHECK(core.Revoke(owner, second) == Result::Ok);
        const auto revokedSequence = core.History().events.back().sequence;
        CHECK(core.History().events.back().reason == Reason::Requested);
        CHECK(core.Revoke(owner, second) == Result::Ok);
        CHECK(core.History().events.back().sequence == revokedSequence);
        // Restored health cannot revive a revoked lease or bypass release.
        CHECK(core.SetHealth(owner, State::Ready) == Result::Ok);
        CHECK(core.Renew(owner, second, 1) == Result::NotReady);
        CHECK(core.AcknowledgeNeutralized(owner, second) == Result::Ok);
        CHECK(core.Acquire("action.look", owner, 1, second) == Result::Ok);
        for (unsigned index = 0; index < 300; ++index) {
            CHECK(core.Renew(owner, second, 1) == Result::Ok);
        }
        auto bounded = core.History();
        CHECK(bounded.events.size() == HeadlessCore::kHistoryLimit);
        CHECK(bounded.dropped > 0);
        CHECK(bounded.events.front().sequence == bounded.dropped + 1);
        CHECK(bounded.events.back().sequence == bounded.dropped + bounded.events.size());
        bounded.events.clear();
        CHECK(core.History().events.size() == HeadlessCore::kHistoryLimit);
        CHECK(core.AdvanceTo(std::numeric_limits<Tick>::max()) == Result::Ok);
        CHECK(core.Renew(owner, second, 1) == Result::Invalid);
        core.Stop();
        CHECK(core.AcknowledgeNeutralized(owner, second) == Result::Ok);
        return true;
    }

    bool TestRegistrationGuardsAndBounds()
    {
        HeadlessCore core;
        Token original{}, token{};
        CHECK(core.Register("", 1, 0, Role::Provider, token) == Result::Invalid);
        CHECK(core.Register(std::string("a\0b", 3), 1, 0, Role::Provider, token) == Result::Invalid);
        CHECK(core.Register("provider", 0, 0, Role::Provider, token) == Result::Invalid);
        CHECK(core.Register("provider", 1, 1, Role::Provider, original) == Result::Ok);
        CHECK(core.Register("provider", 1, 1, Role::Provider, token) == Result::Duplicate);
        CHECK(token == 0);
        CHECK(core.Retire(original) == Result::Ok);
        CHECK(core.Register("provider", 1, 1, Role::Provider, token) == Result::Ok);
        CHECK(original != token);
        CHECK(core.Start() == Result::Ok);
        CHECK(core.SetHealth(original, State::Ready) == Result::Stale);
        core.Stop();
        for (std::size_t index = 1; index < HeadlessCore::kServiceLimit; ++index) {
            CHECK(core.Register("service." + std::to_string(index), 1, 1,
                Role::Subscriber, token) == Result::Ok);
        }
        CHECK(core.Register("over.capacity", 1, 1, Role::Provider, original) == Result::Capacity);
        for (std::size_t index = 0; index < HeadlessCore::kResourceLimit; ++index) {
            CHECK(core.DeclareResource("resource." + std::to_string(index), token) == Result::Ok);
        }
        CHECK(core.DeclareResource("over.capacity", token) == Result::Capacity);
        return true;
    }
}

int main()
{
    if (!TestRegistrationOrdersAndMenuFailure() || !TestOwnershipAndCleanup() ||
        !TestExpiryAndHistory() || !TestRegistrationGuardsAndBounds()) return 1;
    std::cout << "Headless core: registration permutations, readiness, authority, cleanup, "
                 "expiry, restart and bounded diagnostics passed.\n";
}
