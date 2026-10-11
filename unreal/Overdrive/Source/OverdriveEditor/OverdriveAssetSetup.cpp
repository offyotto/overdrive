#include "OverdriveAssetSetup.h"
#include "OverdriveGeometry.h"
#include "OverdriveWorld.h"
#include "OverdriveGameMode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/SavePackage.h"

namespace
{
bool SaveAsset(UObject* Asset, bool bMap = false)
{
    if (!Asset) return false;
    UPackage* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),bMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    Package->MarkPackageDirty();
    const bool bSaved = UPackage::SavePackage(Package,Asset,*Filename,Args);
    if (!bSaved) UE_LOG(LogTemp,Error,TEXT("Could not save OVERDRIVE asset: %s"),*Filename);
    return bSaved;
}

template<class T> T* Existing(const FString& PackageName)
{
    if (!FPackageName::DoesPackageExist(PackageName)) return nullptr;
    return LoadObject<T>(nullptr,*(PackageName+TEXT(".")+FPackageName::GetLongPackageAssetName(PackageName)));
}

template<class T> T* Expression(UMaterial* Material)
{
    T* Node=NewObject<T>(Material); Material->GetExpressionCollection().AddExpression(Node); return Node;
}

UMaterial* BaseMaterial()
{
    const FString Path=TEXT("/Game/Overdrive/Materials/M_Overdrive");
    if(UMaterial* M=Existing<UMaterial>(Path))return M;
    UMaterial* M=NewObject<UMaterial>(CreatePackage(*Path),TEXT("M_Overdrive"),RF_Public|RF_Standalone);
    M->TwoSided=true;
    auto* Vertex=Expression<UMaterialExpressionVertexColor>(M);
    auto* Tint=Expression<UMaterialExpressionVectorParameter>(M);Tint->ParameterName=TEXT("Tint");Tint->DefaultValue=FLinearColor::White;
    auto* Color=Expression<UMaterialExpressionMultiply>(M);Color->A.Connect(0,Vertex);Color->B.Connect(0,Tint);
    auto* Roughness=Expression<UMaterialExpressionScalarParameter>(M);Roughness->ParameterName=TEXT("Roughness");Roughness->DefaultValue=.65;
    auto* Metallic=Expression<UMaterialExpressionScalarParameter>(M);Metallic->ParameterName=TEXT("Metallic");Metallic->DefaultValue=0;
    auto* Glow=Expression<UMaterialExpressionScalarParameter>(M);Glow->ParameterName=TEXT("Glow");Glow->DefaultValue=0;
    auto* Emissive=Expression<UMaterialExpressionMultiply>(M);Emissive->A.Connect(0,Color);Emissive->B.Connect(0,Glow);
    M->GetEditorOnlyData()->BaseColor.Connect(0,Color);M->GetEditorOnlyData()->Roughness.Connect(0,Roughness);
    M->GetEditorOnlyData()->Metallic.Connect(0,Metallic);M->GetEditorOnlyData()->EmissiveColor.Connect(0,Emissive);
    M->PostEditChange();FAssetRegistryModule::AssetCreated(M);FAssetCompilingManager::Get().FinishAllCompilation();
    return SaveAsset(M)?M:nullptr;
}

