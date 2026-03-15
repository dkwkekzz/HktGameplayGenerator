// Copyright Hkt Studios, Inc. All Rights Reserved.
// Animation Generator 타입 정의

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktAnimGeneratorTypes.generated.h"

/**
 * 스켈레톤 풀 타입.
 * 모든 캐릭터가 소수의 Base Skeleton을 공유.
 */
UENUM(BlueprintType)
enum class EHktSkeletonType : uint8
{
	Humanoid,      // 인간형 (2족보행)
	Quadruped,     // 사족보행
	Custom,        // 특수
};

/**
 * FHktAnimIntent — 애니메이션 생성 의도
 *
 * Tag 파싱 결과:
 *   "Anim.FullBody.Locomotion.Run" → Layer=FullBody, Type=Locomotion, Name=Run
 *   "Anim.Montage.Attack.Slash"    → Layer=Montage, Type=Attack, Name=Slash
 */
USTRUCT(BlueprintType)
struct HKTGENERATORCORE_API FHktAnimIntent
{
	GENERATED_BODY()

	/** 원본 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FGameplayTag AnimTag;

	/** 레이어: FullBody, UpperBody, Montage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString Layer;

	/** 타입: Locomotion, Action, Combat, Cast 등 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString Type;

	/** 이름: Run, Idle, Slash, Fireball 등 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString Name;

	/** 대상 스켈레톤 타입 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	EHktSkeletonType SkeletonType = EHktSkeletonType::Humanoid;

	/** 스타일 힌트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	TArray<FString> StyleKeywords;

	/** Tag에서 파싱 */
	static bool FromTag(const FGameplayTag& Tag, FHktAnimIntent& OutIntent);
};

/**
 * FHktCharacterIntent — 캐릭터 메시 생성 의도
 */
USTRUCT(BlueprintType)
struct HKTGENERATORCORE_API FHktCharacterIntent
{
	GENERATED_BODY()

	/** 원본 태그 (Entity.Character.Goblin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FGameplayTag EntityTag;

	/** 종족/이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString Name;

	/** 체형 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	EHktSkeletonType SkeletonType = EHktSkeletonType::Humanoid;

	/** 스타일 힌트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	TArray<FString> StyleKeywords;

	static bool FromTag(const FGameplayTag& Tag, FHktCharacterIntent& OutIntent);
};

/**
 * FHktItemIntent — 아이템 생성 의도
 */
USTRUCT(BlueprintType)
struct HKTGENERATORCORE_API FHktItemIntent
{
	GENERATED_BODY()

	/** 원본 태그 (Entity.Item.Weapon.Sword.Fire) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FGameplayTag ItemTag;

	/** 카테고리: Weapon, Armor, Accessory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString Category;

	/** 서브타입: Sword, Shield, Ring 등 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString SubType;

	/** 속성: Fire, Ice 등 (빈 문자열이면 무속성) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	FString Element;

	/** 등급 (0.0=Common ~ 1.0=Legendary) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent", meta = (ClampMin="0.0", ClampMax="1.0"))
	float Rarity = 0.2f;

	/** 스타일 힌트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent")
	TArray<FString> StyleKeywords;

	static bool FromTag(const FGameplayTag& Tag, FHktItemIntent& OutIntent);
};
