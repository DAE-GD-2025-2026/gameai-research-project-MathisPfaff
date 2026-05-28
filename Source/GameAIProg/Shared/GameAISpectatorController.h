// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "GameAISpectatorController.generated.h"

class AGameAISpectator;
/**
 * 
 */
UCLASS()
class GAMEAIPROG_API AGameAISpectatorController : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameAIController|Input")	
	TArray<UInputMappingContext*> DefaultInputMappingContexts{};

	UPROPERTY(Editanywhere, BlueprintReadWrite, Category="GameAIController|Pawn")
	AGameAISpectator* GameAISpectator{nullptr};
	
	virtual void BeginPlay() override;

	void AddInputMappingContexts(TArray<UInputMappingContext*>& InputMappingContexts);
	void RemoveInputMappingContexts(TArray<UInputMappingContext*>& InputMappingContexts);

protected:
	virtual void SetupInputComponent() override;

	virtual void OnPossess(APawn* InPawn) override;
};
