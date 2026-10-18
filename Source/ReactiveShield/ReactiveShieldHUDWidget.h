#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReactiveShieldHUDWidget.generated.h"

class AReactiveShieldActor;
class UProgressBar;
class UTextBlock;

/** Asset-free UMG layout. Reads shield state without modifying gameplay. */
UCLASS()
class REACTIVESHIELD_API UReactiveShieldHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Optional actor tag; otherwise acquire the closest shield to the owning pawn. */
	FName ShieldActorTag;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshShield();
	void AcquireShield();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> EnergyBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnergyText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;

	TWeakObjectPtr<AReactiveShieldActor> Shield;
	float SampleCountdown = 0.0f;
	float SearchCountdown = 0.0f;
	float TargetEnergy = 0.0f;
	float DisplayEnergy = 0.0f;
	float FeedbackRemaining = 0.0f;
	bool bHasSnapshot = false;
	bool bWasActive = false;
};
