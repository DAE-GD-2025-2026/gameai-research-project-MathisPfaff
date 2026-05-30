#include "Level_FreezeTag.h"

static const ECurveType AllCurves[] = {
    ECurveType::Linear, ECurveType::InverseLinear, ECurveType::SmoothStep,
    ECurveType::SineCurve, ECurveType::Cosine, ECurveType::Exponential, ECurveType::Logarithmic
};

ALevel_FreezeTag::ALevel_FreezeTag()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ALevel_FreezeTag::BeginPlay()
{
    Super::BeginPlay();

    if (TrimWorld)
        TrimWorldSize = TrimWorld->GetTrimWorldSize();

    SpawnUtilityTeam(TeamA, TeamASpeed, false); // fixed curves
    SpawnUtilityTeam(TeamB, TeamBSpeed, true);  // custom — all Linear
    SpawnPSTeam     (TeamC, TeamCSpeed);
}

// ===========================================================================
// SpawnUtilityTeam
// Wires up a UtilityAIComponent brain per agent.
// bCustom=false uses research-fitted curves; bCustom=true starts all Linear.
// ===========================================================================
void ALevel_FreezeTag::SpawnUtilityTeam(TArray<FFreezeTagUtilityAgent>& Team, float Speed, bool bCustom)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < TeamSize; ++i)
    {
        FFreezeTagUtilityAgent Entry;
        Entry.NormalSpeed = Speed;

        float Half = TrimWorldSize * 0.85f;
        FVector SpawnPos = FVector(FMath::RandRange(-Half, Half), FMath::RandRange(-Half, Half), 100.f);

        Entry.Agent = GetWorld()->SpawnActor<ASteeringAgent>(SteeringAgentClass, SpawnPos, FRotator::ZeroRotator, Params);

        if (!Entry.Agent)
        {
            Team.Add(MoveTemp(Entry));
            continue;
        }

        Entry.Agent->SetMaxLinearSpeed(Speed);
        Entry.Agent->SetDebugRenderingEnabled(false);

        Entry.PursuitAction = Entry.Brain.AddAction(std::make_unique<FT_PursuitAction>(bCustom));
        Entry.EvadeAction   = Entry.Brain.AddAction(std::make_unique<FT_EvadeAction>(bCustom));
        Entry.RescueAction  = Entry.Brain.AddAction(std::make_unique<FT_RescueAction>(bCustom));

        Team.Add(MoveTemp(Entry));
    }
}

// ===========================================================================
// SpawnPSTeam
// Builds a priority steering stack per agent (Rescue > Evade > Pursuit).
// ===========================================================================
void ALevel_FreezeTag::SpawnPSTeam(TArray<FFreezeTagAgent>& Team, float Speed)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < TeamSize; ++i)
    {
        FFreezeTagAgent Entry;
        Entry.NormalSpeed = Speed;

        float Half = TrimWorldSize * 0.85f;
        FVector SpawnPos = FVector(FMath::RandRange(-Half, Half), FMath::RandRange(-Half, Half), 100.f);

        Entry.Agent = GetWorld()->SpawnActor<ASteeringAgent>(SteeringAgentClass, SpawnPos, FRotator::ZeroRotator, Params);

        if (!Entry.Agent)
        {
            Team.Add(MoveTemp(Entry));
            continue;
        }

        Entry.Agent->SetMaxLinearSpeed(Speed);
        Entry.Agent->SetDebugRenderingEnabled(false);

        Entry.OwnedRescue  = std::make_unique<Seek>();
        Entry.OwnedEvade   = std::make_unique<Evade>();
        Entry.OwnedPursuit = std::make_unique<Pursuit>();

        Entry.OwnedPS = std::make_unique<PrioritySteering>(
            std::vector<ISteeringBehavior*>{
                Entry.OwnedRescue.get(),
                Entry.OwnedEvade.get(),
                Entry.OwnedPursuit.get()
            });

        Entry.RescueB  = Entry.OwnedRescue.get();
        Entry.EvadeB   = Entry.OwnedEvade.get();
        Entry.PursuitB = Entry.OwnedPursuit.get();

        Entry.Agent->SetSteeringBehavior(Entry.OwnedPS.get());
        Team.Add(MoveTemp(Entry));
    }
}

