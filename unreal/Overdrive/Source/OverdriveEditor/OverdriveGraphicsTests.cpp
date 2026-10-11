#include "Misc/AutomationTest.h"
#include "OverdriveGeometry.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "AssetCompilingManager.h"
#include "OverdriveKart.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverdriveMeshFrameTest,"Overdrive.Graphics.WindingAndUVs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOverdriveMeshFrameTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TMap<FName,ODGeometry::FMesh> Meshes;ODGeometry::BuildAssets(Meshes);
    for(const auto& Entry:Meshes)
    {
        int32 BadWinding=0,BadUV=0,Triangles=0;
        for(const auto& Section:Entry.Value.Sections)
        {
            TestEqual(TEXT("Triangle vertex count"),Section.Vertices.Num()%3,0);
            for(int32 I=0;I+2<Section.Vertices.Num();I+=3)
            {
                const auto& A=Section.Vertices[I];const auto& B=Section.Vertices[I+1];const auto& C=Section.Vertices[I+2];
                const FVector3f Face=FVector3f::CrossProduct(C.Position-A.Position,B.Position-A.Position).GetSafeNormal();
                const FVector3f Normal=(A.Normal+B.Normal+C.Normal).GetSafeNormal();
                if(FVector3f::DotProduct(Face,Normal)<.01f)++BadWinding;
                const FVector2f U=B.UV-A.UV,V=C.UV-A.UV;
                if(!FMath::IsFinite(U.X*V.Y-U.Y*V.X)||FMath::Abs(U.X*V.Y-U.Y*V.X)<1.e-10f)++BadUV;
                ++Triangles;
            }
        }
        TestTrue(*FString::Printf(TEXT("%s contains geometry"),*Entry.Key.ToString()),Triangles>0);
        TestEqual(*FString::Printf(TEXT("%s outward winding"),*Entry.Key.ToString()),BadWinding,0);
        TestEqual(*FString::Printf(TEXT("%s nondegenerate UVs"),*Entry.Key.ToString()),BadUV,0);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverdriveMaterialSlotTest,"Overdrive.Graphics.SavedMaterialSlots",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOverdriveMaterialSlotTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UStaticMesh* Crystal=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Overdrive/Meshes/SM_Crystal.SM_Crystal"));
    if(!TestNotNull(TEXT("Crystal mesh"),Crystal))return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(!TestNotNull(TEXT("Crystal render data"),Crystal->GetRenderData()))return false;
    for(const auto& Section:Crystal->GetRenderData()->LODResources[0].Sections)
    {
        if(!Section.NumTriangles)continue;
        const int32 Index=Section.MaterialIndex;
        if(!TestTrue(TEXT("Material index exists"),Crystal->GetStaticMaterials().IsValidIndex(Index)))continue;
        TestEqual(TEXT("Crystal triangles use emissive material"),Crystal->GetStaticMaterials()[Index].MaterialSlotName,FName(TEXT("MI_Glow")));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverdriveKartAssetTest,"Overdrive.Graphics.KartQualityAndLODs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOverdriveKartAssetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TMap<FName,ODGeometry::FMesh> Generated;ODGeometry::BuildKartAssets(Generated);
    TestEqual(TEXT("Three modular kart meshes"),Generated.Num(),3);
    for(const auto& Entry:Generated)
    {
        int32 Triangles=0,InvalidNormals=0;FBox3f Bounds(ForceInit);
        for(const auto& Section:Entry.Value.Sections)for(const auto& V:Section.Vertices)
        {
            Bounds+=V.Position;
            if(V.Normal.ContainsNaN()||FMath::Abs(V.Normal.SizeSquared()-1.f)>.02f)++InvalidNormals;
            ++Triangles;
        }
        Triangles/=3;
        TestEqual(TEXT("Finite unit vertex normals"),InvalidNormals,0);
        TestTrue(TEXT("Bounded geometry budget"),Triangles>200&&Triangles<90000);
        TestTrue(TEXT("Kart dimensions stay within existing handling envelope"),Bounds.GetSize().GetMax()<500.f);
        UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/Meshes/%s.%s"),ODGeometry::KartAssetRoot,*Entry.Key.ToString(),*Entry.Key.ToString()));
        if(!TestNotNull(*Entry.Key.ToString(),Mesh))continue;
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(!TestNotNull(TEXT("Built kart render data"),Mesh->GetRenderData()))continue;
        TestEqual(TEXT("Stable six material slots"),Mesh->GetStaticMaterials().Num(),6);
        TestEqual(TEXT("Three distance detail levels"),Mesh->GetNumLODs(),3);
        int32 Previous=MAX_int32;
        for(const auto& LOD:Mesh->GetRenderData()->LODResources)
        {
            TestTrue(TEXT("LODs reduce rather than increase triangles"),LOD.GetNumTriangles()>0&&LOD.GetNumTriangles()<=Previous);
            Previous=LOD.GetNumTriangles();
            int32 InvalidTangents=0;
            for(uint32 I=0;I<LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();++I)
            {
                const FVector3f T(LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(I));
                if(T.ContainsNaN()||T.SizeSquared()<.5f)++InvalidTangents;
            }
            TestEqual(TEXT("Every built LOD has usable tangent frames"),InvalidTangents,0);
            for(const auto& Section:LOD.Sections)if(Section.NumTriangles)
                TestTrue(TEXT("LOD retains valid material binding"),Mesh->GetStaticMaterials().IsValidIndex(Section.MaterialIndex));
        }
        TestEqual(TEXT("Paint remains slot zero"),Mesh->GetStaticMaterials()[0].MaterialSlotName,FName(TEXT("MI_Paint")));
        AddInfo(FString::Printf(TEXT("%s: %d source triangles, 3 LODs"),*Entry.Key.ToString(),Triangles));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverdriveKartMotionTest,"Overdrive.Graphics.KartWheelMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOverdriveKartMotionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,FName(TEXT("KartMotionTest")));
    if(!TestNotNull(TEXT("Temporary test world"),World))return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AOverdriveKart* Kart=World->SpawnActor<AOverdriveKart>();
    if(!TestNotNull(TEXT("Native kart actor"),Kart))return false;
    TInlineComponentArray<UStaticMeshComponent*> Components;Kart->GetComponents(Components);
    UStaticMeshComponent* Front=nullptr;UStaticMeshComponent* Rear=nullptr;
    for(auto* Part:Components){if(Part->GetFName()==TEXT("Wheel0"))Front=Part;if(Part->GetFName()==TEXT("Wheel2"))Rear=Part;}
    if(!TestNotNull(TEXT("Front wheel"),Front)||!TestNotNull(TEXT("Rear wheel"),Rear))return false;
    Overdrive::Track Track;Overdrive::Racer State;Kart->UpdatePose(Track,State,0);
    const FQuat Before=Rear->GetRelativeRotation().Quaternion();
    State.Distance=.65*Overdrive::Pi;State.Yaw=.2;State.Steering=.5;Kart->UpdatePose(Track,State,1);
    TestFalse(TEXT("Tires rotate with traveled distance"),Before.Equals(Rear->GetRelativeRotation().Quaternion(),.01));
    TestTrue(TEXT("Front axle turns with steering"),!Front->GetRelativeRotation().Quaternion().RotateVector(FVector::RightVector).Equals(FVector::RightVector,.05));
    TestTrue(TEXT("Rear axle does not steer"),Rear->GetRelativeRotation().Quaternion().RotateVector(FVector::RightVector).Equals(FVector::RightVector,.001));
    Kart->UpdatePose(Track,State,2,true);
    TestTrue(TEXT("Showcase tires stay stationary"),Rear->GetRelativeRotation().IsNearlyZero(.001));
    TestEqual(TEXT("Visuals do not modify race distance"),State.Distance,.65*Overdrive::Pi);
    return true;
}
#endif