bool Materials(bool bRepair)
{
    UMaterial* Base=BaseMaterial();if(!Base)return false;
    const float Roughness[]={.28f,.67f,.23f,.93f,.10f,.35f};
    const float Metallic[]={.35f,0,.82f,.02f,.65f,.1f};
    for(int32 I=0;I<static_cast<int32>(ODGeometry::ESurface::Count);++I)
    {
        const TCHAR* Name=ODGeometry::MaterialName(static_cast<ODGeometry::ESurface>(I));
        const FString Path=FString(TEXT("/Game/Overdrive/Materials/"))+Name;
        auto* M=Existing<UMaterialInstanceConstant>(Path);
        if(M&&!bRepair)continue;
        const bool bNew=M==nullptr;
        if(!M)M=NewObject<UMaterialInstanceConstant>(CreatePackage(*Path),Name,RF_Public|RF_Standalone);
        M->SetParentEditorOnly(Base);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Roughness")),Roughness[I]);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Metallic")),Metallic[I]);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Glow")),I==static_cast<int32>(ODGeometry::ESurface::Glow)?1.25f:0.f);
        M->PostEditChange();if(bNew)FAssetRegistryModule::AssetCreated(M);if(!SaveAsset(M))return false;
    }
    const FString Path=TEXT("/Game/Overdrive/Materials/M_Ocean");
    if(Existing<UMaterial>(Path))return true;
    UMaterial* Ocean=NewObject<UMaterial>(CreatePackage(*Path),TEXT("M_Ocean"),RF_Public|RF_Standalone);
    Ocean->TwoSided=true;
    auto* Color=Expression<UMaterialExpressionConstant3Vector>(Ocean);Color->Constant=FLinearColor(.015f,.19f,.24f);
    auto* Rough=Expression<UMaterialExpressionConstant>(Ocean);Rough->R=.13f;
    auto* Metal=Expression<UMaterialExpressionConstant>(Ocean);Metal->R=.38f;
    auto* World=Expression<UMaterialExpressionWorldPosition>(Ocean);
    auto* Time=Expression<UMaterialExpressionTime>(Ocean);
    auto* Waves=Expression<UMaterialExpressionCustom>(Ocean);Waves->OutputType=CMOT_Float3;Waves->Inputs.Reset();
    Waves->Code=TEXT("float2 p = Position.xy * 0.0007; return normalize(float3(sin(p.x + T * 0.4) * 0.10 + sin(p.y * 2.1 + T) * 0.03, cos(p.y + T * 0.3) * 0.08, 1.0));");
    FCustomInput PositionInput;PositionInput.InputName=TEXT("Position");PositionInput.Input.Connect(0,World);Waves->Inputs.Add(PositionInput);
    FCustomInput TimeInput;TimeInput.InputName=TEXT("T");TimeInput.Input.Connect(0,Time);Waves->Inputs.Add(TimeInput);
    Ocean->GetEditorOnlyData()->BaseColor.Connect(0,Color);Ocean->GetEditorOnlyData()->Roughness.Connect(0,Rough);
    Ocean->GetEditorOnlyData()->Metallic.Connect(0,Metal);Ocean->GetEditorOnlyData()->Normal.Connect(0,Waves);
    Ocean->PostEditChange();FAssetRegistryModule::AssetCreated(Ocean);FAssetCompilingManager::Get().FinishAllCompilation();
    return SaveAsset(Ocean);
}