// ===========================================================================
// Tick
// Order matters:
//   1. Update steering targets / contexts so behaviors have fresh data
//   2. Check tagging so newly frozen agents stop before EnforceFrozen runs
//   3. Check unfreezing so rescuers can free teammates in the same frame
//   4. Enforce frozen state (movement component can drift at speed 0)
//   5. Check win condition
// ===========================================================================
void ALevel_FreezeTag::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bGameOver)
    {
        // RPS: A(utility fixed) freezes B(utility custom) freezes C(PS) freezes A
        UpdateUtilityContexts(TeamA, TeamB, TeamC); // A hunts B, threatened by C
        UpdateUtilityContexts(TeamB, TeamC, TeamA); // B hunts C, threatened by A
        UpdatePSTargets(TeamC, TeamA, TeamB);        // C hunts A, threatened by B

        CheckTagging(TeamA, TeamB); // utility A tags utility B
        CheckTagging(TeamB, TeamC); // utility B tags PS C
        CheckTagging(TeamC, TeamA); // PS C tags utility A

        UnfreezeCheck(TeamA);
        UnfreezeCheck(TeamB);
        UnfreezeCheck(TeamC);

        if      (IsTeamFullyFrozen(TeamB)) { bGameOver = true; WinnerName = TEXT("Team A (Red)");    }
        else if (IsTeamFullyFrozen(TeamC)) { bGameOver = true; WinnerName = TEXT("Team B (Orange)"); }
        else if (IsTeamFullyFrozen(TeamA)) { bGameOver = true; WinnerName = TEXT("Team C (Blue)");   }
    }

    EnforceFrozen(TeamA);
    EnforceFrozen(TeamB);
    EnforceFrozen(TeamC);

    DrawTeamDebug(TeamA, FColor::Red,                FColor(80, 80, 80));
    DrawTeamDebug(TeamB, FColor::Green,        FColor(80, 80, 80)); // orange
    DrawTeamDebug(TeamC, FColor::Blue,               FColor(80, 80, 80));

    DrawImGui();
}

// ===========================================================================
// UpdateUtilityContexts (utility hunters, PS prey, utility threats)
// Calculates FFreezeTagContext per agent and runs their brain.
//
// DangerMeter = clamp( frozenTeammates/TeamSize - frozenPrey/TeamSize, 0, 1 )
//   More frozen teammates → higher danger.
//   More frozen prey (team is winning) → lower danger.
// ===========================================================================
void ALevel_FreezeTag::UpdateUtilityContexts(TArray<FFreezeTagUtilityAgent>& Hunters,
                                              TArray<FFreezeTagAgent>&        Prey,
                                              TArray<FFreezeTagUtilityAgent>& Threats)
{
    int32 FrozenHunters = 0; for (auto& H : Hunters) if (H.bFrozen) ++FrozenHunters;
    int32 FrozenPrey    = 0; for (auto& P : Prey)    if (P.bFrozen) ++FrozenPrey;
    float DangerMeter   = FMath::Clamp((float)FrozenHunters / TeamSize - (float)FrozenPrey / TeamSize, 0.f, 1.f);

    for (auto& H : Hunters)
    {
        if (!H.Agent) continue;

        ASteeringAgent* NearestPrey = nullptr;       float BestPreyDist    = FLT_MAX;
        ASteeringAgent* NearestThreat = nullptr;     float BestThreatDist  = FLT_MAX;
        ASteeringAgent* NearestFrozenMate = nullptr; float BestFrozenDist  = FLT_MAX;

        for (auto& P : Prey)
        {
            if (P.bFrozen || !P.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation());
            if (D < BestPreyDist) { BestPreyDist = D; NearestPrey = P.Agent; }
        }
        for (auto& T : Threats)
        {
            if (T.bFrozen || !T.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), T.Agent->GetActorLocation());
            if (D < BestThreatDist) { BestThreatDist = D; NearestThreat = T.Agent; }
        }
        for (auto& Mate : Hunters)
        {
            if (!Mate.bFrozen || !Mate.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), Mate.Agent->GetActorLocation());
            if (D < BestFrozenDist) { BestFrozenDist = D; NearestFrozenMate = Mate.Agent; }
        }

        FFreezeTagContext Ctx;
        Ctx.DangerMeter               = DangerMeter;
        Ctx.NormalizedProximityToPrey   = NearestPrey    ? FMath::Clamp(1.f - BestPreyDist    / MaxRelevantDist, 0.f, 1.f) : 0.f;
        Ctx.NormalizedProximityToThreat = NearestThreat  ? FMath::Clamp(1.f - BestThreatDist  / MaxRelevantDist, 0.f, 1.f) : 0.f;
        Ctx.FrozenTeammateNearby        = NearestFrozenMate ? FMath::Clamp(1.f - BestFrozenDist / RescueRadius,    0.f, 1.f) : 0.f;

        H.PursuitAction->Context = Ctx;
        H.EvadeAction->Context   = Ctx;
        H.RescueAction->Context  = Ctx;

        if (NearestPrey)
        {
            FTargetData T; T.Position = FVector2D(NearestPrey->GetActorLocation()); T.LinearVelocity = NearestPrey->GetLinearVelocity();
            H.PursuitAction->SetTarget(T);
        }
        if (NearestThreat)
        {
            FTargetData T; T.Position = FVector2D(NearestThreat->GetActorLocation()); T.LinearVelocity = NearestThreat->GetLinearVelocity();
            H.EvadeAction->SetTarget(T);
        }
        if (NearestFrozenMate)
        {
            FTargetData T; T.Position = FVector2D(NearestFrozenMate->GetActorLocation());
            H.RescueAction->SetTarget(T);
        }

        if (!H.bFrozen)
            H.Brain.Update(*H.Agent, 0.f);
    }
}

