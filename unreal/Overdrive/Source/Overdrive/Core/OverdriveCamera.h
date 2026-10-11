#pragma once

#include "OverdriveSimulation.h"

namespace Overdrive
{
struct CameraInput
{
    Vec3 Focus;
    Vec3 Forward{0, 0, 1};
    double Speed = 0;
    bool Boosting = false;
    bool LookBack = false;
    bool Calm = false;
};

struct OVERDRIVE_API CameraSpring
{
    Vec3 Position, Velocity, PreviousGoal;
    void Reset(Vec3 Goal);
    void Step(Vec3 Goal, double Dt, double Frequency);
};

class OVERDRIVE_API FollowCamera
{
public:
    void Reset(const CameraInput& Input);
    void Step(const CameraInput& Input, double Dt);
    Vec3 Position() const { return Boom.Position; }
    Vec3 Target() const { return Aim.Position; }
    double Fov() const { return FieldOfView; }
    bool IsReady() const { return Ready; }
private:
    CameraSpring Anchor, Boom, Aim;
    double Heading = 0, Speed = 0, RearView = 0, FieldOfView = 60;
    bool Ready = false;
};
}
