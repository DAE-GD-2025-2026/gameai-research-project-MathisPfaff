#include "PatrolState.h"
#include "Movement/SteeringBehaviors/SteeringAgent.h"
#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"

namespace GameAI::FSM
{
	PatrolState::PatrolState(ASteeringAgent* InAgent, AGameAIController* InController,
							 TArray<FVector2D> InPatrolPoints)
		: Agent(InAgent), Controller(InController), PatrolPoints(InPatrolPoints)
	{
		ArriveBehavior = std::make_unique<Arrive>();
	}

	void PatrolState::OnEnter()
	{
		if (!Agent || PatrolPoints.IsEmpty()) return;

		Agent->SetSteeringBehavior(ArriveBehavior.get());
		ArriveBehavior->SetTarget(FTargetData{PatrolPoints[PatrolIndex]});
	}

	void PatrolState::OnExit()
	{
		Agent->SetSteeringBehavior(nullptr);
	}

	void PatrolState::Update(float DeltaTime)
	{
		if (!Agent || PatrolPoints.IsEmpty()) return;

		FVector2D ToTarget = PatrolPoints[PatrolIndex] - Agent->GetPosition();
		if (ToTarget.Length() < 100.f)
		{
			PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
			ArriveBehavior->SetTarget(FTargetData{PatrolPoints[PatrolIndex]});
		}
	}
}