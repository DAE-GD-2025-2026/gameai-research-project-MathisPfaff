// Fill out your copyright notice in the Description page of Project Settings.
//#pragma optimize("", off)
#include "SteeringAgent.h"

#include "AIController.h"


// Sets default values
ASteeringAgent::ASteeringAgent()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ASteeringAgent::BeginPlay()
{
	Super::BeginPlay();
}

void ASteeringAgent::BeginDestroy()
{
	Super::BeginDestroy();
}

// Called every frame
void ASteeringAgent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (SteeringBehavior)
	{
		SteeringOutput output = SteeringBehavior->CalculateSteering(DeltaTime, *this);
		if (output.IsValid)
		{
			AddMovementInput(FVector(output.LinearVelocity, 0));
			
			if (!IsAutoOrienting())
			{
				if (AAIController* AIController = Cast<AAIController>(GetController()))
				{
					float const DeltaYaw = FMath::Clamp(output.AngularVelocity, -1.f, 1.f)
					* GetMaxAngularSpeed() * DeltaTime;
					
					float const CurrentSpeed = GetMaxAngularSpeed();
					
					FRotator const CurrentRotation{GetActorForwardVector().ToOrientationRotator()};
					FRotator const DeltaRotation{0, DeltaYaw, 0};
					FRotator const DesiredRotation{CurrentRotation + DeltaRotation};
					
					if (!FMath::IsNearlyEqual(DesiredRotation.Yaw, CurrentRotation.Yaw))
					{
						AIController->SetControlRotation(DesiredRotation);
						FaceRotation(DesiredRotation);
					}
				}
			}
		}
	}
}

// Called to bind functionality to input
void ASteeringAgent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASteeringAgent::SetSteeringBehavior(ISteeringBehavior* NewSteeringBehavior)
{
	SteeringBehavior = NewSteeringBehavior;
}

