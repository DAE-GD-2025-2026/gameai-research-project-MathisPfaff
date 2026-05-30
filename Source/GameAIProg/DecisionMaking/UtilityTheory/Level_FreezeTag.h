#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "Shared/Level_Base.h"
#include "Movement/SteeringBehaviors/CombinedSteering/CombinedSteeringBehaviors.h"
#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "UtilityAction.h"
#include "UtilityAIComponent.h"
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
// Priority steering agent. Owns its behavior stack and freeze state.
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
// FFreezeTagUtilityAgent
// Utility AI agent. Each agent owns its own UtilityAIComponent brain so
// contexts can be updated and scored individually every tick.
// ===========================================================================
struct FFreezeTagUtilityAgent
{
    ASteeringAgent* Agent       = nullptr;
    bool            bFrozen     = false;
    float           NormalSpeed = 300.f;

    UtilityAIComponent Brain;

    FT_PursuitAction* PursuitAction = nullptr; // raw ptrs for context updates (owned by Brain)
    FT_EvadeAction*   EvadeAction   = nullptr;
    FT_RescueAction*  RescueAction  = nullptr;
};

// ===========================================================================
// ALevel_FreezeTag
// Three-team freeze tag with a Rock-Paper-Scissors freeze cycle:
//   Team A — Fixed Utility AI    (Red)    freezes Team B
//   Team B — Custom Utility AI   (Orange) freezes Team C
//   Team C — Priority Steering   (Blue)   freezes Team A
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

    TArray<FFreezeTagUtilityAgent> TeamA; // Red    — fixed utility AI,   freezes B, frozen by C
    TArray<FFreezeTagUtilityAgent> TeamB; // Orange — custom utility AI,  freezes C, frozen by A
    TArray<FFreezeTagAgent>        TeamC; // Blue   — priority steering,  freezes A, frozen by B

    // Spawns TeamSize utility agents with a UtilityAIComponent brain.
    // bCustom=false uses fitted curves; bCustom=true starts all Linear for tuning.
    void SpawnUtilityTeam(TArray<FFreezeTagUtilityAgent>& Team, float Speed, bool bCustom);

    // Spawns TeamSize priority steering agents.
    void SpawnPSTeam(TArray<FFreezeTagAgent>& Team, float Speed);

    // Calculates FFreezeTagContext for each utility agent and runs their brain.
    // Prey and Threats are PS teams (FFreezeTagAgent).
    void UpdateUtilityContexts(TArray<FFreezeTagUtilityAgent>& Hunters,
                                TArray<FFreezeTagAgent>&        Prey,
                                TArray<FFreezeTagUtilityAgent>& Threats);

    // Overload: threat team is also a utility team
    void UpdateUtilityContexts(TArray<FFreezeTagUtilityAgent>& Hunters,
                                TArray<FFreezeTagAgent>&        Prey,
                                TArray<FFreezeTagAgent>&        Threats);
    void UpdateUtilityContexts(TArray<FFreezeTagUtilityAgent>& Hunters,
                            TArray<FFreezeTagUtilityAgent>& Prey,
                            TArray<FFreezeTagAgent>&        Threats);
    // Feeds targets into priority steering agents each tick.
    // Prey and Threats are utility teams (FFreezeTagUtilityAgent).
    void UpdatePSTargets(TArray<FFreezeTagAgent>&        Hunters,
                         TArray<FFreezeTagUtilityAgent>& Prey,
                         TArray<FFreezeTagUtilityAgent>& Threats);

    // Tagging: utility hunters freeze PS prey
    void CheckTagging(TArray<FFreezeTagUtilityAgent>& Hunters, TArray<FFreezeTagAgent>& Prey);
    // Tagging: PS hunters freeze utility prey
    void CheckTagging(TArray<FFreezeTagAgent>& Hunters, TArray<FFreezeTagUtilityAgent>& Prey);
    void CheckTagging(TArray<FFreezeTagUtilityAgent>& Hunters, TArray<FFreezeTagUtilityAgent>& Prey);
    
    void UnfreezeCheck(TArray<FFreezeTagUtilityAgent>& Team);
    void UnfreezeCheck(TArray<FFreezeTagAgent>& Team);

    void EnforceFrozen(TArray<FFreezeTagUtilityAgent>& Team);
    void EnforceFrozen(TArray<FFreezeTagAgent>& Team);

    bool IsTeamFullyFrozen(const TArray<FFreezeTagUtilityAgent>& Team) const;
    bool IsTeamFullyFrozen(const TArray<FFreezeTagAgent>& Team) const;

    void ResetLevel();
    void DrawImGui();

    void DrawTeamStatus(const char* Label, const TArray<FFreezeTagUtilityAgent>& Team, ImVec4 Color);
    void DrawTeamStatus(const char* Label, const TArray<FFreezeTagAgent>& Team, ImVec4 Color);

    void DrawTeamDebug(const TArray<FFreezeTagUtilityAgent>& Team, FColor ActiveColor, FColor FrozenColor);
    void DrawTeamDebug(const TArray<FFreezeTagAgent>& Team, FColor ActiveColor, FColor FrozenColor);

    // Draws the per-agent utility score inspector for Team B (custom team).
    void DrawCustomTeamInspector();

    // ===========================================================================
    // Tuning — adjustable at runtime via ImGui
    // ===========================================================================
    float TagRadius       = 80.f;    // contact distance for tagging and unfreezing
    float EvadeRadius     = 500.f;   // PS: threat must be within this to trigger evade
    int32 RescueThreshold = 3;       // PS: frozen teammates needed before rescue takes priority
    float MaxRelevantDist = 1500.f;  // utility: distance at which proximity normalizes to 0
    float RescueRadius    = 700.f;   // utility: search radius for FrozenTeammateNearby

    float TeamASpeed = 250.f;
    float TeamBSpeed = 250.f;
    float TeamCSpeed = 250.f;

    float TrimWorldSize = 1000.f;

    bool    bGameOver  = false;
    FString WinnerName;

    int32 SelectedCustomAgent = 0;
};