#include "Level_UtilityTheory.h"

ALevel_UtilityTheory::ALevel_UtilityTheory()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALevel_UtilityTheory::BeginPlay()
{
	Super::BeginPlay();
	// Utility Theory agent logic will go here
}

void ALevel_UtilityTheory::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}