// Overload: threat team is a PS team (FFreezeTagAgent)
void ALevel_FreezeTag::UpdateUtilityContexts(TArray<FFreezeTagUtilityAgent>& Hunters,
                                              TArray<FFreezeTagAgent>&        Prey,
                                              TArray<FFreezeTagAgent>&        Threats)
{
    int32 FrozenHunters = 0; for (auto& H : Hunters) if (H.bFrozen) ++FrozenHunters;
    int32 FrozenPrey    = 0; for (auto& P : Prey)    if (P.bFrozen) ++FrozenPrey;
    float DangerMeter   = FMath::Clamp((float)FrozenHunters / TeamSize - (float)FrozenPrey / TeamSize, 0.f, 1.f);

    for (auto& H : Hunters)
    {
        if (!H.Agent) continue;

        ASteeringAgent* NearestPrey = nullptr;       float BestPreyDist    = FLT_MAX;
        ASteeringAgent* NearestThreat = nullptr;     float BestThreatDist  = FLT_MAX;
        ASteeringAgent* NearestFrozenMate = nullptr; float BestFrozenDist  = FLT_MAX;

        for (auto& P : Prey)
        {
            if (P.bFrozen || !P.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation());
            if (D < BestPreyDist) { BestPreyDist = D; NearestPrey = P.Agent; }
        }
        for (auto& T : Threats)
        {
            if (T.bFrozen || !T.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), T.Agent->GetActorLocation());
            if (D < BestThreatDist) { BestThreatDist = D; NearestThreat = T.Agent; }
        }
        for (auto& Mate : Hunters)
        {
            if (!Mate.bFrozen || !Mate.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), Mate.Agent->GetActorLocation());
            if (D < BestFrozenDist) { BestFrozenDist = D; NearestFrozenMate = Mate.Agent; }
        }

        FFreezeTagContext Ctx;
        Ctx.DangerMeter               = DangerMeter;
        Ctx.NormalizedProximityToPrey   = NearestPrey    ? FMath::Clamp(1.f - BestPreyDist    / MaxRelevantDist, 0.f, 1.f) : 0.f;
        Ctx.NormalizedProximityToThreat = NearestThreat  ? FMath::Clamp(1.f - BestThreatDist  / MaxRelevantDist, 0.f, 1.f) : 0.f;
        Ctx.FrozenTeammateNearby        = NearestFrozenMate ? FMath::Clamp(1.f - BestFrozenDist / RescueRadius,    0.f, 1.f) : 0.f;

        H.PursuitAction->Context = Ctx;
        H.EvadeAction->Context   = Ctx;
        H.RescueAction->Context  = Ctx;

        if (NearestPrey)
        {
            FTargetData T; T.Position = FVector2D(NearestPrey->GetActorLocation()); T.LinearVelocity = NearestPrey->GetLinearVelocity();
            H.PursuitAction->SetTarget(T);
        }
        if (NearestThreat)
        {
            FTargetData T; T.Position = FVector2D(NearestThreat->GetActorLocation()); T.LinearVelocity = NearestThreat->GetLinearVelocity();
            H.EvadeAction->SetTarget(T);
        }
        if (NearestFrozenMate)
        {
            FTargetData T; T.Position = FVector2D(NearestFrozenMate->GetActorLocation());
            H.RescueAction->SetTarget(T);
        }

        if (!H.bFrozen)
            H.Brain.Update(*H.Agent, 0.f);
    }
}

