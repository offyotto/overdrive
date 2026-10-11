#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Core/OverdriveSimulation.h"
#include "OverdriveController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
struct FInputKeyEventArgs;

UCLASS()
class OVERDRIVE_API AOverdriveController : public APlayerController
{
    GENERATED_BODY()
public:
    AOverdriveController();
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void PlayerTick(float DeltaSeconds) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    void RefreshInputContext();
    Overdrive::DriverInput GetDrivingInput() const;
    bool CanSafelyResume() const;
    bool IsLookingBack() const { return bLookBack; }
    bool IsUsingController() const { return bUsingController; }
    bool NeedsControllerNeutral() const { return bUsingController && (bDisconnected || !PadGate.IsArmed()); }
    void Feedback(Overdrive::EventType Type);
    FString ConfirmLabel() const;
    FString BackLabel() const;
    FString ControlsHint() const;
private:
    void ResetControls();
    void StopRumble();
    void HandleConnection(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);
    void HandleDeactivation();
    void SetStick(const FInputActionValue& Value);
    void SetPadThrottle(const FInputActionValue& Value);
    void SetPadBrake(const FInputActionValue& Value);
    void SetKeySteer(const FInputActionValue& Value);
    void SetKeyThrottle(const FInputActionValue& Value);
    void SetKeyBrake(const FInputActionValue& Value);
    void SetDrift(const FInputActionValue& Value);
    void SetLookBack(const FInputActionValue& Value);
    void SetMenuX(const FInputActionValue& Value);
    void SetMenuY(const FInputActionValue& Value);
    void Boost();
    void UseItem();
    void Recover();
    void PausePressed();
    void Confirm();
    void BackPressed();
    UPROPERTY() TObjectPtr<UInputMappingContext> DriveContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> MenuContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> CommonContext;
    UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
    Overdrive::NavigationRepeat HorizontalRepeat, VerticalRepeat;
    Overdrive::ControllerNeutralGate PadGate;
    double Stick=0,PadThrottle=0,PadBrake=0,KeySteer=0,KeyThrottle=0,KeyBrake=0,MenuX=0,MenuY=0;
    double RawStick=0,RawThrottle=0,RawBrake=0;
    bool bDrift=false,bLookBack=false,bUsingController=false,bDisconnected=false;
    FInputDeviceId ActiveGamepad, PendingGamepad;
    uint8 PendingAxisMask=0;
    FDelegateHandle ConnectionHandle,DeactivationHandle;
    FDynamicForceFeedbackHandle RumbleHandle=0;
};
