#include "OverdriveSimulation.h"
#include <cstdio>
#include <numeric>

namespace Overdrive
{
double Random::Next()
{
    State += 0x6D2B79F5u;
    std::uint32_t T = (State ^ (State >> 15)) * (1u | State);
    T = (T + (T ^ (T >> 7)) * (61u | T)) ^ T;
    return static_cast<double>(T ^ (T >> 14)) / 4294967296.0;
}

Track::Track()
{
    constexpr std::array<Vec3, 14> Points = {{
        {-75,20,94}, {6,23,111}, {78,28,87}, {109,36,29},
        {97,45,-35}, {53,48,-88}, {-20,42,-110}, {-91,32,-81},
        {-109,24,-24}, {-65,23,9}, {-15,27,-20}, {16,29,14},
        {-19,24,49}, {-74,20,52}
    }};
    std::array<Vec3, 841> Positions{};
    const auto P = [&](int I) { return Points[static_cast<std::size_t>((I % 14 + 14) % 14)]; };
    for (int I = 0; I <= 840; ++I)
    {
        const double Parameter = I / 840.0 * 14;
        const int K = static_cast<int>(std::floor(Parameter));
        const double T = Parameter - K, T2 = T * T, T3 = T2 * T;
        Positions[I] = (P(K) * 2 + (P(K + 1) - P(K - 1)) * T
            + (P(K - 1) * 2 - P(K) * 5 + P(K + 1) * 4 - P(K + 2)) * T2
            + (P(K) * 3 - P(K - 1) - P(K + 1) * 3 + P(K + 2)) * T3) * .5;
    }
    Samples.reserve(841);
    for (int I = 0; I <= 840; ++I)
    {
        if (I > 0) Length += (Positions[I] - Positions[I - 1]).Length();
        const Vec3 Tangent = (Positions[(I + 1) % 840] - Positions[(I + 839) % 840]).Normal();
        Samples.push_back({Positions[I], Tangent, Vec3{0,1,0}.Cross(Tangent).Normal(), Length});
    }
    for (std::size_t I = 0; I < 4; ++I)
    {
        BoostPads[I] = std::array<double, 4>{.14,.39,.62,.86}[I] * Length;
        PickupZones[I] = std::array<double, 4>{.06,.30,.53,.78}[I] * Length;
    }
}

TrackFrame Track::At(double Distance) const
{
    const double D = std::isfinite(Distance) ? Wrap(Distance, Length) : 0;
    std::size_t Lo = 0, Hi = Samples.size() - 1;
    while (Lo < Hi - 1)
    {
        const std::size_t Mid = (Lo + Hi) / 2;
        if (Samples[Mid].Distance <= D) Lo = Mid; else Hi = Mid;
    }
    const auto& A = Samples[Lo]; const auto& B = Samples[Hi];
    const double T = (D - A.Distance) / std::max(1.e-9, B.Distance - A.Distance);
    return {A.Position + (B.Position - A.Position) * T,
        (A.Tangent + (B.Tangent - A.Tangent) * T).Normal(),
        (A.Right + (B.Right - A.Right) * T).Normal(), D};
}

Vec3 Track::Position(double Distance, double Lateral, double Lift) const
{
    const auto S = At(Distance);
    return S.Position + S.Right * Lateral + Vec3{0,Lift,0};
}

double Track::Curvature(double Distance) const
{
    const auto A = At(Distance).Tangent, B = At(Distance + 6).Tangent;
    return std::atan2(A.Z * B.X - A.X * B.Z, A.X * B.X + A.Z * B.Z) / 6;
}

Race::Race() { ResetGrid(); }
void Race::ResetGrid()
{
    for (int I = 0; I < RacerCount; ++I)
    {
        const int Slot = I == 0 ? 3 : I < 4 ? I - 1 : I;
        Racers[I] = {};
        Racers[I].Id = I; Racers[I].Distance = -Slot * 3.4;
        Racers[I].Lateral = Slot % 2 ? 2.1 : -2.1;
        Racers[I].Personality = .82 + I * .015;
    }
}

void Race::Start(int Class)
{
    EngineClass = Class == 50 || Class == 150 ? Class : 100;
    ResetGrid(); State = Phase::Countdown; Elapsed = 0; Countdown = 3.5; LastCountdown = 4;
    LapTimes.clear(); Events.clear(); LapStartedAt = 0; ImpactCooldown = 0; BestLap = -1; Rng = Random(82);
    DriftBuffer = RecoveryLock = PlayerFrontier = PreviousThrottle = 0; PreviousDrift = false;
}
void Race::Pause()
{
    if (State != Phase::Racing && State != Phase::Countdown) return;
    ResumeState = State; State = Phase::Paused;
    auto& P = Player();
    P.Drifting = false; P.DriftCharge = P.DriftTime = 0; P.DriftTier = P.DriftDirection = 0;
    P.LaunchReady = false; DriftBuffer = PreviousThrottle = 0; PreviousDrift = false;
}
void Race::Resume() { if (State == Phase::Paused) State = ResumeState; }
void Race::ToMenu() { State = Phase::Menu; Events.clear(); }
double Race::MaxSpeed() const { return EngineClass == 50 ? 32 : EngineClass == 150 ? 54 : 43; }
int Race::Lap() const { return std::min(LapCount, static_cast<int>(std::max(0.0, Player().Distance) / Circuit.Length) + 1); }
std::array<int, RacerCount> Race::Standings() const
{
    std::array<int, RacerCount> Order{}; std::iota(Order.begin(), Order.end(), 0);
    std::stable_sort(Order.begin(), Order.end(), [&](int A, int B)
    {
        const auto& X = Racers[A]; const auto& Y = Racers[B];
        if (X.FinishTime >= 0 && Y.FinishTime >= 0) return X.FinishTime < Y.FinishTime;
        if (X.FinishTime >= 0) return true;
        if (Y.FinishTime >= 0) return false;
        return X.Distance > Y.Distance;
    });
    return Order;
}
int Race::Position() const
{
    const auto Order = Standings();
    return static_cast<int>(std::find(Order.begin(), Order.end(), 0) - Order.begin()) + 1;
}
bool Race::UseBoost()
{
    auto& P = Player();
    if (State != Phase::Racing || P.Energy < 35 || P.Boost > .5) return false;
    P.Energy -= 35; P.Boost = 1.65; Events.push_back({EventType::Boost}); return true;
}
bool Race::UseItem()
{
    auto& P = Player();
    if (State != Phase::Racing || P.HeldItem == Item::None) return false;
    if (P.HeldItem == Item::Turbo) { P.Boost = std::max(P.Boost, 2.8); Events.push_back({EventType::Boost}); }
    if (P.HeldItem == Item::Shield) { P.Shield = 6; Events.push_back({EventType::Shield}); }
    if (P.HeldItem == Item::Pulse)
    {
        for (int I = 1; I < RacerCount; ++I) if (std::abs(Racers[I].Distance - P.Distance) < 45) Racers[I].Slowed = 3.5;
        Events.push_back({EventType::Pulse});
    }
    P.HeldItem = Item::None; return true;
}
bool Race::Crossed(double Old, double New, double Position) const
{
    return std::floor((New - Position) / Circuit.Length) > std::floor((Old - Position) / Circuit.Length);
}

void Race::Step(double Dt, DriverInput Input)
{
    if (!std::isfinite(Dt) || Dt <= 0 || State == Phase::Menu || State == Phase::Paused) return;
    Dt = std::min(Dt, .05);
    const auto Safe = [](double V, double Min, double Max) { return std::isfinite(V) ? Clamp(V, Min, Max) : 0.0; };
    Input.Throttle = Safe(Input.Throttle, 0, 1); Input.Brake = Safe(Input.Brake, 0, 1); Input.Steer = Safe(Input.Steer, -1, 1);
    if (State == Phase::Countdown)
    {
        if (Input.Throttle > .65 && PreviousThrottle <= .65) Player().LaunchReady = Countdown < 1.0 && Countdown > .18;
        if (Input.Throttle < .4) Player().LaunchReady = false;
        PreviousThrottle = Input.Throttle;
        Countdown -= Dt;
        const int N = static_cast<int>(std::ceil(Countdown));
        if (N != LastCountdown && N > 0 && N <= 3) { Events.push_back({EventType::Countdown, static_cast<double>(N)}); LastCountdown = N; }
        if (Countdown <= 0)
        {
            State = Phase::Racing; Events.push_back({EventType::Go});
            if (Player().LaunchReady) { Player().Boost = 1.15; Events.push_back({EventType::Launch}); }
        }
        return;
    }
    Elapsed += Dt; ImpactCooldown = std::max(0.0, ImpactCooldown - Dt); RecoveryLock = std::max(0.0, RecoveryLock - Dt);
    for (auto& R : Racers)
    {
        if (R.FinishTime >= 0) continue;
        const double Old = R.Distance;
        R.Boost = std::max(0.0, R.Boost - Dt); R.Shield = std::max(0.0, R.Shield - Dt); R.Slowed = std::max(0.0, R.Slowed - Dt);
        if (R.Id == 0) StepPlayer(Dt, Input); else StepRival(R, Dt);
        for (double Pad : Circuit.BoostPads)
            if (Crossed(R.Id == 0 ? std::max(Old, PlayerFrontier) : Old, R.Distance, Pad - StartOffset) && std::abs(R.Lateral) < 4.6)
            {
                R.Boost = std::max(R.Boost, 1.1);
                if (R.Id == 0) Events.push_back({EventType::Boost});
            }
        if (R.Id == 0)
        {
            for (double Zone : Circuit.PickupZones)
            {
                if (Crossed(std::max(Old, PlayerFrontier), R.Distance, Zone - StartOffset) && R.HeldItem == Item::None)
                {
                    R.HeldItem = static_cast<Item>(1 + static_cast<int>(Rng.Next() * 3));
                    Events.push_back({EventType::Pickup});
                }
                for (int J = 0; J < 4; ++J)
                    if (Crossed(std::max(Old, PlayerFrontier), R.Distance, Zone + 14 + J * 4 - StartOffset) && std::abs(R.Lateral) < 2.5)
                    {
                        ++R.Coins; R.Energy = Clamp(R.Energy + 2, 0, 100); Events.push_back({EventType::Coin});
                    }
            }
            const int NextLap = static_cast<int>(std::floor(R.Distance / Circuit.Length));
            if (NextLap > static_cast<int>(LapTimes.size()))
            {
                const double Fraction = Clamp((Circuit.Length * NextLap - Old) / std::max(1.e-9, R.Distance - Old), 0, 1);
                const double CrossingTime = Elapsed - Dt + Fraction * Dt;
                const double LapTime = CrossingTime - LapStartedAt;
                LapTimes.push_back(LapTime); LapStartedAt = CrossingTime;
                BestLap = BestLap < 0 ? LapTime : std::min(BestLap, LapTime);
                if (NextLap < LapCount) Events.push_back({EventType::Lap, static_cast<double>(NextLap + 1)});
            }
            PlayerFrontier = std::max(PlayerFrontier, R.Distance);
        }
        if (R.Distance >= Circuit.Length * LapCount)
        {
            R.FinishTime = Elapsed - Dt + Clamp((Circuit.Length * LapCount - Old) / std::max(1.e-9, R.Distance - Old), 0, 1) * Dt;
            R.Distance = Circuit.Length * LapCount;
            if (R.Id == 0) { State = Phase::Finished; Events.push_back({EventType::Finish}); }
        }
    }
    if (State == Phase::Racing) StepDraft(Dt);
    auto& P = Player();
    if (State == Phase::Racing && P.Shield == 0 && ImpactCooldown == 0)
        for (int I = 1; I < RacerCount; ++I)
        {
            const auto& R = Racers[I]; if (R.FinishTime >= 0) continue;
            const double Gap = R.Distance - P.Distance;
            if (std::abs(Gap) < 2.6 && std::abs(R.Lateral - P.Lateral) < 1.65)
            {
                P.VelocityYaw = std::remainder(P.VelocityYaw + (P.Lateral > R.Lateral ? .12 : -.12), Pi * 2);
                if (Gap > 0) P.Speed *= .88;
                P.DriftCharge *= .7;
                ImpactCooldown = .45; Events.push_back({EventType::Impact, .35}); break;
            }
        }
}

std::vector<RaceEvent> Race::DrainEvents()
{
    std::vector<RaceEvent> Result; Result.swap(Events); return Result;
}

double ControllerFilter::Steering(double Axis, double DeadZone, double Sensitivity)
{
    if (!std::isfinite(Axis) || !std::isfinite(DeadZone) || !std::isfinite(Sensitivity)) return 0;
    DeadZone = Clamp(DeadZone, .05, .4);
    const double Magnitude = std::abs(Clamp(Axis, -1, 1));
    if (Magnitude <= DeadZone) return 0;
    return std::copysign(Clamp(std::pow((Magnitude - DeadZone) / (1 - DeadZone), 1.25) * Clamp(Sensitivity, .5, 1.5), 0, 1), Axis);
}
double ControllerFilter::Trigger(double Axis, double DeadZone)
{
    if (!std::isfinite(Axis) || !std::isfinite(DeadZone)) return 0;
    DeadZone = Clamp(DeadZone, 0, .3);
    return Clamp((Axis - DeadZone) / (1 - DeadZone), 0, 1);
}
bool ControllerFilter::IsNeutral(double Stick, double Throttle, double Brake)
{
    return std::isfinite(Stick) && std::isfinite(Throttle) && std::isfinite(Brake)
        && std::abs(Stick) < .25 && Throttle < .1 && Brake < .1;
}
bool ControllerNeutralGate::Step(double Stick, double Throttle, double Brake, double Dt, bool Connected)
{
    if (!Connected || !std::isfinite(Dt) || Dt < 0 || !std::isfinite(Stick) || !std::isfinite(Throttle) || !std::isfinite(Brake))
    {
        Suspend(); return false;
    }
    if (Armed) return true;
    if (!ControllerFilter::IsNeutral(Stick, Throttle, Brake)) { NeutralSeconds = 0; return false; }
    NeutralSeconds += std::min(Dt, .1);
    Armed = NeutralSeconds >= .15;
    return Armed;
}
int NavigationRepeat::Step(double Axis, double Dt)
{
    if (!std::isfinite(Axis) || !std::isfinite(Dt) || Dt < 0) { Reset(); return 0; }
    const int Next = Axis > .55 ? 1 : Axis < -.55 ? -1 : 0;
    if (Next == 0) { Reset(); return 0; }
    if (Next != Direction) { Direction = Next; Remaining = .4; return Next; }
    Remaining -= std::min(Dt, .1);
    if (Remaining <= 0) { Remaining = .14; return Next; }
    return 0;
}
std::string FormatTime(double Seconds)
{
    if (!std::isfinite(Seconds) || Seconds < 0 || Seconds > 360000) return "--:--.--";
    const auto Hundredths = static_cast<long long>(std::llround(Seconds * 100));
    char Buffer[32];
    std::snprintf(Buffer, sizeof(Buffer), "%02lld:%02lld.%02lld", Hundredths / 6000, (Hundredths / 100) % 60, Hundredths % 100);
    return Buffer;
}
}
