#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "Shared/Level_Base.h"
#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "Level_FSM.generated.h"

UCLASS()
class GAMEAIPROG_API ALevel_FSM : public ALevel_Base
{
	GENERATED_BODY()

public:
	ALevel_FSM();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	ASteeringAgent* Thief{nullptr};
	std::unique_ptr<Seek> ThiefBehavior;

	UPROPERTY()
	ASteeringAgent* Guard{nullptr};

	UPROPERTY(EditAnywhere, Category="AI|Patrol")
	TArray<FVector2D> PatrolPoints;

	UPROPERTY(EditAnywhere, Category="AI|Search")
	float SearchDuration{5.f};
};