#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/OverdriveSimulation.h"
#include "OverdriveWorld.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

UCLASS()
class OVERDRIVE_API AOverdriveWorld : public AActor
{
    GENERATED_BODY()
public:
    AOverdriveWorld();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    void SetNight(bool bNight);
    void AnimatePickups(const Overdrive::Track& Track, float Time);
    bool IsReady() const;
private:
    void LoadAssets();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Road;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Islands;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Scenery;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Clouds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ocean;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Crystals;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Coins;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> SkyLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent> Atmosphere;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Fog;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> PostProcess;
};
