#pragma once

#include <cstdint>

#include "runtime/AimRestrictionPolicy.h"
#include "runtime/LookAxes.h"

// The weapon field-of-fire limit, from Absolute Head Tracking. With the head turned, direct
// fire follows where the pilot looks, but only inside a cone around the ship's nose; past it,
// fire goes to the cone's rim and the HUD's lead circle shows where. A locked target outside
// the cone can't be fired at.
namespace AimRestriction {

struct Decision {
    AimRestrictionPolicy::Mode mode{AimRestrictionPolicy::Mode::Native};
    AimRestrictionPolicy::Reason reason{AimRestrictionPolicy::Reason::Disabled};
    std::uint64_t generation{};
    float measuredDot{1.0F};
    float measuredAngleDegrees{};
    float halfAngleDegrees{30.0F};
    bool fresh{};
    bool buildValidated{};
};

struct Status {
    bool installed{};
    bool frameFresh{};
    Decision lastDecision{};
};

bool Install();
void Shutdown();
bool Ready() noexcept;
// The firing cone's half angle in degrees, either side of the ship's nose. 0 lifts the limit.
void SetFiringCone(float halfAngleDegrees) noexcept;
void PublishCameraFrame(const AbsoluteControl::Runtime::Quaternion& headRotation) noexcept;
void ClearCameraFrame() noexcept;
void ResetSpatialState() noexcept;
Status GetStatus();

} // namespace AimRestriction
