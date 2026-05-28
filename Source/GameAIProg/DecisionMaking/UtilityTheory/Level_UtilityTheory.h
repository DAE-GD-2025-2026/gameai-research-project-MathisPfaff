#pragma once

#include "CoreMinimal.h"
#include "Shared/Level_Base.h"
#include "UtilityAction.h"
#include "UtilityAIComponent.h"
#include "Level_UtilityTheory.generated.h"

UCLASS()
class GAMEAIPROG_API ALevel_UtilityTheory : public ALevel_Base
{
	GENERATED_BODY()

public:
	ALevel_UtilityTheory();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	// --- Agents ---
	UPROPERTY()
	ASteeringAgent* SeekAgent    = nullptr;   // "it" — seeks the player directly
	UPROPERTY()
	ASteeringAgent* UtilityAgent = nullptr;   // driven by utility brain

	// --- Seek agent owns its behavior ---
	Seek m_SeekBehavior;

	// --- Utility brain ---
	UtilityAIComponent Brain;

	// Typed pointers so we can update context each tick
	UAPursuitAction* PursuitAction = nullptr;
	UAEvadeAction*   EvadeAction   = nullptr;
	UAWanderAction*  WanderAction  = nullptr;

	bool bSeekAgentIsIt = true;
	FTargetData SeekMouseTarget;

	// How far before the seeker is considered "dangerous"
	float MaxRelevantDistance = 1500.f;

	void UpdateContexts();
	void CheckTagging();
	void DrawImGui();
	
	// Tagging cooldown
	float TagFreezeTime     = 2.f;
	float FreezeTimer       = 0.f;
	bool  bUtilityAgentFrozen = false;  // only freeze the utility agent (the one being chased)
	
	float SeekAgentSpeed    = 400.f;
	float UtilityAgentSpeed = 300.f;
};