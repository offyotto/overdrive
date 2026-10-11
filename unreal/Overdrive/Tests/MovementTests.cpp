#include "Core/OverdriveCamera.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace Overdrive;
namespace
{
int Checks = 0;
void Require(bool Value, const char* Message)
{
    ++Checks;
    if (!Value) { std::cerr << "FAIL: " << Message << '\n'; std::exit(1); }
}
void Near(double A, double B, double Epsilon, const char* Message) { Require(std::abs(A-B) <= Epsilon, Message); }
Race Ready()
{
    Race R;R.Start(100);R.State=Phase::Racing;
    for(int I=1;I<RacerCount;++I){R.Racers[I].Distance=500+I*10;R.Racers[I].FinishTime=1;}
    R.Player().Distance=0;R.Player().Lateral=0;return R;
}
void Step(Race& R,double Time,DriverInput Input)
{
    for(int I=0;I<static_cast<int>(std::ceil(Time/FixedStep));++I)R.Step(FixedStep,Input);
}
DriverInput Pilot(const Race& R)
{
    const auto& P=R.Player();const double Curve=R.Circuit.Curvature(StartOffset+P.Distance+2);
    const double Target=Clamp(-P.Lateral*.075,-.35,.35),Limit=Lerp(2.35,1.15,Clamp(P.Speed/55,0,1));
    const double Steer=Clamp((Curve*P.Speed+std::remainder(Target-P.Yaw,Pi*2)*3.3+std::remainder(Target-P.VelocityYaw,Pi*2)*1.3)/Limit,-1,1);
    const double Brake=Clamp((P.Speed-R.CornerSpeed(P.Distance)*.86)/7,0,1);
    return {Brake>.1?.2:1,Brake,Steer,false};
}
}

