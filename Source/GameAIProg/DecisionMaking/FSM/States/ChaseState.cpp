#include "ChaseState.h"
#include "DecisionMaking/GameAIController.h"
#include "Movement/SteeringBehaviors/SteeringAgent.h"
#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"

namespace GameAI::FSM
{
	ChaseState::ChaseState(ASteeringAgent* InAgent, AGameAIController* InController)
		: Agent(InAgent), Controller(InController)
	{
		PursuitBehavior = std::make_unique<Pursuit>();
	}

	void ChaseState::OnEnter()
	{
		if (!Agent) return;
		Agent->SetSteeringBehavior(PursuitBehavior.get());
	}

	void ChaseState::OnExit()
	{
		Agent->SetSteeringBehavior(nullptr);
	}

	void ChaseState::Update(float DeltaTime)
	{
		if (!Agent || !Controller) return;

		UBlackboardComponent* BB = Controller->GetBlackboardComponent();
		if (!BB) return;

		AActor* Target = Cast<AActor>(BB->GetValueAsObject("TargetActor"));
		if (!Target) return;

		FTargetData TargetData;
		TargetData.Position       = FVector2D(Target->GetActorLocation());
		TargetData.LinearVelocity = FVector2D(Target->GetVelocity());
		PursuitBehavior->SetTarget(TargetData);

		BB->SetValueAsVector("LastKnownLocation", Target->GetActorLocation());
	}
}