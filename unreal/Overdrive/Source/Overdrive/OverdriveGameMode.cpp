#include "OverdriveGameMode.h"
#include "OverdriveController.h"
#include "OverdriveHUD.h"
#include "OverdriveKart.h"
#include "OverdriveWorld.h"
#include "OverdriveSaveGame.h"
#include "OverdriveGeometry.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundWaveProcedural.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
constexpr TCHAR SaveSlot[] = TEXT("OverdriveNative_v2");
int32 ClassIndex(int32 Class) { return Class==50?0:Class==150?2:1; }
AOverdriveController* Driver(const UObject* Context) { return Cast<AOverdriveController>(UGameplayStatics::GetPlayerController(Context,0)); }
#if !UE_BUILD_SHIPPING
Overdrive::DriverInput VerificationDriver(const Overdrive::Race& Race)
{
    using namespace Overdrive;
    const auto& P=Race.Player();const double Curve=Race.Circuit.Curvature(StartOffset+P.Distance+2);
    const double TurnLimit=Lerp(2.35,1.15,Clamp(P.Speed/55,0,1));
    const double Heading=Clamp(-P.Lateral*.075,-.35,.35);
    const double Steer=Clamp((Curve*P.Speed+std::remainder(Heading-P.Yaw,Pi*2)*3.3+std::remainder(Heading-P.VelocityYaw,Pi*2)*1.3)/TurnLimit,-1,1);
    const double Brake=Clamp((P.Speed-Race.CornerSpeed(P.Distance)*.86)/7,0,1);
    const double D=Wrap(P.Distance,Race.Circuit.Length);
    const bool Slide=(D>120&&D<190&&P.Speed>14)||(P.Drifting&&P.DriftTime<2.65);
    if(Slide&&(!P.Drifting||P.DriftTime<2.65))
    {
        const double Direction=P.Drifting?P.DriftDirection:(Curve>0?1:-1);
        const double Target=Direction*.30-Clamp(P.Lateral*.08,-.3,.3);
        const double Rate=Curve*P.Speed+std::remainder(Target-P.Yaw,Pi*2)*3;
        return {.75,Clamp((P.Speed-25)/8,0,1),Clamp((Rate/TurnLimit-Direction*.28)/.95,-1,1),true};
    }
    return {Brake>.1?.2:1,Brake,Steer,false};
}
#endif
}

AOverdriveGameMode::AOverdriveGameMode()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;
    DefaultPawnClass=AOverdriveKart::StaticClass();PlayerControllerClass=AOverdriveController::StaticClass();HUDClass=AOverdriveHUD::StaticClass();
}

void AOverdriveGameMode::BeginPlay()
{
    Super::BeginPlay();
    Settings=Cast<UOverdriveSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0));
    if(!Settings)
    {
        Settings=Cast<UOverdriveSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("OverdriveNative_v1"),0));
        if(Settings)Settings->BestLaps={-1,-1,-1};
    }
    if(!Settings)Settings=Cast<UOverdriveSaveGame>(UGameplayStatics::CreateSaveGameObject(UOverdriveSaveGame::StaticClass()));
    Settings->SaveVersion=2;Settings->Sanitize();
    PreviousRacers=RenderRacers=Simulation.Racers;
    for(TActorIterator<AOverdriveWorld> It(GetWorld());It;++It){Coast=*It;break;}
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if(!Coast)Coast=GetWorld()->SpawnActor<AOverdriveWorld>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    if(!Coast||!Coast->IsReady())BootError=TEXT("The native coast assets are missing. In Unreal, run OverdriveSetup before cooking, then open /Game/Overdrive/Maps/CloudtopCoast. Check Output Log for details.");
    for(int32 I=0;I<Overdrive::RacerCount;++I)
    {
        AOverdriveKart* Kart=I==0?Cast<AOverdriveKart>(UGameplayStatics::GetPlayerPawn(this,0)):nullptr;
        if(!Kart)Kart=GetWorld()->SpawnActor<AOverdriveKart>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        if(!Kart){BootError=TEXT("A kart could not be created. Check Unreal's Output Log.");return;}
        Karts.Add(Kart);Kart->SetPaint(ODGeometry::Color(Overdrive::RivalColors[I]));
    }
    if(AOverdriveController* PC=Driver(this)){PC->Possess(Karts[0]);PC->SetViewTarget(Karts[0]);PC->RefreshInputContext();}
    EngineSound=NewObject<USoundWaveProcedural>(this);EngineSound->SetSampleRate(22050);EngineSound->NumChannels=1;
    EngineSound->Duration=INDEFINITELY_LOOPING_DURATION;EngineSound->bLooping=false;
    EngineAudio=NewObject<UAudioComponent>(this);EngineAudio->bAutoActivate=false;EngineAudio->bIsUISound=true;EngineAudio->SetSound(EngineSound);
    EngineAudio->RegisterComponent();FillAudio();EngineAudio->Play();
    ApplySettings();
