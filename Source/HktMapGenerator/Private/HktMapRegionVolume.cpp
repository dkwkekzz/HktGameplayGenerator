// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapRegionVolume.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktRegion, Log, All);

AHktMapRegionVolume::AHktMapRegionVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBox"));
	BoxComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BoxComponent->SetGenerateOverlapEvents(true);
	BoxComponent->SetCanEverAffectNavigation(false);

#if WITH_EDITORONLY_DATA
	BoxComponent->SetLineThickness(2.f);
	BoxComponent->ShapeColor = FColor::Cyan;
#endif

	RootComponent = BoxComponent;
}

void AHktMapRegionVolume::InitFromRegionData(const FString& Name, FVector Center, FVector Extent, const TMap<FString, FString>& Props)
{
	RegionName = Name;
	Properties = Props;
	SetActorLocation(Center);
	BoxComponent->SetBoxExtent(Extent);
}

bool AHktMapRegionVolume::ContainsPoint(FVector WorldPoint) const
{
	FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	FVector Ext = BoxComponent->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Ext.X
		&& FMath::Abs(Local.Y) <= Ext.Y
		&& FMath::Abs(Local.Z) <= Ext.Z;
}

void AHktMapRegionVolume::BeginPlay()
{
	Super::BeginPlay();

	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AHktMapRegionVolume::OnBoxBeginOverlap);
	BoxComponent->OnComponentEndOverlap.AddDynamic(this, &AHktMapRegionVolume::OnBoxEndOverlap);
}

void AHktMapRegionVolume::OnBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		UE_LOG(LogHktRegion, Log, TEXT("Player entered region '%s'"), *RegionName);
		// Streaming subsystem will handle activation via delegate or polling
	}
}

void AHktMapRegionVolume::OnBoxEndOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		UE_LOG(LogHktRegion, Log, TEXT("Player exited region '%s'"), *RegionName);
	}
}
