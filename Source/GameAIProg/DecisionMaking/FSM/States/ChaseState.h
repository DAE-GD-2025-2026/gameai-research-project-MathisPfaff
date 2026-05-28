#pragma once
#include "State.h"
#include <memory>

class ASteeringAgent;
class AGameAIController;
class Pursuit;

namespace GameAI::FSM
{
	class ChaseState : public State
	{
	public:
		ChaseState(ASteeringAgent* InAgent, AGameAIController* InController);

		void OnEnter()  override;
		void OnExit()   override;
		void Update(float DeltaTime) override;

	private:
		ASteeringAgent*           Agent{nullptr};
		AGameAIController*        Controller{nullptr};
		std::unique_ptr<Pursuit>  PursuitBehavior;
	};
}