#if !UE_BUILD_SHIPPING
    FString CapturePath;
    if(FParse::Value(FCommandLine::Get(),TEXT("ODCapture="),CapturePath))
    {
        bVerificationCapture=true;Settings->bAutoAccelerate=false;
        bDriveVerification=FParse::Param(FCommandLine::Get(),TEXT("ODDriveTest"));
        if(bDriveVerification)StartRace();
        if(FParse::Param(FCommandLine::Get(),TEXT("ODCaptureGarage")))SetPage(EOverdrivePage::Garage);
        double Distance=0;
        if(FParse::Value(FCommandLine::Get(),TEXT("ODCaptureDistance="),Distance))
        {
            StartRace();Simulation.State=Overdrive::Phase::Racing;Simulation.Player().Distance=Distance;
            PreviousRacers=RenderRacers=Simulation.Racers;
        }
        float Delay=12.f;FParse::Value(FCommandLine::Get(),TEXT("ODCaptureDelay="),Delay);Delay=FMath::Clamp(Delay,2.f,60.f);
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer,FTimerDelegate::CreateLambda([CapturePath]()
        {
            FScreenshotRequest::RequestScreenshot(CapturePath,true,false);
        }),Delay,false);
        if(FParse::Param(FCommandLine::Get(),TEXT("ODCaptureExit")))
        {
            FTimerHandle ExitTimer;GetWorldTimerManager().SetTimer(ExitTimer,FTimerDelegate::CreateLambda([](){FPlatformMisc::RequestExit(false);}),Delay+4.f,false);
        }
    }
#endif
}

void AOverdriveGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
#if !UE_BUILD_SHIPPING
    if(bDriveVerification)UE_LOG(LogTemp,Display,TEXT("Movement verification: frames=%d seconds=%.2f max_camera_speed=%.3f_mps max_chase_speed=%.3f_mps max_separation=%.3f_m drift_tier=%d distance=%.2f"),VerificationFrames,VerificationSeconds,VerificationCameraSpeed,VerificationChaseSpeed,VerificationSeparation,VerificationTier,Simulation.Player().Distance);
#endif
    if(EngineAudio)EngineAudio->Stop();Super::EndPlay(Reason);
}

bool AOverdriveGameMode::IsMenuVisible() const
{
    return Simulation.State==Overdrive::Phase::Menu||Simulation.State==Overdrive::Phase::Paused||CurrentPage==EOverdrivePage::Results;
}

void AOverdriveGameMode::SetPage(EOverdrivePage Page)
{
    CurrentPage=Page;SelectedRow=0;
    if(AOverdriveController* PC=Driver(this))PC->RefreshInputContext();
}
void AOverdriveGameMode::ShowNotice(const FString& Text,float Duration){Notice=Text;NoticeRemaining=Duration;}
void AOverdriveGameMode::SelectRow(int32 Index){SelectedRow=FMath::Clamp(Index,0,FMath::Max(0,GetMenuRows().Num()-1));}
void AOverdriveGameMode::MoveSelection(int32 Direction)
{
    const int32 Count=GetMenuRows().Num();if(Count>0)SelectedRow=(SelectedRow+Direction+Count)%Count;
}

