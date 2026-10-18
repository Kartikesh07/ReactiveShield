#include "ReactiveShieldActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ReactiveShield.h"
#include "ShieldTestProjectile.h"
#include "UObject/ConstructorHelpers.h"

AReactiveShieldActor::AReactiveShieldActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnergyMaterial(
		TEXT("/Game/Materials/M_ReactiveShield.M_ReactiveShield"));
	if (EnergyMaterial.Succeeded())
	{
		ShieldMaterial = EnergyMaterial.Object;
		ShieldMesh->SetMaterial(0, ShieldMaterial);
	}
}

void AReactiveShieldActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ShieldRadius = FMath::Max(1.0f, ShieldRadius);
	ShieldCollision->SetSphereRadius(ShieldRadius);
	// The engine's basic sphere has a radius of 50 cm.
	ShieldMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f));
	if (HasActorBegunPlay() && ShieldDynamicMaterial && ShieldDynamicMaterial->Parent == ShieldMaterial)
	{
		// Moving the actor with simulation gizmos must not restart its current ripple.
		ShieldMesh->SetMaterial(0, ShieldDynamicMaterial);
		UpdateMaterialParameters();
	}
	else
	{
		InitializeShieldMaterial();
	}
}

void AReactiveShieldActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeShieldMaterial();
	SetActorTickEnabled(false);
	if (!ShieldDynamicMaterial)
	{
		UE_LOG(LogReactiveShield, Warning, TEXT("Shield %s needs M_ReactiveShield assigned to ShieldMaterial for energy ripples."), *GetName());
	}
}

void AReactiveShieldActor::InitializeShieldMaterial()
{
	ShieldDynamicMaterial = nullptr;
	RippleElapsed = 0.0f;
	ShieldMesh->SetMaterial(0, ShieldMaterial);
	if (ShieldMaterial)
	{
		ShieldDynamicMaterial = UMaterialInstanceDynamic::Create(ShieldMaterial, this);
		ShieldMesh->SetMaterial(0, ShieldDynamicMaterial);
		UpdateMaterialParameters();
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleActive"), 0.0f);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleProgress"), 0.0f);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleRadius"), 0.0f);
	}
}

void AReactiveShieldActor::UpdateMaterialParameters()
{
	if (ShieldDynamicMaterial)
	{
		ShieldDynamicMaterial->SetVectorParameterValue(TEXT("ShieldColor"), ShieldColor);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("ShieldRadius"), ShieldRadius);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("GlowIntensity"), FMath::Max(0.0f, GlowIntensity));
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleWidth"), FMath::Max(0.1f, RippleWidth));
	}
}

void AReactiveShieldActor::StartImpactRipple(const FVector& WorldImpactPoint)
{
	if (!ShieldDynamicMaterial)
	{
		return;
	}

	// Match the material's Absolute World -> Local transform, including mesh radius scale.
	// A new hit replaces the previous ripple; no world-space position is retained.
	const FVector LocalImpact = ShieldMesh->GetComponentTransform().InverseTransformPosition(WorldImpactPoint);
	ShieldDynamicMaterial->SetVectorParameterValue(TEXT("ImpactPositionLocal"),
		FLinearColor(LocalImpact.X, LocalImpact.Y, LocalImpact.Z, 0.0f));
	RippleElapsed = 0.0f;
	UpdateMaterialParameters();
	ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleRadius"), 0.0f);
	ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleProgress"), 0.0f);
	ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleActive"), 1.0f);
	SetActorTickEnabled(true);
}

void AReactiveShieldActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ShieldDynamicMaterial)
	{
		SetActorTickEnabled(false);
		return;
	}

	RippleElapsed += DeltaSeconds;
	const float Progress = FMath::Clamp(RippleElapsed / FMath::Max(0.01f, RippleDuration), 0.0f, 1.0f);
	UpdateMaterialParameters();
	ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleRadius"), FMath::Max(0.0f, RippleSpeed) * RippleElapsed);
	ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleProgress"), Progress);
	if (Progress >= 1.0f)
	{
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleActive"), 0.0f);
		SetActorTickEnabled(false);
	}
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
	else
	{
		StartImpactRipple(ImpactPoint);
	}

	if (bDrawImpacts)
	{
		DrawDebugSphere(GetWorld(), ImpactPoint, 8.0f, 12, FColor::Red, false, DebugLifetime, 0, 2.0f);
		DrawDebugDirectionalArrow(GetWorld(), ImpactPoint, ImpactPoint + SurfaceNormal * DebugNormalLength,
			15.0f, FColor::Green, false, DebugLifetime, 0, 2.0f);
	}
}
