#include "scaleform/BridgeProtocolState.h"

#include <iostream>
#include <limits>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " #expression << '\n'; \
    return 1; } } while (false)

int main()
{
    using State = AbsoluteControlPanelResearch::Scaleform::BridgeProtocolState;
    using Ready = State::ReadyResult;
    using Ack = State::AckResult;
    State state;
    CHECK(!state.IsReady() && !state.HasAppliedModel());
    CHECK(!state.BeginPublication(1));
    CHECK(state.Acknowledge(0) == Ack::Rejected);
    CHECK(state.Ready(1, 1) == Ready::Accepted);
    CHECK(state.Ready(1, 1) == Ready::AlreadyReady);
    CHECK(state.Acknowledge(0) == Ack::Heartbeat);
    CHECK(!state.BeginPublication(0));
    CHECK(state.BeginPublication(5));
    CHECK(!state.BeginPublication(6));
    CHECK(state.Acknowledge(0) == Ack::Rejected);
    CHECK(state.Acknowledge(4) == Ack::Rejected);
    CHECK(state.Acknowledge(6) == Ack::Rejected);
    CHECK(state.Acknowledge(5) == Ack::Publication);
    CHECK(!state.HasAppliedModel());
    CHECK(state.CompletePublication(true));
    CHECK(state.HasAppliedModel());
    CHECK(state.Acknowledge(5) == Ack::Heartbeat);
    CHECK(state.Acknowledge(0) == Ack::Rejected);
    CHECK(!state.BeginPublication(5));
    CHECK(state.BeginPublication(9));
    CHECK(state.Acknowledge(9) == Ack::Publication);
    CHECK(state.CompletePublication(true));
    CHECK(state.Acknowledge(5) == Ack::Rejected);
    CHECK(state.Acknowledge(9) == Ack::Heartbeat);

    // A stale/mismatched movie can return successfully without accepting data.
    State noAck;
    CHECK(noAck.Ready(1, 1) == Ready::Accepted);
    CHECK(noAck.BeginPublication(1));
    CHECK(!noAck.CompletePublication(true));
    CHECK(noAck.Failed() && !noAck.HasAppliedModel());
    CHECK(noAck.Ready(1, 1) == Ready::Rejected);

    State failedInvoke;
    CHECK(failedInvoke.Ready(1, 1) == Ready::Accepted);
    CHECK(failedInvoke.BeginPublication(1));
    CHECK(failedInvoke.Acknowledge(1) == Ack::Publication);
    CHECK(!failedInvoke.CompletePublication(false));

    for (auto version : {0ULL, 2ULL, std::numeric_limits<unsigned long long>::max()}) {
        State incompatible;
        CHECK(incompatible.Ready(1, version) == Ready::Rejected);
        CHECK(!incompatible.IsReady());
    }
    for (auto count : {0U, 2U}) {
        State malformed;
        CHECK(malformed.Ready(count, 1) == Ready::Rejected);
    }
    state.Fail();
    CHECK(!state.HasAppliedModel());
    CHECK(state.Acknowledge(9) == Ack::Rejected);
    return 0;
}
