#include "OverdriveCamera.h"

namespace Overdrive
{
namespace
{
bool Finite(Vec3 V) { return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z); }
double Angle(double A) { return std::remainder(A, Pi * 2); }
Vec3 Direction(double A) { return {std::sin(A), 0, std::cos(A)}; }
}

void CameraSpring::Reset(Vec3 Goal)
{
    Position = PreviousGoal = Goal;
    Velocity = {};
}

void CameraSpring::Step(Vec3 Goal, double Dt, double Frequency)
{
    if (!Finite(Goal) || !std::isfinite(Dt) || Dt <= 0 || !std::isfinite(Frequency) || Frequency <= 0) return;
    const Vec3 GoalDelta = Goal - PreviousGoal;
    const double MaxGoalTravel = 120 * Dt;
    if (GoalDelta.Length() > MaxGoalTravel) Goal = PreviousGoal + GoalDelta.Normal() * MaxGoalTravel;
    const Vec3 GoalVelocity = (Goal - PreviousGoal) * (1 / Dt);
    const Vec3 Error = Position - PreviousGoal;
    const Vec3 RelativeVelocity = Velocity - GoalVelocity;
    const Vec3 C = RelativeVelocity + Error * Frequency;
    const double Decay = std::exp(-Frequency * Dt);
    Position = Goal + (Error + C * Dt) * Decay;
    Velocity = GoalVelocity + (RelativeVelocity - C * (Frequency * Dt)) * Decay;
    PreviousGoal = Goal;
}

void FollowCamera::Reset(const CameraInput& Input)
{
    if (!Finite(Input.Focus) || !Finite(Input.Forward)) return;
    Heading = std::atan2(Input.Forward.X, Input.Forward.Z);
    Speed = std::isfinite(Input.Speed) ? Clamp(Input.Speed, 0, 100) : 0;
    RearView = Input.LookBack ? 1 : 0;
    const Vec3 Forward = Direction(Heading + RearView * Pi);
    Anchor.Reset(Input.Focus);
    Boom.Reset(Input.Focus - Forward * (9.8 + Speed * .025) + Vec3{0, 5.4 + Speed * .008, 0});
    Aim.Reset(Input.Focus + Forward * (3.0 + Speed * .025) + Vec3{0, .85, 0});
    FieldOfView = Input.Calm ? 60 : 59 + Clamp(Speed / 60, 0, 1) * 7;
    Ready = true;
}

void FollowCamera::Step(const CameraInput& Input, double Dt)
{
    if (!Finite(Input.Focus) || !Finite(Input.Forward) || !std::isfinite(Dt) || Dt <= 0) return;
    if (!Ready) { Reset(Input); return; }
    Dt = std::min(Dt, .25);
    const double DesiredSpeed = std::isfinite(Input.Speed) ? Clamp(Input.Speed, 0, 100) : 0;
    Speed = Damp(Speed, DesiredSpeed, 5, Dt);
    const double DesiredHeading = Input.Forward.X * Input.Forward.X + Input.Forward.Z * Input.Forward.Z > 1.e-8
        ? std::atan2(Input.Forward.X, Input.Forward.Z) : Heading;
    const double Turn = Angle(DesiredHeading - Heading) * (1 - std::exp(-(Input.Calm ? 4.0 : 5.5) * Dt));
    Heading = Angle(Heading + Clamp(Turn, -2.8 * Dt, 2.8 * Dt));
    const double RearStep = (Input.LookBack ? 1.0 : 0.0) - RearView;
    RearView += Clamp(RearStep * (1 - std::exp(-9 * Dt)), -1.35 * Dt, 1.35 * Dt);
    Anchor.Step(Input.Focus, Dt, 18);
    const Vec3 Forward = Direction(Heading + RearView * Pi);
    const double Distance = 9.8 + Speed * .025;
    Boom.Step(Anchor.Position - Forward * Distance + Vec3{0, 5.4 + Speed * .008, 0}, Dt, 22);
    Aim.Step(Anchor.Position + Forward * (3.0 + Speed * .025) + Vec3{0, .85, 0}, Dt, 20);
    const double DesiredFov = Input.Calm ? 60 : 59 + Clamp(Speed / 60, 0, 1) * 7 + (Input.Boosting ? 1.5 : 0);
    FieldOfView = Damp(FieldOfView, DesiredFov, 4, Dt);
}
}
