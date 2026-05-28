#pragma once
#include "State.h"
#include <memory>

class ASteeringAgent;
class AGameAIController;
class Arrive;
class Wander;

namespace GameAI::FSM
{
	class SearchState : public State
	{
	public:
		SearchState(ASteeringAgent* InAgent, AGameAIController* InController);

		void OnEnter()  override;
		void OnExit()   override;
		void Update(float DeltaTime) override;

	private:
		ASteeringAgent*           Agent{nullptr};
		AGameAIController*        Controller{nullptr};
		std::unique_ptr<Arrive>   ArriveBehavior;
		std::unique_ptr<Wander>   WanderBehavior;
		bool                      bArrivedAtLastKnown{false};
		float                     ArrivalRadius{150.f};
	};
}