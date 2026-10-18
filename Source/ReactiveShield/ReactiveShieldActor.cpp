#include "ReactiveShieldActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ReactiveShield.h"
#include "ShieldTestProjectile.h"
#include "TimerManager.h"
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
	ShieldMesh->SetDefaultCustomPrimitiveDataFloat(0, 0.0f);

	ReformationPulseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReformationPulseMesh"));
	ReformationPulseMesh->SetupAttachment(ShieldCollision);
	ReformationPulseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReformationPulseMesh->SetGenerateOverlapEvents(false);
	ReformationPulseMesh->SetCanEverAffectNavigation(false);
	ReformationPulseMesh->SetCastShadow(false);
	ReformationPulseMesh->SetDefaultCustomPrimitiveDataFloat(0, 1.0f);
	ReformationPulseMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ShieldMesh->SetStaticMesh(SphereMesh.Object);
		ReformationPulseMesh->SetStaticMesh(SphereMesh.Object);
	}
	ShieldMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f));
	ReformationPulseMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnergyMaterial(
		TEXT("/Game/Materials/M_ReactiveShield.M_ReactiveShield"));
	if (EnergyMaterial.Succeeded())
	{
		ShieldMaterial = EnergyMaterial.Object;
		ShieldMesh->SetMaterial(0, ShieldMaterial);
	}
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ImpactSystem(
		TEXT("/Game/VFX/NS_ShieldImpact.NS_ShieldImpact"));
	if (ImpactSystem.Succeeded())
	{
		ImpactNiagaraSystem = ImpactSystem.Object;
	}
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> CollapseSystem(
		TEXT("/Game/VFX/NS_ShieldCollapse.NS_ShieldCollapse"));
	if (CollapseSystem.Succeeded())
	{
		CollapseNiagaraSystem = CollapseSystem.Object;
	}
}

void AReactiveShieldActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	MaxEnergy = FMath::Max(1.0f, MaxEnergy);
	CurrentEnergy = HasActorBegunPlay() ? FMath::Clamp(CurrentEnergy, 0.0f, MaxEnergy) : MaxEnergy;
	ShieldRadius = FMath::Max(1.0f, ShieldRadius);
	ShieldCollision->SetSphereRadius(ShieldRadius);
	// The engine's basic sphere has a radius of 50 cm.
	ShieldMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f));
	const float PulseScale = VisualTransition == EVisualTransition::Reforming
		? FMath::Lerp(0.05f, 1.3f, TransitionProgress) : 1.0f;
	ReformationPulseMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f * PulseScale));
	if (HasActorBegunPlay() && ShieldDynamicMaterial && ShieldDynamicMaterial->Parent == ShieldMaterial)
	{
		// Moving the actor with simulation gizmos must not restart its current ripple.
		ShieldMesh->SetMaterial(0, ShieldDynamicMaterial);
		ReformationPulseMesh->SetMaterial(0, ShieldDynamicMaterial);
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
	MaxEnergy = FMath::Max(1.0f, MaxEnergy);
	CurrentEnergy = MaxEnergy;
	bShieldActive = true;
	bRegenerating = false;
	VisualTransition = EVisualTransition::None;
	ShieldVisualVisibility = 1.0f;
	TransitionProgress = 1.0f;
	TransitionElapsed = 0.0f;
	ActiveCollisionMode = ShieldCollision->GetCollisionEnabled();
	InitializeShieldMaterial();
	SetActorTickEnabled(false);
	if (!ShieldDynamicMaterial)
	{
		UE_LOG(LogReactiveShield, Warning, TEXT("Shield %s needs M_ReactiveShield assigned to ShieldMaterial for energy ripples."), *GetName());
	}
}

void AReactiveShieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RegenerationTimerHandle);
	Super::EndPlay(EndPlayReason);
}

float AReactiveShieldActor::GetEnergyNormalized() const
{
	return FMath::Clamp(CurrentEnergy / FMath::Max(1.0f, MaxEnergy), 0.0f, 1.0f);
}

void AReactiveShieldActor::SetShieldActive(bool bActive)
{
	if (bShieldActive == bActive)
	{
		return;
	}
	bShieldActive = bActive;
	ShieldCollision->SetCollisionEnabled(bShieldActive ? ActiveCollisionMode : ECollisionEnabled::NoCollision);
	StartVisualTransition(bShieldActive);
	if (!bShieldActive)
	{
		// This guarded state transition fires once per depletion, not once per frame.
		SpawnNiagaraBurst(CollapseNiagaraSystem, ShieldCollision->GetComponentLocation(), FRotator::ZeroRotator,
			FVector::UpVector, CollapseParticleColor, CollapseParticleIntensity, CollapseParticleLifetime, 80);
	}
	UE_LOG(LogReactiveShield, Log, TEXT("Shield %s %s: Energy=%.2f/%.2f"),
		*GetName(), bShieldActive ? TEXT("reactivated") : TEXT("depleted"), CurrentEnergy, MaxEnergy);
}

