#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/OverdriveSimulation.h"
#include "Core/OverdriveCamera.h"
#include "OverdriveGameMode.generated.h"

class AOverdriveKart;
class AOverdriveWorld;
class UOverdriveSaveGame;
class USoundWaveProcedural;
class UAudioComponent;

enum class EOverdrivePage : uint8 { Home, Garage, Settings, Controls, Records, Pause, Results };
enum class EOverdriveCommand : uint8
{
    Start, EngineClass, Garage, Weather, Settings, Controls, Records, Quit,
    Paint, Quality, DeadZone, Sensitivity, AutoAccelerate, Vibration, Volume,
    Labels, CalmCamera, Back, Resume, Restart, Home
};
struct FOverdriveMenuRow
{
    EOverdriveCommand Command;
    FString Label;
    FString Value;
};

UCLASS()
class OVERDRIVE_API AOverdriveGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AOverdriveGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void StartRace();
    void PauseRace(const FString& Reason = FString());
    void ResumeRace();
    void RecoverKart();
    void ReturnHome();
    void SetPage(EOverdrivePage Page);
    void MoveSelection(int32 Direction);
    void AdjustSelection(int32 Direction);
    void ActivateSelection();
    void Back();
    void SelectRow(int32 Index);
    void ShowNotice(const FString& Text, float Duration = 2.0f);
    TArray<FOverdriveMenuRow> GetMenuRows() const;
    EOverdrivePage GetPage() const { return CurrentPage; }
    int32 GetSelectedRow() const { return SelectedRow; }
    bool IsMenuVisible() const;
    UOverdriveSaveGame* GetSettings() const { return Settings; }
    const FString& GetNotice() const { return Notice; }
    const FString& GetBootError() const { return BootError; }
    float GetSceneTime() const { return SceneTime; }
    float GetGoTime() const { return GoRemaining; }
    Overdrive::Race Simulation;
private:
    void ApplySettings();
    void SaveProgress();
    void UpdateVisuals(float Dt);
    void HandleEvent(const Overdrive::RaceEvent& Event);
    void FillAudio();
    UPROPERTY() TObjectPtr<UOverdriveSaveGame> Settings;
    UPROPERTY() TObjectPtr<AOverdriveWorld> Coast;
    UPROPERTY() TArray<TObjectPtr<AOverdriveKart>> Karts;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> EngineSound;
    UPROPERTY() TObjectPtr<UAudioComponent> EngineAudio;
    EOverdrivePage CurrentPage = EOverdrivePage::Home;
    int32 SelectedRow = 0;
    double Accumulator = 0, SoundTime = 0, BeepRemaining = 0, BeepFrequency = 440;
    float SceneTime = 0, NoticeRemaining = 0, GoRemaining = 0, ResultsDelay = -1;
    FString Notice, BootError;
    bool bCameraReady = false, bRecorded = false, bVerificationCapture = false;
    FVector CameraPosition = FVector::ZeroVector, CameraTarget = FVector::ZeroVector;
    Overdrive::FollowCamera ChaseRig;
    std::array<Overdrive::Racer, Overdrive::RacerCount> PreviousRacers{}, RenderRacers{};
#if !UE_BUILD_SHIPPING
    bool bDriveVerification = false;
    double VerificationSeconds = 0, VerificationCameraSpeed = 0, VerificationSeparation = 0;
    double VerificationNextBoost = 6, VerificationChaseSpeed = 0;
    int32 VerificationFrames = 0, VerificationTier = 0;
    FVector VerificationLastCamera = FVector::ZeroVector;
#endif
};