void ALevel_FreezeTag::UpdateUtilityContexts(TArray<FFreezeTagUtilityAgent>& Hunters,
                                              TArray<FFreezeTagUtilityAgent>& Prey,
                                              TArray<FFreezeTagAgent>&        Threats)
{
    int32 FrozenHunters = 0; for (auto& H : Hunters) if (H.bFrozen) ++FrozenHunters;
    int32 FrozenPrey    = 0; for (auto& P : Prey)    if (P.bFrozen) ++FrozenPrey;
    float DangerMeter   = FMath::Clamp((float)FrozenHunters / TeamSize - (float)FrozenPrey / TeamSize, 0.f, 1.f);

    for (auto& H : Hunters)
    {
        if (!H.Agent) continue;

        ASteeringAgent* NearestPrey = nullptr;       float BestPreyDist   = FLT_MAX;
        ASteeringAgent* NearestThreat = nullptr;     float BestThreatDist = FLT_MAX;
        ASteeringAgent* NearestFrozenMate = nullptr; float BestFrozenDist = FLT_MAX;

        for (auto& P : Prey)
        {
            if (P.bFrozen || !P.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation());
            if (D < BestPreyDist) { BestPreyDist = D; NearestPrey = P.Agent; }
        }
        for (auto& T : Threats)
        {
            if (T.bFrozen || !T.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), T.Agent->GetActorLocation());
            if (D < BestThreatDist) { BestThreatDist = D; NearestThreat = T.Agent; }
        }
        for (auto& Mate : Hunters)
        {
            if (!Mate.bFrozen || !Mate.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), Mate.Agent->GetActorLocation());
            if (D < BestFrozenDist) { BestFrozenDist = D; NearestFrozenMate = Mate.Agent; }
        }

        FFreezeTagContext Ctx;
        Ctx.DangerMeter               = DangerMeter;
        Ctx.NormalizedProximityToPrey   = NearestPrey       ? FMath::Clamp(1.f - BestPreyDist    / MaxRelevantDist, 0.f, 1.f) : 0.f;
        Ctx.NormalizedProximityToThreat = NearestThreat     ? FMath::Clamp(1.f - BestThreatDist  / MaxRelevantDist, 0.f, 1.f) : 0.f;
        Ctx.FrozenTeammateNearby        = NearestFrozenMate ? FMath::Clamp(1.f - BestFrozenDist  / RescueRadius,    0.f, 1.f) : 0.f;

        H.PursuitAction->Context = Ctx;
        H.EvadeAction->Context   = Ctx;
        H.RescueAction->Context  = Ctx;

        if (NearestPrey)       { FTargetData T; T.Position = FVector2D(NearestPrey->GetActorLocation());       T.LinearVelocity = NearestPrey->GetLinearVelocity();   H.PursuitAction->SetTarget(T); }
        if (NearestThreat)     { FTargetData T; T.Position = FVector2D(NearestThreat->GetActorLocation());     T.LinearVelocity = NearestThreat->GetLinearVelocity(); H.EvadeAction->SetTarget(T);   }
        if (NearestFrozenMate) { FTargetData T; T.Position = FVector2D(NearestFrozenMate->GetActorLocation());                                                         H.RescueAction->SetTarget(T);  }

        if (!H.bFrozen)
            H.Brain.Update(*H.Agent, 0.f);
    }
}

