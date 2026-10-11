#include "Core/OverdriveSimulation.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <functional>

using namespace Overdrive;
int Checks = 0;
void Require(bool Condition, const char* Message)
{
    ++Checks;
    if (!Condition) { std::cerr << "FAIL: " << Message << '\n'; std::exit(EXIT_FAILURE); }
}
void Near(double A, double B, double Tolerance, const char* Message) { Require(std::abs(A - B) <= Tolerance, Message); }
void Run(Race& R, double Seconds, DriverInput Input = {1,0,0,false})
{
    for (int I=0;I<static_cast<int>(std::ceil(Seconds/FixedStep));++I) R.Step(FixedStep,Input);
}
Race Ready(int Class=100) { Race R; R.Start(Class); Run(R,3.6,{}); R.DrainEvents(); return R; }

int main()
{
    Track T;
    Near(T.Length,903.7079806610471,1.e-8,"same circuit length as browser version");
    for(double D=-T.Length;D<T.Length*2;D+=7.7)
    {
        const auto S=T.At(D);Near(S.Tangent.Length(),1,1.e-9,"unit track tangent");
        Near(S.Right.Length(),1,1.e-9,"unit track right");Near(S.Tangent.Dot(S.Right),0,.002,"orthogonal track frame");
        Require(std::isfinite(T.Curvature(D)),"finite curvature");
    }
    Near((T.At(0).Position-T.At(T.Length).Position).Length(),0,1.e-9,"closed circuit");
    Near((T.At(.001).Position-T.At(T.Length-.001).Position).Length(),.002,.0001,"continuous circuit seam");
    Random A(42),B(42);for(int I=0;I<100;++I)Near(A.Next(),B.Next(),0,"deterministic seeded random");

    Race Grid;Grid.Start(100);Require(Grid.Position()==4,"fourth-place player grid slot");
    for(int I=0;I<RacerCount;++I)for(int J=I+1;J<RacerCount;++J)
        Require(std::abs(Grid.Racers[I].Distance-Grid.Racers[J].Distance)>=4||std::abs(Grid.Racers[I].Lateral-Grid.Racers[J].Lateral)>=2.6,"non-overlapping grid");
    const double Start=Grid.Player().Distance;Run(Grid,1);Near(Grid.Player().Distance,Start,0,"countdown locks movement");
    Grid.Pause();const double Count=Grid.Countdown;Run(Grid,3);Near(Grid.Countdown,Count,0,"paused countdown freezes");
    Grid.Resume();Run(Grid,3);Require(Grid.State==Phase::Racing,"countdown resumes into race");

    auto Slow=Ready(50),Fast=Ready(150);Run(Slow,2);Run(Fast,2);Require(Fast.Player().Speed>Slow.Player().Speed+12,"engine classes have distinct speeds");
    auto Partial=Ready(),Full=Ready();Run(Partial,1.5,{.4,0,0,false});Run(Full,1.5);Require(Partial.Player().Speed<Full.Player().Speed*.65,"analog throttle is proportional");
    const double BeforeBrake=Full.Player().Speed;Run(Full,1,{1,1,0,false});Require(Full.Player().Speed<BeforeBrake*.15,"brake overrides held throttle");
    auto HalfBrake=Ready();Run(HalfBrake,1.5);Run(HalfBrake,.4,{1,.4,0,false});Require(HalfBrake.Player().Speed>Full.Player().Speed,"analog brake modulation");

    auto Turbo=Ready();Require(Turbo.UseBoost(),"turbo starts");Near(Turbo.Player().Energy,65,1.e-9,"turbo costs 35 energy");Require(!Turbo.UseBoost(),"boost cannot spam");
    Turbo.Pause();Require(!Turbo.UseBoost(),"boost disabled in pause");Turbo.Resume();Turbo.Player().Boost=0;Turbo.Player().Energy=10;Require(!Turbo.UseBoost(),"insufficient energy blocks boost");
    auto Drift=Ready();Run(Drift,1.2);Run(Drift,.8,{1,0,.5,true});Require(Drift.Player().DriftCharge>.2,"analog steering charges drift");
    Drift.Step(FixedStep,{1,0,0,false});Require(Drift.Player().Boost>0&&!Drift.Player().Drifting,"drift release awards mini turbo");
    auto Wall=Ready(150);Run(Wall,8,{1,0,1,true});Require(std::abs(Wall.Player().Lateral)<=6.320001,"guardrails contain kart");

    auto Items=Ready();Items.Player().Distance=Items.Circuit.PickupZones[0]-StartOffset-.01;Items.Player().Speed=40;Items.Step(FixedStep,{1,0,0,false});Require(Items.Player().HeldItem!=Item::None,"crossing pickup awards item");
    Items.Player().HeldItem=Item::Shield;Require(Items.UseItem(),"shield activates");Near(Items.Player().Shield,6,0,"shield duration");
    Items.Player().HeldItem=Item::Turbo;Items.UseItem();Require(Items.Player().Boost>=2.8,"item turbo duration");
    Items.Player().HeldItem=Item::Pulse;Items.Racers[1].Distance=Items.Player().Distance+10;Items.UseItem();Near(Items.Racers[1].Slowed,3.5,0,"pulse slows nearby opponent");
    auto Pads=Ready();Pads.Player().Distance=Pads.Circuit.BoostPads[0]-StartOffset-.01;Pads.Player().Lateral=0;Pads.Player().Speed=40;Pads.Step(FixedStep,{1,0,0,false});Require(Pads.Player().Boost>=1.1,"boost strip works");
    auto Coins=Ready();Coins.Player().Distance=Coins.Circuit.PickupZones[0]+14-StartOffset-.01;Coins.Player().Lateral=0;Coins.Player().Speed=40;Coins.Player().Energy=50;Coins.Step(FixedStep,{1,0,0,false});Require(Coins.Player().Coins==1&&Coins.Player().Energy>=52,"coins recharge turbo");

    for(int Class:{50,100,150})
    {
        auto R=Ready(Class);
        for(int Frame=0;Frame<60*180&&R.State!=Phase::Finished;++Frame)
        {
            const auto& P=R.Player();const double Curve=R.Circuit.Curvature(StartOffset+P.Distance+2);
            const double Heading=Clamp(-P.Lateral*.075,-.35,.35);
            const double TurnLimit=Lerp(2.35,1.15,Clamp(P.Speed/55,0,1));
            const double Steer=Clamp((Curve*P.Speed+std::remainder(Heading-P.Yaw,Pi*2)*3.3+std::remainder(Heading-P.VelocityYaw,Pi*2)*1.3)/TurnLimit,-1,1);
            const double SafeSpeed=R.CornerSpeed(P.Distance)*.86;
            const double Brake=Clamp((P.Speed-SafeSpeed)/7,0,1);
            R.Step(FixedStep,{Brake>.1?.2:1,Brake,Steer,false});
            if(Frame%240==0&&R.CornerSpeed(P.Distance)>R.MaxSpeed()*.99&&std::abs(Curve)<.008)R.UseBoost();
            if(R.Player().HeldItem!=Item::None)R.UseItem();
        }
        Require(R.State==Phase::Finished,"full three-lap race finishes for every class");Require(R.LapTimes.size()==3,"exactly three laps recorded");
        const double Finish=R.Player().FinishTime;Run(R,100);Near(R.Player().FinishTime,Finish,0,"finish time remains fixed");
        for(const auto& Driver:R.Racers)Require(Driver.FinishTime>=0,"all AI rivals finish");
        const auto Order=R.Standings();for(int I=1;I<RacerCount;++I)Require(R.Racers[Order[I]].FinishTime>=R.Racers[Order[I-1]].FinishTime,"standings sort by actual finish");
        R.Start(Class);Require(R.LapTimes.empty()&&R.Player().Energy==100&&R.Elapsed==0,"restart clears race state");
    }
    const double Nan=std::numeric_limits<double>::quiet_NaN();auto Invalid=Ready();const double Before=Invalid.Elapsed;
    Invalid.Step(Nan,{});Invalid.Step(-1,{});Invalid.Step(std::numeric_limits<double>::infinity(),{});Near(Invalid.Elapsed,Before,0,"invalid time steps ignored");
    Invalid.Step(FixedStep,{Nan,Nan,Nan,false});Require(std::isfinite(Invalid.Player().Speed)&&std::isfinite(Invalid.Player().Lateral),"invalid device axes cannot corrupt simulation");

    Near(ControllerFilter::Steering(.1),0,0,"stick drift inside deadzone ignored");Near(ControllerFilter::Steering(-.16),0,0,"negative deadzone boundary");
    Near(ControllerFilter::Steering(1),1,1.e-9,"full right steering");Near(ControllerFilter::Steering(-1),-1,1.e-9,"full left steering");
    Near(ControllerFilter::Steering(.5),-ControllerFilter::Steering(-.5),1.e-9,"symmetric steering curve");
    Require(ControllerFilter::Steering(.8)>ControllerFilter::Steering(.4),"proportional stick steering");
    Near(ControllerFilter::Trigger(.03),0,0,"trigger noise ignored");Near(ControllerFilter::Trigger(1),1,0,"full trigger range");
    Require(ControllerFilter::Trigger(.5)>0&&ControllerFilter::Trigger(.5)<1,"half trigger remains analog");
    Near(ControllerFilter::Steering(Nan),0,0,"invalid stick input sanitized");Near(ControllerFilter::Trigger(Nan),0,0,"invalid trigger input sanitized");
    Require(ControllerFilter::IsNeutral(0,0,0),"neutral controller can resume");Require(!ControllerFilter::IsNeutral(.8,0,0),"held stick blocks resume");Require(!ControllerFilter::IsNeutral(0,.9,0),"held throttle blocks resume");
    ControllerNeutralGate Gate;Require(!Gate.IsArmed(),"controller starts with analog inputs suppressed");
    for(int I=0;I<30;++I)Require(!Gate.Step(0,1,0,FixedStep,true),"held trigger cannot rearm controller");
    for(int I=0;I<5;++I)Require(!Gate.Step(0,0,0,FixedStep,true),"neutral must be sustained, not a flushed frame");
    Require(!Gate.Step(.8,0,0,FixedStep,true),"stick movement resets neutral timer");
    for(int I=0;I<12;++I)Gate.Step(0,0,0,FixedStep,true);
    Require(Gate.IsArmed(),"neutral controller rearms");Require(Gate.Step(.8,1,0,FixedStep,true),"armed controller can drive normally");
    Require(!Gate.Step(0,0,0,FixedStep,false)&&!Gate.IsArmed(),"disconnect suppresses analog immediately");
    for(int I=0;I<12;++I)Gate.Step(0,0,0,FixedStep,true);
    Gate.Suspend();Require(!Gate.IsArmed(),"keyboard resume leaves gamepad behind neutral gate");
    Require(!Gate.Step(0,0,0,FixedStep,true),"first resumed frame cannot rearm a held controller");
    Require(!Gate.Step(Nan,0,0,FixedStep,true),"invalid gamepad axes fail closed");
    Require(!Gate.Step(0,0,0,Nan,true),"invalid device timing fails closed");
    NavigationRepeat Nav;Require(Nav.Step(.8,FixedStep)==1,"menu stick first press");
    for(int I=0;I<20;++I)Require(Nav.Step(.8,FixedStep)==0,"menu navigation does not repeat too soon");
    bool Repeated=false;for(int I=0;I<10;++I)Repeated|=Nav.Step(.8,FixedStep)==1;Require(Repeated,"held menu navigation repeats");
    Require(Nav.Step(0,FixedStep)==0,"menu neutral releases");Require(Nav.Step(-.8,FixedStep)==-1,"menu opposite direction fires");Nav.Reset();Require(Nav.Step(.8,FixedStep)==1,"disconnect reset clears navigation latch");
    Require(FormatTime(59.999)=="01:00.00"&&FormatTime(119.999)=="02:00.00","timer minute rollover");Require(FormatTime(Nan)=="--:--.--","invalid time placeholder");
    std::cout << "PASS: " << Checks << " native simulation and controller checks\n";
    return EXIT_SUCCESS;
}