void AOverdriveGameMode::StartRace()
{
    if(!Settings||!BootError.IsEmpty()){ShowNotice(BootError,6);return;}
    Simulation.Start(Settings->EngineClass);Simulation.BestLap=Settings->BestLaps[ClassIndex(Settings->EngineClass)];
    PreviousRacers=RenderRacers=Simulation.Racers;
    CurrentPage=EOverdrivePage::Home;SelectedRow=0;Accumulator=0;bCameraReady=false;bRecorded=false;ResultsDelay=-1;GoRemaining=0;Notice.Empty();
    for(AOverdriveKart* Kart:Karts)Kart->SetActorScale3D(FVector::OneVector);
    if(AOverdriveController* PC=Driver(this))PC->RefreshInputContext();
}
void AOverdriveGameMode::PauseRace(const FString& Reason)
{
    if(Simulation.State!=Overdrive::Phase::Racing&&Simulation.State!=Overdrive::Phase::Countdown)return;
    Simulation.Pause();PreviousRacers=RenderRacers=Simulation.Racers;Accumulator=0;SetPage(EOverdrivePage::Pause);
    if(!Reason.IsEmpty())ShowNotice(Reason,8);
    if(EngineAudio)EngineAudio->SetVolumeMultiplier(0);
}
void AOverdriveGameMode::ResumeRace()
{
    if(Simulation.State!=Overdrive::Phase::Paused)return;
    if(AOverdriveController* PC=Driver(this))if(!PC->CanSafelyResume()){ShowNotice(TEXT("Release both triggers and center the left stick to resume."),3);return;}
    Simulation.Resume();PreviousRacers=RenderRacers=Simulation.Racers;Accumulator=0;Notice.Empty();CurrentPage=EOverdrivePage::Home;
    if(AOverdriveController* PC=Driver(this))PC->RefreshInputContext();
}
void AOverdriveGameMode::RecoverKart()
{
    if(!Simulation.RecoverPlayer())return;
    PreviousRacers=RenderRacers=Simulation.Racers;Accumulator=0;
}
void AOverdriveGameMode::ReturnHome()
{
    Simulation.ToMenu();Accumulator=0;bCameraReady=false;ResultsDelay=-1;Notice.Empty();SetPage(EOverdrivePage::Home);
}
void AOverdriveGameMode::Back()
{
    if(CurrentPage==EOverdrivePage::Pause){ResumeRace();return;}
    if(CurrentPage==EOverdrivePage::Results){ReturnHome();return;}
    if(Simulation.State==Overdrive::Phase::Menu)SetPage(EOverdrivePage::Home);
}

