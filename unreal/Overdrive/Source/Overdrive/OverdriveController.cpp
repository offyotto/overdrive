#include "OverdriveController.h"
#include "OverdriveGameMode.h"
#include "OverdriveSaveGame.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/CoreDelegates.h"

namespace
{
AOverdriveGameMode* Mode(const APlayerController* PC) { return PC->GetWorld()?PC->GetWorld()->GetAuthGameMode<AOverdriveGameMode>():nullptr; }
}

AOverdriveController::AOverdriveController()
{
    bEnableMotionControls=false;
}

void AOverdriveController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UEnhancedInputComponent* Input=Cast<UEnhancedInputComponent>(InputComponent);
    if(!Input)
    {
        Input=NewObject<UEnhancedInputComponent>(this,TEXT("OverdriveEnhancedInput"));
        Input->RegisterComponent();InputComponent=Input;
    }
    DriveContext=NewObject<UInputMappingContext>(this,TEXT("Driving"));
    MenuContext=NewObject<UInputMappingContext>(this,TEXT("Menus"));
    CommonContext=NewObject<UInputMappingContext>(this,TEXT("Common"));
    const auto Action=[&](const TCHAR* Name,EInputActionValueType Type)
    {
        UInputAction* A=NewObject<UInputAction>(this,Name);A->ValueType=Type;A->bTriggerWhenPaused=true;
        Actions.Add(A);return A;
    };
    const auto Axis=[&](const TCHAR* Name,UInputMappingContext* Context,TArray<FKey> Keys,auto Handler)
    {
        UInputAction* A=Action(Name,EInputActionValueType::Axis1D);
        auto* Trigger=NewObject<UInputTriggerDown>(A);Trigger->ActuationThreshold=.001f;A->Triggers.Add(Trigger);
        for(const FKey& Key:Keys)Context->MapKey(A,Key);
        Input->BindAction(A,ETriggerEvent::Triggered,this,Handler);
        Input->BindAction(A,ETriggerEvent::Completed,this,Handler);
        Input->BindAction(A,ETriggerEvent::Canceled,this,Handler);
        return A;
    };
    const auto Negative=[&](UInputMappingContext* Context,UInputAction* A,FKey Key)
    {
        FEnhancedActionKeyMapping& Mapping=Context->MapKey(A,Key);
        Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
    };
    const auto Button=[&](const TCHAR* Name,UInputMappingContext* Context,TArray<FKey> Keys,auto Handler)
    {
        UInputAction* A=Action(Name,EInputActionValueType::Boolean);
        for(const FKey& Key:Keys)Context->MapKey(A,Key);
        Input->BindAction(A,ETriggerEvent::Started,this,Handler);
    };
    Axis(TEXT("SteeringStick"),DriveContext,{EKeys::Gamepad_LeftX},&AOverdriveController::SetStick);
    Axis(TEXT("ThrottleTrigger"),DriveContext,{EKeys::Gamepad_RightTriggerAxis},&AOverdriveController::SetPadThrottle);
    Axis(TEXT("BrakeTrigger"),DriveContext,{EKeys::Gamepad_LeftTriggerAxis},&AOverdriveController::SetPadBrake);
    UInputAction* Steering=Axis(TEXT("KeyboardSteering"),DriveContext,{EKeys::D,EKeys::Right},&AOverdriveController::SetKeySteer);
    Negative(DriveContext,Steering,EKeys::A);Negative(DriveContext,Steering,EKeys::Left);
    Axis(TEXT("KeyboardThrottle"),DriveContext,{EKeys::W,EKeys::Up},&AOverdriveController::SetKeyThrottle);
    Axis(TEXT("KeyboardBrake"),DriveContext,{EKeys::S,EKeys::Down},&AOverdriveController::SetKeyBrake);
    UInputAction* DriftAction=Action(TEXT("Drift"),EInputActionValueType::Boolean);
    for(const FKey& Key:{EKeys::LeftShift,EKeys::RightShift,EKeys::Gamepad_RightShoulder})DriveContext->MapKey(DriftAction,Key);
    Input->BindAction(DriftAction,ETriggerEvent::Triggered,this,&AOverdriveController::SetDrift);
    Input->BindAction(DriftAction,ETriggerEvent::Completed,this,&AOverdriveController::SetDrift);
    Input->BindAction(DriftAction,ETriggerEvent::Canceled,this,&AOverdriveController::SetDrift);
    UInputAction* Look=Action(TEXT("LookBack"),EInputActionValueType::Boolean);
    DriveContext->MapKey(Look,EKeys::C);DriveContext->MapKey(Look,EKeys::Gamepad_FaceButton_Top);
    Input->BindAction(Look,ETriggerEvent::Triggered,this,&AOverdriveController::SetLookBack);
    Input->BindAction(Look,ETriggerEvent::Completed,this,&AOverdriveController::SetLookBack);
    Input->BindAction(Look,ETriggerEvent::Canceled,this,&AOverdriveController::SetLookBack);
    Button(TEXT("Turbo"),DriveContext,{EKeys::SpaceBar,EKeys::Gamepad_FaceButton_Bottom},&AOverdriveController::Boost);
    Button(TEXT("UseItem"),DriveContext,{EKeys::E,EKeys::Gamepad_FaceButton_Left},&AOverdriveController::UseItem);
    Button(TEXT("RecoverKart"),DriveContext,{EKeys::R,EKeys::Gamepad_FaceButton_Right},&AOverdriveController::Recover);
    Button(TEXT("Pause"),CommonContext,{EKeys::Escape,EKeys::Gamepad_Special_Right},&AOverdriveController::PausePressed);
    Button(TEXT("Confirm"),MenuContext,{EKeys::Enter,EKeys::Gamepad_FaceButton_Bottom},&AOverdriveController::Confirm);
    Button(TEXT("Back"),MenuContext,{EKeys::BackSpace,EKeys::Gamepad_FaceButton_Right},&AOverdriveController::BackPressed);
    UInputAction* X=Axis(TEXT("MenuHorizontal"),MenuContext,{EKeys::Right,EKeys::Gamepad_DPad_Right,EKeys::Gamepad_LeftX},&AOverdriveController::SetMenuX);
    Negative(MenuContext,X,EKeys::Left);Negative(MenuContext,X,EKeys::Gamepad_DPad_Left);
    UInputAction* Y=Axis(TEXT("MenuVertical"),MenuContext,{EKeys::Down,EKeys::Gamepad_DPad_Down},&AOverdriveController::SetMenuY);
    Negative(MenuContext,Y,EKeys::Up);Negative(MenuContext,Y,EKeys::Gamepad_DPad_Up);Negative(MenuContext,Y,EKeys::Gamepad_LeftY);
}