bool KartMaterials()
{
    const FString Root=FString(ODGeometry::KartAssetRoot)+TEXT("/Materials/");
    const FString BasePath=Root+TEXT("M_KartFinish");
    UMaterial* Base=Existing<UMaterial>(BasePath);
    if(!Base)
    {
        Base=NewObject<UMaterial>(CreatePackage(*BasePath),TEXT("M_KartFinish"),RF_Public|RF_Standalone);
        Base->TwoSided=false;Base->SetShadingModel(MSM_ClearCoat);
        auto* Vertex=Expression<UMaterialExpressionVertexColor>(Base);
        auto* Tint=Expression<UMaterialExpressionVectorParameter>(Base);Tint->ParameterName=TEXT("Tint");Tint->DefaultValue=FLinearColor::White;
        auto* Color=Expression<UMaterialExpressionMultiply>(Base);Color->A.Connect(0,Vertex);Color->B.Connect(0,Tint);
        const auto Scalar=[&](const TCHAR* Name,float Value){auto* Node=Expression<UMaterialExpressionScalarParameter>(Base);Node->ParameterName=Name;Node->DefaultValue=Value;return Node;};
        auto* Rough=Scalar(TEXT("Roughness"),.32f);auto* Metal=Scalar(TEXT("Metallic"),0.f);auto* Coat=Scalar(TEXT("Coat"),0.f);
        auto* CoatRough=Scalar(TEXT("CoatRoughness"),.13f);auto* Glow=Scalar(TEXT("Glow"),0.f);auto* Weave=Scalar(TEXT("Weave"),0.f);
        auto* World=Expression<UMaterialExpressionWorldPosition>(Base);
        auto* Local=Expression<UMaterialExpressionTransformPosition>(Base);Local->Input.Connect(0,World);
        Local->TransformSourceType=TRANSFORMPOSSOURCE_World;Local->TransformType=TRANSFORMPOSSOURCE_Local;
        auto* Detail=Expression<UMaterialExpressionCustom>(Base);Detail->OutputType=CMOT_Float1;Detail->Inputs.Reset();
        Detail->Code=TEXT("float2 p = P.xy * 1.4; float aa = saturate(1.0 - max(length(ddx(p)), length(ddy(p)))); float w = sin(p.x * 6.2831853) * sin(p.y * 6.2831853); return 1.0 - Strength * aa * (0.055 + 0.045 * w);");
        FCustomInput Position;Position.InputName=TEXT("P");Position.Input.Connect(0,Local);Detail->Inputs.Add(Position);
        FCustomInput Strength;Strength.InputName=TEXT("Strength");Strength.Input.Connect(0,Weave);Detail->Inputs.Add(Strength);
        auto* Finish=Expression<UMaterialExpressionMultiply>(Base);Finish->A.Connect(0,Color);Finish->B.Connect(0,Detail);
        auto* Emissive=Expression<UMaterialExpressionMultiply>(Base);Emissive->A.Connect(0,Color);Emissive->B.Connect(0,Glow);
        Base->GetEditorOnlyData()->BaseColor.Connect(0,Finish);Base->GetEditorOnlyData()->Roughness.Connect(0,Rough);
        Base->GetEditorOnlyData()->Metallic.Connect(0,Metal);Base->GetEditorOnlyData()->ClearCoat.Connect(0,Coat);
        Base->GetEditorOnlyData()->ClearCoatRoughness.Connect(0,CoatRough);Base->GetEditorOnlyData()->EmissiveColor.Connect(0,Emissive);
        Base->PostEditChange();FAssetRegistryModule::AssetCreated(Base);FAssetCompilingManager::Get().FinishAllCompilation();
        if(!SaveAsset(Base))return false;
    }
    const float Roughness[]={.27f,.43f,.26f,.87f,.095f,.28f};
    const float Metallic[]={.32f,0.f,.82f,0.f,.72f,.15f};
    const float Coat[]={1.f,.3f,.1f,0.f,1.f,.4f};
    for(int32 I=0;I<static_cast<int32>(ODGeometry::ESurface::Count);++I)
    {
        const TCHAR* Name=ODGeometry::MaterialName(static_cast<ODGeometry::ESurface>(I));const FString Path=Root+Name;
        if(Existing<UMaterialInstanceConstant>(Path))continue;
        auto* M=NewObject<UMaterialInstanceConstant>(CreatePackage(*Path),Name,RF_Public|RF_Standalone);M->SetParentEditorOnly(Base);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Roughness")),Roughness[I]);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Metallic")),Metallic[I]);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Coat")),Coat[I]);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Weave")),I==1?1.f:0.f);
        M->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Glow")),I==5?1.3f:0.f);
        M->PostEditChange();FAssetRegistryModule::AssetCreated(M);if(!SaveAsset(M))return false;
    }
    return true;
}

