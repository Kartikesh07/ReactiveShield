#include "ReactiveShieldActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "ReactiveShield.h"
#include "ShieldTestProjectile.h"
#include "UObject/ConstructorHelpers.h"

AReactiveShieldActor::AReactiveShieldActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ShieldCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ShieldCollision"));
	SetRootComponent(ShieldCollision);
	ShieldCollision->InitSphereRadius(ShieldRadius);
	ShieldCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShieldCollision->SetCollisionObjectType(ECC_WorldDynamic);
	ShieldCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShieldCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	ShieldCollision->SetGenerateOverlapEvents(false);
	ShieldCollision->SetCanEverAffectNavigation(false);

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(ShieldCollision);
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldMesh->SetGenerateOverlapEvents(false);
	ShieldMesh->SetCanEverAffectNavigation(false);
	ShieldMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ShieldMesh->SetStaticMesh(SphereMesh.Object);
	}
	ShieldMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f));
}

void AReactiveShieldActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ShieldRadius = FMath::Max(1.0f, ShieldRadius);
	ShieldCollision->SetSphereRadius(ShieldRadius);
	// The engine's basic sphere has a radius of 50 cm.
	ShieldMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f));
}

void AReactiveShieldActor::HandleProjectileImpact(AShieldTestProjectile* Projectile, const FHitResult& Hit)
{
	if (!IsValid(Projectile) || !Hit.bBlockingHit || Hit.GetComponent() != ShieldCollision.Get())
	{
		return;
	}

	// ImpactPoint is the surface contact; Location is the swept projectile's center.
	const FVector ImpactPoint = Hit.ImpactPoint;
	const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
	UE_LOG(LogReactiveShield, Log,
		TEXT("Shield %s hit by %s: ImpactPoint=%s ImpactNormal=%s SweepTime=%.4f InitialPenetration=%s"),
		*GetName(), *GetNameSafe(Projectile), *ImpactPoint.ToString(), *SurfaceNormal.ToString(),
		Hit.Time, Hit.bStartPenetrating ? TEXT("true") : TEXT("false"));

	if (Hit.bStartPenetrating)
	{
		UE_LOG(LogReactiveShield, Warning, TEXT("Projectile started inside the shield; initial penetration has no unique surface contact. Spawn outside the boundary."));
	}

	if (bDrawImpacts)
	{
		DrawDebugSphere(GetWorld(), ImpactPoint, 8.0f, 12, FColor::Red, false, DebugLifetime, 0, 2.0f);
		DrawDebugDirectionalArrow(GetWorld(), ImpactPoint, ImpactPoint + SurfaceNormal * DebugNormalLength,
			15.0f, FColor::Green, false, DebugLifetime, 0, 2.0f);
	}
}