void AOverdriveController::BeginPlay()
{
    Super::BeginPlay();bEnableClickEvents=true;bEnableMouseOverEvents=true;bForceFeedbackEnabled=true;
    ConnectionHandle=IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this,&AOverdriveController::HandleConnection);
    DeactivationHandle=FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this,&AOverdriveController::HandleDeactivation);
    RefreshInputContext();
}
void AOverdriveController::EndPlay(const EEndPlayReason::Type Reason)
{
    StopRumble();
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnectionHandle);
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivationHandle);
    if(ULocalPlayer* Local=GetLocalPlayer())if(auto* Subsystem=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        if(DriveContext)Subsystem->RemoveMappingContext(DriveContext);
        if(MenuContext)Subsystem->RemoveMappingContext(MenuContext);
        if(CommonContext)Subsystem->RemoveMappingContext(CommonContext);
    }
    Super::EndPlay(Reason);
}

bool AOverdriveController::InputKey(const FInputKeyEventArgs& Params)
{
    const bool Gamepad=Params.Key.IsGamepadKey();
    if(Gamepad&&ActiveGamepad.IsValid()&&Params.InputDevice!=ActiveGamepad&&!bDisconnected)return false;
    const bool Handled=Super::InputKey(Params);
    if(Params.Event==IE_Pressed)
    {
        bUsingController=Gamepad;
        if(Gamepad)
        {
            if(ActiveGamepad!=Params.InputDevice)
            {
                ActiveGamepad=Params.InputDevice;PadGate.Suspend();
                RawStick=GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
                RawThrottle=GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis);
                RawBrake=GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
            }
            bDisconnected=false;
        }
    }
    if(Gamepad&&Params.Event==IE_Axis)
    {
        if(PendingGamepad!=Params.InputDevice){PendingGamepad=Params.InputDevice;PendingAxisMask=0;}
        if(Params.Key==EKeys::Gamepad_LeftX)PendingAxisMask|=1;
        if(Params.Key==EKeys::Gamepad_RightTriggerAxis)PendingAxisMask|=2;
        if(Params.Key==EKeys::Gamepad_LeftTriggerAxis)PendingAxisMask|=4;
        if(Params.Key==EKeys::Gamepad_LeftY)PendingAxisMask|=8;
    }
    return Handled;
}

