#include "OverdriveHUD.h"
#include "OverdriveGameMode.h"
#include "OverdriveController.h"
#include "OverdriveSaveGame.h"
#include "OverdriveGeometry.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
const FLinearColor Orange(.99f,.48f,.28f,1);
const FLinearColor Mint(.65f,.92f,.85f,1);
const FLinearColor Muted(.64f,.73f,.80f,1);
const FLinearColor Ink(.035f,.065f,.11f,.94f);
FString Time(double Seconds){return UTF8_TO_TCHAR(Overdrive::FormatTime(Seconds).c_str());}
FString Rank(int32 N){return FString::Printf(TEXT("%d%s"),N,N==1?TEXT("st"):N==2?TEXT("nd"):N==3?TEXT("rd"):TEXT("th"));}
}

void AOverdriveHUD::Text(const FString& Value,float X,float Y,float Size,FLinearColor Color,bool bRight)
{
    if(!Canvas||!FSlateApplication::IsInitialized())return;
    if(!RuntimeFont)
    {
        RuntimeFont=NewObject<UFont>(this);
        RuntimeFont->FontCacheType=EFontCacheType::Runtime;
        RuntimeFont->CompositeFont=*FCoreStyle::GetDefaultFont();
    }
    const float PixelSize=FMath::Max(Size,15.f)*Scale*.75f;
    FSlateFontInfo Font(RuntimeFont,FMath::Max(1,FMath::RoundToInt(PixelSize)),Size>=28?FName("Bold"):FName("Regular"));
    FVector2D Position=Origin+FVector2D(X,Y)*Scale;
    if(bRight)Position.X-=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Font).X;
    FCanvasTextItem Item(Position,FText::FromString(Value),Font,Color);
    Item.EnableShadow(FLinearColor(0,0,0,.35f),FVector2D(0,1));Canvas->DrawItem(Item);
}
void AOverdriveHUD::Rect(float X,float Y,float W,float H,FLinearColor Color)
{
    DrawRect(Color,Origin.X+X*Scale,Origin.Y+Y*Scale,W*Scale,H*Scale);
}
void AOverdriveHUD::Line(float X1,float Y1,float X2,float Y2,FLinearColor Color,float Width)
{
    DrawLine(Origin.X+X1*Scale,Origin.Y+Y1*Scale,Origin.X+X2*Scale,Origin.Y+Y2*Scale,Color,Width*Scale);
}

void AOverdriveHUD::MiniMap(float X,float Y,float W,float H)
{
    const auto Point=[&](const Overdrive::Vec3& P){return FVector2D(X+(P.X+120)/240*W,Y+(P.Z+120)/240*H);};
    const auto& Samples=Game->Simulation.Circuit.Samples;
    for(std::size_t I=0;I+6<Samples.size();I+=6)
    {
        const FVector2D A=Point(Samples[I].Position),B=Point(Samples[I+6].Position);
        Line(A.X,A.Y,B.X,B.Y,FLinearColor(.03f,.10f,.17f,.8f),10);
        Line(A.X,A.Y,B.X,B.Y,Mint,2.5);
    }
    if(Game->Simulation.State!=Overdrive::Phase::Menu)
        for(int32 I=7;I>=0;--I)
        {
            const auto& R=Game->Simulation.Racers[I];const FVector2D P=Point(Game->Simulation.Circuit.At(Overdrive::StartOffset+R.Distance).Position);
            Rect(P.X-4,P.Y-4,I==0?9:6,I==0?9:6,I==0?Orange:ODGeometry::Color(Overdrive::RivalColors[I]));
        }
}

void AOverdriveHUD::Rows(float X,float Y,float W,float Height)
{
    const auto Items=Game->GetMenuRows();
    for(int32 I=0;I<Items.Num();++I)
    {
        const auto& Row=Items[I];const bool Selected=I==Game->GetSelectedRow();const float Top=Y+I*Height;
        Rect(X,Top,W,Height-6,Selected?FLinearColor(.99f,.48f,.28f,.94f):FLinearColor(.12f,.19f,.28f,.70f));
        if(Selected)Rect(X-5,Top,3,Height-6,Mint);
        Text(Row.Label,X+18,Top+12,18,Selected?Ink:FLinearColor::White);
        if(!Row.Value.IsEmpty())Text(Row.Value,X+W-18,Top+14,12,Selected?Ink:Muted,true);
        AddHitBox(Origin+FVector2D(X,Top)*Scale,FVector2D(W,Height-6)*Scale,FName(*FString::Printf(TEXT("ODRow_%d"),I)),true,I);
    }
}

