// Fill out your copyright notice in the Description page of Project Settings.


#include "GameAISpectatorController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameAISpectator.h"

void AGameAISpectatorController::BeginPlay()
{
	Super::BeginPlay();

	
}

void AGameAISpectatorController::AddInputMappingContexts(TArray<UInputMappingContext*>& InputMappingContexts)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* InputMapping : InputMappingContexts)
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}
}

void AGameAISpectatorController::RemoveInputMappingContexts(TArray<UInputMappingContext*>& InputMappingContexts)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* InputMapping : InputMappingContexts)
		{
			Subsystem->RemoveMappingContext(InputMapping);
		}
	}
}

void AGameAISpectatorController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Setup user defined default mappings
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* InputMapping : DefaultInputMappingContexts)
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}

	// Setup action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Actions set up in BP's
		// Setup input events here if you want to...
	}
	// else
	// {
	// 	UE_LOG(LogGameAIProg, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	// }
}

void AGameAISpectatorController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	GameAISpectator = Cast<AGameAISpectator>(InPawn);
}