void AOverdriveController::ResetControls()
{
    Stick=PadThrottle=PadBrake=KeySteer=KeyThrottle=KeyBrake=MenuX=MenuY=0;
    bDrift=false;bLookBack=false;PadGate.Suspend();HorizontalRepeat.Reset();VerticalRepeat.Reset();
}
void AOverdriveController::RefreshInputContext()
{
    if(!DriveContext||!MenuContext||!CommonContext)return;
    ResetControls();StopRumble();
    const AOverdriveGameMode* Game=Mode(this);const bool Menu=!Game||Game->IsMenuVisible();
    bShowMouseCursor=Menu;
    FInputModeGameOnly InputMode;InputMode.SetConsumeCaptureMouseDown(false);SetInputMode(InputMode);
    if(ULocalPlayer* Local=GetLocalPlayer())if(auto* Subsystem=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        Subsystem->RemoveMappingContext(DriveContext);Subsystem->RemoveMappingContext(MenuContext);
        FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;
        Subsystem->AddMappingContext(CommonContext,0,Options);
        Subsystem->AddMappingContext(Menu?MenuContext.Get():DriveContext.Get(),1,Options);
    }
}

Overdrive::DriverInput AOverdriveController::GetDrivingInput() const
{
    const AOverdriveGameMode* Game=Mode(this);
    if(!Game||Game->IsMenuVisible())return {};
    const UOverdriveSaveGame* Settings=Game->GetSettings();
    const double DeadZone=Settings?Settings->SteeringDeadZone:.16;
    const double Sensitivity=Settings?Settings->SteeringSensitivity:1;
    const bool PadReady=!bDisconnected&&PadGate.IsArmed();
    return {std::max(KeyThrottle,PadReady?Overdrive::ControllerFilter::Trigger(PadThrottle):0.0),
        std::max(KeyBrake,PadReady?Overdrive::ControllerFilter::Trigger(PadBrake):0.0),
        Overdrive::Clamp(KeySteer+(PadReady?Overdrive::ControllerFilter::Steering(Stick,DeadZone,Sensitivity):0.0),-1,1),bDrift};
}

bool AOverdriveController::CanSafelyResume() const
{
    if(!bUsingController)return true;
    if(bDisconnected)return false;
    return Overdrive::ControllerFilter::IsNeutral(RawStick,RawThrottle,RawBrake);
}
void AOverdriveController::HandleConnection(EInputDeviceConnectionState State,FPlatformUserId User,FInputDeviceId Device)
{
    if(GetLocalPlayer()&&User!=GetLocalPlayer()->GetPlatformUserId())return;
    if(!ActiveGamepad.IsValid()||Device!=ActiveGamepad)return;
    if(State==EInputDeviceConnectionState::Disconnected)
    {
        bDisconnected=true;ResetControls();StopRumble();PendingAxisMask=0;
        if(PlayerInput)PlayerInput->FlushPressedKeys();
        if(auto* Game=Mode(this))Game->PauseRace(TEXT("Controller disconnected. Reconnect, or press Enter to continue with the keyboard."));
    }
    else if(State==EInputDeviceConnectionState::Connected&&bDisconnected)
    {
        bDisconnected=false;ResetControls();
        if(auto* Game=Mode(this))Game->ShowNotice(TEXT("Controller reconnected. Release the triggers, then choose Back to the race."),5);
    }
}
void AOverdriveController::HandleDeactivation()
{
    ResetControls();StopRumble();PendingAxisMask=0;
    if(PlayerInput)PlayerInput->FlushPressedKeys();
    if(auto* Game=Mode(this))Game->PauseRace(TEXT("Paused while the game was in the background."));
}
void AOverdriveController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if(PendingAxisMask)
    {
        const double X=GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
        const double Throttle=GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis);
        const double Brake=GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
        const double Y=GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
        const bool Meaningful=((PendingAxisMask&1)&&FMath::Abs(X)>.25)||((PendingAxisMask&2)&&Throttle>.15)
            ||((PendingAxisMask&4)&&Brake>.15)||((PendingAxisMask&8)&&FMath::Abs(Y)>.25);
        if(Meaningful&&(!ActiveGamepad.IsValid()||bDisconnected))
        {
            ActiveGamepad=PendingGamepad;RawStick=X;RawThrottle=Throttle;RawBrake=Brake;
            bDisconnected=false;PadGate.Suspend();
        }
        if(PendingGamepad==ActiveGamepad&&!bDisconnected)
        {
            if(PendingAxisMask&1)RawStick=X;
            if(PendingAxisMask&2)RawThrottle=Throttle;
            if(PendingAxisMask&4)RawBrake=Brake;
            if(Meaningful)bUsingController=true;
        }
        PendingAxisMask=0;
    }
    PadGate.Step(RawStick,RawThrottle,RawBrake,DeltaSeconds,ActiveGamepad.IsValid()&&!bDisconnected);
    if(auto* Game=Mode(this))if(Game->IsMenuVisible())
    {
        if(const int32 Step=VerticalRepeat.Step(MenuY,DeltaSeconds))Game->MoveSelection(Step);
        if(const int32 Step=HorizontalRepeat.Step(MenuX,DeltaSeconds))Game->AdjustSelection(Step);
    }
}

