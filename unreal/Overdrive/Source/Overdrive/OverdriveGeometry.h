#pragma once

#include "CoreMinimal.h"
#include "Core/OverdriveSimulation.h"

namespace ODGeometry
{
enum class ESurface : uint8 { Paint, Surface, Metal, Rubber, Visor, Glow, Count };
struct FVertex
{
    FVector3f Position;
    FVector3f Normal;
    FLinearColor Color;
    FVector2f UV;
};
struct FSection { TArray<FVertex> Vertices; };
struct FMesh { TArray<FSection> Sections; FMesh() { Sections.SetNum(static_cast<int32>(ESurface::Count)); } };

inline FVector ToUnreal(const Overdrive::Vec3& P) { return FVector(P.Z, P.X, P.Y) * 100.0; }
inline FVector DirectionToUnreal(const Overdrive::Vec3& P) { return FVector(P.Z, P.X, P.Y); }
inline FLinearColor Color(uint32 RGB) { return FLinearColor(FColor((RGB >> 16) & 255, (RGB >> 8) & 255, RGB & 255)); }
OVERDRIVE_API void PrepareMeshForUnreal(FMesh& Mesh);
OVERDRIVE_API void BuildAssets(TMap<FName, FMesh>& Output);
OVERDRIVE_API void BuildKartAssets(TMap<FName, FMesh>& Output);
inline constexpr const TCHAR* KartAssetRoot = TEXT("/Game/Overdrive/Karts/V2");
inline bool IsKartAsset(FName Name) { return Name == TEXT("SM_Kart") || Name == TEXT("SM_KartWheel") || Name == TEXT("SM_KartSteering"); }
OVERDRIVE_API const TCHAR* MaterialName(ESurface Surface);
}