void AOverdriveHUD::Menu()
{
    const auto Page=Game->GetPage();const auto* Settings=Game->GetSettings();
    const bool Home=Page==EOverdrivePage::Home,Garage=Page==EOverdrivePage::Garage;
    for(int32 I=0;I<160;++I)Rect(I*6,0,6,900,FLinearColor(.025f,.05f,.09f,.96f*(1-I/160.f)));
    Text(TEXT("///"),62,38,36,Orange);Text(TEXT("OVERDRIVE"),126,38,34);Text(TEXT("KARTING. ELEVATED."),129,77,10,Muted);
    Line(62,111,1378,111,FLinearColor(1,1,1,.18f));
    Text(TEXT("UNREAL ENGINE 5"),1375,48,12,Mint,true);
    Text(Driver->IsUsingController()?TEXT("Controller active"):TEXT("Keyboard / mouse"),1375,75,11,Muted,true);
    if(Home)
    {
        Text(TEXT("A LITTLE ALTITUDE. A LOT OF ATTITUDE."),65,161,12,Muted);
        Text(TEXT("TAKE THE"),58,203,71);Text(TEXT("SCENIC ROUTE."),58,278,65);
        Text(TEXT("Leave ordinary in the rearview."),65,359,18,Muted);
        Rows(65,407,565,47);
        Text(TEXT("YOUR NEXT GREAT ESCAPE"),1365,566,11,Mint,true);
        Text(TEXT("Cloudtop"),1365,599,58,FLinearColor::White,true);Text(TEXT("Coast"),1365,658,66,FLinearColor::White,true);
        Text(TEXT("3 laps. 8 racers. Endless sky."),1365,744,15,Muted,true);
    }
    else if(Garage)
    {
        Text(TEXT("YOUR RIDE. YOUR RULES."),65,165,12,Muted);Text(TEXT("A little personality."),65,208,52);
        Text(TEXT("COMET GT"),65,292,45);Text(TEXT("Sculpted aero / Forged wheels / Clear-coat finish"),65,351,15,Muted);
        for(int32 I=0;I<6;++I)
        {
            const float X=65+I*72;Rect(X,401,54,54,ODGeometry::Color(Overdrive::Paints[I]));
            if(I==Settings->PaintIndex){Line(X,463,X+54,463,Mint,3);Text(TEXT("*"),X+22,415,24,Ink);}
        }
        Text(TEXT("Speed"),65,508,16,Muted);Rect(180,514,230,5,FLinearColor(.20f,.29f,.36f,1));Rect(180,514,177,5,Mint);
        Text(TEXT("Handling"),65,550,16,Muted);Rect(180,556,230,5,FLinearColor(.20f,.29f,.36f,1));Rect(180,556,207,5,Mint);
        Text(TEXT("Boost"),65,592,16,Muted);Rect(180,598,230,5,FLinearColor(.20f,.29f,.36f,1));Rect(180,598,188,5,Mint);
        Rows(65,660,565,52);Text(TEXT("Paint is cosmetic. The driver makes the difference."),65,792,13,Muted);
    }
    else
    {
        Rect(235,150,970,652,Ink);
        if(Page==EOverdrivePage::Settings)
        {
            Text(TEXT("Your kind of ride."),275,178,44);Text(TEXT("Left / right to adjust. Your changes are saved."),275,236,15,Muted);
            Rows(275,277,890,49);
            Text(TEXT("Lumen lighting. TSR. Native rendering. Hardware RT depends on platform and GPU."),275,748,12,Muted);
        }
        else if(Page==EOverdrivePage::Controls)
        {
            Text(TEXT("Find your flow."),275,183,47);Text(TEXT("Three laps. Seven rivals. First across the line wins."),275,249,16,Muted);
            const bool PS=Settings->ControllerLabels==1;
            const TArray<FString> Labels={TEXT("Steer"),TEXT("Accelerate / Brake"),TEXT("Hop into drift"),TEXT("Turbo"),TEXT("Use item"),TEXT("Look back"),TEXT("Recover kart"),TEXT("Pause / Menus")};
            const TArray<FString> Pad={TEXT("Left stick"),PS?TEXT("R2 / L2"):TEXT("RT / LT"),PS?TEXT("R1 + steer"):TEXT("RB + steer"),PS?TEXT("Cross"):TEXT("A"),PS?TEXT("Square"):TEXT("X"),PS?TEXT("Triangle"):TEXT("Y"),PS?TEXT("Circle"):TEXT("B"),PS?TEXT("Options / D-pad"):TEXT("Menu / D-pad")};
            const TArray<FString> Keyboard={TEXT("A D / Left Right"),TEXT("W S / Up Down"),TEXT("Shift + steer"),TEXT("Space"),TEXT("E"),TEXT("C"),TEXT("R"),TEXT("Escape / Arrows / Enter")};
            Text(TEXT("ACTION"),275,294,12,Mint);Text(TEXT("CONTROLLER"),555,294,12,Mint);Text(TEXT("KEYBOARD"),975,294,12,Mint);
            for(int32 I=0;I<Labels.Num();++I){const float Y=329+I*37;Text(Labels[I],275,Y,16);Text(Pad[I],555,Y,14,Muted);Text(Keyboard[I],975,Y,13,Muted);}
            Text(TEXT("Brake before tight turns. Tap drift + steer, hold your slide, then release for turbo."),275,648,14,Mint);
            Text(TEXT("Blue / orange / purple = 3 drift tiers. Straighten the kart for a clean-exit bonus."),275,675,13,Muted);
            Text(TEXT("Follow a rival to build slipstream. Start accelerating just after 1 for a launch boost."),275,700,13,Muted);
            Rows(275,729,890,52);
        }
        else if(Page==EOverdrivePage::Records)
        {
            Text(TEXT("Your high points."),275,191,49);Text(TEXT("Native records are saved separately from the browser version."),275,259,16,Muted);
            for(int32 I=0;I<3;++I)
            {
                const float Y=340+I*87;Text(FString::Printf(TEXT("%dcc  /  Best lap"),std::array<int32,3>{50,100,150}[I]),275,Y,21,Muted);
                Text(Time(Settings->BestLaps[I]),1157,Y-7,37,Mint,true);Line(275,Y+57,1165,Y+57,FLinearColor(1,1,1,.12f));
            }
            Text(FString::Printf(TEXT("Races completed: %d"),Settings->RacesCompleted),275,639,20);Rows(275,723,890,52);
        }
        else if(Page==EOverdrivePage::Pause)
        {
            Text(TEXT("Enjoying the view?"),275,243,59);Text(TEXT("The coast will be right where you left it."),275,328,21,Muted);
            Rows(275,443,890,70);
            Text(TEXT("Reconnect a controller or use the keyboard. The race never resumes automatically."),275,701,13,Muted);
        }
        else if(Page==EOverdrivePage::Results)
        {
            Text(Game->Simulation.Position()==1?TEXT("First class. First place."):TEXT("What a joyride."),275,181,45);
            Text(FString::Printf(TEXT("Cloudtop Coast  /  %dcc  /  %s"),Settings->EngineClass,*Rank(Game->Simulation.Position())),275,241,17,Muted);
            Text(TEXT("YOUR TIME"),275,293,11,Muted);Text(Time(Game->Simulation.Player().FinishTime),275,318,37,Mint);
            Text(TEXT("BEST LAP"),745,293,11,Muted);Text(Time(Game->Simulation.BestLap),745,318,37,Mint);
            const auto Standings=Game->Simulation.Standings();
            for(int32 I=0;I<8;++I)
            {
                const auto& R=Game->Simulation.Racers[Standings[I]];const float Y=385+I*35;
                if(R.Id==0)Rect(267,Y-5,905,31,FLinearColor(.99f,.48f,.28f,.13f));
                Text(FString::Printf(TEXT("%02d"),I+1),280,Y,15,Muted);
                Rect(332,Y+5,8,8,ODGeometry::Color(Overdrive::RivalColors[R.Id]));
                Text(UTF8_TO_TCHAR(Overdrive::Names[R.Id]),358,Y,17,R.Id==0?Orange:FLinearColor::White);
                Text(R.FinishTime>=0?Time(R.FinishTime):TEXT("Still racing"),1155,Y,15,Muted,true);
            }
            Rows(275,687,890,49);
        }
    }
    Line(62,838,1378,838,FLinearColor(1,1,1,.16f));
    Text(FString::Printf(TEXT("%s  Select     %s  Back     D-PAD / STICK / ARROWS  Navigate"),*Driver->ConfirmLabel(),*Driver->BackLabel()),65,863,13,Muted);
    Text(TEXT("Cloudtop Coast / Native edition"),1375,863,12,Mint,true);
}

