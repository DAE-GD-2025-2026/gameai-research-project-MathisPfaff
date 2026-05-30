#include "Level_FreezeTag.h"

ALevel_FreezeTag::ALevel_FreezeTag()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ALevel_FreezeTag::BeginPlay()
{
    Super::BeginPlay();

    // Sync the cached size so the ImGui slider starts at the actual volume size
    if (TrimWorld)
        TrimWorldSize = TrimWorld->GetTrimWorldSize();

    SpawnTeam(TeamA, TeamASpeed);
    SpawnTeam(TeamB, TeamBSpeed);
    SpawnTeam(TeamC, TeamCSpeed);
}

// ===========================================================================
// SpawnTeam
// Spawns TeamSize agents at random positions within the trim bounds and
// builds each agent's priority steering stack (Rescue > Evade > Pursuit).
// Debug rendering is disabled — team color spheres are drawn by DrawTeamDebug.
// ===========================================================================
void ALevel_FreezeTag::SpawnTeam(TArray<FFreezeTagAgent>& Team, float Speed)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < TeamSize; ++i)
    {
        FFreezeTagAgent Entry;
        Entry.NormalSpeed = Speed;

        // Stay 15 % inside the trim edge so agents don't immediately wrap
        float Half = TrimWorldSize * 0.85f;
        FVector SpawnPos = FVector(
            FMath::RandRange(-Half, Half),
            FMath::RandRange(-Half, Half),
            100.f);

        Entry.Agent = GetWorld()->SpawnActor<ASteeringAgent>(
            SteeringAgentClass, SpawnPos, FRotator::ZeroRotator, Params);

        if (!Entry.Agent)
        {
            Team.Add(MoveTemp(Entry));
            continue;
        }

        Entry.Agent->SetMaxLinearSpeed(Speed);
        Entry.Agent->SetDebugRenderingEnabled(false); // colored spheres used instead

        
        Entry.OwnedRescue  = std::make_unique<Seek>();
        Entry.OwnedEvade   = std::make_unique<Evade>();
        Entry.OwnedPursuit = std::make_unique<Pursuit>();

        Entry.OwnedPS = std::make_unique<PrioritySteering>(
            std::vector<ISteeringBehavior*>{
                Entry.OwnedRescue.get(),   // highest priority
                Entry.OwnedEvade.get(),
                Entry.OwnedPursuit.get()   // fallback
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
//   1. Update steering targets so behaviors have fresh data this frame
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
        
        UpdatePSTargets(TeamA, TeamB, TeamC);
        UpdatePSTargets(TeamB, TeamC, TeamA);
        UpdatePSTargets(TeamC, TeamA, TeamB);

        CheckTagging(TeamA, TeamB);
        CheckTagging(TeamB, TeamC);
        CheckTagging(TeamC, TeamA);

        UnfreezeCheck(TeamA);
        UnfreezeCheck(TeamB);
        UnfreezeCheck(TeamC);

        if      (IsTeamFullyFrozen(TeamB)) { bGameOver = true; WinnerName = TEXT("Team A (Red)");   }
        else if (IsTeamFullyFrozen(TeamC)) { bGameOver = true; WinnerName = TEXT("Team B (Blue)");  }
        else if (IsTeamFullyFrozen(TeamA)) { bGameOver = true; WinnerName = TEXT("Team C (Green)"); }
    }

    EnforceFrozen(TeamA);
    EnforceFrozen(TeamB);
    EnforceFrozen(TeamC);

    DrawTeamDebug(TeamA, FColor::Red,   FColor(80, 80, 80));
    DrawTeamDebug(TeamB, FColor::Blue,  FColor(80, 80, 80));
    DrawTeamDebug(TeamC, FColor::Green, FColor(80, 80, 80));

    DrawImGui();
}

// ===========================================================================
// UpdatePSTargets
// Feeds targets into each agent's priority slots each tick. A slot is
// "disabled" by pointing it at the agent's own position — this produces a
// near-zero steering vector which PrioritySteering treats as inactive and
// falls through to the next slot.
//
//   Rescue  active when: frozen teammates >= RescueThreshold
//   Evade   active when: nearest threat is within EvadeRadius
//   Pursuit active when: unfrozen prey exists (fallback: nudge forward)
// ===========================================================================
void ALevel_FreezeTag::UpdatePSTargets(TArray<FFreezeTagAgent>& Hunters,
                                        TArray<FFreezeTagAgent>& Prey,
                                        TArray<FFreezeTagAgent>& Threats)
{
    int32 FrozenHunters = 0;
    for (auto& H : Hunters)
        if (H.bFrozen) ++FrozenHunters;

    for (auto& H : Hunters)
    {
        if (!H.Agent || H.bFrozen) continue;

        // Find nearest frozen teammate to rescue
        ASteeringAgent* NearestFrozenMate = nullptr;
        float BestFrozenDist = FLT_MAX;
        for (auto& Mate : Hunters)
        {
            if (!Mate.bFrozen || !Mate.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), Mate.Agent->GetActorLocation());
            if (D < BestFrozenDist) { BestFrozenDist = D; NearestFrozenMate = Mate.Agent; }
        }

        // Find nearest unfrozen prey to freeze
        ASteeringAgent* NearestPrey = nullptr;
        float BestPreyDist = FLT_MAX;
        for (auto& P : Prey)
        {
            if (P.bFrozen || !P.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), P.Agent->GetActorLocation());
            if (D < BestPreyDist) { BestPreyDist = D; NearestPrey = P.Agent; }
        }

        // Find nearest unfrozen threat to evade
        ASteeringAgent* NearestThreat = nullptr;
        float BestThreatDist = FLT_MAX;
        for (auto& T : Threats)
        {
            if (T.bFrozen || !T.Agent) continue;
            float D = FVector::Dist(H.Agent->GetActorLocation(), T.Agent->GetActorLocation());
            if (D < BestThreatDist) { BestThreatDist = D; NearestThreat = T.Agent; }
        }

        // --- Rescue slot ---
        // Active only when enough teammates are frozen and one is reachable
        {
            FTargetData T;
            bool bShouldRescue = (FrozenHunters >= RescueThreshold) && NearestFrozenMate;
            T.Position = bShouldRescue
                ? FVector2D(NearestFrozenMate->GetActorLocation())
                : FVector2D(H.Agent->GetActorLocation()); // self-target disables the slot
            H.RescueB->SetTarget(T);
        }

        // --- Evade slot ---
        // Active only when a threat is close enough to be dangerous
        {
            FTargetData T;
            if (NearestThreat && BestThreatDist < EvadeRadius)
            {
                T.Position       = FVector2D(NearestThreat->GetActorLocation());
                T.LinearVelocity = NearestThreat->GetLinearVelocity(); // needed for predicted position
            }
            else
            {
                T.Position = FVector2D(H.Agent->GetActorLocation()); // self-target disables the slot
            }
            H.EvadeB->SetTarget(T);
        }

        // --- Pursuit slot ---
        // Always has a target — falls back to a small nudge when all prey are frozen
        {
            FTargetData T;
            if (NearestPrey)
            {
                T.Position       = FVector2D(NearestPrey->GetActorLocation());
                T.LinearVelocity = NearestPrey->GetLinearVelocity(); // needed for predicted position
            }
            else
            {
                // All prey frozen — drift forward so the agent doesn't stand still
                T.Position = FVector2D(H.Agent->GetActorLocation()) + FVector2D(200.f, 0.f);
            }
            H.PursuitB->SetTarget(T);
        }
    }
}