// ===========================================================================
// UpdatePSTargets
// Feeds targets into each PS agent's priority slots. Prey and Threats are
// utility teams. A slot is disabled by pointing it at the agent's own
// position — near-zero output causes PrioritySteering to fall through.
// ===========================================================================
void ALevel_FreezeTag::UpdatePSTargets(TArray<FFreezeTagAgent>&        Hunters,
                                        TArray<FFreezeTagUtilityAgent>& Prey,
                                        TArray<FFreezeTagUtilityAgent>& Threats)
{
    int32 FrozenHunters = 0;
    for (auto& H : Hunters) if (H.bFrozen) ++FrozenHunters;

    for (auto& H : Hunters)
    {
        if (!H.Agent || H.bFrozen) continue;

        ASteeringAgent* NearestFrozenMate = nullptr; float BestFrozenDist  = FLT_MAX;
        ASteeringAgent* NearestPrey       = nullptr; float BestPreyDist    = FLT_MAX;
        ASteeringAgent* NearestThreat     = nullptr; float BestThreatDist  = FLT_MAX;

        for (auto& Mate : Hunters)
        {
            if (!Mate.bFrozen || !Mate.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), Mate.Agent->GetActorLocation());
            if (D < BestFrozenDist) { BestFrozenDist = D; NearestFrozenMate = Mate.Agent; }
        }
        for (auto& P : Prey)
        {
            if (P.bFrozen || !P.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation());
            if (D < BestPreyDist) { BestPreyDist = D; NearestPrey = P.Agent; }
        }
        for (auto& T : Threats)
        {
            if (T.bFrozen || !T.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), T.Agent->GetActorLocation());
            if (D < BestThreatDist) { BestThreatDist = D; NearestThreat = T.Agent; }
        }

        // Rescue slot — active when enough teammates are frozen and one is reachable
        {
            FTargetData T;
            bool bRescue = (FrozenHunters >= RescueThreshold) && NearestFrozenMate;
            T.Position = bRescue ? FVector2D(NearestFrozenMate->GetActorLocation()) : FVector2D(H.Agent->GetActorLocation());
            H.RescueB->SetTarget(T);
        }
        // Evade slot — active only when a threat is within EvadeRadius
        {
            FTargetData T;
            if (NearestThreat && BestThreatDist < EvadeRadius)
            {
                T.Position       = FVector2D(NearestThreat->GetActorLocation());
                T.LinearVelocity = NearestThreat->GetLinearVelocity();
            }
            else { T.Position = FVector2D(H.Agent->GetActorLocation()); }
            H.EvadeB->SetTarget(T);
        }
        // Pursuit slot — always has a target, nudges forward if all prey are frozen
        {
            FTargetData T;
            if (NearestPrey)
            {
                T.Position       = FVector2D(NearestPrey->GetActorLocation());
                T.LinearVelocity = NearestPrey->GetLinearVelocity();
            }
            else { T.Position = FVector2D(H.Agent->GetActorLocation()) + FVector2D(200.f, 0.f); }
            H.PursuitB->SetTarget(T);
        }
    }
}

// ===========================================================================
// Tagging overloads
// ===========================================================================
void ALevel_FreezeTag::CheckTagging(TArray<FFreezeTagUtilityAgent>& Hunters, TArray<FFreezeTagAgent>& Prey)
{
    for (auto& H : Hunters)
    {
        if (!H.Agent || H.bFrozen) continue;
        for (auto& P : Prey)
        {
            if (!P.Agent || P.bFrozen) continue;
            if (FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation()) <= TagRadius)
            {
                P.bFrozen = true;
                P.Agent->SetMaxLinearSpeed(0.f);
                P.Agent->GetCharacterMovement()->StopMovementImmediately();
            }
        }
    }
}

void ALevel_FreezeTag::CheckTagging(TArray<FFreezeTagAgent>& Hunters, TArray<FFreezeTagUtilityAgent>& Prey)
{
    for (auto& H : Hunters)
    {
        if (!H.Agent || H.bFrozen) continue;
        for (auto& P : Prey)
        {
            if (!P.Agent || P.bFrozen) continue;
            if (FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation()) <= TagRadius)
            {
                P.bFrozen = true;
                P.Agent->SetMaxLinearSpeed(0.f);
                P.Agent->GetCharacterMovement()->StopMovementImmediately();
            }
        }
    }
}

void ALevel_FreezeTag::CheckTagging(TArray<FFreezeTagUtilityAgent>& Hunters, TArray<FFreezeTagUtilityAgent>& Prey)
{
    for (auto& H : Hunters)
    {
        if (!H.Agent || H.bFrozen) continue;
        for (auto& P : Prey)
        {
            if (!P.Agent || P.bFrozen) continue;
            if (FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation()) <= TagRadius)
            {
                P.bFrozen = true;
                P.Agent->SetMaxLinearSpeed(0.f);
                P.Agent->GetCharacterMovement()->StopMovementImmediately();
            }
        }
    }
}

// ===========================================================================
// Unfreeze overloads
// ===========================================================================
void ALevel_FreezeTag::UnfreezeCheck(TArray<FFreezeTagUtilityAgent>& Team)
{
    for (auto& Frozen : Team)
    {
        if (!Frozen.bFrozen || !Frozen.Agent) continue;
        for (auto& Rescuer : Team)
        {
            if (Rescuer.bFrozen || !Rescuer.Agent) continue;
            if (FVector::Dist(Frozen.Agent->GetActorLocation(), Rescuer.Agent->GetActorLocation()) <= TagRadius)
            {
                Frozen.bFrozen = false;
                Frozen.Agent->SetMaxLinearSpeed(Frozen.NormalSpeed);
                break;
            }
        }
    }
}

