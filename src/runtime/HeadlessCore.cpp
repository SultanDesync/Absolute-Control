#include "runtime/HeadlessCore.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace AbsoluteControl::Runtime
{
    namespace
    {
        bool ValidId(const std::string& id)
        {
            return !id.empty() && id.size() <= HeadlessCore::kIdLimit &&
                id.find('\0') == std::string::npos;
        }
    }

    Service* HeadlessCore::FindService(Token token)
    {
        const auto found = std::find_if(services_.begin(), services_.end(),
            [token](const auto& service) { return service.token == token; });
        return found == services_.end() ? nullptr : &*found;
    }

    HeadlessCore::Resource* HeadlessCore::FindClaim(Token owner, Token lease)
    {
        const auto found = std::find_if(resources_.begin(), resources_.end(),
            [owner, lease](const auto& resource) {
                return resource.claim && resource.claim->owner == owner &&
                    resource.claim->lease == lease;
            });
        return found == resources_.end() ? nullptr : &*found;
    }

    void HeadlessCore::Record(EventKind kind, Token owner, Token lease,
        const std::string& subject, Reason reason, State state)
    {
        if (history_.size() == kHistoryLimit) {
            history_.pop_front();
            ++dropped_;
        }
        history_.push_back({ nextEvent_++, now_, kind, owner, lease, subject, reason, state });
    }

    Result HeadlessCore::Register(std::string id, std::uint32_t version,
        std::uint64_t capabilities, Role role, Token& token)
    {
        token = 0;
        if (!ValidId(id) || version == 0 ||
            (role != Role::Provider && role != Role::Subscriber && role != Role::Shadow)) {
            return Result::Invalid;
        }
        if (state_ == State::Stopping) return Result::NotReady;
        if (std::any_of(services_.begin(), services_.end(),
                [&id](const auto& service) { return service.id == id; })) return Result::Duplicate;
        if (services_.size() == kServiceLimit ||
            nextToken_ == std::numeric_limits<Token>::max()) return Result::Capacity;
        token = nextToken_++;
        services_.push_back({ std::move(id), version, capabilities, role, token,
            state_ == State::Ready ? State::Starting : State::Unavailable });
        Record(EventKind::Registered, token, 0, services_.back().id,
            Reason::None, services_.back().state);
        return Result::Ok;
    }

    Result HeadlessCore::Retire(Token token)
    {
        const auto service = FindService(token);
        if (!service) return Result::Stale;
        // Even an unclaimed startup selection pins this instance for the run.
        if (std::any_of(resources_.begin(), resources_.end(), [token](const auto& resource) {
                return resource.selectedOwner == token;
            })) return Result::Busy;
        Record(EventKind::Retired, token, 0, service->id);
        std::erase_if(services_, [token](const auto& entry) { return entry.token == token; });
        return Result::Ok;
    }

    Result HeadlessCore::Discover(const std::string& id, std::uint32_t version,
        std::uint64_t capabilities, Service& service) const
    {
        service = {};
        const auto found = std::find_if(services_.begin(), services_.end(),
            [&id](const auto& entry) { return entry.id == id; });
        if (found == services_.end()) return Result::NotFound;
        service = *found; // Present-but-unready remains visible to the caller.
        if (found->version != version ||
            (found->capabilities & capabilities) != capabilities) return Result::Incompatible;
        return state_ == State::Ready && found->state == State::Ready ?
            Result::Ok : Result::NotReady;
    }

    Result HeadlessCore::Start()
    {
        if (state_ != State::Unavailable) return Result::NotReady;
        state_ = State::Starting;
        Record(EventKind::Started, 0, 0, {}, Reason::None, state_);
        for (auto& service : services_) {
            service.state = State::Starting;
            Record(EventKind::HealthChanged, service.token, 0, service.id,
                Reason::None, service.state);
        }
        state_ = State::Ready; // Registry readiness does not imply provider readiness.
        Record(EventKind::Ready, 0, 0, {}, Reason::None, state_);
        return Result::Ok;
    }

    Result HeadlessCore::SetHealth(Token token, State state)
    {
        auto* service = FindService(token);
        if (!service) return Result::Stale;
        if (state_ != State::Ready) return Result::NotReady;
        if (state != State::Ready && state != State::Degraded &&
            state != State::Unavailable && state != State::Starting) return Result::Invalid;
        service->state = state;
        Record(EventKind::HealthChanged, token, 0, service->id, Reason::None, state);
        if (state != State::Ready) {
            for (auto& resource : resources_) {
                if (resource.claim && resource.claim->owner == token) {
                    BeginRevocation(resource, Reason::HealthLost);
                }
            }
        }
        return Result::Ok;
    }

    Result HeadlessCore::DeclareResource(std::string id, Token selectedOwner)
    {
        if (state_ != State::Unavailable) return Result::NotReady;
        if (!ValidId(id)) return Result::Invalid;
        const auto* owner = FindService(selectedOwner);
        if (!owner) return Result::Stale;
        if (owner->role == Role::Shadow) return Result::Forbidden;
        if (std::any_of(resources_.begin(), resources_.end(),
                [&id](const auto& resource) { return resource.id == id; })) return Result::Duplicate;
        if (resources_.size() == kResourceLimit) return Result::Capacity;
        resources_.push_back({ std::move(id), selectedOwner, std::nullopt });
        Record(EventKind::Selected, selectedOwner, 0, resources_.back().id);
        return Result::Ok;
    }

    bool HeadlessCore::ValidDuration(Tick duration) const
    {
        return duration != 0 && duration <= std::numeric_limits<Tick>::max() - now_;
    }

    Result HeadlessCore::Acquire(const std::string& resource, Token owner,
        Tick duration, Token& lease)
    {
        lease = 0;
        if (!ValidDuration(duration)) return Result::Invalid;
        if (state_ != State::Ready) return Result::NotReady;
        const auto* service = FindService(owner);
        if (!service) return Result::Stale;
        if (service->role == Role::Shadow) return Result::Forbidden;
        const auto found = std::find_if(resources_.begin(), resources_.end(),
            [&resource](const auto& entry) { return entry.id == resource; });
        if (found == resources_.end()) return Result::NotFound;
        if (found->selectedOwner != owner) return Result::Forbidden;
        if (found->claim) return Result::Busy;
        if (service->state != State::Ready) return Result::NotReady;
        if (nextToken_ == std::numeric_limits<Token>::max()) return Result::Capacity;
        lease = nextToken_++;
        found->claim = Claim{ resource, owner, lease, now_ + duration, false };
        Record(EventKind::Granted, owner, lease, resource);
        return Result::Ok;
    }

    Result HeadlessCore::Renew(Token owner, Token lease, Tick duration)
    {
        if (!ValidDuration(duration)) return Result::Invalid;
        auto* resource = FindClaim(owner, lease);
        if (!resource) return Result::Stale;
        if (state_ != State::Ready || resource->claim->revoking) return Result::NotReady;
        resource->claim->deadline = now_ + duration;
        Record(EventKind::Renewed, owner, lease, resource->id);
        return Result::Ok;
    }

    void HeadlessCore::BeginRevocation(Resource& resource, Reason reason)
    {
        if (!resource.claim || resource.claim->revoking) return;
        resource.claim->revoking = true;
        Record(EventKind::Revoked, resource.claim->owner, resource.claim->lease,
            resource.id, reason);
    }

    Result HeadlessCore::Revoke(Token owner, Token lease)
    {
        auto* resource = FindClaim(owner, lease);
        if (!resource) return Result::Stale;
        BeginRevocation(*resource, Reason::Requested);
        return Result::Ok;
    }

    Result HeadlessCore::AcknowledgeNeutralized(Token owner, Token lease)
    {
        auto* resource = FindClaim(owner, lease);
        if (!resource) return Result::Stale;
        if (!resource->claim->revoking) return Result::Invalid;
        Record(EventKind::Neutralized, owner, lease, resource->id);
        resource->claim.reset();
        FinishStop();
        return Result::Ok;
    }

    Result HeadlessCore::AdvanceTo(Tick now)
    {
        if (now < now_) return Result::Invalid;
        now_ = now;
        for (auto& resource : resources_) {
            if (resource.claim && resource.claim->deadline <= now_) {
                BeginRevocation(resource, Reason::Expired);
            }
        }
        return Result::Ok;
    }

    void HeadlessCore::Stop()
    {
        if (state_ == State::Unavailable || state_ == State::Stopping) return;
        state_ = State::Stopping;
        Record(EventKind::Stopping, 0, 0, {}, Reason::Shutdown, state_);
        for (auto& service : services_) service.state = State::Stopping;
        for (auto& resource : resources_) BeginRevocation(resource, Reason::Shutdown);
        FinishStop();
    }

    void HeadlessCore::FinishStop()
    {
        if (state_ != State::Stopping || std::any_of(resources_.begin(), resources_.end(),
                [](const auto& resource) { return resource.claim.has_value(); })) return;
        resources_.clear(); // Next startup must explicitly select authority again.
        for (auto& service : services_) service.state = State::Unavailable;
        state_ = State::Unavailable;
        Record(EventKind::Stopped, 0, 0, {}, Reason::Shutdown, state_);
    }

    std::optional<Claim> HeadlessCore::Ownership(const std::string& resource) const
    {
        const auto found = std::find_if(resources_.begin(), resources_.end(),
            [&resource](const auto& entry) { return entry.id == resource; });
        return found == resources_.end() ? std::nullopt : found->claim;
    }

    Journal HeadlessCore::History() const
    {
        return { dropped_, { history_.begin(), history_.end() } };
    }
}
