// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorTypes.h"

// ============================================================================
// FHktAnimIntent
// ============================================================================

bool FHktAnimIntent::FromTag(const FGameplayTag& Tag, FHktAnimIntent& OutIntent)
{
	if (!Tag.IsValid()) return false;

	FString TagStr = Tag.ToString();
	if (!TagStr.StartsWith(TEXT("Anim."))) return false;

	// Anim.{Layer}.{Type}.{Name}
	TArray<FString> Parts;
	TagStr.ParseIntoArray(Parts, TEXT("."));

	OutIntent.AnimTag = Tag;

	if (Parts.Num() >= 2) OutIntent.Layer = Parts[1]; // FullBody, UpperBody, Montage
	if (Parts.Num() >= 3) OutIntent.Type = Parts[2];  // Locomotion, Action, Combat, Cast
	if (Parts.Num() >= 4) OutIntent.Name = Parts[3];  // Run, Idle, Slash, Fireball

	// 이름이 없으면 타입을 이름으로
	if (OutIntent.Name.IsEmpty() && !OutIntent.Type.IsEmpty())
	{
		OutIntent.Name = OutIntent.Type;
	}

	return true;
}

// ============================================================================
// FHktCharacterIntent
// ============================================================================

bool FHktCharacterIntent::FromTag(const FGameplayTag& Tag, FHktCharacterIntent& OutIntent)
{
	if (!Tag.IsValid()) return false;

	FString TagStr = Tag.ToString();
	if (!TagStr.StartsWith(TEXT("Entity."))) return false;

	// Entity.Character.{Name} 또는 Entity.{Type}.{Name}
	TArray<FString> Parts;
	TagStr.ParseIntoArray(Parts, TEXT("."));

	OutIntent.EntityTag = Tag;

	if (Parts.Num() >= 3)
	{
		OutIntent.Name = Parts.Last();
	}
	else if (Parts.Num() >= 2)
	{
		OutIntent.Name = Parts[1];
	}

	// 기본 스켈레톤 타입 추론
	if (Parts.Num() >= 2)
	{
		FString TypeStr = Parts[1].ToLower();
		if (TypeStr == TEXT("quadruped") || TypeStr == TEXT("beast") || TypeStr == TEXT("mount"))
		{
			OutIntent.SkeletonType = EHktSkeletonType::Quadruped;
		}
		else
		{
			OutIntent.SkeletonType = EHktSkeletonType::Humanoid;
		}
	}

	return true;
}

// ============================================================================
// FHktItemIntent
// ============================================================================

bool FHktItemIntent::FromTag(const FGameplayTag& Tag, FHktItemIntent& OutIntent)
{
	if (!Tag.IsValid()) return false;

	FString TagStr = Tag.ToString();
	if (!TagStr.StartsWith(TEXT("Entity.Item."))) return false;

	// Entity.Item.{Category}.{SubType}[.{Element}]
	TArray<FString> Parts;
	TagStr.ParseIntoArray(Parts, TEXT("."));

	OutIntent.ItemTag = Tag;

	if (Parts.Num() >= 3) OutIntent.Category = Parts[2]; // Weapon, Armor, Accessory
	if (Parts.Num() >= 4) OutIntent.SubType = Parts[3];  // Sword, Shield, Ring
	if (Parts.Num() >= 5) OutIntent.Element = Parts[4];  // Fire, Ice, etc.

	return true;
}