void ALevel_FreezeTag::UnfreezeCheck(TArray<FFreezeTagAgent>& Team)
{
    for (auto& Frozen : Team)
    {
        if (!Frozen.bFrozen || !Frozen.Agent) continue;
        for (auto& Rescuer : Team)
        {
            if (Rescuer.bFrozen || !Rescuer.Agent) continue;
            if (FVector::Dist(Frozen.Agent->GetActorLocation(), Rescuer.Agent->GetActorLocation()) <= TagRadius)
            {
                Frozen.bFrozen = false;
                Frozen.Agent->SetMaxLinearSpeed(Frozen.NormalSpeed);
                break;
            }
        }
    }
}

// ===========================================================================
// EnforceFrozen overloads
// The movement component can accumulate residual velocity even at speed 0.
// ===========================================================================
void ALevel_FreezeTag::EnforceFrozen(TArray<FFreezeTagUtilityAgent>& Team)
{
    for (auto& A : Team)
        if (A.bFrozen && A.Agent)
        {
            A.Agent->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            A.Agent->GetCharacterMovement()->StopMovementImmediately();
        }
}

void ALevel_FreezeTag::EnforceFrozen(TArray<FFreezeTagAgent>& Team)
{
    for (auto& A : Team)
        if (A.bFrozen && A.Agent)
        {
            A.Agent->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            A.Agent->GetCharacterMovement()->StopMovementImmediately();
        }
}

bool ALevel_FreezeTag::IsTeamFullyFrozen(const TArray<FFreezeTagUtilityAgent>& Team) const
{
    for (auto& A : Team) if (!A.bFrozen) return false;
    return Team.Num() > 0;
}

bool ALevel_FreezeTag::IsTeamFullyFrozen(const TArray<FFreezeTagAgent>& Team) const
{
    for (auto& A : Team) if (!A.bFrozen) return false;
    return Team.Num() > 0;
}

// ===========================================================================
// ResetLevel
// Destroys all agents and respawns them at fresh random positions.
// ===========================================================================
void ALevel_FreezeTag::ResetLevel()
{
    bGameOver  = false;
    WinnerName = TEXT("");

    auto DestroyUtility = [](TArray<FFreezeTagUtilityAgent>& Team)
    {
        for (auto& A : Team) if (A.Agent) A.Agent->Destroy();
        Team.Empty();
    };
    auto DestroyPS = [](TArray<FFreezeTagAgent>& Team)
    {
        for (auto& A : Team) if (A.Agent) A.Agent->Destroy();
        Team.Empty();
    };

    DestroyUtility(TeamA);
    DestroyUtility(TeamB);
    DestroyPS(TeamC);

    SpawnUtilityTeam(TeamA, TeamASpeed, false);
    SpawnUtilityTeam(TeamB, TeamBSpeed, true);
    SpawnPSTeam     (TeamC, TeamCSpeed);
}

// ===========================================================================
// DrawImGui
// ===========================================================================
void ALevel_FreezeTag::DrawImGui()
{
    ImGui::SetNextWindowPos(WindowPos);
    ImGui::SetNextWindowSize(WindowSize);
    ImGui::Begin("Freeze Tag");

    if (bGameOver)
    {
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.f, 1.f), "WINNER: %s", TCHAR_TO_ANSI(*WinnerName));
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Settings"))
    {
        if (TrimWorld)
        {
            ImGui::Checkbox("World Trimming", &TrimWorld->bShouldTrimWorld);
            ImGui::Checkbox("World Looping",  &TrimWorld->bIsWorldLooping);
            if (ImGui::SliderFloat("World Size", &TrimWorldSize, 300.f, 3000.f, "%.0f"))
                TrimWorld->SetTrimWorldSize(TrimWorldSize);
        }
        
        if (ImGui::Button("Reset")) ResetLevel();
    }

    ImGui::Spacing();

    DrawTeamStatus("Team A (Red)    — fixed utility,  freezes B", TeamA, ImVec4(1.f, 0.3f, 0.3f, 1.f));
    DrawTeamStatus("Team B (Orange) — custom utility, freezes C", TeamB, ImVec4(1.f, 0.6f, 0.1f, 1.f));
    DrawTeamStatus("Team C (Blue)   — priority steer, freezes A", TeamC, ImVec4(0.3f, 0.5f, 1.f, 1.f));

    ImGui::Spacing();

    DrawCustomTeamInspector();

    ImGui::End();
}