// ===========================================================================
// CheckTagging
// An unfrozen Hunter that enters TagRadius of an unfrozen Prey freezes it:
// speed is set to 0 and movement is stopped immediately.
// ===========================================================================
void ALevel_FreezeTag::CheckTagging(TArray<FFreezeTagAgent>& Hunters,
                                     TArray<FFreezeTagAgent>& Prey)
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
// UnfreezeCheck
// An active teammate that walks within TagRadius of a frozen agent unfreezes
// it and restores its normal speed.
// ===========================================================================
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
                break; // one rescuer is enough
            }
        }
    }
}

// ===========================================================================
// EnforceFrozen
// The character movement component can accumulate a small residual velocity
// even at MaxWalkSpeed 0. This clears it every tick to keep frozen agents
// visually pinned in place.
// ===========================================================================
void ALevel_FreezeTag::EnforceFrozen(TArray<FFreezeTagAgent>& Team)
{
    for (auto& A : Team)
    {
        if (A.bFrozen && A.Agent)
        {
            A.Agent->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            A.Agent->GetCharacterMovement()->StopMovementImmediately();
        }
    }
}

bool ALevel_FreezeTag::IsTeamFullyFrozen(const TArray<FFreezeTagAgent>& Team) const
{
    for (auto& A : Team)
        if (!A.bFrozen) return false;
    return Team.Num() > 0;
}

