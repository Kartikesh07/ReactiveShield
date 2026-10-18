// Copyright Epic Games, Inc. All Rights Reserved.


#include "ReactiveShieldPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "ReactiveShield.h"
#include "ReactiveShieldHUDWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AReactiveShieldPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController() && !ShieldHUD)
	{
		ShieldHUD = CreateWidget<UReactiveShieldHUDWidget>(this, UReactiveShieldHUDWidget::StaticClass());
		if (ShieldHUD)
		{
			ShieldHUD->ShieldActorTag = HUDShieldActorTag;
			ShieldHUD->AddToPlayerScreen(1);
		}
	}

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogReactiveShield, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AReactiveShieldPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ShieldHUD)
	{
		ShieldHUD->RemoveFromParent();
		ShieldHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AReactiveShieldPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AReactiveShieldPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