// ===========================================================================
// DrawTeamStatus overloads
// ===========================================================================
void ALevel_FreezeTag::DrawTeamStatus(const char* Label, const TArray<FFreezeTagUtilityAgent>& Team, ImVec4 Color)
{
    int32 Frozen = 0;
    for (auto& A : Team) if (A.bFrozen) ++Frozen;

    ImGui::TextColored(Color, "%s", Label);
    ImGui::SameLine(280); ImGui::Text("%d / %d frozen", Frozen, Team.Num());

    for (int32 i = 0; i < Team.Num(); ++i)
    {
        const bool bFrz = Team[i].bFrozen;
        const char* ActionName = (!bFrz && Team[i].Brain.GetCurrentActionName()) ? Team[i].Brain.GetCurrentActionName() : "";
        ImGui::TextColored(
            bFrz ? ImVec4(0.5f, 0.5f, 1.f, 1.f) : Color,
            "  [%d] %s%s%s", i,
            bFrz ? "FROZEN" : "active",
            (!bFrz && ActionName[0] != '\0') ? " — " : "",
            (!bFrz && ActionName[0] != '\0') ? ActionName : "");
    }
    ImGui::Spacing();
}

void ALevel_FreezeTag::DrawTeamStatus(const char* Label, const TArray<FFreezeTagAgent>& Team, ImVec4 Color)
{
    int32 Frozen = 0;
    for (auto& A : Team) if (A.bFrozen) ++Frozen;

    ImGui::TextColored(Color, "%s", Label);
    ImGui::SameLine(280); ImGui::Text("%d / %d frozen", Frozen, Team.Num());

    for (int32 i = 0; i < Team.Num(); ++i)
    {
        const bool bFrz = Team[i].bFrozen;
        ImGui::TextColored(bFrz ? ImVec4(0.5f, 0.5f, 1.f, 1.f) : Color,
            "  [%d] %s", i, bFrz ? "FROZEN" : "active");
    }
    ImGui::Spacing();
}

// ===========================================================================
// DrawTeamDebug overloads
// ===========================================================================
void ALevel_FreezeTag::DrawTeamDebug(const TArray<FFreezeTagUtilityAgent>& Team, FColor ActiveColor, FColor FrozenColor)
{
    for (auto& A : Team)
    {
        if (!A.Agent) continue;
        DrawDebugSphere(GetWorld(), A.Agent->GetActorLocation(), 30.f, 8,
            A.bFrozen ? FrozenColor : ActiveColor, false, -1.f, 0, 2.f);
    }
}

void ALevel_FreezeTag::DrawTeamDebug(const TArray<FFreezeTagAgent>& Team, FColor ActiveColor, FColor FrozenColor)
{
    for (auto& A : Team)
    {
        if (!A.Agent) continue;
        DrawDebugSphere(GetWorld(), A.Agent->GetActorLocation(), 30.f, 8,
            A.bFrozen ? FrozenColor : ActiveColor, false, -1.f, 0, 2.f);
    }
}

