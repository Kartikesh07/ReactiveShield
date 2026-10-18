#include "ShieldTestProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ReactiveShield.h"
#include "ReactiveShieldActor.h"
#include "UObject/ConstructorHelpers.h"

AShieldTestProjectile::AShieldTestProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 10.0f;

	ProjectileCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision"));
	SetRootComponent(ProjectileCollision);
	ProjectileCollision->InitSphereRadius(10.0f);
	ProjectileCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProjectileCollision->SetCollisionObjectType(ECC_PhysicsBody);
	ProjectileCollision->SetCollisionResponseToAllChannels(ECR_Block);
	ProjectileCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ProjectileCollision->SetGenerateOverlapEvents(false);
	ProjectileCollision->SetCanEverAffectNavigation(false);
	ProjectileCollision->OnComponentHit.AddDynamic(this, &AShieldTestProjectile::OnProjectileHit);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(ProjectileCollision);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetGenerateOverlapEvents(false);
	ProjectileMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereMesh.Object);
	}
	ProjectileMesh->SetRelativeScale3D(FVector(0.2f));

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(ProjectileCollision);
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bSweepCollision = true;
}

void AShieldTestProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner())
	{
		ProjectileCollision->IgnoreActorWhenMoving(GetOwner(), true);
	}
	const FVector Direction = LocalDirection.GetSafeNormal();
	ProjectileMovement->InitialSpeed = FMath::Max(0.0f, Speed);
	ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
	ProjectileMovement->Velocity = GetActorRotation().RotateVector(
		Direction.IsNearlyZero() ? FVector::ForwardVector : Direction) * ProjectileMovement->InitialSpeed;
}

void AShieldTestProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bImpactHandled || !Hit.bBlockingHit)
	{
		return;
	}
	bImpactHandled = true;

	if (AReactiveShieldActor* Shield = Cast<AReactiveShieldActor>(OtherActor))
	{
		Shield->HandleProjectileImpact(this, Hit);
	}
	else
	{
		UE_LOG(LogReactiveShield, Log, TEXT("Projectile %s hit %s: ImpactPoint=%s ImpactNormal=%s"),
			*GetName(), *GetNameSafe(OtherActor), *Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString());
	}

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	ProjectileCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bDestroyOnImpact)
	{
		Destroy();
	}
}
