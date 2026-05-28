#pragma once

#include <functional>
#include <memory>

#include "CoreMinimal.h"
#include "BrainComponent.h"
#include "States/State.h"
#include "FSMComponent.generated.h"

namespace GameAI::FSM
{
	struct Transition
	{
		State* From;
		State* To;
		std::function<bool()> Condition;
	};

	class FSM
	{
	public:
		void AddState(std::unique_ptr<State> NewState);
		void AddTransition(State* From, State* To, std::function<bool()> Condition);
		void Start();
		void Update(float DeltaTime);
		State* GetCurrentState() const { return CurrentState; }

	private:
		std::vector<std::unique_ptr<State>> States;
		std::vector<Transition> Transitions;
		State* CurrentState{nullptr};

		void SwitchTo(State* NewState);
	};
}

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEAIPROG_API UFSMComponent : public UBrainComponent
{
	GENERATED_BODY()

public:
	UFSMComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void StartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	
	virtual bool IsRunning() const override; 
	
	void AddState(std::unique_ptr<GameAI::FSM::State>&& NewState);
	void AddTransition(GameAI::FSM::State* From, GameAI::FSM::State* To, std::function<bool()> EvalFunc) const;
		
	virtual void BeginPlay() override;

private:
	std::unique_ptr<GameAI::FSM::FSM> FSMInstance;
	bool bIsRunning{false};
};