bool MeshAsset(FName Name,const ODGeometry::FMesh& Source,bool bRepair)
{
    const bool bKart=ODGeometry::IsKartAsset(Name);
    const FString Root=bKart?ODGeometry::KartAssetRoot:TEXT("/Game/Overdrive");
    const FString Path=Root+TEXT("/Meshes/")+Name.ToString();
    UStaticMesh* Mesh=Existing<UStaticMesh>(Path);
    if(Mesh&&!bRepair)return true;
    const bool bNew=Mesh==nullptr;
    if(!Mesh)Mesh=NewObject<UStaticMesh>(CreatePackage(*Path),Name,RF_Public|RF_Standalone);
    Mesh->SetNumSourceModels(0);Mesh->GetStaticMaterials().Reset();Mesh->GetSectionInfoMap().Clear();Mesh->GetOriginalSectionInfoMap().Clear();
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
    auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
    auto Colors=Attributes.GetVertexInstanceColors();auto UVs=Attributes.GetVertexInstanceUVs();UVs.SetNumChannels(1);
    auto MaterialNames=Attributes.GetPolygonGroupMaterialSlotNames();
    for(int32 SectionIndex=0;SectionIndex<Source.Sections.Num();++SectionIndex)
    {
        const auto Surface=static_cast<ODGeometry::ESurface>(SectionIndex);
        const FString MaterialPath=Root+TEXT("/Materials/")+ODGeometry::MaterialName(Surface);
        UMaterialInterface* Material=Existing<UMaterialInterface>(MaterialPath);if(!Material)return false;
        const FName Slot=FName(ODGeometry::MaterialName(Surface));
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,Slot,Slot));
        const FPolygonGroupID Group=Description.CreatePolygonGroup();MaterialNames[Group]=Slot;
        const auto& Vertices=Source.Sections[SectionIndex].Vertices;
        for(int32 I=0;I+2<Vertices.Num();I+=3)
        {
            TArray<FVertexInstanceID,TInlineAllocator<3>> Corners;
            const FVector3f E1=Vertices[I+1].Position-Vertices[I].Position,E2=Vertices[I+2].Position-Vertices[I].Position;
            const FVector2f UV1=Vertices[I+1].UV-Vertices[I].UV,UV2=Vertices[I+2].UV-Vertices[I].UV;
            const float Det=UV1.X*UV2.Y-UV1.Y*UV2.X;
            const bool bValidUV=FMath::IsFinite(Det)&&FMath::Abs(Det)>1.e-12f;
            const FVector3f U=bValidUV?(E1*UV2.Y-E2*UV1.Y)/Det:E1,W=bValidUV?(E2*UV1.X-E1*UV2.X)/Det:E2;
            for(int32 K=0;K<3;++K)
            {
                const auto& V=Vertices[I+K];const FVertexID Vertex=Description.CreateVertex();Positions[Vertex]=V.Position;
                const FVertexInstanceID Instance=Description.CreateVertexInstance(Vertex);
                Normals[Instance]=V.Normal;Colors[Instance]=FVector4f(V.Color.R,V.Color.G,V.Color.B,V.Color.A);UVs.Set(Instance,0,V.UV);Corners.Add(Instance);
                FVector3f T=(U-V.Normal*FVector3f::DotProduct(U,V.Normal)).GetSafeNormal();
                if(T.IsNearlyZero())T=FVector3f::CrossProduct(V.Normal,FMath::Abs(V.Normal.Z)>.9f?FVector3f(0,1,0):FVector3f(0,0,1)).GetSafeNormal();
                Tangents[Instance]=T;Signs[Instance]=FVector3f::DotProduct(FVector3f::CrossProduct(V.Normal,T),W)<0?-1.f:1.f;
            }
            Description.CreatePolygon(Group,Corners);
        }
    }
    auto& Model=Mesh->AddSourceModel();Model.BuildSettings.bRecomputeNormals=false;Model.BuildSettings.bRecomputeTangents=!bKart;
    Model.BuildSettings.bUseHighPrecisionTangentBasis=bKart;Model.BuildSettings.bUseFullPrecisionUVs=bKart;
    Model.BuildSettings.bUseMikkTSpace=true;Model.BuildSettings.bRemoveDegenerates=true;Model.BuildSettings.bGenerateLightmapUVs=false;
    Model.BuildSettings.DistanceFieldResolutionScale=Name==TEXT("SM_Road")?2.0f:1.0f;
    Model.BuildSettings.bGenerateDistanceFieldAsIfTwoSided=Name==TEXT("SM_Scenery");
    Mesh->bGenerateMeshDistanceField=Name!=TEXT("SM_Ocean")&&Name!=TEXT("SM_Clouds");
    *Mesh->CreateMeshDescription(0)=MoveTemp(Description);Mesh->CommitMeshDescription(0);
    if(bKart)
    {
        const FMeshBuildSettings Settings=Model.BuildSettings;
        Mesh->bAutoComputeLODScreenSize=false;Model.ScreenSize.Default=1.f;
        for(int32 I=1;I<3;++I)
        {
            auto& LOD=Mesh->AddSourceModel();LOD.BuildSettings=Settings;
            LOD.ReductionSettings.BaseLODModel=0;LOD.ReductionSettings.PercentTriangles=I==1?.5f:.20f;
            LOD.ScreenSize.Default=Name==TEXT("SM_Kart")?(I==1?.20f:.07f):(I==1?.065f:.02f);
        }
    }
    Mesh->Build(false);Mesh->PostEditChange();if(bNew)FAssetRegistryModule::AssetCreated(Mesh);
    FAssetCompilingManager::Get().FinishAllCompilation();
    return SaveAsset(Mesh);
}
}

