#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OverdriveSaveGame.generated.h"

UCLASS()
class OVERDRIVE_API UOverdriveSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 SaveVersion = 2;
    UPROPERTY() int32 EngineClass = 100;
    UPROPERTY() int32 PaintIndex = 0;
    UPROPERTY() int32 Quality = 2;
    UPROPERTY() int32 RacesCompleted = 0;
    UPROPERTY() TArray<double> BestLaps = {-1, -1, -1};
    UPROPERTY() bool bNight = false;
    UPROPERTY() bool bAutoAccelerate = false;
    UPROPERTY() bool bCalmCamera = false;
    UPROPERTY() bool bVibration = true;
    UPROPERTY() float SteeringDeadZone = .16f;
    UPROPERTY() float SteeringSensitivity = 1.f;
    UPROPERTY() float Volume = .45f;
    UPROPERTY() int32 ControllerLabels = 0;

    void Sanitize()
    {
        EngineClass = EngineClass == 50 || EngineClass == 150 ? EngineClass : 100;
        PaintIndex = FMath::Clamp(PaintIndex, 0, 5); Quality = FMath::Clamp(Quality, 1, 3);
        RacesCompleted = FMath::Clamp(RacesCompleted, 0, 999999);
        if (BestLaps.Num() != 3) BestLaps = {-1, -1, -1};
        for (double& Lap : BestLaps) if (!FMath::IsFinite(Lap) || Lap <= 0) Lap = -1;
        SteeringDeadZone = FMath::IsFinite(SteeringDeadZone) ? FMath::Clamp(SteeringDeadZone, .05f, .4f) : .16f;
        SteeringSensitivity = FMath::IsFinite(SteeringSensitivity) ? FMath::Clamp(SteeringSensitivity, .5f, 1.5f) : 1.f;
        Volume = FMath::IsFinite(Volume) ? FMath::Clamp(Volume, 0.f, 1.f) : .45f;
        ControllerLabels = FMath::Clamp(ControllerLabels, 0, 1);
    }
};
