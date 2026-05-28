#include "FSMComponent.h"

void GameAI::FSM::FSM::AddState(std::unique_ptr<State> NewState)
{
	States.push_back(std::move(NewState));
}

void GameAI::FSM::FSM::AddTransition(State* From, State* To, std::function<bool()> Condition)
{
	Transitions.push_back({From, To, Condition});
}

void GameAI::FSM::FSM::Start()
{
	if (!States.empty())
	{
		CurrentState = States[0].get();
		CurrentState->OnEnter();
	}
}

void GameAI::FSM::FSM::Update(float DeltaTime)
{
	if (!CurrentState) return;

	for (auto& T : Transitions)
	{
		if ((T.From == nullptr || T.From == CurrentState) && T.Condition())
		{
			SwitchTo(T.To);
			return;
		}
	}

	CurrentState->Update(DeltaTime);
}

void GameAI::FSM::FSM::SwitchTo(State* NewState)
{
	CurrentState->OnExit();
	CurrentState = NewState;
	CurrentState->OnEnter();
}

UFSMComponent::UFSMComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	FSMInstance = std::make_unique<GameAI::FSM::FSM>();
}

void UFSMComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType,
								   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsRunning)
	{
		FSMInstance->Update(DeltaTime);
	}
}

void UFSMComponent::StartLogic()
{
	Super::StartLogic();
	FSMInstance->Start();
	bIsRunning = true;
}

void UFSMComponent::StopLogic(const FString& Reason)
{
	if (FSMInstance && FSMInstance->GetCurrentState())
	{
		FSMInstance->GetCurrentState()->OnExit();
	}

	bIsRunning = false;
}

void UFSMComponent::AddState(std::unique_ptr<GameAI::FSM::State>&& NewState)
{
	FSMInstance->AddState(std::move(NewState));
}

void UFSMComponent::AddTransition(GameAI::FSM::State* From, GameAI::FSM::State* To,
								   std::function<bool()> EvalFunc) const
{
	FSMInstance->AddTransition(From, To, EvalFunc);
}

bool UFSMComponent::IsRunning() const
{
	return bIsRunning;
}

