#include "GameAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "FSM/FSMComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h" 
#include "Movement/SteeringBehaviors/SteeringAgent.h"

AGameAIController::AGameAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    BrainComponent = CreateDefaultSubobject<UFSMComponent>(TEXT("FSMComponent"));
}

void AGameAIController::BeginPlay()
{
    Super::BeginPlay();
    InitFiniteStateMachine();

    GetWorldTimerManager().SetTimer(PerceptionTimerHandle, this,
        &AGameAIController::UpdatePerception, PerceptionInterval, true);
}

void AGameAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AGameAIController::InitFiniteStateMachine()
{
    UFSMComponent* FSMComp = FindComponentByClass<UFSMComponent>();
    if (ensure(FSMComp) && FSMBlackboardAsset)
    {
        UBlackboardComponent* BlackboardComp = Blackboard;
        UseBlackboard(FSMBlackboardAsset, BlackboardComp);
        Blackboard = BlackboardComp;
    }
}

void AGameAIController::UpdatePerception()
{
    UBlackboardComponent* BB = GetBlackboardComponent();
    APawn* GuardPawn = GetPawn();
    if (!BB || !GuardPawn) return;

    TArray<AActor*> FoundAgents;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASteeringAgent::StaticClass(), FoundAgents);

    ASteeringAgent* Thief = nullptr;
    for (AActor* Actor : FoundAgents)
    {
        if (Actor != GuardPawn)
        {
            Thief = Cast<ASteeringAgent>(Actor);
            break;
        }
    }
    if (!Thief) return;

    bool bVisible = false;
    float Dist = FVector::Dist(GuardPawn->GetActorLocation(), Thief->GetActorLocation());

    if (Dist <= DetectionRadius)
    {
        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(GuardPawn);

        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult,
            GuardPawn->GetActorLocation(), Thief->GetActorLocation(),
            ECC_Visibility, Params);

        if (!bHit || HitResult.GetActor() == Thief)
        {
            bVisible = true;
            BB->SetValueAsObject("TargetActor", Thief);
            BB->SetValueAsVector("LastKnownLocation", Thief->GetActorLocation());
        }
    }

    BB->SetValueAsBool("IsTargetVisible",    bVisible);
    BB->SetValueAsBool("IsTargetNotVisible", !bVisible);
}

void AGameAIController::RunFiniteStateMachine()
{
    UFSMComponent* FSMComp = FindComponentByClass<UFSMComponent>();
    if (ensure(FSMComp))
        FSMComp->StartLogic();
}