int main()
{
    {
        auto R=Ready();R.Player().Distance=145;R.Player().Speed=22;
        const auto Before=R.Circuit.At(StartOffset+R.Player().Distance);
        const double StartHeading=std::atan2(Before.Tangent.X,Before.Tangent.Z);
        Step(R,.1,{0,0,0,false});
        const auto After=R.Circuit.At(StartOffset+R.Player().Distance);
        const double EndHeading=std::atan2(After.Tangent.X,After.Tangent.Z)+R.Player().Yaw;
        Near(std::remainder(EndHeading-StartHeading,Pi*2),0,.0001,"no-input heading does not automatically follow a corner");
        Require(std::abs(R.Player().Lateral)>.015,"road curves away from an unsteered kart");
    }
    {
        auto A=Ready(),B=Ready();A.Player().Speed=B.Player().Speed=30;
        Step(A,.18,{1,0,.9,false});Step(B,.18,{1,0,.9,true});
        Require(B.Player().Drifting&&B.Player().HopHeight>0,"drift begins with a visible hop");
        Require(std::abs(B.Player().SlipAngle)>std::abs(A.Player().SlipAngle),"drifting changes actual momentum slip");
        Require(std::abs(B.Player().Yaw-B.Player().VelocityYaw)>.05,"body and velocity can point differently");
    }
    {
        auto R=Ready();R.Player().Speed=23;
        R.Step(FixedStep,{1,0,0,true});
        R.Step(FixedStep,{1,0,.5,true});Require(R.Player().Drifting,"early drift input buffers until steering arrives");
        R.Pause();const auto Before=R.Player();Step(R,1,{1,0,1,true});
        Near(R.Player().Yaw,Before.Yaw,0,"pause freezes body heading");Near(R.Player().HopTime,Before.HopTime,0,"pause freezes drift hop");
    }
    {
        auto R=Ready();auto& P=R.Player();P.Speed=24;P.Energy=40;
        P.Drifting=true;P.DriftTime=1.5;P.DriftCharge=.70;P.DriftTier=2;P.DriftDirection=1;
        P.Yaw=.10;P.VelocityYaw=.02;P.HopTime=.13;P.HopHeight=.28;
        const auto Before=P;
        R.Pause();Step(R,.5,{});
        Near(P.Distance,Before.Distance,0,"pausing a charged drift preserves position");
        Near(P.Yaw,Before.Yaw,0,"pausing a charged drift preserves heading");
        Near(P.HopHeight,Before.HopHeight,0,"pausing mid-hop preserves rendered height");
        R.Resume();R.Step(FixedStep,{});
        Require(P.Boost==0,"input reset after pause cannot cash out a drift turbo");
        Require(!P.Drifting&&P.DriftCharge==0&&P.DriftTier==0,"paused drift must be deliberately restarted");
        Require(P.Energy<Before.Energy+1,"pause cannot award drift energy");
        for(auto E:R.DrainEvents())Require(E.Type!=EventType::Drift&&E.Type!=EventType::CleanExit,"pause cannot emit a drift release reward");
    }
    {
        auto R=Ready();R.Player().Speed=23;R.Player().Distance=120;
        const double Direction=R.Circuit.Curvature(StartOffset+R.Player().Distance)>0?1:-1;
        R.Step(FixedStep,{1,0,Direction*.7,true});int MaxTier=0;
        for(int I=0;I<210&&R.Player().Drifting;++I)
        {
            const auto& P=R.Player();const double Target=Direction*.30-Clamp(P.Lateral*.08,-.3,.3);
            const double Rate=R.Circuit.Curvature(StartOffset+P.Distance+2)*P.Speed+std::remainder(Target-P.Yaw,Pi*2)*3;
            const double TurnLimit=Lerp(2.35,1.15,Clamp(P.Speed/55,0,1));
            const double Steer=Clamp((Rate/TurnLimit-Direction*.28)/.95,-1,1);
            R.Step(FixedStep,{.75,Clamp((P.Speed-25)/8,0,1),Steer,true});MaxTier=std::max(MaxTier,R.Player().DriftTier);
        }
        Require(MaxTier==3,"controlled sustained drift can naturally reach all three tiers");
        R.Step(FixedStep,{1,0,0,false});Require(R.Player().Boost>=1.8,"natural tier-three drift releases an ultra turbo");
    }
    {
        auto R=Ready();R.Player().Speed=24;R.Player().Drifting=true;R.Player().DriftTime=1.5;R.Player().DriftCharge=.70;
        R.Player().DriftDirection=1;R.Player().Yaw=.10;R.Player().VelocityYaw=.02;
        R.Step(FixedStep,{1,0,0,false});
        Require(R.Player().Boost>1.4,"clean tier-two exit earns extra turbo duration");
        bool Clean=false;for(auto E:R.DrainEvents())Clean|=E.Type==EventType::CleanExit;Require(Clean,"clean exit generates distinct feedback");
        const double Boost=R.Player().Boost;R.Step(FixedStep,{1,0,0,false});Require(R.Player().Boost<Boost,"release cannot repeatedly cash out drift charge");
    }
    {
        auto R=Ready();R.Player().Speed=24;R.Player().Lateral=5.80;R.Player().Yaw=R.Player().VelocityYaw=1;
        R.Player().Drifting=true;R.Player().DriftTime=2;R.Player().DriftCharge=.99;R.Player().DriftDirection=1;
        R.Step(FixedStep,{1,0,1,true});Require(!R.Player().Drifting&&R.Player().DriftCharge==0,"wall collision forfeits drift reward");
        Require(R.Player().Speed<24,"wall contact costs momentum");
    }
    {
        auto R=Ready();R.Player().Speed=20;R.Player().Distance=100;R.Player().Yaw=1;
        Require(R.RecoverPlayer(),"manual recovery accepted in race");Require(R.Player().Distance<=100&&R.Player().Speed==0,"recovery cannot gain progress or speed");
        Require(!R.RecoverPlayer(),"recovery has a cooldown");R.Pause();Require(!R.RecoverPlayer(),"recovery disabled while paused");
    }
    {
        auto R=Ready();const double Zone=R.Circuit.PickupZones[0]+14-StartOffset;
        R.Player().Distance=Zone-.01;R.Player().Speed=20;R.Step(FixedStep,{1,0,0,false});const int Coins=R.Player().Coins;
        R.Player().Distance=Zone-.01;R.Player().Speed=20;R.Step(FixedStep,{1,0,0,false});
        Require(Coins==1&&R.Player().Coins==Coins,"re-crossing one coin cannot farm rewards");
        R.Player().Distance=R.Circuit.Length-.01;R.Step(FixedStep,{1,0,0,false});
        R.Player().Distance=R.Circuit.Length-.01;R.Step(FixedStep,{1,0,0,false});Require(R.LapTimes.size()==1,"re-crossing start line cannot duplicate a lap");
    }
    {
        Race Timed;Timed.Start(100);while(Timed.Countdown>.8)Timed.Step(FixedStep,{});
        while(Timed.State==Phase::Countdown)Timed.Step(FixedStep,{1,0,0,false});Require(Timed.Player().Boost>1,"well-timed throttle earns launch turbo");
        Race Held;Held.Start(100);while(Held.State==Phase::Countdown)Held.Step(FixedStep,{1,0,0,false});Require(Held.Player().Boost==0,"holding throttle from start does not farm launch reward");
    }
    {
        auto R=Ready();R.Player().Speed=20;R.Racers[1].FinishTime=-1;
        for(int I=0;I<90;++I){R.Racers[1].Distance=R.Player().Distance+10;R.Racers[1].Lateral=R.Player().Lateral;R.Step(FixedStep,Pilot(R));}
        bool Draft=false;for(auto E:R.DrainEvents())Draft|=E.Type==EventType::Slipstream;Require(Draft,"holding an opponent's wake earns a passing boost");
    }
    {
        Racer A,B;A.Yaw=3.13;B.Yaw=-3.13;A.Distance=100;B.Distance=102;
        const auto Mid=InterpolateRacer(A,B,.5);Near(Mid.Distance,101,1.e-9,"fixed-step pose interpolates distance");
        Require(std::abs(Mid.Yaw)>3.1,"angle interpolation uses shortest arc across wrap");
    }
    {
        CameraSpring A,B;A.Reset({});B.Reset({});
        for(int I=1;I<=30;++I)A.Step({0,0,I/30.0*20},1.0/30,12);
        for(int I=1;I<=144;++I)B.Step({0,0,I/144.0*20},1.0/144,12);
        Near((A.Position-B.Position).Length(),0,1.e-8,"spring tracks a moving goal independent of timestep partition");
        const Vec3 Before=A.Position;A.Step({0,0,100},1.0/144,12);
        Require((A.Position-Before).Length()<2,"camera spring does not snap to a discontinuous goal");
    }
    {
        FollowCamera Camera;CameraInput Input;Camera.Reset(Input);const auto Start=Camera.Position();
        for(int I=0;I<120;++I)Camera.Step(Input,1.0/60);
        Near((Camera.Position()-Start).Length(),0,1.e-7,"stationary camera does not drift");
        Input.LookBack=true;Camera.Step(Input,1.0/120);
        Require((Camera.Position()-Start).Length()<.5,"look-back does not teleport to opposite side");
        for(int I=0;I<180;++I)Camera.Step(Input,1.0/120);
        Require(Camera.Position().Z>8,"look-back eventually reaches front view");
        Input.Calm=true;for(int I=0;I<120;++I)Camera.Step(Input,1.0/60);Near(Camera.Fov(),60,.01,"calm camera suppresses dynamic FOV");
    }
    {
        const auto At=[](double Time)
        {
            CameraInput I;I.Focus={30*std::sin(Time*.5),2*std::sin(Time*.4),30*std::cos(Time*.5)};
            I.Forward={std::cos(Time*.5),0,-std::sin(Time*.5)};I.Speed=15;I.Boosting=Time>=4&&Time<5;I.LookBack=Time>=6&&Time<7;return I;
        };
        const auto RunCamera=[&](int FPS)
        {
            FollowCamera Camera;Camera.Reset(At(0));for(int I=1;I<=FPS*9;++I)Camera.Step(At(I/static_cast<double>(FPS)),1.0/FPS);return Camera;
        };
        const auto Reference=RunCamera(144);
        for(int FPS:{30,60})
        {
            const auto Camera=RunCamera(FPS);
            Require((Camera.Position()-Reference.Position()).Length()<.45,"camera framing stays consistent at 30, 60 and 144 fps");
            Require((Camera.Target()-Reference.Target()).Length()<.45,"camera aim stays consistent across frame rates");
            Near(Camera.Fov(),Reference.Fov(),.05,"FOV transition is frame-rate independent");
        }
    }
    {
        auto R=Ready();FollowCamera Camera;auto Previous=R.Racers;double Accumulator=0,Time=0;Vec3 Last;bool First=true;
        double MaximumDelta=0,MaximumDistance=0;
        for(int Frame=0;Time<40;++Frame)
        {
            const double Dt=Frame%131==0?.075:Frame%3==0?1.0/30:1.0/144;Accumulator+=Dt;Time+=Dt;
            while(Accumulator>=FixedStep){Previous=R.Racers;R.Step(FixedStep,Pilot(R));Accumulator-=FixedStep;}
            const auto P=InterpolateRacer(Previous[0],R.Player(),Accumulator/FixedStep);const auto F=R.Circuit.At(StartOffset+P.Distance);
            CameraInput I;I.Focus=R.Circuit.Position(StartOffset+P.Distance,P.Lateral,.6);I.Forward=F.Tangent*std::cos(P.VelocityYaw)+F.Right*std::sin(P.VelocityYaw);I.Speed=P.Speed;
            Camera.Step(I,Dt);const auto Position=Camera.Position();
            Require(std::isfinite(Position.Length()),"camera remains finite across hitches and corners");
            MaximumDistance=std::max(MaximumDistance,(Position-I.Focus).Length());
            if(!First)MaximumDelta=std::max(MaximumDelta,(Position-Last).Length()/Dt);First=false;Last=Position;
        }
        Require(MaximumDistance<19,"camera keeps kart framed through corners and hitches");Require(MaximumDelta<90,"camera never teleports during normal racing");
        std::cout<<"Camera maximum separation "<<MaximumDistance<<"m; maximum travel rate "<<MaximumDelta<<"m/s\n";
    }
    std::cout<<"Passed "<<Checks<<" movement and camera checks.\n";
}