// ===========================================================================
// ResetLevel
// Destroys all existing agents and respawns them at fresh random positions.
// This is equivalent to a new BeginPlay for the agent state.
// ===========================================================================
void ALevel_FreezeTag::ResetLevel()
{
    bGameOver  = false;
    WinnerName = TEXT("");

    auto DestroyTeam = [](TArray<FFreezeTagAgent>& Team)
    {
        for (auto& A : Team)
            if (A.Agent) A.Agent->Destroy();
        Team.Empty();
    };

    DestroyTeam(TeamA);
    DestroyTeam(TeamB);
    DestroyTeam(TeamC);

    SpawnTeam(TeamA, TeamASpeed);
    SpawnTeam(TeamB, TeamBSpeed);
    SpawnTeam(TeamC, TeamCSpeed);
}




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
    
    DrawTeamStatus("Team A (Red)   — freezes B", TeamA, ImVec4(1.f, 0.3f, 0.3f, 1.f));
    DrawTeamStatus("Team B (Blue)  — freezes C", TeamB, ImVec4(0.3f, 0.5f, 1.f, 1.f));
    DrawTeamStatus("Team C (Green) — freezes A", TeamC, ImVec4(0.3f, 1.f,  0.4f, 1.f));

    ImGui::End();
}

void ALevel_FreezeTag::DrawTeamStatus(const char* Label,
                                       const TArray<FFreezeTagAgent>& Team,
                                       ImVec4 Color)
{
    int32 Frozen = 0;
    for (auto& A : Team) if (A.bFrozen) ++Frozen;

    ImGui::TextColored(Color, "%s", Label);
    ImGui::SameLine(240);
    ImGui::Text("%d / %d frozen", Frozen, Team.Num());

    for (int32 i = 0; i < Team.Num(); ++i)
    {
        const bool bFrz = Team[i].bFrozen;
        ImGui::TextColored(
            bFrz ? ImVec4(0.5f, 0.5f, 1.f, 1.f) : Color,
            "  [%d] %s", i, bFrz ? "FROZEN" : "active");
    }
    ImGui::Spacing();
}

// ===========================================================================
// DrawTeamDebug
// Draws a sphere at each agent's position using the team color when active
// and gray when frozen, making it easy to read team membership and freeze
// state at a glance without cluttering the viewport with steering arrows.
// ===========================================================================
void ALevel_FreezeTag::DrawTeamDebug(const TArray<FFreezeTagAgent>& Team,
                                      FColor ActiveColor, FColor FrozenColor)
{
    for (auto& A : Team)
    {
        if (!A.Agent) continue;
        FColor Col = A.bFrozen ? FrozenColor : ActiveColor;
        DrawDebugSphere(GetWorld(), A.Agent->GetActorLocation(), 30.f, 8, Col, false, -1.f, 0, 2.f);
    }
}