void AOverdriveHUD::RaceHUD()
{
    const auto& Race=Game->Simulation;const auto& P=Race.Player();
    Rect(0,0,1440,122,FLinearColor(.02f,.055f,.09f,.5f));
    Text(Rank(Race.Position()),51,30,70);Text(TEXT("OF 8 RACERS"),56,104,11,Muted);
    Line(208,33,208,108,FLinearColor(1,1,1,.25f));Text(TEXT("LAP"),241,34,11,Muted);
    Text(FString::Printf(TEXT("%d / 3"),Race.Lap()),241,57,39);
    Text(TEXT("RACE TIME"),1381,36,11,Muted,true);Text(Time(P.FinishTime>=0?P.FinishTime:Race.Elapsed),1381,62,37,FLinearColor::White,true);
    Rect(1277,178,103,90,FLinearColor(.02f,.055f,.09f,.66f));
    const FString Item=P.HeldItem==Overdrive::Item::Turbo?TEXT("TURBO"):P.HeldItem==Overdrive::Item::Shield?TEXT("SHIELD"):P.HeldItem==Overdrive::Item::Pulse?TEXT("PULSE"):TEXT("ITEM");
    Text(Item,1329,207,18,P.HeldItem==Overdrive::Item::None?Muted:Mint,true);
    Text(FString::Printf(TEXT("%02d COINS"),P.Coins),1381,305,22,FLinearColor(1,.82f,.47f,1),true);
    MiniMap(52,644,195,147);Text(TEXT("CLOUDTOP COAST"),60,811,11,Mint);
    Text(FString::Printf(TEXT("%d"),FMath::RoundToInt(P.Speed*3.6)),1326,684,91,FLinearColor::White,true);Text(TEXT("KM/H"),1380,758,12,Muted,true);
    Text(TEXT("TURBO"),1172,806,13,Mint);Rect(1172,833,207,7,FLinearColor(.03f,.07f,.10f,.7f));Rect(1172,833,207*P.Energy/100,7,Mint);
    if(P.Drifting)
    {
        const FLinearColor Charge=P.DriftTier>=3?FLinearColor(.75f,.4f,1.f):P.DriftTier>=2?Orange:FLinearColor(.25f,.75f,1.f);
        Rect(53,174,344,98,FLinearColor(.02f,.055f,.09f,.78f));
        Text(P.DriftTier>0?FString::Printf(TEXT("DRIFT  /  TIER %d READY"),P.DriftTier):TEXT("DRIFT  /  BUILD YOUR CHARGE"),69,190,17,Charge);
        Rect(69,222,309,8,FLinearColor(.12f,.20f,.26f,1));Rect(69,222,309*P.DriftCharge,8,Charge);
        for(float Threshold:{.32f,.65f,.98f})Line(69+309*Threshold,219,69+309*Threshold,233,FLinearColor::White,1);
        Text(P.DriftTier?TEXT("Release for turbo. Straighten for a clean exit."):TEXT("Hold a controlled slide. Wall hits lose charge."),69,246,11,Muted);
    }
    if(P.DraftCharge>.03)
    {
        Text(TEXT("SLIPSTREAM"),57,305,14,Mint);Rect(57,331,230,5,FLinearColor(.08f,.16f,.2f,1));Rect(57,331,230*P.DraftCharge,5,Mint);
    }
    const double Corner=Race.CornerSpeed(P.Distance);
    if(Race.State==Overdrive::Phase::Racing&&P.Speed>Corner*1.06)
    {
        const bool Right=Race.Circuit.Curvature(Overdrive::StartOffset+P.Distance+18)>0;
        Text(FString::Printf(TEXT("%s TURN  /  BRAKE TO %d KM/H"),Right?TEXT("RIGHT"):TEXT("LEFT"),FMath::RoundToInt(Corner*3.6)),520,704,17,Orange);
    }
    else if(P.Grip<.38&&!P.Drifting)Text(TEXT("GRIP LIMIT  /  EASE OFF OR BRAKE"),521,704,16,Orange);
    if(std::abs(P.Yaw)>1.55&&Race.State==Overdrive::Phase::Racing)Text(TEXT("WRONG WAY  /  TURN AROUND OR USE RECOVER"),460,654,17,Orange);
    Text(Driver->ControlsHint(),303,866,12,Muted);
    if(Race.State==Overdrive::Phase::Countdown)
    {
        Text(FString::FromInt(FMath::Clamp(static_cast<int32>(std::ceil(Race.Countdown)),1,3)),680,312,180);
        const TCHAR* Hint=Driver->NeedsControllerNeutral()?TEXT("Release triggers and center the stick first"):
            P.LaunchReady?TEXT("LAUNCH READY  /  KEEP ACCELERATING"):
            Race.Countdown<1?TEXT("ACCELERATE NOW FOR A LAUNCH BOOST"):TEXT("Time your acceleration just after 1");
        Text(Hint,440,550,19,P.LaunchReady?Mint:FLinearColor::White);
    }
    else if(Game->GetGoTime()>0)Text(TEXT("GO!"),623,319,152,Mint);
    else if(Driver->NeedsControllerNeutral())Text(TEXT("Release triggers and center the stick to enable controller driving."),370,594,17,Orange);
}

