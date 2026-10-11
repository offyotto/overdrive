#include "OverdriveSimulation.h"

namespace Overdrive
{
namespace
{
double Angle(double Value) { return std::remainder(Value, Pi * 2); }
int Tier(double Charge) { return Charge >= .98 ? 3 : Charge >= .65 ? 2 : Charge >= .32 ? 1 : 0; }
constexpr double Barrier = TrackWidth * .5 - 1.65;
constexpr double HopDuration = .26;
}

Racer InterpolateRacer(const Racer& Previous, const Racer& Current, double Alpha)
{
    Racer Result = Current;
    const double T = std::isfinite(Alpha) ? Clamp(Alpha, 0, 1) : 1;
    Result.Distance = Lerp(Previous.Distance, Current.Distance, T);
    Result.Lateral = Lerp(Previous.Lateral, Current.Lateral, T);
    Result.Speed = Lerp(Previous.Speed, Current.Speed, T);
    Result.Yaw = Angle(Previous.Yaw + Angle(Current.Yaw - Previous.Yaw) * T);
    Result.VelocityYaw = Angle(Previous.VelocityYaw + Angle(Current.VelocityYaw - Previous.VelocityYaw) * T);
    Result.Steering = Lerp(Previous.Steering, Current.Steering, T);
    Result.YawRate = Lerp(Previous.YawRate, Current.YawRate, T);
    Result.HopHeight = Lerp(Previous.HopHeight, Current.HopHeight, T);
    return Result;
}

double Race::CornerSpeed(double Distance) const
{
    double Curve = .0001;
    for (double Ahead : {4.0, 12.0, 24.0, 36.0}) Curve = std::max(Curve, std::abs(Circuit.Curvature(StartOffset + Distance + Ahead)));
    return std::min(MaxSpeed(), std::sqrt((EngineClass == 50 ? 39.0 : 34.0) / Curve));
}

bool Race::RecoverPlayer()
{
    if (State != Phase::Racing || RecoveryLock > 0) return false;
    auto& P = Player();
    P.Distance = std::max(-10.2, P.Distance - 3.0);
    P.Lateral = P.LateralVelocity = P.Yaw = P.VelocityYaw = P.YawRate = P.Steering = 0;
    P.Speed = 0; P.Boost = 0; P.Slowed = 1.2; P.Drifting = false;
    P.DriftCharge = P.DriftTime = P.HopTime = P.HopHeight = P.DraftCharge = 0;
    P.DriftTier = P.DriftDirection = 0; P.SlipAngle = 0; P.Grip = 1;
    P.Energy = std::max(0.0, P.Energy - 12); RecoveryLock = 2;
    DriftBuffer = 0; Events.push_back({EventType::Recover});
    return true;
}

void Race::StepPlayer(double Dt, DriverInput Input)
{
    auto& P = Player();
    const auto Before = Circuit.At(StartOffset + P.Distance);
    const double Curve = Circuit.Curvature(StartOffset + P.Distance);
    P.DraftCooldown = std::max(0.0, P.DraftCooldown - Dt);
    P.HopTime = std::max(0.0, P.HopTime - Dt);
    DriftBuffer = std::max(0.0, DriftBuffer - Dt);
    if (Input.Drift && !PreviousDrift) DriftBuffer = .16;
    PreviousDrift = Input.Drift;

    if (P.Drifting && (!Input.Drift || P.Speed < 7))
    {
        const int Level = Tier(P.DriftCharge);
        if (Level > 0 && P.DriftTime >= .55 && P.Speed >= 11)
        {
            const bool Clean = std::abs(Angle(P.Yaw - P.VelocityYaw)) < .24 && std::abs(P.Yaw) < .8 && Input.Brake < .2;
            const double Duration = std::array<double, 4>{0, .65, 1.15, 1.85}[Level] + (Clean ? .35 : 0);
            P.Boost = std::max(P.Boost, Duration); P.Energy = Clamp(P.Energy + Level * 5 + (Clean ? 5 : 0), 0, 100);
            Events.push_back({EventType::Drift, static_cast<double>(Level)});
            if (Clean) Events.push_back({EventType::CleanExit, static_cast<double>(Level)});
        }
        P.Drifting = false; P.DriftCharge = P.DriftTime = 0; P.DriftTier = P.DriftDirection = 0;
    }
    if (!P.Drifting && Input.Drift && DriftBuffer > 0 && std::abs(Input.Steer) > .15 && P.Speed > 11)
    {
        P.Drifting = true; P.DriftDirection = Input.Steer > 0 ? 1 : -1;
        P.DriftCharge = P.DriftTime = 0; P.DriftTier = 0; P.HopTime = HopDuration; DriftBuffer = 0;
    }
    P.HopHeight = P.HopTime > 0 ? .28 * std::sin(Pi * (1 - P.HopTime / HopDuration)) : 0;
    P.Steering = Damp(P.Steering, Input.Steer, 18, Dt);
    const double SteeringDemand = P.Drifting ? P.DriftDirection * .28 + P.Steering * .95 : P.Steering;
    const double TurnLimit = Lerp(2.35, 1.15, Clamp(P.Speed / 55, 0, 1));
    P.YawRate = Damp(P.YawRate, SteeringDemand * TurnLimit * Clamp(P.Speed / 7, 0, 1), 14, Dt);
    P.Yaw = Angle(P.Yaw + P.YawRate * Dt);

    const double BaseGrip = EngineClass == 50 ? 46 : EngineClass == 150 ? 38 : 42;
    const double GripAcceleration = BaseGrip * (P.Drifting ? .84 : 1.0) * (1 + Input.Brake * .20);
    const double DesiredVelocityYaw = P.Yaw - (P.Drifting ? P.DriftDirection * .30 : 0);
    const double TurnError = Angle(DesiredVelocityYaw - P.VelocityYaw);
    const double MaximumVelocityTurn = GripAcceleration / std::max(5.0, P.Speed);
    P.VelocityYaw = Angle(P.VelocityYaw + Clamp(TurnError * (P.Drifting ? 4.5 : 11.0), -MaximumVelocityTurn, MaximumVelocityTurn) * Dt);
    P.SlipAngle = Angle(P.Yaw - P.VelocityYaw);
    P.Grip = Clamp(1 - std::abs(P.SlipAngle) / .9, 0, 1);

    const double Limit = MaxSpeed() * (P.Boost > 0 ? 1.32 : 1) * (P.Slowed > 0 ? .62 : 1);
    const double Acceleration = EngineClass == 50 ? 18 : EngineClass == 150 ? 25 : 22;
    const double Drive = Input.Throttle * (1 - Input.Brake) * (Acceleration + (P.Boost > 0 ? 25 : 0)) * (1 - std::pow(Clamp(P.Speed / Limit, 0, 1), 1.6));
    const double Drag = .50 + .0018 * P.Speed * P.Speed + (1 - Input.Throttle) * 1.4
        + std::max(0.0, P.Speed - Limit) * .8 + std::sin(P.SlipAngle) * std::sin(P.SlipAngle) * 4.5 + (P.Drifting ? .6 : 0);
    P.Speed = Clamp(P.Speed + (Drive - Drag - Input.Brake * 38 - Before.Tangent.Y * 9.81) * Dt, 0, MaxSpeed() * 1.5);
    if (P.Speed < .12 && Input.Throttle == 0) P.Speed = 0;
    const double Along = P.Speed * std::cos(P.VelocityYaw);
    P.LateralVelocity = P.Speed * std::sin(P.VelocityYaw);
    P.Distance += Along / Clamp(1 - Curve * P.Lateral, .45, 1.55) * Dt;
    P.Lateral += P.LateralVelocity * Dt;

    const auto After = Circuit.At(StartOffset + P.Distance);
    const double RoadTurn = std::atan2(Before.Tangent.Z * After.Tangent.X - Before.Tangent.X * After.Tangent.Z,
        Before.Tangent.X * After.Tangent.X + Before.Tangent.Z * After.Tangent.Z);
    P.Yaw = Angle(P.Yaw - RoadTurn); P.VelocityYaw = Angle(P.VelocityYaw - RoadTurn);
    if (std::abs(P.Lateral) > Barrier)
    {
        const double Side = P.Lateral > 0 ? 1.0 : -1.0;
        const double Impact = std::abs(P.LateralVelocity);
        P.Lateral = Side * (Barrier - std::min(.25, (std::abs(P.Lateral) - Barrier) * .2));
        const double Forward = P.Speed * std::cos(P.VelocityYaw);
        P.VelocityYaw = std::atan2(-Side * Impact * .22, Forward);
        P.Yaw = Angle(P.Yaw + Angle(P.VelocityYaw - P.Yaw) * .20);
        P.Speed = std::max(0.0, P.Speed - (P.Shield > 0 ? .5 : 2.5 + Impact * .35));
        P.Drifting = false; P.DriftCharge = P.DriftTime = 0; P.DriftTier = P.DriftDirection = 0;
        if (ImpactCooldown == 0 && Impact > 1)
        {
            P.Energy = std::max(0.0, P.Energy - 4); ImpactCooldown = .35;
            Events.push_back({EventType::Impact, Clamp(Impact / 18, .15, 1)});
        }
    }
    if (P.Drifting)
    {
        P.DriftTime += Dt;
        const double Slip = Angle(P.Yaw - P.VelocityYaw) * P.DriftDirection;
        if (Slip > .13 && Slip < .90 && P.Speed > 12 && std::abs(P.Lateral) < Barrier - .15)
            P.DriftCharge = Clamp(P.DriftCharge + Dt * (.22 + .23 * Clamp(P.Speed / 35, 0, 1) + .08 * P.Grip), 0, 1);
        else P.DriftCharge = std::max(0.0, P.DriftCharge - Dt * .15);
        const int Level = Tier(P.DriftCharge);
        if (Level > P.DriftTier) Events.push_back({EventType::DriftTier, static_cast<double>(Level)});
        P.DriftTier = Level;
    }
    P.LateralVelocity = P.Speed * std::sin(P.VelocityYaw);
    P.SlipAngle = Angle(P.Yaw - P.VelocityYaw); P.Grip = Clamp(1 - std::abs(P.SlipAngle) / .9, 0, 1);
    P.Energy = Clamp(P.Energy + Dt * (P.Speed > 10 ? .85 : .45), 0, 100);
}

void Race::StepRival(Racer& R, double Dt)
{
    const double Curve = Circuit.Curvature(StartOffset + R.Distance);
    const double AheadCurve = Circuit.Curvature(StartOffset + R.Distance + 24);
    const double CornerLimit = CornerSpeed(R.Distance) * R.Personality;
    const double Desired = std::min(MaxSpeed() * R.Personality * (R.Boost > 0 ? 1.23 : 1), CornerLimit * 1.04) * (R.Slowed > 0 ? .55 : 1);
    const double Difference = Desired - R.Speed;
    R.Speed += Clamp(Difference * 1.5, -30.0, 19.0) * Dt;
    double Lane = Clamp(Curve * 160 - AheadCurve * 115, -3.8, 3.8) + (R.Id % 3 - 1) * .5;
    for (const auto& Ahead : Racers)
        if (Ahead.Id != R.Id && Ahead.FinishTime < 0 && Ahead.Distance > R.Distance && Ahead.Distance - R.Distance < 11 && std::abs(Ahead.Lateral - Lane) < 1.8)
            Lane = Clamp(Ahead.Lateral + (R.Id % 2 ? 2.5 : -2.5), -4.7, 4.7);
    const double Before = R.Lateral;
    R.Lateral = Damp(R.Lateral, Lane, 1.6, Dt); R.LateralVelocity = (R.Lateral - Before) / Dt;
    R.VelocityYaw = std::atan2(R.LateralVelocity, std::max(R.Speed, .1));
    R.Yaw = Damp(R.Yaw, R.VelocityYaw, 8, Dt); R.YawRate = Curve * R.Speed;
    R.Steering = Clamp((Curve * R.Speed + R.VelocityYaw * 2) / 1.5, -1, 1);
    R.Distance += R.Speed / Clamp(1 - Curve * R.Lateral, .6, 1.4) * Dt;
    if (std::abs(Curve) < .008 && CornerSpeed(R.Distance) >= MaxSpeed() * .96
        && std::floor((Elapsed + R.Id * 2.7) / 14) != std::floor((Elapsed - Dt + R.Id * 2.7) / 14)) R.Boost = 1;
}

void Race::StepDraft(double Dt)
{
    auto& P = Player();
    bool InWake = false;
    if (P.Speed > 14 && std::abs(P.VelocityYaw) < .4 && P.FinishTime < 0)
        for (int I = 1; I < RacerCount; ++I)
        {
            const auto& Ahead = Racers[I]; const double Gap = Ahead.Distance - P.Distance;
            if (Ahead.FinishTime < 0 && Gap > 3 && Gap < 20 && std::abs(Ahead.Lateral - P.Lateral) < 1.5) { InWake = true; break; }
        }
    P.DraftCharge = Clamp(P.DraftCharge + Dt * (InWake && P.DraftCooldown == 0 ? 1 / 1.15 : -1.8), 0, 1);
    if (P.DraftCharge >= 1 && P.DraftCooldown == 0)
    {
        P.Boost = std::max(P.Boost, 1.2); P.Energy = Clamp(P.Energy + 5, 0, 100);
        P.DraftCooldown = 4; Events.push_back({EventType::Slipstream});
    }
}
}