// ===========================================================================
// DrawCustomTeamInspector
// Per-agent utility score breakdown for Team B (custom). Same style as the
// UtilityTheory level — active action highlighted green, curve dropdowns
// let the player tune each consideration live.
// ===========================================================================
void ALevel_FreezeTag::DrawCustomTeamInspector()
{
    if (!ImGui::CollapsingHeader("Custom Utility AI Inspector", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (TeamB.IsEmpty()) return;

    // Agent selector dropdown
    char PreviewLabel[32];
    FCStringAnsi::Snprintf(PreviewLabel, sizeof(PreviewLabel), "Agent %d", SelectedCustomAgent);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##agentselect", PreviewLabel))
    {
        for (int32 i = 0; i < TeamB.Num(); ++i)
        {
            char Buf[32];
            FCStringAnsi::Snprintf(Buf, sizeof(Buf), "Agent %d  [%s]", i,
                TeamB[i].bFrozen ? "FROZEN" : TeamB[i].Brain.GetCurrentActionName());
            if (ImGui::Selectable(Buf, SelectedCustomAgent == i))
                SelectedCustomAgent = i;
            if (SelectedCustomAgent == i) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (!TeamB.IsValidIndex(SelectedCustomAgent)) return;
    auto& A = TeamB[SelectedCustomAgent];

    ImGui::Spacing();
    if (A.bFrozen)
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.f, 1.f), "State: FROZEN");
    else
        ImGui::Text("State: %s", A.Brain.GetCurrentActionName());
    ImGui::Spacing();

    // When a curve changes on the selected agent, mirror it to all other agents
    auto SyncCurvesToAll = [&](FT_PursuitAction* Src)
    {
        for (auto& Other : TeamB)
        {
            if (!Other.PursuitAction) continue;
            for (int32 i = 0; i < (int32)Src->Considerations.size(); ++i)
                Other.PursuitAction->Considerations[i].CurveType = Src->Considerations[i].CurveType;
        }
    };
    auto SyncEvadeToAll = [&](FT_EvadeAction* Src)
    {
        for (auto& Other : TeamB)
        {
            if (!Other.EvadeAction) continue;
            for (int32 i = 0; i < (int32)Src->Considerations.size(); ++i)
                Other.EvadeAction->Considerations[i].CurveType = Src->Considerations[i].CurveType;
        }
    };
    auto SyncRescueToAll = [&](FT_RescueAction* Src)
    {
        for (auto& Other : TeamB)
        {
            if (!Other.RescueAction) continue;
            for (int32 i = 0; i < (int32)Src->Considerations.size(); ++i)
                Other.RescueAction->Considerations[i].CurveType = Src->Considerations[i].CurveType;
        }
    };

    auto DrawAction = [&](const char* ActionName, float TotalScore, bool bIsActive, auto* Action, auto SyncFn)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, bIsActive
            ? ImVec4(0.2f, 1.f, 0.2f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f));
        bool bOpen = ImGui::TreeNode(ActionName);
        ImGui::PopStyleColor();

        ImGui::SameLine(120);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bIsActive
            ? ImVec4(0.2f, 0.9f, 0.2f, 1.f) : ImVec4(0.2f, 0.4f, 0.9f, 1.f));
        char BarLabel[32];
        FCStringAnsi::Snprintf(BarLabel, sizeof(BarLabel), "%.2f", TotalScore);
        ImGui::ProgressBar(TotalScore, ImVec2(-1, 14), BarLabel);
        ImGui::PopStyleColor();

        if (bOpen)
        {
            ImGui::Indent(8.f);
            bool bAnyCurveChanged = false;
            for (auto& C : Action->Considerations)
            {
                float Score = C.Evaluate(Action->Context);

                ImGui::Text("%-20s", C.Name.c_str());
                ImGui::SameLine(140);

                ImVec4 CColor = Score > 0.6f ? ImVec4(0.2f, 1.f, 0.2f, 1.f)
                              : Score > 0.3f ? ImVec4(1.f, 0.85f, 0.f, 1.f)
                                             : ImVec4(1.f, 0.3f, 0.3f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, CColor);
                char CLabel[32];
                FCStringAnsi::Snprintf(CLabel, sizeof(CLabel), "%.2f", Score);
                ImGui::ProgressBar(Score, ImVec2(-1, 12), CLabel);
                ImGui::PopStyleColor();

                std::string ComboId = C.Name + "##curve";
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo(ComboId.c_str(), CurveTypeName(C.CurveType)))
                {
                    for (ECurveType Curve : AllCurves)
                    {
                        bool bSelected = (C.CurveType == Curve);
                        if (ImGui::Selectable(CurveTypeName(Curve), bSelected))
                        {
                            C.CurveType = Curve;
                            bAnyCurveChanged = true;
                        }
                        if (bSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Spacing();
            }

            // Propagate any curve change to all agents in the team
            if (bAnyCurveChanged) SyncFn(Action);

            ImGui::Unindent(8.f);
            ImGui::TreePop();
        }
    };

    bool bPursuitActive = (strcmp(A.Brain.GetCurrentActionName(), "Pursuit") == 0);
    bool bEvadeActive   = (strcmp(A.Brain.GetCurrentActionName(), "Evade")   == 0);
    bool bRescueActive  = (strcmp(A.Brain.GetCurrentActionName(), "Rescue")  == 0);

    DrawAction("Pursuit", A.PursuitAction->Score(), bPursuitActive, A.PursuitAction, SyncCurvesToAll);
    DrawAction("Evade",   A.EvadeAction->Score(),   bEvadeActive,   A.EvadeAction,   SyncEvadeToAll);
    DrawAction("Rescue",  A.RescueAction->Score(),  bRescueActive,  A.RescueAction,  SyncRescueToAll);
}