#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "Shared/Level_Base.h"
#include "Movement/SteeringBehaviors/CombinedSteering/CombinedSteeringBehaviors.h"
#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "Level_FreezeTag.generated.h"

// ===========================================================================
// NullSteering
// Always returns zero output — used to disable a priority slot without
// removing it from the stack. PrioritySteering falls through to the next
// behavior when it sees a zero result.
// ===========================================================================
class NullSteering : public ISteeringBehavior
{
public:
    SteeringOutput CalculateSteering(float, ASteeringAgent&) override { return {}; }
};

// ===========================================================================
// FFreezeTagAgent
// Bundles an agent pointer with its owned steering behaviors and freeze state.
// Ownership lives here (unique_ptr) so the TArray can be emptied and refilled
// on reset without any dangling pointers.
//
// Priority stack (highest first):
//   Rescue  — Seek toward nearest frozen teammate
//   Evade   — Evade nearest threat that can freeze this team
//   Pursuit — Pursuit nearest unfrozen prey this team can freeze
// ===========================================================================
struct FFreezeTagAgent
{
    ASteeringAgent* Agent       = nullptr;
    bool            bFrozen     = false;
    float           NormalSpeed = 300.f;

    
    std::unique_ptr<Seek>             OwnedRescue;   // priority 0: rescue frozen teammate
    std::unique_ptr<Evade>            OwnedEvade;    // priority 1: flee incoming threat
    std::unique_ptr<Pursuit>          OwnedPursuit;  // priority 2: chase freezable prey
    std::unique_ptr<PrioritySteering> OwnedPS;

    
    Seek*    RescueB  = nullptr;
    Evade*   EvadeB   = nullptr;
    Pursuit* PursuitB = nullptr;
};

// ===========================================================================
// ALevel_FreezeTag
// Three-team freeze tag with a Rock-Paper-Scissors freeze cycle:
//   Team A (Red)   freezes Team B
//   Team B (Blue)  freezes Team C
//   Team C (Green) freezes Team A
//
// A team wins by freezing all members of the team it preys on.
// Frozen agents keep their AI running but have MaxLinearSpeed = 0.
// An unfrozen teammate can unfreeze them by moving adjacent.
// ===========================================================================
UCLASS()
class GAMEAIPROG_API ALevel_FreezeTag : public ALevel_Base
{
    GENERATED_BODY()

public:
    ALevel_FreezeTag();
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    static constexpr int32 TeamSize = 5;

    TArray<FFreezeTagAgent> TeamA; // Red   — freezes B, frozen by C
    TArray<FFreezeTagAgent> TeamB; // Blue  — freezes C, frozen by A
    TArray<FFreezeTagAgent> TeamC; // Green — freezes A, frozen by B

    
    
    void SpawnTeam(TArray<FFreezeTagAgent>& Team, float Speed);

    
    
    
    
    void UpdatePSTargets(TArray<FFreezeTagAgent>& Hunters,
                         TArray<FFreezeTagAgent>& Prey,
                         TArray<FFreezeTagAgent>& Threats);

    
    void CheckTagging(TArray<FFreezeTagAgent>& Hunters, TArray<FFreezeTagAgent>& Prey);

    
    void UnfreezeCheck(TArray<FFreezeTagAgent>& Team);

    
    
    void EnforceFrozen(TArray<FFreezeTagAgent>& Team);

    
    bool IsTeamFullyFrozen(const TArray<FFreezeTagAgent>& Team) const;

    
    void ResetLevel();

    void DrawImGui();
    void DrawTeamStatus(const char* Label, const TArray<FFreezeTagAgent>& Team, ImVec4 Color);

    
    void DrawTeamDebug(const TArray<FFreezeTagAgent>& Team, FColor ActiveColor, FColor FrozenColor);

    // ---------------------------------------------------------------------------
    // Tuning — adjustable at runtime via ImGui
    // ---------------------------------------------------------------------------
    float EvadeRadius     = 500.f;  // threat must be within this distance to trigger evade
    int32 RescueThreshold = 3;      // frozen teammates needed before rescue takes priority
    float TagRadius       = 80.f;   // contact distance for tagging and unfreezing

    float TeamASpeed = 300.f;
    float TeamBSpeed = 300.f;
    float TeamCSpeed = 300.f;

    float TrimWorldSize = 1000.f;   // cached so the ImGui slider has a value to bind to

    bool    bGameOver  = false;
    FString WinnerName;
};