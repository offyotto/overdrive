#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "OverdriveSetupCommandlet.generated.h"

UCLASS()
class UOverdriveSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UOverdriveSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
