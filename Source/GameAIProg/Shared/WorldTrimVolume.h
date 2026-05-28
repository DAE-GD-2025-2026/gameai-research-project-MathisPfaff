// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "WorldTrimVolume.generated.h"

/*
 * Keeps actors within it's bounds, used by Level_Base
 *
 * Known issue: if disabled, actors leave then re-enabled actors will not be back in the trim zone
 */

UCLASS()
class GAMEAIPROG_API AWorldTrimVolume : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsWorldLooping{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShouldTrimWorld{true};
	
	// Sets default values for this actor's properties
	AWorldTrimVolume();
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void SetTrimWorldSize(float NewSize);
	float GetTrimWorldSize() const { return TrimWorldSize; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UBoxComponent* TrimVolume{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TrimWorldSize{1000.f};
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
	
};
