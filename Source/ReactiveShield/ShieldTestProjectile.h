#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldTestProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** Launches once at BeginPlay. Local direction is transformed by the actor rotation. */
UCLASS()
class REACTIVESHIELD_API AShieldTestProjectile : public AActor
{
	GENERATED_BODY()

public:
	AShieldTestProjectile();

	/** Set before FinishSpawning so BeginPlay launches along the aimed rotation. */
	void ConfigureLaunch(float InSpeed)
	{
		Speed = InSpeed;
		LocalDirection = FVector::ForwardVector;
	}

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> ProjectileCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", Units = "cm/s"))
	float Speed = 2000.0f;

	/** Normalized at launch; zero falls back to local +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector LocalDirection = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	bool bDestroyOnImpact = true;

private:
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	bool bImpactHandled = false;
};