TArray<FOverdriveMenuRow> AOverdriveGameMode::GetMenuRows() const
{
    if(!Settings)return {};
    using C=EOverdriveCommand;
    const FString Yes=TEXT("On"),No=TEXT("Off");
    switch(CurrentPage)
    {
        case EOverdrivePage::Home:return {
            {C::Start,TEXT("LET'S RACE"),TEXT("Cloudtop Coast / 3 laps")},
            {C::EngineClass,TEXT("Engine class"),FString::Printf(TEXT("%dcc"),Settings->EngineClass)},
            {C::Garage,TEXT("Garage"),UTF8_TO_TCHAR(Overdrive::PaintNames[Settings->PaintIndex])},
            {C::Weather,TEXT("Time of day"),Settings->bNight?TEXT("Blue hour"):TEXT("Golden hour")},
            {C::Settings,TEXT("Settings"),TEXT("Graphics / Controller / Sound")},
            {C::Controls,TEXT("How to play"),TEXT("Find your flow")},
            {C::Records,TEXT("Your high points"),TEXT("Race records")},
            {C::Quit,TEXT("Quit game"),TEXT("")}
        };
        case EOverdrivePage::Garage:return {{C::Paint,TEXT("Comet paint"),UTF8_TO_TCHAR(Overdrive::PaintNames[Settings->PaintIndex])},{C::Back,TEXT("Looking good. Let's go."),TEXT("")}};
        case EOverdrivePage::Settings:return {
            {C::Quality,TEXT("Graphics"),Settings->Quality==3?TEXT("Epic"):Settings->Quality==2?TEXT("High / Lumen"):TEXT("Performance")},
            {C::DeadZone,TEXT("Stick dead zone"),FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Settings->SteeringDeadZone*100))},
            {C::Sensitivity,TEXT("Steering sensitivity"),FString::Printf(TEXT("%.1fx"),Settings->SteeringSensitivity)},
            {C::AutoAccelerate,TEXT("Auto-accelerate"),Settings->bAutoAccelerate?Yes:No},
            {C::Vibration,TEXT("Controller vibration"),Settings->bVibration?Yes:No},
            {C::Volume,TEXT("Sound effects"),FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Settings->Volume*100))},
            {C::Labels,TEXT("Button labels"),Settings->ControllerLabels==1?TEXT("PlayStation"):TEXT("Xbox / Generic")},
            {C::CalmCamera,TEXT("Calmer camera"),Settings->bCalmCamera?Yes:No},
            {C::Back,TEXT("All set"),TEXT("")}
        };
        case EOverdrivePage::Pause:return {{C::Resume,TEXT("Back to the race"),TEXT("")},{C::Restart,TEXT("Start fresh"),TEXT("")},{C::Home,TEXT("Back to the coast"),TEXT("")}};
        case EOverdrivePage::Results:return {{C::Restart,TEXT("One more?"),TEXT("Race again")},{C::Home,TEXT("Back to the coast"),TEXT("")}};
        default:return {{C::Back,TEXT("Back to the coast"),TEXT("")}};
    }
}

void AOverdriveGameMode::AdjustSelection(int32 Direction)
{
    if(!Settings)return;
    const auto Rows=GetMenuRows();if(!Rows.IsValidIndex(SelectedRow))return;
    switch(Rows[SelectedRow].Command)
    {
        case EOverdriveCommand::EngineClass:Settings->EngineClass=std::array<int32,3>{50,100,150}[(ClassIndex(Settings->EngineClass)+Direction+3)%3];break;
        case EOverdriveCommand::Paint:Settings->PaintIndex=(Settings->PaintIndex+Direction+6)%6;break;
        case EOverdriveCommand::Weather:Settings->bNight=!Settings->bNight;break;
        case EOverdriveCommand::Quality:Settings->Quality=FMath::Clamp(Settings->Quality+Direction,1,3);break;
        case EOverdriveCommand::DeadZone:Settings->SteeringDeadZone=FMath::Clamp(Settings->SteeringDeadZone+Direction*.01f,.05f,.4f);break;
        case EOverdriveCommand::Sensitivity:Settings->SteeringSensitivity=FMath::Clamp(Settings->SteeringSensitivity+Direction*.1f,.5f,1.5f);break;
        case EOverdriveCommand::AutoAccelerate:Settings->bAutoAccelerate=!Settings->bAutoAccelerate;break;
        case EOverdriveCommand::Vibration:Settings->bVibration=!Settings->bVibration;break;
        case EOverdriveCommand::Volume:Settings->Volume=FMath::Clamp(Settings->Volume+Direction*.05f,0.f,1.f);break;
        case EOverdriveCommand::Labels:Settings->ControllerLabels=1-Settings->ControllerLabels;break;
        case EOverdriveCommand::CalmCamera:Settings->bCalmCamera=!Settings->bCalmCamera;break;
        default:return;
    }
    ApplySettings();SaveProgress();
}

