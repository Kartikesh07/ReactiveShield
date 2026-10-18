// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReactiveShieldCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ReactiveShield.h"
#include "ShieldTestProjectile.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

AReactiveShieldCharacter::AReactiveShieldCharacter()
{
	ProjectileClass = AShieldTestProjectile::StaticClass();
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AReactiveShieldCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	StopFiring();
	// Key bindings coexist with the template's Enhanced Input movement/look actions.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AReactiveShieldCharacter::StartFiring);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AReactiveShieldCharacter::StopFiring);
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AReactiveShieldCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AReactiveShieldCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AReactiveShieldCharacter::Look);
	}
	else
	{
		UE_LOG(LogReactiveShield, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AReactiveShieldCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFiring();
	Super::EndPlay(EndPlayReason);
}

void AReactiveShieldCharacter::UnPossessed()
{
	StopFiring();
	Super::UnPossessed();
}

void AReactiveShieldCharacter::StartFiring()
{
	if (!GetWorldTimerManager().IsTimerActive(FireTimerHandle))
	{
		FireProjectile();
	}
}

void AReactiveShieldCharacter::StopFiring()
{
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

void AReactiveShieldCharacter::FireProjectile()
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!InputEnabled() || !InputComponent || !PlayerController
		|| !PlayerController->IsLocalController() || PlayerController->IsPaused()
		|| !PlayerController->IsInputKeyDown(EKeys::LeftMouseButton) || !ProjectileClass
		|| !FMath::IsFinite(FireRate) || FireRate <= 0.0f
		|| !FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed <= 0.0f || SpawnOffset.ContainsNaN())
	{
		StopFiring();
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const float Interval = 1.0f / FMath::Clamp(FireRate, 0.1f, 20.0f);
	if (Now < NextFireTime)
	{
		GetWorldTimerManager().SetTimer(FireTimerHandle, this,
			&AReactiveShieldCharacter::FireProjectile, float(NextFireTime - Now), false);
		return;
	}
	NextFireTime = Now + Interval;
	// One-shot rearming prevents catch-up bursts after slow frames. Cooldown survives release.
	GetWorldTimerManager().SetTimer(FireTimerHandle, this,
		&AReactiveShieldCharacter::FireProjectile, Interval, false);

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + ViewDirection * 100000.0f;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileAim), false, this);
	FHitResult AimHit;
	GetWorld()->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);
	const FVector AimPoint = AimHit.bBlockingHit ? AimHit.ImpactPoint : TraceEnd;
	const FVector SpawnLocation = GetActorLocation() + GetActorRotation().RotateVector(SpawnOffset);

	// Reject obstructed offsets rather than spawning through a nearby wall.
	const USphereComponent* Collision = ProjectileClass.GetDefaultObject()->FindComponentByClass<USphereComponent>();
	const float Radius = Collision ? FMath::Max(1.0f, Collision->GetScaledSphereRadius()) : 10.0f;
	FHitResult SpawnHit;
	if (GetWorld()->SweepSingleByChannel(SpawnHit, GetActorLocation(), SpawnLocation,
		FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), QueryParams))
	{
		return;
	}

	const FVector AimDelta = AimPoint - SpawnLocation;
	// A camera hit behind the muzzle must not send a projectile back through the player.
	const FVector FireDirection = FVector::DotProduct(AimDelta, ViewDirection) > 0.0f
		? AimDelta.GetSafeNormal() : ViewDirection;
	const FTransform SpawnTransform(FireDirection.Rotation(), SpawnLocation);
	AShieldTestProjectile* Projectile = GetWorld()->SpawnActorDeferred<AShieldTestProjectile>(
		ProjectileClass, SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	if (Projectile)
	{
		Projectile->ConfigureLaunch(ProjectileSpeed);
		Projectile->FinishSpawning(SpawnTransform);
	}
}

void AReactiveShieldCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AReactiveShieldCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AReactiveShieldCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AReactiveShieldCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AReactiveShieldCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AReactiveShieldCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
