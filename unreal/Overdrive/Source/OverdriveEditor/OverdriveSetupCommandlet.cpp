#include "OverdriveSetupCommandlet.h"
#include "OverdriveAssetSetup.h"
#include "Misc/Parse.h"

UOverdriveSetupCommandlet::UOverdriveSetupCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true;
    LogToConsole = true; ShowErrorCount = true;
    HelpDescription = TEXT("Create missing OVERDRIVE materials, static meshes and Cloudtop Coast. Existing assets are preserved.");
}

int32 UOverdriveSetupCommandlet::Main(const FString& Params)
{
    if(FParse::Param(*Params,TEXT("KartsOnly")))return BuildOverdriveKartAssets(FParse::Param(*Params,TEXT("RebuildKarts")))?0:1;
    return BuildOverdriveAssets(FParse::Param(*Params,TEXT("RepairGraphics"))) ? 0 : 1;
}
