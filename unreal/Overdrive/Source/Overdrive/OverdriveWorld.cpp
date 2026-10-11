#include "OverdriveWorld.h"
#include "OverdriveGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AOverdriveWorld::AOverdriveWorld()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Coast")); SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    Road = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Circuit"));
    Islands = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloatingIslands"));
    Scenery = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LighthouseAndFoliage"));
    Clouds = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Clouds"));
    Ocean = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ocean"));
    for (UStaticMeshComponent* Mesh : {Road.Get(),Islands.Get(),Scenery.Get(),Clouds.Get(),Ocean.Get()})
    {
        Mesh->SetupAttachment(SceneRoot); Mesh->SetMobility(EComponentMobility::Static);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    Ocean->SetCastShadow(false); Clouds->SetCastShadow(false);
    Crystals = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ItemCrystals")); Crystals->SetupAttachment(SceneRoot);
    Coins = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TurboCoins")); Coins->SetupAttachment(SceneRoot);
    for (UInstancedStaticMeshComponent* Mesh : {Crystals.Get(),Coins.Get()})
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetMobility(EComponentMobility::Movable);
    }
    Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("GoldenHour")); Sun->SetupAttachment(SceneRoot);
    Sun->SetMobility(EComponentMobility::Movable); Sun->bAtmosphereSunLight = true; Sun->SetCastShadows(true);
    Sun->SetRelativeRotation(FRotator(-31,-42,0)); Sun->SetIntensity(7.5); Sun->SetLightColor(FLinearColor(1,.81f,.61f));
    Sun->LightSourceAngle = 1.4f;
    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("CoastalSky")); SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetMobility(EComponentMobility::Movable); SkyLight->bRealTimeCapture = true; SkyLight->SetIntensity(1.1);
    Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere")); Atmosphere->SetupAttachment(SceneRoot);
    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("SeaHaze")); Fog->SetupAttachment(SceneRoot);
    Fog->SetRelativeLocation(FVector(0,0,-4500)); Fog->SetFogDensity(.007); Fog->SetFogHeightFalloff(.15);
    Fog->SetFogInscatteringColor(FLinearColor(.32f,.58f,.67f));
    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("CoastalGrade")); PostProcess->SetupAttachment(SceneRoot);
    PostProcess->bUnbound = true;
    auto& S = PostProcess->Settings;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = .16f;
    S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = 1;
    S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 1;
    S.bOverride_AutoExposureBias = true; S.AutoExposureBias = .2f;
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .22f;
    S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0;
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.08,1.08,1.08,1);
}

void AOverdriveWorld::LoadAssets()
{
    const auto Load = [](const TCHAR* Name) { return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Overdrive/Meshes/%s.%s"),Name,Name)); };
    Road->SetStaticMesh(Load(TEXT("SM_Road"))); Islands->SetStaticMesh(Load(TEXT("SM_Islands")));
    Scenery->SetStaticMesh(Load(TEXT("SM_Scenery"))); Clouds->SetStaticMesh(Load(TEXT("SM_Clouds")));
    Ocean->SetStaticMesh(Load(TEXT("SM_Ocean"))); Crystals->SetStaticMesh(Load(TEXT("SM_Crystal"))); Coins->SetStaticMesh(Load(TEXT("SM_Coin")));
    if (UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Overdrive/Materials/M_Ocean.M_Ocean")))
        Ocean->SetMaterial(static_cast<int32>(ODGeometry::ESurface::Surface),Water);
    Crystals->ClearInstances(); Coins->ClearInstances();
    for (int32 I=0;I<12;++I) Crystals->AddInstance(FTransform::Identity);
    for (int32 I=0;I<16;++I) Coins->AddInstance(FTransform::Identity);
    AnimatePickups(Overdrive::Track(),0);
}

void AOverdriveWorld::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform); LoadAssets();
}
void AOverdriveWorld::BeginPlay() { Super::BeginPlay(); LoadAssets(); }
bool AOverdriveWorld::IsReady() const { return Road->GetStaticMesh() && Islands->GetStaticMesh() && Scenery->GetStaticMesh(); }

void AOverdriveWorld::SetNight(bool bNight)
{
    Sun->SetIntensity(bNight ? .65f : 7.5f);
    Sun->SetLightColor(bNight ? FLinearColor(.35f,.49f,1) : FLinearColor(1,.81f,.61f));
    Sun->SetRelativeRotation(bNight ? FRotator(-18,138,0) : FRotator(-31,-42,0));
    SkyLight->SetIntensity(bNight ? .32f : 1.1f);
    Fog->SetFogInscatteringColor(bNight ? FLinearColor(.035f,.055f,.15f) : FLinearColor(.32f,.58f,.67f));
}

void AOverdriveWorld::AnimatePickups(const Overdrive::Track& Track, float Time)
{
    int32 CrystalIndex=0,CoinIndex=0;
    for (double Zone : Track.PickupZones)
    {
        for (double Lane : {-4.0,0.0,4.0})
        {
            const FTransform Pose(FRotator(0,Time*49,0),ODGeometry::ToUnreal(Track.Position(Zone,Lane,2.1+FMath::Sin(Time*2+Lane)*.18)));
            Crystals->UpdateInstanceTransform(CrystalIndex++,Pose,false,false,true);
        }
        for (int32 J=0;J<4;++J)
        {
            const FTransform Pose(FRotator(0,Time*98+J*17,0),ODGeometry::ToUnreal(Track.Position(Zone+14+J*4,0,1.9+FMath::Sin(Time*2+J)*.12)));
            Coins->UpdateInstanceTransform(CoinIndex++,Pose,false,false,true);
        }
    }
    Crystals->MarkRenderStateDirty(); Coins->MarkRenderStateDirty();
}