void AOverdriveGameMode::ActivateSelection()
{
    const auto Rows=GetMenuRows();if(!Rows.IsValidIndex(SelectedRow))return;
    switch(Rows[SelectedRow].Command)
    {
        case EOverdriveCommand::Start:case EOverdriveCommand::Restart:StartRace();break;
        case EOverdriveCommand::Garage:SetPage(EOverdrivePage::Garage);break;
        case EOverdriveCommand::Settings:SetPage(EOverdrivePage::Settings);break;
        case EOverdriveCommand::Controls:SetPage(EOverdrivePage::Controls);break;
        case EOverdriveCommand::Records:SetPage(EOverdrivePage::Records);break;
        case EOverdriveCommand::Resume:ResumeRace();break;
        case EOverdriveCommand::Home:ReturnHome();break;
        case EOverdriveCommand::Back:Back();break;
        case EOverdriveCommand::Quit:UKismetSystemLibrary::QuitGame(this,Driver(this),EQuitPreference::Quit,false);break;
        default:AdjustSelection(1);break;
    }
}

void AOverdriveGameMode::ApplySettings()
{
    if(!Settings)return;
    if(Coast)Coast->SetNight(Settings->bNight);
    if(Karts.Num()>0)Karts[0]->SetPaint(ODGeometry::Color(Overdrive::Paints[Settings->PaintIndex]));
    if(EngineAudio)EngineAudio->SetVolumeMultiplier(Settings->Volume);
    if(GEngine&&GEngine->GetGameUserSettings())
    {
        auto* User=GEngine->GetGameUserSettings();User->SetOverallScalabilityLevel(Settings->Quality);User->ApplyNonResolutionSettings();User->SaveSettings();
    }
}
void AOverdriveGameMode::SaveProgress()
{
    if(bVerificationCapture)return;
    if(Settings&&!UGameplayStatics::SaveGameToSlot(Settings,SaveSlot,0))ShowNotice(TEXT("Your progress could not be saved. Check free disk space and permissions."),5);
}

void AOverdriveGameMode::HandleEvent(const Overdrive::RaceEvent& Event)
{
    using E=Overdrive::EventType;
    switch(Event.Type)
    {
        case E::Countdown:BeepFrequency=440;BeepRemaining=.16;break;
        case E::Go:GoRemaining=.8f;BeepFrequency=880;BeepRemaining=.4;break;
        case E::Boost:ShowNotice(TEXT("TURBO ENGAGED"),1);BeepFrequency=260;BeepRemaining=.2;break;
        case E::Drift:ShowNotice(Event.Value>=3?TEXT("ULTRA MINI-TURBO"):Event.Value>=2?TEXT("SUPER MINI-TURBO"):TEXT("MINI-TURBO"),1.3);BeepFrequency=660+Event.Value*110;BeepRemaining=.18;break;
        case E::DriftTier:BeepFrequency=500+Event.Value*210;BeepRemaining=.11;break;
        case E::CleanExit:ShowNotice(TEXT("CLEAN EXIT  /  BONUS TURBO"),1.2);BeepFrequency=1174;BeepRemaining=.18;break;
        case E::Slipstream:ShowNotice(TEXT("SLIPSTREAM  /  MAKE YOUR MOVE"),1.3);BeepFrequency=740;BeepRemaining=.20;break;
        case E::Launch:ShowNotice(TEXT("PERFECT LAUNCH"),1.4);BeepFrequency=1174;BeepRemaining=.23;break;
        case E::Recover:ShowNotice(TEXT("BACK ON TRACK  /  MOMENTUM LOST"),1.8);break;
        case E::Pickup:ShowNotice(Simulation.Player().HeldItem==Overdrive::Item::Turbo?TEXT("SUPER TURBO"):Simulation.Player().HeldItem==Overdrive::Item::Shield?TEXT("ENERGY SHIELD"):TEXT("SONIC PULSE"),1.2);BeepFrequency=990;BeepRemaining=.12;break;
        case E::Coin:BeepFrequency=1700;BeepRemaining=.07;break;
        case E::Impact:BeepFrequency=75;BeepRemaining=.09;break;
        case E::Shield:ShowNotice(TEXT("SHIELD ONLINE"));BeepFrequency=750;BeepRemaining=.2;break;
        case E::Pulse:ShowNotice(TEXT("PULSE RELEASED"));BeepFrequency=330;BeepRemaining=.3;break;
        case E::Lap:ShowNotice(Event.Value==3?TEXT("FINAL LAP"):TEXT("LAP 2"),2.2);BeepFrequency=1046;BeepRemaining=.3;break;
        case E::Finish:
            if(!bRecorded)
            {
                bRecorded=true;ResultsDelay=.8f;BeepFrequency=1046;BeepRemaining=.6;
                Settings->RacesCompleted=FMath::Min(Settings->RacesCompleted+1,999999);
                const double Best=*std::min_element(Simulation.LapTimes.begin(),Simulation.LapTimes.end());
                double& Saved=Settings->BestLaps[ClassIndex(Settings->EngineClass)];if(Saved<0||Best<Saved)Saved=Best;
                SaveProgress();
            }
            break;
    }
#if !UE_BUILD_SHIPPING
    if(bDriveVerification&&Event.Type==E::DriftTier&&Event.Value==2)
    {
        FString Path;if(FParse::Value(FCommandLine::Get(),TEXT("ODCapture="),Path))FScreenshotRequest::RequestScreenshot(Path+TEXT(".drift.png"),true,false);
    }
#endif
    if(AOverdriveController* PC=Driver(this))PC->Feedback(Event.Type);
}

