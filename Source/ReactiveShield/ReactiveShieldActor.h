#pragma once

#include "CoreMinimal.h"
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

protected:
	virtual void BeginPlay() override;

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
