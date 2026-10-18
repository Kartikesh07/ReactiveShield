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

namespace
{
	struct FRippleMaterialParameters
	{
		FName Position;
		FName Radius;
		FName Progress;
		FName Active;
	};

	// Slot zero retains the original parameter names and single-impact appearance.
	const FRippleMaterialParameters RippleParameters[] =
	{
		{TEXT("ImpactPositionLocal"), TEXT("RippleRadius"), TEXT("RippleProgress"), TEXT("RippleActive")},
		{TEXT("ImpactPositionLocal1"), TEXT("RippleRadius1"), TEXT("RippleProgress1"), TEXT("RippleActive1")},
		{TEXT("ImpactPositionLocal2"), TEXT("RippleRadius2"), TEXT("RippleProgress2"), TEXT("RippleActive2")},
		{TEXT("ImpactPositionLocal3"), TEXT("RippleRadius3"), TEXT("RippleProgress3"), TEXT("RippleActive3")}
	};
}

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
	NextImpactOrder = 0;
	for (FImpactRipple& Ripple : RippleSlots)
	{
		Ripple = FImpactRipple();
	}
	SetActorTickEnabled(false);
	ShieldMesh->SetMaterial(0, ShieldMaterial);
	if (ShieldMaterial)
	{
		ShieldDynamicMaterial = UMaterialInstanceDynamic::Create(ShieldMaterial, this);
		ShieldMesh->SetMaterial(0, ShieldDynamicMaterial);
		UpdateMaterialParameters();
		static_assert(UE_ARRAY_COUNT(RippleParameters) == MaxImpactRipples);
		for (int32 Index = 0; Index < MaxImpactRipples; ++Index)
		{
			UpdateRippleMaterialParameters(Index);
		}
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

void AReactiveShieldActor::UpdateRippleMaterialParameters(int32 SlotIndex)
{
	const FImpactRipple& Ripple = RippleSlots[SlotIndex];
	const FRippleMaterialParameters& Parameters = RippleParameters[SlotIndex];
	ShieldDynamicMaterial->SetVectorParameterValue(Parameters.Position,
		FLinearColor(Ripple.LocalImpact.X, Ripple.LocalImpact.Y, Ripple.LocalImpact.Z, 0.0f));
	ShieldDynamicMaterial->SetScalarParameterValue(Parameters.Radius, Ripple.Radius);
	ShieldDynamicMaterial->SetScalarParameterValue(Parameters.Progress, Ripple.Progress);
	ShieldDynamicMaterial->SetScalarParameterValue(Parameters.Active, Ripple.bActive ? 1.0f : 0.0f);
}

void AReactiveShieldActor::StartImpactRipple(const FVector& WorldImpactPoint)
{
	if (!ShieldDynamicMaterial)
	{
		return;
	}

	int32 SlotIndex = 0;
	for (int32 Index = 0; Index < MaxImpactRipples; ++Index)
	{
		if (!RippleSlots[Index].bActive)
		{
			SlotIndex = Index;
			break;
		}
		if (RippleSlots[Index].ImpactOrder < RippleSlots[SlotIndex].ImpactOrder)
		{
			SlotIndex = Index;
		}
	}

	FImpactRipple& Ripple = RippleSlots[SlotIndex];
	Ripple = FImpactRipple();
	// Match the material's Absolute World -> Local transform, including mesh radius scale.
	Ripple.LocalImpact = ShieldMesh->GetComponentTransform().InverseTransformPosition(WorldImpactPoint);
	// Sequence order also distinguishes hits received in the same frame.
	Ripple.ImpactOrder = NextImpactOrder++;
	Ripple.bActive = true;
	UpdateMaterialParameters();
	UpdateRippleMaterialParameters(SlotIndex);
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

	UpdateMaterialParameters();
	bool bAnyActive = false;
	for (int32 Index = 0; Index < MaxImpactRipples; ++Index)
	{
		FImpactRipple& Ripple = RippleSlots[Index];
		if (!Ripple.bActive)
		{
			continue;
		}
		Ripple.Elapsed += DeltaSeconds;
		Ripple.Radius = FMath::Max(0.0f, RippleSpeed) * Ripple.Elapsed;
		Ripple.Progress = FMath::Clamp(Ripple.Elapsed / FMath::Max(0.01f, RippleDuration), 0.0f, 1.0f);
		Ripple.bActive = Ripple.Progress < 1.0f;
		UpdateRippleMaterialParameters(Index);
		bAnyActive |= Ripple.bActive;
	}
	if (!bAnyActive)
	{
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