void AOverdriveGameMode::FillAudio()
{
    if(!EngineSound)return;
    while(EngineSound->GetAvailableAudioByteCount()<4410)
    {
        TArray<int16> Samples;Samples.SetNumUninitialized(1102);
        const auto& P=Simulation.Player();const bool Running=Simulation.State==Overdrive::Phase::Racing;
        for(int16& Sample:Samples)
        {
            const double Frequency=42+P.Speed*2.7+(P.Boost>0?30:0);
            double Signal=Running?(std::sin(SoundTime*Frequency*2*Overdrive::Pi)*.045+std::sin(SoundTime*Frequency*Overdrive::Pi)*.025):0;
            if(Running&&P.Speed>8&&P.HopHeight<.08)
            {
                const double Slip=Overdrive::Clamp((std::abs(P.SlipAngle)-.10)*.18,0,.055);
                Signal+=Slip*(std::sin(SoundTime*(1050+P.Speed*9)*2*Overdrive::Pi)+.35*std::sin(SoundTime*1730*2*Overdrive::Pi));
            }
            if(BeepRemaining>0){Signal+=std::sin(SoundTime*BeepFrequency*2*Overdrive::Pi)*std::min(BeepRemaining*8,.10);BeepRemaining=std::max(0.0,BeepRemaining-1.0/22050);}
            Sample=static_cast<int16>(Overdrive::Clamp(Signal,-1,1)*32767);SoundTime+=1.0/22050;
        }
        EngineSound->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
    }
}

