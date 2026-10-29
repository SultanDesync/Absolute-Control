#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace AbsoluteControl::Runtime
{
    // Internal harness contract, deliberately not a public DLL ABI. All calls
    // are serialized on one owner thread. No provider pointers or callbacks
    // are retained; returned records are copies. Time is monotonic caller time.
    enum class State { Unavailable, Starting, Ready, Degraded, Stopping };
    enum class Role { Provider, Subscriber, Shadow };
    enum class Result {
        Ok, Invalid, Duplicate, NotFound, Incompatible, NotReady, Busy,
        Stale, Forbidden, Capacity
    };
    enum class EventKind {
        Started, Ready, Registered, Retired, HealthChanged, Selected, Granted, Renewed,
        Revoked, Neutralized, Stopping, Stopped
    };
    enum class Reason { None, Requested, Expired, HealthLost, Shutdown };

    using Token = std::uint64_t;
    using Tick = std::uint64_t;

    struct Service
    {
        std::string id;
        std::uint32_t version{};
        std::uint64_t capabilities{};
        Role role{ Role::Provider };
        Token token{};
        State state{ State::Unavailable };
    };

    struct Claim
    {
        std::string resource;
        Token owner{};
        Token lease{};
        Tick deadline{};
        bool revoking{};
    };

    struct Event
    {
        std::uint64_t sequence{};
        Tick time{};
        EventKind kind{};
        Token owner{};
        Token lease{};
        std::string subject;
        Reason reason{};
        State state{ State::Unavailable };
    };

    struct Journal
    {
        std::uint64_t dropped{};
        std::vector<Event> events;
    };

    struct ResourceAuthority
    {
        std::string id;
        Token selectedOwner{};
        std::optional<Claim> claim;
    };

    class HeadlessCore final
    {
    public:
        HeadlessCore() = default;
        HeadlessCore(const HeadlessCore&) = delete;
        HeadlessCore& operator=(const HeadlessCore&) = delete;

        static constexpr std::size_t kServiceLimit = 64;
        static constexpr std::size_t kResourceLimit = 128;
        static constexpr std::size_t kHistoryLimit = 256;
        static constexpr std::size_t kIdLimit = 127;

        [[nodiscard]] Result Register(std::string id, std::uint32_t version,
            std::uint64_t capabilities, Role role, Token& token);
        [[nodiscard]] Result Retire(Token token);
        [[nodiscard]] Result Discover(const std::string& id,
            std::uint32_t version, std::uint64_t capabilities, Service& service) const;
        [[nodiscard]] Result Start();
        [[nodiscard]] Result SetHealth(Token token, State state);
        void Stop();
        [[nodiscard]] State Lifecycle() const { return state_; }

        // The startup plan must explicitly declare each exclusive resource and
        // select its owner before Start. Health/absence never selects fallback.
        [[nodiscard]] Result DeclareResource(std::string id, Token selectedOwner);
        [[nodiscard]] Result Acquire(const std::string& resource, Token owner,
            Tick duration, Token& lease);
        [[nodiscard]] Result Renew(Token owner, Token lease, Tick duration);
        [[nodiscard]] Result Revoke(Token owner, Token lease);
        // A backend acknowledges only after its documented release operation.
        // Until then the resource stays reserved, including after expiry/Stop.
        [[nodiscard]] Result AcknowledgeNeutralized(Token owner, Token lease);
        [[nodiscard]] Result AdvanceTo(Tick now);

        [[nodiscard]] std::optional<Claim> Ownership(const std::string& resource) const;
        [[nodiscard]] std::vector<Service> Services() const { return services_; }
        [[nodiscard]] std::vector<ResourceAuthority> Authority() const { return resources_; }
        [[nodiscard]] Journal History() const;

    private:
        using Resource = ResourceAuthority;
        [[nodiscard]] Service* FindService(Token token);
        [[nodiscard]] Resource* FindClaim(Token owner, Token lease);
        [[nodiscard]] bool ValidDuration(Tick duration) const;
        void BeginRevocation(Resource& resource, Reason reason);
        void FinishStop();
        void Record(EventKind kind, Token owner = 0, Token lease = 0,
            const std::string& subject = {}, Reason reason = Reason::None,
            State state = State::Unavailable);

        State state_{ State::Unavailable };
        Tick now_{};
        Token nextToken_{ 1 };
        std::uint64_t nextEvent_{ 1 };
        std::uint64_t dropped_{};
        std::vector<Service> services_;
        std::vector<Resource> resources_;
        std::deque<Event> history_;
    };
}
