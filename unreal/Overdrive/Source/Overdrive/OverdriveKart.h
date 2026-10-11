#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/OverdriveSimulation.h"
#include "OverdriveKart.generated.h"

class UStaticMeshComponent;
class UCameraComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class OVERDRIVE_API AOverdriveKart : public APawn
{
    GENERATED_BODY()
public:
    AOverdriveKart();
    virtual void BeginPlay() override;
    void SetPaint(const FLinearColor& Color);
    void UpdatePose(const Overdrive::Track& Track, const Overdrive::Racer& State, float Time, bool bShowcase = false);
    UCameraComponent* GetRaceCamera() const { return Camera; }
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Model;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SteeringWheel;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Exhaust;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Shield;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Sparks;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> BoostLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Paint;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DriftGlow;
    int32 VisualDriftTier = -1;
    FLinearColor RequestedPaint = FLinearColor::White;
};