void AOverdriveGameMode::UpdateVisuals(float Dt)
{
    if(Karts.Num()!=Overdrive::RacerCount)return;
    const bool Menu=Simulation.State==Overdrive::Phase::Menu;
    FVector DesiredPosition,DesiredTarget;float Fov=62;
    if(Menu)
    {
        auto Hero=Simulation.Player();Hero.Distance=82-Overdrive::StartOffset;Hero.Lateral=0;Hero.Yaw=-.36;Hero.Boost=0;Hero.Shield=0;Hero.Drifting=false;
        const bool Garage=CurrentPage==EOverdrivePage::Garage;
        if(Garage)Hero.Yaw=SceneTime*.22;
#if !UE_BUILD_SHIPPING
        if(bVerificationCapture){double Angle;if(FParse::Value(FCommandLine::Get(),TEXT("ODCaptureAngle="),Angle))Hero.Yaw=Angle;}
#endif
        Karts[0]->SetActorScale3D(FVector(2.15));Karts[0]->UpdatePose(Simulation.Circuit,Hero,SceneTime,true);
        for(int32 I=1;I<Karts.Num();++I){auto R=Simulation.Racers[I];R.Distance=190+I*119+SceneTime*(11+I);R.Lateral=I%2?2:-2;R.Boost=0;R.Drifting=false;R.Shield=0;Karts[I]->SetActorHiddenInGame(false);Karts[I]->UpdatePose(Simulation.Circuit,R,SceneTime);}
        const FVector HeroPosition=Karts[0]->GetActorLocation();
        DesiredPosition=HeroPosition+(Garage?FVector(1770,1430,1050):FVector(2350,1800,1180));
        DesiredTarget=HeroPosition+(Garage?FVector(-160,260,280):FVector(-350,480,290));
        Fov=Garage?51:54;
    }
    else
    {
        const AOverdriveController* PC=Driver(this);bool LookBack=PC&&PC->IsLookingBack();
#if !UE_BUILD_SHIPPING
        if(bDriveVerification)LookBack=Simulation.Elapsed>12&&Simulation.Elapsed<13;
#endif
        for(int32 I=0;I<Karts.Num();++I)
        {
            auto Pose=RenderRacers[I];
            if(Pose.FinishTime>=0&&I!=0)
            {
                Pose.Distance=Simulation.Circuit.Length*Overdrive::LapCount+28+I*7;
                Pose.Lateral=I%2?2.1:-2.1;Pose.Yaw=0;Pose.Boost=0;Pose.Drifting=false;Pose.Shield=0;Pose.HopHeight=0;
            }
            const double Gap=Pose.Distance-RenderRacers[0].Distance;
            const bool ObstructsCamera=LookBack?(Gap>2.4&&Gap<13):(Gap<-2.4&&Gap>-15);
            Karts[I]->SetActorHiddenInGame(I!=0&&ObstructsCamera);
            Karts[I]->UpdatePose(Simulation.Circuit,Pose,SceneTime);
        }
        const auto& P=RenderRacers[0];const auto Frame=Simulation.Circuit.At(Overdrive::StartOffset+P.Distance);
        const double Heading=P.Speed>2?P.VelocityYaw:P.Yaw;
        Overdrive::CameraInput Input;
        Input.Focus=Simulation.Circuit.Position(Overdrive::StartOffset+P.Distance,P.Lateral,.60);
        Input.Forward=Frame.Tangent*std::cos(Heading)+Frame.Right*std::sin(Heading);
        Input.Speed=P.FinishTime>=0?0:P.Speed;Input.Boosting=P.Boost>0;Input.LookBack=LookBack;Input.Calm=Settings->bCalmCamera;
        if(!bCameraReady){ChaseRig.Reset(Input);bCameraReady=true;}
        else if(Simulation.State!=Overdrive::Phase::Paused)ChaseRig.Step(Input,Dt);
        CameraPosition=ODGeometry::ToUnreal(ChaseRig.Position());CameraTarget=ODGeometry::ToUnreal(ChaseRig.Target());
        UCameraComponent* Camera=Karts[0]->GetRaceCamera();Camera->SetWorldLocation(CameraPosition);Camera->SetWorldRotation((CameraTarget-CameraPosition).Rotation());
        Camera->SetFieldOfView(ChaseRig.Fov());
        if(Coast)Coast->AnimatePickups(Simulation.Circuit,SceneTime);
        return;
    }
    if(!bCameraReady){CameraPosition=DesiredPosition;CameraTarget=DesiredTarget;bCameraReady=true;}
    else
    {
        CameraPosition=FMath::Lerp(CameraPosition,DesiredPosition,1-FMath::Exp(-Dt*(Menu?3:Settings->bCalmCamera?8:6)));
        CameraTarget=FMath::Lerp(CameraTarget,DesiredTarget,1-FMath::Exp(-Dt*8));
    }
    UCameraComponent* Camera=Karts[0]->GetRaceCamera();Camera->SetWorldLocation(CameraPosition);Camera->SetWorldRotation((CameraTarget-CameraPosition).Rotation());
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,Fov,Dt,3));
    if(Coast)Coast->AnimatePickups(Simulation.Circuit,SceneTime);
}

void AOverdriveGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!Settings||!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;
    const float Dt=FMath::Clamp(DeltaSeconds,0.f,.25f);
    if(NoticeRemaining>0){NoticeRemaining-=Dt;if(NoticeRemaining<=0)Notice.Empty();}
    GoRemaining=FMath::Max(0.f,GoRemaining-Dt);
    if(Simulation.State!=Overdrive::Phase::Paused)SceneTime+=Dt;
    Overdrive::DriverInput Input;
    if(AOverdriveController* PC=Driver(this))Input=PC->GetDrivingInput();
    if(Settings->bAutoAccelerate)Input.Throttle=1;
    Accumulator=std::min(Accumulator+Dt,.25);
    while(Accumulator>=Overdrive::FixedStep)
    {
#if !UE_BUILD_SHIPPING
        if(bDriveVerification)
        {
            Input=Simulation.State==Overdrive::Phase::Countdown?Overdrive::DriverInput{Simulation.Countdown<.8?1.0:0.0,0,0,false}:VerificationDriver(Simulation);
            if(Simulation.Elapsed>VerificationNextBoost&&Simulation.CornerSpeed(Simulation.Player().Distance)>=Simulation.MaxSpeed()*.98)
            {
                Simulation.UseBoost();VerificationNextBoost=Simulation.Elapsed+8;
            }
        }
#endif
        PreviousRacers=Simulation.Racers;Simulation.Step(Overdrive::FixedStep,Input);Accumulator-=Overdrive::FixedStep;
    }
    const double Alpha=Accumulator/Overdrive::FixedStep;
    for(int32 I=0;I<Overdrive::RacerCount;++I)RenderRacers[I]=Overdrive::InterpolateRacer(PreviousRacers[I],Simulation.Racers[I],Alpha);
    for(const auto& Event:Simulation.DrainEvents())HandleEvent(Event);
    if(ResultsDelay>=0){ResultsDelay-=Dt;if(ResultsDelay<0)SetPage(EOverdrivePage::Results);}
    UpdateVisuals(Dt);
#if !UE_BUILD_SHIPPING
    if(bDriveVerification&&Karts.Num()>0)
    {
        if(VerificationFrames>0)
        {
            const double CameraSpeed=(CameraPosition-VerificationLastCamera).Length()/(100*Dt);
            if(CameraSpeed>VerificationCameraSpeed&&CameraSpeed>75)UE_LOG(LogTemp,Display,TEXT("Camera motion peak: time=%.4f dt=%.6f camera_speed=%.3f kart_speed=%.3f distance=%.3f phase=%d"),Simulation.Elapsed,Dt,CameraSpeed,Simulation.Player().Speed,Simulation.Player().Distance,static_cast<int32>(Simulation.State));
            VerificationCameraSpeed=FMath::Max(VerificationCameraSpeed,CameraSpeed);
            if(Simulation.Elapsed<12||Simulation.Elapsed>15)VerificationChaseSpeed=FMath::Max(VerificationChaseSpeed,CameraSpeed);
        }
        VerificationSeparation=FMath::Max(VerificationSeparation,(CameraPosition-Karts[0]->GetActorLocation()).Length()/100);
        VerificationLastCamera=CameraPosition;VerificationSeconds+=Dt;++VerificationFrames;
        VerificationTier=FMath::Max(VerificationTier,Simulation.Player().DriftTier);
    }
#endif
    if(EngineAudio)EngineAudio->SetVolumeMultiplier(Simulation.State==Overdrive::Phase::Paused?0:Settings->Volume);
    FillAudio();
}
