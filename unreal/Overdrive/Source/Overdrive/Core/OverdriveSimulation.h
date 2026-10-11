#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#ifndef OVERDRIVE_API
#define OVERDRIVE_API
#endif

namespace Overdrive
{
constexpr double Pi = 3.14159265358979323846;
constexpr double TrackWidth = 15.0;
constexpr double StartOffset = 21.0;
constexpr int RacerCount = 8;
constexpr int LapCount = 3;
constexpr double FixedStep = 1.0 / 60.0;

inline double Clamp(double V, double A, double B) { return std::clamp(V, A, B); }
inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
inline double Damp(double A, double B, double Rate, double Dt) { return Lerp(A, B, 1.0 - std::exp(-Rate * Dt)); }
inline double Wrap(double V, double M) { return std::fmod(std::fmod(V, M) + M, M); }

struct Vec3
{
    double X = 0, Y = 0, Z = 0;
    Vec3 operator+(const Vec3& B) const { return {X + B.X, Y + B.Y, Z + B.Z}; }
    Vec3 operator-(const Vec3& B) const { return {X - B.X, Y - B.Y, Z - B.Z}; }
    Vec3 operator*(double S) const { return {X * S, Y * S, Z * S}; }
    double Dot(const Vec3& B) const { return X * B.X + Y * B.Y + Z * B.Z; }
    Vec3 Cross(const Vec3& B) const { return {Y * B.Z - Z * B.Y, Z * B.X - X * B.Z, X * B.Y - Y * B.X}; }
    double Length() const { return std::sqrt(Dot(*this)); }
    Vec3 Normal() const { const double L = Length(); return L > 1.e-12 ? *this * (1.0 / L) : Vec3{}; }
};

struct TrackFrame
{
    Vec3 Position, Tangent, Right;
    double Distance = 0;
};

class OVERDRIVE_API Random
{
public:
    explicit Random(std::uint32_t Seed) : State(Seed) {}
    double Next();
private:
    std::uint32_t State;
};

class OVERDRIVE_API Track
{
public:
    Track();
    TrackFrame At(double Distance) const;
    Vec3 Position(double Distance, double Lateral = 0, double Lift = 0) const;
    double Curvature(double Distance) const;
    double Length = 0;
    std::vector<TrackFrame> Samples;
    std::array<double, 4> BoostPads{};
    std::array<double, 4> PickupZones{};
};

enum class Phase { Menu, Countdown, Racing, Paused, Finished };
enum class Item { None, Turbo, Shield, Pulse };
enum class EventType { Countdown, Go, Boost, Drift, Pickup, Coin, Impact, Lap, Finish, Shield, Pulse, DriftTier, CleanExit, Slipstream, Launch, Recover };

struct DriverInput
{
    double Throttle = 0;
    double Brake = 0;
    double Steer = 0;
    bool Drift = false;
};

struct RaceEvent
{
    EventType Type;
    double Value = 0;
};

struct Racer
{
    int Id = 0;
    double Distance = 0, Lateral = 0, LateralVelocity = 0;
    double Speed = 0, Yaw = 0, DriftCharge = 0, Boost = 0;
    double VelocityYaw = 0, YawRate = 0, Steering = 0, SlipAngle = 0, Grip = 1;
    double DriftTime = 0, HopTime = 0, HopHeight = 0, DraftCharge = 0, DraftCooldown = 0;
    double Energy = 100, Shield = 0, Slowed = 0, Personality = .82;
    int Coins = 0, DriftDirection = 0, DriftTier = 0;
    Item HeldItem = Item::None;
    bool Drifting = false, LaunchReady = false;
    double FinishTime = -1;
};

OVERDRIVE_API Racer InterpolateRacer(const Racer& Previous, const Racer& Current, double Alpha);

class OVERDRIVE_API Race
{
public:
    Race();
    void Start(int Class);
    void Pause();
    void Resume();
    void ToMenu();
    void Step(double Dt, DriverInput Input);
    bool UseBoost();
    bool UseItem();
    bool RecoverPlayer();
    double CornerSpeed(double Distance) const;
    int Lap() const;
    int Position() const;
    double MaxSpeed() const;
    std::array<int, RacerCount> Standings() const;
    std::vector<RaceEvent> DrainEvents();
    Racer& Player() { return Racers[0]; }
    const Racer& Player() const { return Racers[0]; }
    Track Circuit;
    std::array<Racer, RacerCount> Racers{};
    Phase State = Phase::Menu;
    int EngineClass = 100;
    double Elapsed = 0, Countdown = 3.5, BestLap = -1;
    std::vector<double> LapTimes;
private:
    void ResetGrid();
    void StepPlayer(double Dt, DriverInput Input);
    void StepRival(Racer& Racer, double Dt);
    void StepDraft(double Dt);
    bool Crossed(double OldDistance, double NewDistance, double Position) const;
    double LapStartedAt = 0, ImpactCooldown = 0, DriftBuffer = 0, RecoveryLock = 0, PlayerFrontier = 0;
    double PreviousThrottle = 0;
    bool PreviousDrift = false;
    int LastCountdown = 4;
    Phase ResumeState = Phase::Racing;
    Random Rng{82};
    std::vector<RaceEvent> Events;
};

struct OVERDRIVE_API ControllerFilter
{
    static double Steering(double Axis, double DeadZone = .16, double Sensitivity = 1.0);
    static double Trigger(double Axis, double DeadZone = .04);
    static bool IsNeutral(double Stick, double Throttle, double Brake);
};

class OVERDRIVE_API ControllerNeutralGate
{
public:
    bool Step(double Stick, double Throttle, double Brake, double Dt, bool Connected);
    void Suspend() { Armed = false; NeutralSeconds = 0; }
    bool IsArmed() const { return Armed; }
private:
    bool Armed = false;
    double NeutralSeconds = 0;
};

class OVERDRIVE_API NavigationRepeat
{
public:
    int Step(double Axis, double Dt);
    void Reset() { Direction = 0; Remaining = 0; }
private:
    int Direction = 0;
    double Remaining = 0;
};

OVERDRIVE_API std::string FormatTime(double Seconds);
inline constexpr std::array<const char*, RacerCount> Names = {"You", "Nova", "Milo", "Indie", "Jett", "Rio", "Cleo", "Dash"};
inline constexpr std::array<const char*, 6> PaintNames = {"Solar orange", "Lagoon blue", "Cosmic lilac", "Mint condition", "Cherry pop", "Polar white"};
inline constexpr std::array<std::uint32_t, 6> Paints = {0xff7545, 0x47b9e5, 0xb49ae7, 0x62d9b0, 0xed6076, 0xe8eeee};
inline constexpr std::array<std::uint32_t, RacerCount> RivalColors = {0xff7545, 0x52c6db, 0xaa8ee2, 0xf1c867, 0xee6d93, 0x64d5a5, 0x769eef, 0xdf98ca};
}
