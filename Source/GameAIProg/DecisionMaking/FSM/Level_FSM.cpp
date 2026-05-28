#include "Level_FSM.h"

#include "FSMComponent.h"
#include "DecisionMaking/GameAIController.h"
#include "States/PatrolState.h"
#include "States/ChaseState.h"
#include "States/SearchState.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnhancedInputComponent.h"

ALevel_FSM::ALevel_FSM()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ALevel_FSM::BeginPlay()
{
    Super::BeginPlay();
    
    if (PatrolPoints.IsEmpty())
    {
        PatrolPoints = {
            FVector2D{ 500,    0},
            FVector2D{ 500,  500},
            FVector2D{   0,  500},
            FVector2D{   0,    0}
        };
    }

    // ────────────────────────────────────────────────
    // THIEF — follows mouse clicks using Seek
    // ────────────────────────────────────────────────
    Thief = GetWorld()->SpawnActor<ASteeringAgent>(SteeringAgentClass,
        FVector{200, 0, 90}, FRotator::ZeroRotator);
    Thief->SetDebugRenderingEnabled(true);

    ThiefBehavior = std::make_unique<Seek>();
    ThiefBehavior->SetTarget(MouseTarget);
    Thief->SetSteeringBehavior(ThiefBehavior.get());

    // ────────────────────────────────────────────────
    // GUARD — controlled by FSM
    // ────────────────────────────────────────────────
    Guard = GetWorld()->SpawnActor<ASteeringAgent>(SteeringAgentClass,
        FVector{0, 0, 90}, FRotator::ZeroRotator);
    Guard->SetDebugRenderingEnabled(false);

    if (AGameAIController* AIController = Cast<AGameAIController>(Guard->GetController()))
    {
        if (UFSMComponent* FSM = Cast<UFSMComponent>(AIController->GetBrainComponent()))
        {
            auto PatrolPtr = std::make_unique<GameAI::FSM::PatrolState>(Guard, AIController, PatrolPoints);
            auto ChasePtr  = std::make_unique<GameAI::FSM::ChaseState>(Guard, AIController);
            auto SearchPtr = std::make_unique<GameAI::FSM::SearchState>(Guard, AIController);

            GameAI::FSM::State* Patrol = PatrolPtr.get();
            GameAI::FSM::State* Chase  = ChasePtr.get();
            GameAI::FSM::State* Search = SearchPtr.get();

            FSM->AddState(std::move(PatrolPtr));
            FSM->AddState(std::move(ChasePtr));
            FSM->AddState(std::move(SearchPtr));

            // Patrol → Chase: thief spotted
            FSM->AddTransition(Patrol, Chase, [AIController]()
            {
                UBlackboardComponent* BB = AIController->GetBlackboardComponent();
                return BB && BB->GetValueAsBool("IsTargetVisible");
            });

            // Chase → Search: thief lost
            FSM->AddTransition(Chase, Search, [AIController]()
            {
                UBlackboardComponent* BB = AIController->GetBlackboardComponent();
                return BB && !BB->GetValueAsBool("IsTargetVisible");
            });

            // Search → Chase: thief spotted again while searching
            FSM->AddTransition(Search, Chase, [AIController]()
            {
                UBlackboardComponent* BB = AIController->GetBlackboardComponent();
                return BB && BB->GetValueAsBool("IsTargetVisible");
            });

            // Search → Patrol: gave up searching
            FSM->AddTransition(Search, Patrol, [AIController, this]()
            {
                UBlackboardComponent* BB = AIController->GetBlackboardComponent();
                return BB && BB->GetValueAsFloat("SearchTime") >= SearchDuration;
            });

            AIController->RunFiniteStateMachine();
        }
    }
}

void ALevel_FSM::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ThiefBehavior)
    {
        ThiefBehavior->SetTarget(MouseTarget);
    }
}