void AReactiveShieldActor::SpawnNiagaraBurst(UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation,
	const FVector& Normal, const FLinearColor& Color, float Intensity, float Lifetime, int32 BaseCount)
{
	if (!System || Intensity <= 0.0f || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float SafeLifetime = FMath::Clamp(Lifetime, 0.05f, 5.0f);
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, Rotation,
		FVector::OneVector, true, false, ENCPoolMethod::None, true);
	if (!Effect)
	{
		return;
	}

	// Set overrides BEFORE activation so the time-zero burst uses this hit's values.
	Effect->SetVariableVec3(TEXT("User.ImpactNormal"), Normal);
	Effect->SetVariableLinearColor(TEXT("User.Color"),
		FLinearColor(Color.R * Intensity * 8.0f, Color.G * Intensity * 8.0f, Color.B * Intensity * 8.0f, Color.A));
	Effect->SetVariableFloat(TEXT("User.Lifetime"), SafeLifetime);
	Effect->SetVariableInt(TEXT("User.BurstCount"), FMath::RoundToInt(FMath::Clamp(BaseCount * Intensity, 1.0f, 256.0f)));
	const float WorldRadius = ShieldCollision->GetScaledSphereRadius();
	Effect->SetVariableFloat(TEXT("User.ShieldRadius"), WorldRadius);
	Effect->SetVariableFloat(TEXT("User.ShockwaveStartScale"), WorldRadius / 50.0f);
	Effect->SetVariableFloat(TEXT("User.ShockwaveEndScale"), WorldRadius / 50.0f * 2.0f);
	Effect->Activate(true);

	// Finite emitters auto-destroy on completion. Bound even a user-assigned looping asset.
	// No pooling: an old cleanup timer can never destroy a component reused by another hit.
	FTimerHandle CleanupHandle;
	GetWorldTimerManager().SetTimer(CleanupHandle, FTimerDelegate::CreateWeakLambda(Effect,
		[Effect]() { Effect->DestroyComponent(); }), SafeLifetime + 0.25f, false);
}

void AReactiveShieldActor::StartVisualTransition(bool bReforming)
{
	// Interrupt rather than queue transitions; retain the currently displayed opacity.
	TransitionStartVisibility = ShieldVisualVisibility;
	VisualTransition = bReforming ? EVisualTransition::Reforming : EVisualTransition::Collapsing;
	TransitionStartTime = GetWorld()->GetTimeSeconds();
	TransitionElapsed = 0.0f;
	TransitionProgress = 0.0f;
	ShieldMesh->SetVisibility(true);
	ReformationPulseMesh->SetVisibility(bReforming);
	if (bReforming)
	{
		ReformationPulseMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f * 0.05f));
	}
	UpdateMaterialParameters();
	SetActorTickEnabled(true);
}

void AReactiveShieldActor::UpdateVisualTransition()
{
	if (VisualTransition == EVisualTransition::None)
	{
		return;
	}
	const bool bReforming = VisualTransition == EVisualTransition::Reforming;
	TransitionElapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - TransitionStartTime);
	const float Duration = FMath::Max(0.01f, bReforming ? ReformationDuration : CollapseDuration);
	TransitionProgress = FMath::Clamp(TransitionElapsed / Duration, 0.0f, 1.0f);
	const float SmoothProgress = TransitionProgress * TransitionProgress * (3.0f - 2.0f * TransitionProgress);
	ShieldVisualVisibility = FMath::Lerp(TransitionStartVisibility, bReforming ? 1.0f : 0.0f, SmoothProgress);
	if (bReforming)
	{
		// Only this collision-free visual shell expands; the root collision sphere never scales.
		ReformationPulseMesh->SetRelativeScale3D(FVector(ShieldRadius / 50.0f
			* FMath::Lerp(0.05f, 1.3f, TransitionProgress)));
	}
	if (TransitionProgress >= 1.0f)
	{
		VisualTransition = EVisualTransition::None;
		ShieldVisualVisibility = bShieldActive ? 1.0f : 0.0f;
		ShieldMesh->SetVisibility(bShieldActive);
		ReformationPulseMesh->SetVisibility(false);
	}
}

void AReactiveShieldActor::ScheduleRegeneration()
{
	GetWorldTimerManager().ClearTimer(RegenerationTimerHandle);
	bRegenerating = false;
	if (CurrentEnergy < MaxEnergy && RegenerationRate > 0.0f)
	{
		if (RegenerationDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(RegenerationTimerHandle, this,
				&AReactiveShieldActor::BeginRegeneration, RegenerationDelay, false);
		}
		else
		{
			BeginRegeneration();
		}
	}
}

