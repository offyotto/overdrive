#include "OverdriveKart.h"
#include "OverdriveGeometry.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AOverdriveKart::AOverdriveKart()
{
    PrimaryActorTick.bCanEverTick = false;
    AutoPossessAI = EAutoPossessAI::Disabled;
    Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Comet")); SetRootComponent(Model);
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision); Model->SetMobility(EComponentMobility::Movable);
    for(int32 I=0;I<4;++I)
    {
        auto* Wheel=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"),I));
        Wheel->SetupAttachment(Model);Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);Wheel->SetMobility(EComponentMobility::Movable);
        Wheel->SetRelativeLocation(FVector(I<2?123:-123,I%2?132:-132,65));Wheels.Add(Wheel);
    }
    SteeringWheel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SteeringWheel"));SteeringWheel->SetupAttachment(Model);
    SteeringWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);SteeringWheel->SetMobility(EComponentMobility::Movable);
    SteeringWheel->SetRelativeLocation(FVector(56,0,143));
    Exhaust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurboExhaust")); Exhaust->SetupAttachment(Model);
    Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnergyShield")); Shield->SetupAttachment(Model);
    Sparks = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DriftSparks")); Sparks->SetupAttachment(Model);
    for (UStaticMeshComponent* Effect : {Exhaust.Get(), Shield.Get(), Sparks.Get()})
    {
        Effect->SetCollisionEnabled(ECollisionEnabled::NoCollision); Effect->SetCastShadow(false); Effect->SetVisibility(false);
    }
    BoostLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoostLight")); BoostLight->SetupAttachment(Model);
    BoostLight->SetRelativeLocation(FVector(-220, 0, 90)); BoostLight->SetLightColor(FLinearColor(.15f,.8f,1));
    BoostLight->SetIntensity(2000); BoostLight->SetAttenuationRadius(500); BoostLight->SetCastShadows(false); BoostLight->SetVisibility(false);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera")); Camera->SetupAttachment(Model);
    Camera->SetAbsolute(true, true, true); Camera->SetFieldOfView(62); Camera->bConstrainAspectRatio = false;
}

void AOverdriveKart::BeginPlay()
{
    Super::BeginPlay();
    const auto Load = [](const TCHAR* Name)
    {
        return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Overdrive/Meshes/%s.%s"),Name,Name));
    };
    const auto LoadKart=[](const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/Meshes/%s.%s"),ODGeometry::KartAssetRoot,Name,Name));};
    UStaticMesh* Body=LoadKart(TEXT("SM_Kart"));
    UStaticMesh* WheelMesh=LoadKart(TEXT("SM_KartWheel"));UStaticMesh* SteeringMesh=LoadKart(TEXT("SM_KartSteering"));
    const bool bUpgraded=Body&&WheelMesh&&SteeringMesh;
    Model->SetStaticMesh(bUpgraded?Body:Load(TEXT("SM_Kart")));
    for(auto& Wheel:Wheels){Wheel->SetStaticMesh(WheelMesh);Wheel->SetVisibility(bUpgraded);}
    SteeringWheel->SetStaticMesh(SteeringMesh);SteeringWheel->SetVisibility(bUpgraded);
    Exhaust->SetStaticMesh(Load(TEXT("SM_Exhaust")));Shield->SetStaticMesh(Load(TEXT("SM_Shield")));Sparks->SetStaticMesh(Load(TEXT("SM_Sparks")));
    if (Model->GetStaticMesh()) Paint = Model->CreateDynamicMaterialInstance(static_cast<int32>(ODGeometry::ESurface::Paint));
    if (Paint) Paint->SetVectorParameterValue(TEXT("Tint"), RequestedPaint);
    if(Sparks->GetStaticMesh())DriftGlow=Sparks->CreateDynamicMaterialInstance(static_cast<int32>(ODGeometry::ESurface::Glow));
}

void AOverdriveKart::SetPaint(const FLinearColor& Color)
{
    RequestedPaint = Color;
    if (Paint) Paint->SetVectorParameterValue(TEXT("Tint"), Color);
}

void AOverdriveKart::UpdatePose(const Overdrive::Track& Track, const Overdrive::Racer& State, float Time, bool bShowcase)
{
    const auto Frame = Track.At(Overdrive::StartOffset + State.Distance);
    FRotator Rotation = ODGeometry::DirectionToUnreal(Frame.Tangent).Rotation();
    Rotation.Yaw += FMath::RadiansToDegrees(State.Yaw);Rotation.Roll=bShowcase?0:FMath::Clamp(-State.YawRate*State.Speed*.075,-6.0,6.0);
    SetActorLocationAndRotation(ODGeometry::ToUnreal(Track.Position(Overdrive::StartOffset+State.Distance,State.Lateral,.08+(bShowcase?0:State.HopHeight))),Rotation);
    const double Steer=bShowcase?-.16:State.Steering*.46;
    const double Roll=bShowcase?0:std::remainder(State.Distance/.65,Overdrive::Pi*2);
    for(int32 I=0;I<Wheels.Num();++I)
    {
        double Angle=0;
        if(I<2&&std::abs(Steer)>.0001)
        {
            const double Radius=2.46/std::tan(std::abs(Steer)),Side=I%2?1.0:-1.0;
            Angle=std::copysign(std::atan2(2.46,Radius-Side*std::copysign(1.32,Steer)),Steer);
        }
        Wheels[I]->SetRelativeRotation(FQuat(FVector::UpVector,Angle)*FQuat(FVector::RightVector,Roll));
    }
    SteeringWheel->SetRelativeRotation(FQuat(FVector(.75,0,.66).GetSafeNormal(),Steer*1.8));
    Exhaust->SetVisibility(State.Boost > 0); BoostLight->SetVisibility(State.Boost > 0);
    Exhaust->SetRelativeScale3D(FVector(1 + FMath::Sin(Time * 32) * .09, 1, 1));
    Shield->SetVisibility(State.Shield > 0); Shield->SetRelativeRotation(FRotator(0, Time * 35, 0));
    Sparks->SetVisibility(State.Drifting&&State.HopHeight<.10);Sparks->SetRelativeLocation(FVector(0,0,FMath::Sin(Time*38)*4));
    if(DriftGlow&&VisualDriftTier!=State.DriftTier)
    {
        VisualDriftTier=State.DriftTier;
        const FLinearColor Tint=State.DriftTier>=3?FLinearColor(.75f,.10f,1.f):State.DriftTier>=2?FLinearColor(1.f,.23f,.03f):FLinearColor(.12f,.65f,1.f);
        DriftGlow->SetVectorParameterValue(TEXT("Tint"),Tint);DriftGlow->SetScalarParameterValue(TEXT("Glow"),State.DriftTier>0?2.4f:1.f);
    }
}
