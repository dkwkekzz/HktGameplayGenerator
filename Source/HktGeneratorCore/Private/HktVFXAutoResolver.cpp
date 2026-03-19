// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXAutoResolver.h"

bool FHktVFXAutoResolver::ParseTagToIntent(const FGameplayTag& VFXTag, FHktVFXIntent& OutIntent)
{
	if (!VFXTag.IsValid()) return false;

	FString TagStr = VFXTag.ToString();
	if (!TagStr.StartsWith(TEXT("VFX."))) return false;

	// VFX.{EventType}.{Element}[.{Surface}] 파싱
	TArray<FString> Parts;
	TagStr.ParseIntoArray(Parts, TEXT("."));

	if (Parts.Num() < 2) return false;

	// VFX.Custom.{Description}
	if (Parts.Num() >= 2 && Parts[1] == TEXT("Custom"))
	{
		OutIntent.EventType = EHktVFXEventType::Custom;
		OutIntent.Element = EHktVFXElement::Physical;
		if (Parts.Num() >= 3)
		{
			// 나머지 부분을 CustomDescription으로 결합
			FString Desc;
			for (int32 i = 2; i < Parts.Num(); ++i)
			{
				if (!Desc.IsEmpty()) Desc += TEXT(" ");
				Desc += Parts[i];
			}
			OutIntent.CustomDescription = Desc;
		}
		return true;
	}

	// VFX.Niagara.{Name} — Niagara 직접 참조 태그 (이름 기반)
	if (Parts.Num() >= 3 && Parts[1] == TEXT("Niagara"))
	{
		OutIntent.EventType = EHktVFXEventType::Custom;
		OutIntent.Element = EHktVFXElement::Physical;
		OutIntent.CustomDescription = Parts[2];
		OutIntent.Intensity = 0.5f;
		OutIntent.Radius = 200.f;
		OutIntent.Duration = 1.0f;
		OutIntent.SourcePower = 0.5f;
		return true;
	}

	// VFX.{EventType}
	if (Parts.Num() >= 2)
	{
		OutIntent.EventType = ParseEventType(Parts[1]);
	}

	// VFX.{EventType}.{Element}
	if (Parts.Num() >= 3)
	{
		OutIntent.Element = ParseElement(Parts[2]);
	}
	else
	{
		OutIntent.Element = EHktVFXElement::Physical;
	}

	// VFX.{EventType}.{Element}.{Surface}
	if (Parts.Num() >= 4)
	{
		OutIntent.SurfaceType = ParseSurfaceType(Parts[3]);
	}

	// 기본값
	OutIntent.Intensity = 0.5f;
	OutIntent.Radius = 200.f;
	OutIntent.Duration = 1.0f;
	OutIntent.SourcePower = 0.5f;

	return true;
}

FString FHktVFXAutoResolver::GetAssetKeyFromTag(const FGameplayTag& VFXTag)
{
	FHktVFXIntent Intent;
	if (ParseTagToIntent(VFXTag, Intent))
	{
		return Intent.GetAssetKey();
	}
	// 폴백: 태그 문자열에서 직접 키 생성
	FString Key = VFXTag.ToString();
	Key.ReplaceInline(TEXT("."), TEXT("_"));
	return Key;
}

EHktVFXEventType FHktVFXAutoResolver::ParseEventType(const FString& Str)
{
	FString Lower = Str.ToLower();
	if (Lower == TEXT("explosion"))      return EHktVFXEventType::Explosion;
	if (Lower == TEXT("projectilehit") || Lower == TEXT("hit") || Lower == TEXT("directhit"))
		return EHktVFXEventType::ProjectileHit;
	if (Lower == TEXT("projectiletrail") || Lower == TEXT("trail"))
		return EHktVFXEventType::ProjectileTrail;
	if (Lower == TEXT("areaeffect") || Lower == TEXT("area"))
		return EHktVFXEventType::AreaEffect;
	if (Lower == TEXT("buff"))           return EHktVFXEventType::Buff;
	if (Lower == TEXT("debuff"))         return EHktVFXEventType::Debuff;
	if (Lower == TEXT("heal"))           return EHktVFXEventType::Heal;
	if (Lower == TEXT("summon") || Lower == TEXT("spawn") || Lower == TEXT("spawneffect"))
		return EHktVFXEventType::Summon;
	if (Lower == TEXT("teleport"))       return EHktVFXEventType::Teleport;
	if (Lower == TEXT("shield"))         return EHktVFXEventType::Shield;
	if (Lower == TEXT("channel"))        return EHktVFXEventType::Channel;
	if (Lower == TEXT("death"))          return EHktVFXEventType::Death;
	if (Lower == TEXT("levelup"))        return EHktVFXEventType::LevelUp;
	return EHktVFXEventType::Custom;
}

EHktVFXElement FHktVFXAutoResolver::ParseElement(const FString& Str)
{
	FString Lower = Str.ToLower();
	if (Lower == TEXT("fire"))      return EHktVFXElement::Fire;
	if (Lower == TEXT("ice"))       return EHktVFXElement::Ice;
	if (Lower == TEXT("lightning")) return EHktVFXElement::Lightning;
	if (Lower == TEXT("water"))     return EHktVFXElement::Water;
	if (Lower == TEXT("earth"))     return EHktVFXElement::Earth;
	if (Lower == TEXT("wind"))      return EHktVFXElement::Wind;
	if (Lower == TEXT("dark"))      return EHktVFXElement::Dark;
	if (Lower == TEXT("holy"))      return EHktVFXElement::Holy;
	if (Lower == TEXT("poison"))    return EHktVFXElement::Poison;
	if (Lower == TEXT("arcane"))    return EHktVFXElement::Arcane;
	if (Lower == TEXT("physical"))  return EHktVFXElement::Physical;
	if (Lower == TEXT("nature"))    return EHktVFXElement::Nature;
	return EHktVFXElement::Physical;
}

EHktVFXSurfaceType FHktVFXAutoResolver::ParseSurfaceType(const FString& Str)
{
	FString Lower = Str.ToLower();
	if (Lower == TEXT("stone"))  return EHktVFXSurfaceType::Stone;
	if (Lower == TEXT("metal"))  return EHktVFXSurfaceType::Metal;
	if (Lower == TEXT("wood"))   return EHktVFXSurfaceType::Wood;
	if (Lower == TEXT("dirt"))   return EHktVFXSurfaceType::Dirt;
	if (Lower == TEXT("sand"))   return EHktVFXSurfaceType::Sand;
	if (Lower == TEXT("water"))  return EHktVFXSurfaceType::Water;
	if (Lower == TEXT("snow"))   return EHktVFXSurfaceType::Snow;
	if (Lower == TEXT("grass"))  return EHktVFXSurfaceType::Grass;
	return EHktVFXSurfaceType::None;
}
