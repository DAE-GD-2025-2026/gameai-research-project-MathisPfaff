
#include "CombinedSteeringBehaviors.h"
#include <algorithm>
#include "../SteeringAgent.h"

BlendedSteering::BlendedSteering(const std::vector<WeightedBehavior>& WeightedBehaviors)
	:WeightedBehaviors(WeightedBehaviors)
{};

//****************
//BLENDED STEERING
SteeringOutput BlendedSteering::CalculateSteering(float DeltaT, ASteeringAgent& Agent)
{
	SteeringOutput BlendedSteering = {};
	float TotalWeight = 0.f;
	Agent.SetIsAutoOrienting(true);

	for (auto& WeightedBehavior : WeightedBehaviors)
	{
		if (WeightedBehavior.pBehavior && WeightedBehavior.Weight > 0.f)
		{
			SteeringOutput BehaviorOutput = WeightedBehavior.pBehavior->CalculateSteering(DeltaT, Agent);

			if (!BehaviorOutput.LinearVelocity.IsNearlyZero())
			{
				BehaviorOutput.LinearVelocity.Normalize();
			}

			BlendedSteering.LinearVelocity  += BehaviorOutput.LinearVelocity  * WeightedBehavior.Weight;
			BlendedSteering.AngularVelocity += BehaviorOutput.AngularVelocity * WeightedBehavior.Weight;
			TotalWeight += WeightedBehavior.Weight;
		}
	}

	if (TotalWeight > 0.f)
	{
		BlendedSteering.LinearVelocity *= 1.f / TotalWeight;
	}
	
	// TODO: Add debug drawing

	return BlendedSteering;
}

float* BlendedSteering::GetWeight(ISteeringBehavior* const SteeringBehavior)
{
	auto it = find_if(WeightedBehaviors.begin(),
		WeightedBehaviors.end(),
		[SteeringBehavior](const WeightedBehavior& Elem)
		{
			return Elem.pBehavior == SteeringBehavior;
		}
	);

	if(it!= WeightedBehaviors.end())
		return &it->Weight;
	
	return nullptr;
}

//*****************
//PRIORITY STEERING
SteeringOutput PrioritySteering::CalculateSteering(float DeltaT, ASteeringAgent& Agent)
{
	SteeringOutput Steering = {};

	for (ISteeringBehavior* const pBehavior : m_PriorityBehaviors)
	{
		Steering = pBehavior->CalculateSteering(DeltaT, Agent);

		if (!Steering.LinearVelocity.IsNearlyZero())
		{
			break;
		}
	}

	return Steering;
}