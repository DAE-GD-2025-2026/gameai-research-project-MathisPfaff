// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldTrimVolume.h"

#include "Kismet/KismetMathLibrary.h"


// Sets default values
AWorldTrimVolume::AWorldTrimVolume()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	TrimVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TrimVolume"));
	TrimVolume->SetHiddenInGame(true);

	RootComponent = TrimVolume;

	SetTrimWorldSize(TrimWorldSize);
}

// Called when the game starts or when spawned
void AWorldTrimVolume::BeginPlay()
{
	Super::BeginPlay();
}

void AWorldTrimVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (!bShouldTrimWorld) return;
	
	FVector Origin;
	FVector BoxExtent;
	GetActorBounds(false, Origin, BoxExtent);
	FVector2D const TopRight{Origin.X + BoxExtent.X, Origin.Y + BoxExtent.Y};
	FVector2D const BottomLeft{Origin.X - BoxExtent.X, Origin.Y - BoxExtent.Y};
	
	FVector2D NewPos = FVector2D(OtherActor->GetActorLocation().X, OtherActor->GetActorLocation().Y);
	if (bIsWorldLooping)
	{
		if (NewPos.X > TopRight.X)
			NewPos.X = BottomLeft.X;
		else if (NewPos.X < BottomLeft.X)
			NewPos.X = TopRight.X;

		if (NewPos.Y > TopRight.Y)
			NewPos.Y = BottomLeft.Y;
		else if (NewPos.Y < BottomLeft.Y)
			NewPos.Y = TopRight.Y;
	}
	else
	{
		NewPos.X = UKismetMathLibrary::Clamp(NewPos.X, BottomLeft.X, TopRight.Y);
		NewPos.Y = UKismetMathLibrary::Clamp(NewPos.Y, BottomLeft.Y, TopRight.Y);
	}

	OtherActor->SetActorLocation(FVector{NewPos, OtherActor->GetActorLocation().Z});

}

// Called every frame
void AWorldTrimVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bShouldTrimWorld) return;
	
	FVector Origin;
	FVector BoxExtent;
	GetActorBounds(false, Origin, BoxExtent);
	BoxExtent.Z = 1;
	
	DrawDebugBox(GetWorld(), Origin, BoxExtent, FColor::Red);
}

void AWorldTrimVolume::SetTrimWorldSize(float NewSize)
{
	TrimWorldSize = NewSize;
	TrimVolume->SetBoxExtent(FVector(TrimWorldSize, TrimWorldSize, 5000));
}