void AReactiveShieldActor::BeginRegeneration()
{
	if (CurrentEnergy < MaxEnergy && RegenerationRate > 0.0f)
	{
		bRegenerating = true;
		LastRegenerationUpdateTime = GetWorld()->GetTimeSeconds();
		SetActorTickEnabled(true);
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
	VisualEnergyNormalized = GetEnergyNormalized();
	SetActorTickEnabled(bRegenerating || VisualTransition != EVisualTransition::None);
	ShieldMesh->SetMaterial(0, ShieldMaterial);
	ReformationPulseMesh->SetMaterial(0, ShieldMaterial);
	ShieldMesh->SetCustomPrimitiveDataFloat(0, 0.0f);
	ReformationPulseMesh->SetCustomPrimitiveDataFloat(0, 1.0f);
	ShieldMesh->SetVisibility(bShieldActive || VisualTransition != EVisualTransition::None);
	ReformationPulseMesh->SetVisibility(VisualTransition == EVisualTransition::Reforming);
	if (ShieldMaterial)
	{
		ShieldDynamicMaterial = UMaterialInstanceDynamic::Create(ShieldMaterial, this);
		ShieldMesh->SetMaterial(0, ShieldDynamicMaterial);
		ReformationPulseMesh->SetMaterial(0, ShieldDynamicMaterial);
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
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("EnergyNormalized"), VisualEnergyNormalized);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("RippleWidth"), FMath::Max(0.1f, RippleWidth));
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("ShieldVisibility"), ShieldVisualVisibility);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("TransitionPhase"),
			VisualTransition == EVisualTransition::Collapsing ? 1.0f : (VisualTransition == EVisualTransition::Reforming ? 2.0f : 0.0f));
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("TransitionProgress"), TransitionProgress);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("TransitionElapsed"), TransitionElapsed);
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("CollapseIntensity"), FMath::Max(0.0f, CollapseIntensity));
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("ReformationIntensity"), FMath::Max(0.0f, ReformationIntensity));
		ShieldDynamicMaterial->SetScalarParameterValue(TEXT("CollapseFlickerIntensity"), FMath::Clamp(CollapseFlickerIntensity, 0.0f, 1.0f));
	}
}

void AReactiveShieldActor::UpdateRippleMaterialParameters(int32 SlotIndex)
{
	if (!ShieldDynamicMaterial)
	{
		return;
	}
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
	if (bRegenerating)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		// Do not credit time spent waiting for the regeneration timer on its first frame.
		const float RegenerationDelta = static_cast<float>(FMath::Max(0.0, Now - LastRegenerationUpdateTime));
		LastRegenerationUpdateTime = Now;
		CurrentEnergy = FMath::Min(MaxEnergy, CurrentEnergy + FMath::Max(0.0f, RegenerationRate) * RegenerationDelta);
		if (!bShieldActive && CurrentEnergy >= MaxEnergy * 0.25f)
		{
			SetShieldActive(true);
		}
		bRegenerating = CurrentEnergy < MaxEnergy && RegenerationRate > 0.0f;
	}

	const float TargetEnergy = GetEnergyNormalized();
	UpdateVisualTransition();
	VisualEnergyNormalized = FMath::FInterpTo(VisualEnergyNormalized, TargetEnergy, DeltaSeconds, 8.0f);
	if (FMath::IsNearlyEqual(VisualEnergyNormalized, TargetEnergy, 0.001f))
	{
		VisualEnergyNormalized = TargetEnergy;
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
	const bool bEnergyVisualChanging = ShieldDynamicMaterial && VisualEnergyNormalized != TargetEnergy;
	SetActorTickEnabled(bAnyActive || bRegenerating || bEnergyVisualChanging || VisualTransition != EVisualTransition::None);
}

void AReactiveShieldActor::HandleProjectileImpact(AShieldTestProjectile* Projectile, const FHitResult& Hit)
{
	if (!bShieldActive || !IsValid(Projectile) || !Hit.bBlockingHit || Hit.GetComponent() != ShieldCollision.Get())
	{
		return;
	}

	CurrentEnergy = FMath::Clamp(CurrentEnergy - FMath::Max(0.0f, DamagePerHit), 0.0f, MaxEnergy);
	if (CurrentEnergy <= 0.0f)
	{
		SetShieldActive(false);
	}
	ScheduleRegeneration();
	// Energy still regenerates when the visual material is unavailable.
	SetActorTickEnabled(true);
	UE_LOG(LogReactiveShield, Log, TEXT("Shield %s impact energy: %.2f/%.2f"), *GetName(), CurrentEnergy, MaxEnergy);

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
		SpawnNiagaraBurst(ImpactNiagaraSystem, ImpactPoint, FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator(),
			SurfaceNormal, ImpactParticleColor, ImpactParticleIntensity, ImpactParticleLifetime, 32);
	}

	if (bDrawImpacts)
	{
		DrawDebugSphere(GetWorld(), ImpactPoint, 8.0f, 12, FColor::Red, false, DebugLifetime, 0, 2.0f);
		DrawDebugDirectionalArrow(GetWorld(), ImpactPoint, ImpactPoint + SurfaceNormal * DebugNormalLength,
			15.0f, FColor::Green, false, DebugLifetime, 0, 2.0f);
	}
}
