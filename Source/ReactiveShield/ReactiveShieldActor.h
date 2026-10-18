#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReactiveShieldActor.generated.h"

class AShieldTestProjectile;
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

	/** Hit must come from the projectile's blocking sweep against this shield. */
	void HandleProjectileImpact(AShieldTestProjectile* Projectile, const FHitResult& Hit);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<USphereComponent> ShieldCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	/** Unscaled radius in centimeters. Use uniform actor scale for a spherical boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "1.0", Units = "cm"))
	float ShieldRadius = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Shield|Debug")
	bool bDrawImpacts = true;

	UPROPERTY(EditAnywhere, Category = "Shield|Debug", meta = (ClampMin = "0.1", Units = "s"))
	float DebugLifetime = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Shield|Debug", meta = (ClampMin = "1.0", Units = "cm"))
	float DebugNormalLength = 100.0f;
};