void AOverdriveHUD::DrawHUD()
{
    Super::DrawHUD();
    if(!Canvas)return;
    Game=GetWorld()->GetAuthGameMode<AOverdriveGameMode>();Driver=Cast<AOverdriveController>(GetOwningPlayerController());
    if(!Game||!Driver||!Game->GetSettings())return;
    Scale=FMath::Min(Canvas->SizeX/1440.f,Canvas->SizeY/900.f);
    Origin=FVector2D((Canvas->SizeX-1440*Scale)*.5f,(Canvas->SizeY-900*Scale)*.5f);
    if(!Game->GetBootError().IsEmpty())
    {
        Rect(0,0,1440,900,Ink);Text(TEXT("Let's get you on track."),150,210,52);
        Text(TEXT("Native assets need to be generated before racing."),150,305,24,Orange);
        Text(TEXT("Open Unreal Editor and run the OverdriveSetup commandlet."),150,373,19,Muted);
        Text(TEXT("Then open /Game/Overdrive/Maps/CloudtopCoast and press Play."),150,412,19,Muted);return;
    }
    if(Game->IsMenuVisible())Menu();else RaceHUD();
    if(!Game->GetNotice().IsEmpty())
    {
        const bool MenuVisible=Game->IsMenuVisible();
        Rect(245,MenuVisible?117:180,950,42,FLinearColor(.025f,.055f,.10f,.86f));
        Text(Game->GetNotice(),266,MenuVisible?129:190,17,MenuVisible?Orange:Mint);
    }
}

void AOverdriveHUD::NotifyHitBoxClick(FName BoxName)
{
    const FString Name=BoxName.ToString();
    if(Game&&Game->IsMenuVisible()&&Name.StartsWith(TEXT("ODRow_"))){Game->SelectRow(FCString::Atoi(*Name.Mid(6)));Game->ActivateSelection();}
}
void AOverdriveHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
    const FString Name=BoxName.ToString();
    if(Game&&Game->IsMenuVisible()&&Name.StartsWith(TEXT("ODRow_")))Game->SelectRow(FCString::Atoi(*Name.Mid(6)));
}
