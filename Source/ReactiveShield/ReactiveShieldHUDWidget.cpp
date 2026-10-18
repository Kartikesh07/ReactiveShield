#include "ReactiveShieldHUDWidget.h"

#include "ReactiveShieldActor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor OnlineColor = FLinearColor::FromSRGBColor(FColor(145, 164, 180));
	const FLinearColor RechargeColor = FLinearColor::FromSRGBColor(FColor(192, 162, 105));
	const FLinearColor DepletedColor = FLinearColor::FromSRGBColor(FColor(190, 105, 96));
	const FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor(222, 221, 215));

	void SetTextIfChanged(UTextBlock* Widget, const FText& Text)
	{
		if (!Widget->GetText().EqualTo(Text))
		{
			Widget->SetText(Text);
		}
	}
}

void UReactiveShieldHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.012f, 0.012f, 0.012f, 0.88f));
	Panel->SetPadding(FMargin(14.0f, 10.0f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.0f, 1.0f));
	PanelSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	PanelSlot->SetPosition(FVector2D(32.0f, -32.0f));
	PanelSlot->SetSize(FVector2D(280.0f, 132.0f));
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Stack);

	auto AddLabel = [this, Stack](const TCHAR* Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size));
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(Color));
		Stack->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		return Label;
	};
	AddLabel(TEXT("REACTIVE / SHIELD"), 10, OnlineColor);
	EnergyText = AddLabel(TEXT("--%"), 17, TextColor);
	EnergyBar = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle BarStyle;
	BarStyle.BackgroundImage = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.045f, 0.045f, 0.045f));
	BarStyle.FillImage = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	// SProgressBar takes its desired height from the marquee brush even in determinate mode.
	BarStyle.MarqueeImage.ImageSize = FVector2D(1.0f, 6.0f);
	EnergyBar->SetWidgetStyle(BarStyle);
	EnergyBar->SetFillColorAndOpacity(OnlineColor);
	EnergyBar->SetPercent(0.0f);
	EnergyBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
	EnergyBar->SetBorderPadding(FVector2D::ZeroVector);
	Stack->AddChildToVerticalBox(EnergyBar)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));
	StatusText = AddLabel(TEXT("NO SHIELD"), 11, RechargeColor);
	FeedbackText = AddLabel(TEXT(""), 10, OnlineColor);

	// Four small arms leave the camera's exact center unobscured.
	const FVector2D ArmPositions[] = {{-7, -1}, {3, -1}, {-1, -7}, {-1, 3}};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UBorder* Arm = WidgetTree->ConstructWidget<UBorder>();
		Arm->SetBrushColor(FLinearColor(TextColor.R, TextColor.G, TextColor.B, 0.8f));
		UCanvasPanelSlot* ArmSlot = Canvas->AddChildToCanvas(Arm);
		ArmSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ArmSlot->SetPosition(ArmPositions[Index]);
		ArmSlot->SetSize(Index < 2 ? FVector2D(4, 2) : FVector2D(2, 4));
	}
}

void UReactiveShieldHUDWidget::AcquireShield()
{
	// A replacement shield starts a fresh snapshot, not a false reactivation event.
	Shield.Reset();
	bHasSnapshot = false;
	FeedbackRemaining = 0.0f;
	FeedbackText->SetText(FText::GetEmpty());
	const APawn* Pawn = GetOwningPlayerPawn();
	const FVector Origin = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	double BestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<AReactiveShieldActor> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || (!ShieldActorTag.IsNone() && !It->ActorHasTag(ShieldActorTag)))
		{
			continue;
		}
		const double Distance = FVector::DistSquared(Origin, It->GetActorLocation());
		if (Distance < BestDistance)
		{
			Shield = *It;
			BestDistance = Distance;
		}
	}
	SearchCountdown = 1.0f;
}

void UReactiveShieldHUDWidget::RefreshShield()
{
	AReactiveShieldActor* CurrentShield = Shield.Get();
	if (!IsValid(CurrentShield))
	{
		bHasSnapshot = false;
		TargetEnergy = 0.0f;
		FeedbackRemaining = 0.0f;
		FeedbackText->SetText(FText::GetEmpty());
		SetTextIfChanged(EnergyText, FText::FromString(TEXT("--%")));
		SetTextIfChanged(StatusText, FText::FromString(TEXT("NO SHIELD")));
		StatusText->SetColorAndOpacity(FSlateColor(RechargeColor));
		EnergyBar->SetFillColorAndOpacity(RechargeColor);
		return;
	}

	TargetEnergy = CurrentShield->GetEnergyNormalized();
	const bool bActive = CurrentShield->IsShieldActive();
	const bool bRecharging = CurrentShield->IsRegenerating();
	const FLinearColor Color = !bActive ? DepletedColor : (bRecharging || TargetEnergy <= 0.25f ? RechargeColor : OnlineColor);
	if (bHasSnapshot && bActive != bWasActive)
	{
		FeedbackText->SetText(FText::FromString(bActive ? TEXT("SHIELD REACTIVATED") : TEXT("SHIELD COLLAPSED")));
		FeedbackText->SetColorAndOpacity(FSlateColor(bActive ? OnlineColor : DepletedColor));
		FeedbackRemaining = 1.5f;
	}
	if (!bHasSnapshot)
	{
		DisplayEnergy = TargetEnergy;
	}
	bHasSnapshot = true;
	bWasActive = bActive;
	SetTextIfChanged(EnergyText, FText::FromString(FString::Printf(TEXT("%d%%"),
		FMath::RoundToInt(TargetEnergy * 100.0f))));
	SetTextIfChanged(StatusText, FText::FromString(!bActive ? TEXT("DEPLETED") : (bRecharging ? TEXT("RECHARGING") : TEXT("ONLINE"))));
	StatusText->SetColorAndOpacity(FSlateColor(Color));
	EnergyBar->SetFillColorAndOpacity(Color);
}

void UReactiveShieldHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SearchCountdown -= InDeltaTime;
	SampleCountdown -= InDeltaTime;
	if (SampleCountdown <= 0.0f)
	{
		if (!Shield.IsValid() && SearchCountdown <= 0.0f)
		{
			AcquireShield();
		}
		RefreshShield();
		SampleCountdown = 0.1f;
	}
	DisplayEnergy = FMath::FInterpTo(DisplayEnergy, TargetEnergy, InDeltaTime, 10.0f);
	if (FMath::IsNearlyEqual(DisplayEnergy, TargetEnergy, 0.001f))
	{
		DisplayEnergy = TargetEnergy;
	}
	if (!FMath::IsNearlyEqual(EnergyBar->GetPercent(), DisplayEnergy, 0.0001f))
	{
		EnergyBar->SetPercent(DisplayEnergy);
	}
	FeedbackRemaining = FMath::Max(0.0f, FeedbackRemaining - InDeltaTime);
	FeedbackText->SetRenderOpacity(FMath::Clamp(FeedbackRemaining / 0.4f, 0.0f, 1.0f));
}
