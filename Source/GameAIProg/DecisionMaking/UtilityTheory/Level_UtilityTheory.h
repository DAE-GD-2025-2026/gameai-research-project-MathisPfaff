#pragma once

#include "CoreMinimal.h"
#include "Shared/Level_Base.h"
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
};