bool BuildOverdriveKartAssets(bool bRebuild)
{
    if(bRebuild)
    {
        const FString Source=FPaths::ProjectContentDir()/TEXT("Overdrive/Karts/V2");
        const FString Backup=FPaths::ProjectSavedDir()/TEXT("AssetBackups")/(TEXT("KartV2-")+FGuid::NewGuid().ToString());
        if(IFileManager::Get().DirectoryExists(*Source)&&!FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*Backup,*Source,false))
        {
            UE_LOG(LogTemp,Error,TEXT("Kart rebuild stopped: asset backup failed."));return false;
        }
    }
    if(!KartMaterials())return false;
    if(!bRebuild&&FPackageName::DoesPackageExist(FString(ODGeometry::KartAssetRoot)+TEXT("/Meshes/SM_Kart"))
        &&FPackageName::DoesPackageExist(FString(ODGeometry::KartAssetRoot)+TEXT("/Meshes/SM_KartWheel"))
        &&FPackageName::DoesPackageExist(FString(ODGeometry::KartAssetRoot)+TEXT("/Meshes/SM_KartSteering")))return true;
    TMap<FName,ODGeometry::FMesh> Meshes;ODGeometry::BuildKartAssets(Meshes);
    for(const auto& Entry:Meshes)if(!MeshAsset(Entry.Key,Entry.Value,bRebuild))return false;
    UE_LOG(LogTemp,Display,TEXT("Comet GT kart assets ready. Original karts, scenery and map were preserved."));
    return true;
}

bool BuildOverdriveAssets(bool bRepairGraphics)
{
    if(bRepairGraphics)
    {
        const FString Source=FPaths::ProjectContentDir()/TEXT("Overdrive");
        const FString Backup=FPaths::ProjectSavedDir()/TEXT("AssetBackups")/FGuid::NewGuid().ToString();
        if(IFileManager::Get().DirectoryExists(*Source)&&!FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*Backup,*Source,false))
        {
            UE_LOG(LogTemp,Error,TEXT("Graphics repair stopped: asset backup failed."));return false;
        }
        UE_LOG(LogTemp,Display,TEXT("Original assets backed up to %s"),*Backup);
    }
    UE_LOG(LogTemp,Display,TEXT("Building OVERDRIVE assets (graphics repair: %s)."),bRepairGraphics?TEXT("yes"):TEXT("no"));
    if(!Materials(bRepairGraphics)||!KartMaterials())return false;
    TMap<FName,ODGeometry::FMesh> Meshes;ODGeometry::BuildAssets(Meshes);
    for(const auto& Entry:Meshes)if(!MeshAsset(Entry.Key,Entry.Value,bRepairGraphics))return false;
    const FString MapPath=TEXT("/Game/Overdrive/Maps/CloudtopCoast");
    if(FPackageName::DoesPackageExist(MapPath))return true;
    UPackage* Package=CreatePackage(*MapPath);
    UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("CloudtopCoast"),Package);
    if(!World)return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->SetFlags(RF_Public|RF_Standalone);
    World->GetWorldSettings()->DefaultGameMode=AOverdriveGameMode::StaticClass();
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AOverdriveWorld* Coast=World->SpawnActor<AOverdriveWorld>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    if(!Coast||!Coast->IsReady())return false;
    Coast->SetActorLabel(TEXT("Cloudtop Coast"));
    World->SpawnActor<APlayerStart>(FVector(9400,-7500,2500),FRotator::ZeroRotator,Spawn);
    FAssetRegistryModule::AssetCreated(World);
    FAssetCompilingManager::Get().FinishAllCompilation();
    const bool bSaved=SaveAsset(World,true);
    UE_LOG(LogTemp,Display,TEXT("OVERDRIVE setup %s. Open /Game/Overdrive/Maps/CloudtopCoast and press Play."),bSaved?TEXT("complete"):TEXT("failed"));
    return bSaved;
}
