#pragma once

#include "State.h"
#include <memory>

class ASteeringAgent;
class AGameAIController;
class Arrive;

namespace GameAI::FSM
{
	class PatrolState : public State
	{
	public:
		PatrolState(ASteeringAgent* InAgent, AGameAIController* InController,
					TArray<FVector2D> InPatrolPoints);

		void OnEnter()  override;
		void OnExit()   override;
		void Update(float DeltaTime) override;

	private:
		ASteeringAgent*          Agent{nullptr};
		AGameAIController*       Controller{nullptr};
		TArray<FVector2D>        PatrolPoints;
		int32                    PatrolIndex{0};
		std::unique_ptr<Arrive>  ArriveBehavior;
	};
}