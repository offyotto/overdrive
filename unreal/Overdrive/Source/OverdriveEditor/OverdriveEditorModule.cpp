#include "Modules/ModuleManager.h"
#include "OverdriveAssetSetup.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"

class FOverdriveEditorModule final : public IModuleInterface
{
    FTSTicker::FDelegateHandle StartupHandle;
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet()) return;
        StartupHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            if (!GEditor) return true;
            const bool bMapExists = FPackageName::DoesPackageExist(TEXT("/Game/Overdrive/Maps/CloudtopCoast"));
            if (!bMapExists && !BuildOverdriveAssets())
            {
                UE_LOG(LogTemp, Error, TEXT("OVERDRIVE setup failed. See Output Log; run the OverdriveSetup commandlet to retry."));
                return false;
            }
            if(bMapExists&&!BuildOverdriveKartAssets())
            {
                UE_LOG(LogTemp,Error,TEXT("Kart upgrade assets could not be generated. Run the OverdriveSetup commandlet with -KartsOnly."));return false;
            }
            UWorld* Current = GEditor->GetEditorWorldContext().World();
            if (Current && Current->GetPackage()->GetName() == TEXT("/Engine/Maps/Entry"))
            {
                const FString MapFile = FPackageName::LongPackageNameToFilename(TEXT("/Game/Overdrive/Maps/CloudtopCoast"),FPackageName::GetMapPackageExtension());
                if (!UEditorLoadingAndSavingUtils::LoadMap(MapFile))
                    UE_LOG(LogTemp, Error, TEXT("Could not open Cloudtop Coast: %s"),*MapFile);
            }
            return false;
        }), 1.0f);
    }
    virtual void ShutdownModule() override
    {
        if (StartupHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(StartupHandle);
    }
};

IMPLEMENT_MODULE(FOverdriveEditorModule, OverdriveEditor);