void AOverdriveController::SetStick(const FInputActionValue& V){Stick=V.Get<float>();}
void AOverdriveController::SetPadThrottle(const FInputActionValue& V){PadThrottle=V.Get<float>();}
void AOverdriveController::SetPadBrake(const FInputActionValue& V){PadBrake=V.Get<float>();}
void AOverdriveController::SetKeySteer(const FInputActionValue& V){KeySteer=V.Get<float>();}
void AOverdriveController::SetKeyThrottle(const FInputActionValue& V){KeyThrottle=V.Get<float>();}
void AOverdriveController::SetKeyBrake(const FInputActionValue& V){KeyBrake=V.Get<float>();}
void AOverdriveController::SetDrift(const FInputActionValue& V){bDrift=V.Get<bool>();}
void AOverdriveController::SetLookBack(const FInputActionValue& V){bLookBack=V.Get<bool>();}
void AOverdriveController::SetMenuX(const FInputActionValue& V){MenuX=V.Get<float>();}
void AOverdriveController::SetMenuY(const FInputActionValue& V){MenuY=V.Get<float>();}
void AOverdriveController::Boost(){if(auto* Game=Mode(this))Game->Simulation.UseBoost();}
void AOverdriveController::UseItem(){if(auto* Game=Mode(this))Game->Simulation.UseItem();}
void AOverdriveController::Recover(){if(auto* Game=Mode(this))Game->RecoverKart();}
void AOverdriveController::PausePressed()
{
    if(auto* Game=Mode(this))
    {
        if(Game->Simulation.State==Overdrive::Phase::Racing||Game->Simulation.State==Overdrive::Phase::Countdown)Game->PauseRace();else Game->Back();
    }
}
void AOverdriveController::Confirm(){if(auto* Game=Mode(this))if(Game->IsMenuVisible())Game->ActivateSelection();}
void AOverdriveController::BackPressed(){if(auto* Game=Mode(this))Game->Back();}

void AOverdriveController::StopRumble()
{
    if(RumbleHandle)RumbleHandle=PlayDynamicForceFeedback(0,0,false,false,false,false,EDynamicForceFeedbackAction::Stop,RumbleHandle);
}
void AOverdriveController::Feedback(Overdrive::EventType Type)
{
    const auto* Game=Mode(this);if(!bUsingController||bDisconnected||!Game||!Game->GetSettings()||!Game->GetSettings()->bVibration)return;
    float Strength=0,Duration=.14f;
    switch(Type)
    {
        case Overdrive::EventType::Impact:Strength=.75;Duration=.18;break;
        case Overdrive::EventType::Boost:case Overdrive::EventType::Drift:Strength=.4;Duration=.3;break;
        case Overdrive::EventType::DriftTier:Strength=.18;Duration=.07;break;
        case Overdrive::EventType::CleanExit:case Overdrive::EventType::Slipstream:case Overdrive::EventType::Launch:Strength=.36;Duration=.22;break;
        case Overdrive::EventType::Pickup:Strength=.22;break;
        case Overdrive::EventType::Finish:Strength=.45;Duration=.5;break;
        default:return;
    }
    StopRumble();RumbleHandle=PlayDynamicForceFeedback(Strength,Duration,true,false,false,true,EDynamicForceFeedbackAction::Start,0);
}
FString AOverdriveController::ConfirmLabel() const
{
    if(!bUsingController)return TEXT("ENTER");
    const auto* Game=Mode(this);return Game&&Game->GetSettings()&&Game->GetSettings()->ControllerLabels==1?TEXT("CROSS"):TEXT("A");
}
FString AOverdriveController::BackLabel() const
{
    if(!bUsingController)return TEXT("ESC");
    const auto* Game=Mode(this);return Game&&Game->GetSettings()&&Game->GetSettings()->ControllerLabels==1?TEXT("CIRCLE"):TEXT("B");
}
FString AOverdriveController::ControlsHint() const
{
    if(!bUsingController)return TEXT("WASD  Drive / Brake    SHIFT  Drift    SPACE  Turbo    E  Item    R  Recover    ESC  Pause");
    const auto* Game=Mode(this);const bool PS=Game&&Game->GetSettings()&&Game->GetSettings()->ControllerLabels==1;
    return PS?TEXT("L STICK  Steer    R2 / L2  Drive / Brake    R1  Drift    CROSS  Turbo    CIRCLE  Recover")
        :TEXT("L STICK  Steer    RT / LT  Drive / Brake    RB  Drift    A  Turbo    B  Recover");
}
