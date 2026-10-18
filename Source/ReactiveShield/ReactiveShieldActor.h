#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "ReactiveShieldActor.generated.h"

class AShieldTestProjectile;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USphereComponent;
class UStaticMeshComponent;

/** Spherical projectile boundary. Configure collision responses on ShieldCollision. */
UCLASS()
class REACTIVESHIELD_API AReactiveShieldActor : public AActor
{
	GENERATED_BODY()

public:
	AReactiveShieldActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Hit must come from the projectile's blocking sweep against this shield. */
	void HandleProjectileImpact(AShieldTestProjectile* Projectile, const FHitResult& Hit);

	UFUNCTION(BlueprintPure, Category = "Shield|Energy")
	float GetEnergyNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Shield|Energy")
	float GetCurrentEnergy() const { return CurrentEnergy; }

	UFUNCTION(BlueprintPure, Category = "Shield|Energy")
	bool IsShieldActive() const { return bShieldActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Energy", meta = (ClampMin = "1.0"))
	float MaxEnergy = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Energy", meta = (ClampMin = "0.0"))
	float DamagePerHit = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Energy", meta = (ClampMin = "0.0"))
	float RegenerationRate = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Energy", meta = (ClampMin = "0.0", Units = "s"))
	float RegenerationDelay = 2.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Shield|Energy")
	float CurrentEnergy = 100.0f;

	/** Energy state; depleted shields remain inactive until 25% energy is restored. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Shield|Energy")
	bool bShieldActive = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Shield|Energy")
	bool bRegenerating = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<USphereComponent> ShieldCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	/** Unscaled radius in centimeters. Use uniform actor scale for a spherical boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "1.0", Units = "cm"))
	float ShieldRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Appearance")
	TObjectPtr<UMaterialInterface> ShieldMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Appearance")
	FLinearColor ShieldColor = FLinearColor(0.0f, 0.65f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Appearance", meta = (ClampMin = "0.0"))
	float GlowIntensity = 5.0f;

	/** Expansion speed in unscaled shield centimeters per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Ripple", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RippleSpeed = 180.0f;

	/** Soft ring width measured along the sphere's surface in unscaled centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Ripple", meta = (ClampMin = "0.1", Units = "cm"))
	float RippleWidth = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Ripple", meta = (ClampMin = "0.01", Units = "s"))
	float RippleDuration = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Transitions", meta = (ClampMin = "0.01", Units = "s"))
	float CollapseDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Transitions", meta = (ClampMin = "0.01", Units = "s"))
	float ReformationDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Transitions", meta = (ClampMin = "0.0"))
	float CollapseIntensity = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Transitions", meta = (ClampMin = "0.0"))
	float ReformationIntensity = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Transitions", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollapseFlickerIntensity = 0.85f;

	/** Visual shell only; shares the shield MID and never contributes collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<UStaticMeshComponent> ReformationPulseMesh;

	UPROPERTY(EditAnywhere, Category = "Shield|Debug")
	bool bDrawImpacts = true;

	UPROPERTY(EditAnywhere, Category = "Shield|Debug", meta = (ClampMin = "0.1", Units = "s"))
	float DebugLifetime = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Shield|Debug", meta = (ClampMin = "1.0", Units = "cm"))
	float DebugNormalLength = 100.0f;

private:
	void InitializeShieldMaterial();
	void UpdateMaterialParameters();
	void UpdateRippleMaterialParameters(int32 SlotIndex);
	void StartImpactRipple(const FVector& WorldImpactPoint);
	void ScheduleRegeneration();
	void BeginRegeneration();
	void SetShieldActive(bool bActive);
	void StartVisualTransition(bool bReforming);
	void UpdateVisualTransition();

	enum class EVisualTransition : uint8 { None, Collapsing, Reforming };
	EVisualTransition VisualTransition = EVisualTransition::None;
	double TransitionStartTime = 0.0;
	float TransitionElapsed = 0.0f;
	float TransitionProgress = 1.0f;
	float TransitionStartVisibility = 1.0f;
	float ShieldVisualVisibility = 1.0f;

	FTimerHandle RegenerationTimerHandle;
	double LastRegenerationUpdateTime = 0.0;
	float VisualEnergyNormalized = 1.0f;
	ECollisionEnabled::Type ActiveCollisionMode = ECollisionEnabled::QueryOnly;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldDynamicMaterial;

	static constexpr int32 MaxImpactRipples = 4;

	struct FImpactRipple
	{
		FVector LocalImpact = FVector::ZeroVector;
		float Elapsed = 0.0f;
		float Radius = 0.0f;
		float Progress = 0.0f;
		uint64 ImpactOrder = 0;
		bool bActive = false;
	};

	FImpactRipple RippleSlots[MaxImpactRipples];
	uint64 NextImpactOrder = 0;
};
