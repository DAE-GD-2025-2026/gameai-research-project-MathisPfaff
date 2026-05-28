#pragma once

namespace GameAI::FSM
{
	class State
	{
	public:
		virtual ~State() = default;

		virtual void OnEnter()  {}
		virtual void OnExit()   {}
		virtual void Update(float DeltaTime) {}
	};
}