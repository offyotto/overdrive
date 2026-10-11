#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OverdriveHUD.generated.h"

class AOverdriveGameMode;
class AOverdriveController;

UCLASS()
class OVERDRIVE_API AOverdriveHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
private:
    void Text(const FString& Value,float X,float Y,float Size,FLinearColor Color=FLinearColor::White,bool bRight=false);
    void Rect(float X,float Y,float W,float H,FLinearColor Color);
    void Line(float X1,float Y1,float X2,float Y2,FLinearColor Color,float Width=1);
    void MiniMap(float X,float Y,float W,float H);
    void Menu();
    void RaceHUD();
    void Rows(float X,float Y,float W,float Height);
    UPROPERTY(Transient) TObjectPtr<UFont> RuntimeFont;
    float Scale=1;
    FVector2D Origin=FVector2D::ZeroVector;
    AOverdriveGameMode* Game=nullptr;
    AOverdriveController* Driver=nullptr;
};
