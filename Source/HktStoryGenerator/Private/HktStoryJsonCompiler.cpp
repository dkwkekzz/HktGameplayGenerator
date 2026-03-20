// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryJsonCompiler.h"
#include "HktStoryBuilder.h"
#include "HktStoryTypes.h"
#include "HktStoryOpRegistry.h"
#include "HktCoreProperties.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStoryJsonCompiler, Log, All);

// ============================================================================
// 태그 자동 등록 + INI 영속화
// ============================================================================

/**
 * 프로젝트 DefaultGameplayTags.ini에 태그를 영속화.
 * 에디터 재시작 시에도 태그가 유지되도록 보장.
 */
static void PersistTagToProjectConfig(const FString& TagName)
{
	const FString IniPath = FPaths::ProjectConfigDir() / TEXT("DefaultGameplayTags.ini");
	const FString Section = TEXT("/Script/GameplayTags.GameplayTagsSettings");
	const FString Key = TEXT("GameplayTagList");
	const FString Entry = FString::Printf(TEXT("(Tag=\"%s\",DevComment=\"Auto-registered by HktStoryGenerator\")"), *TagName);

	TArray<FString> Existing;
	GConfig->GetArray(*Section, *Key, Existing, IniPath);
	for (const FString& E : Existing)
	{
		if (E.Contains(TagName))
		{
			return;
		}
	}

	Existing.Add(Entry);
	GConfig->SetArray(*Section, *Key, Existing, IniPath);
	GConfig->Flush(false, IniPath);

	UE_LOG(LogHktStoryJsonCompiler, Log, TEXT("Persisted tag to %s: %s"), *IniPath, *TagName);
}

/**
 * GameplayTag가 유효한지 확인하고, 없으면 동적 등록 + INI 영속화.
 */
static FGameplayTag EnsureTagRegistered(const FString& TagName)
{
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
	if (Tag.IsValid())
	{
		return Tag;
	}

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	Tag = Manager.AddNativeGameplayTag(FName(*TagName), TEXT("Auto-registered by StoryJsonCompiler"));

	if (Tag.IsValid())
	{
		UE_LOG(LogHktStoryJsonCompiler, Log, TEXT("Auto-registered GameplayTag: %s"), *TagName);
		PersistTagToProjectConfig(TagName);
	}
	else
	{
		UE_LOG(LogHktStoryJsonCompiler, Warning, TEXT("Failed to auto-register GameplayTag: %s"), *TagName);
	}

	return Tag;
}

// ============================================================================
// 태그 해결
// ============================================================================

FGameplayTag FHktStoryJsonCompiler::ResolveTag(const FString& TagStr, const TMap<FString, FGameplayTag>& TagAliases)
{
	if (const FGameplayTag* Found = TagAliases.Find(TagStr))
	{
		return *Found;
	}
	return EnsureTagRegistered(TagStr);
}

// ============================================================================
// ApplyStep — FHktStoryOpRegistry 기반 자동 dispatch
// ============================================================================

bool FHktStoryJsonCompiler::ApplyStep(
	FHktStoryBuilder& Builder,
	const TSharedPtr<FJsonObject>& Step,
	const TMap<FString, FGameplayTag>& TagAliases,
	TArray<FGameplayTag>& OutReferencedTags,
	TArray<FString>& OutErrors,
	int32 StepIndex)
{
	FString Op = Step->GetStringField(TEXT("op"));

	// 레지스트리에서 Operation 정의 검색
	const FHktStoryOpDef* OpDef = FHktStoryOpRegistry::Get().Find(Op);
	if (!OpDef)
	{
		OutErrors.Add(FString::Printf(TEXT("Step %d: unknown operation '%s'"), StepIndex, *Op));
		return false;
	}

	// 파라미터 파싱: OpDef의 파라미터 정의에 따라 JSON → FHktStoryOpArg 변환
	TMap<FString, FHktStoryOpArg> ParsedArgs;
	bool bHasError = false;

	for (const FHktStoryParamDef& ParamDef : OpDef->Params)
	{
		FHktStoryOpArg Arg;

		switch (ParamDef.Type)
		{
		case EHktStoryParamType::Register:
		{
			FString Val;
			if (!Step->TryGetStringField(ParamDef.Name, Val))
			{
				if (ParamDef.bOptional)
				{
					Arg.RegIdx = ParamDef.DefaultInt;
				}
				else
				{
					OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *ParamDef.Name));
					bHasError = true;
					continue;
				}
			}
			else
			{
				Arg.RegIdx = FHktStoryOpRegistry::ParseRegister(Val);
				if (Arg.RegIdx < 0)
				{
					OutErrors.Add(FString::Printf(TEXT("Step %d (%s): invalid register '%s'"), StepIndex, *Op, *Val));
					bHasError = true;
					continue;
				}
			}
			break;
		}
		case EHktStoryParamType::Int:
		{
			double Val;
			if (Step->TryGetNumberField(ParamDef.Name, Val))
			{
				Arg.IntVal = static_cast<int32>(Val);
			}
			else if (ParamDef.bOptional)
			{
				Arg.IntVal = ParamDef.DefaultInt;
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *ParamDef.Name));
				bHasError = true;
				continue;
			}
			break;
		}
		case EHktStoryParamType::Float:
		{
			double Val;
			if (Step->TryGetNumberField(ParamDef.Name, Val))
			{
				Arg.FloatVal = static_cast<float>(Val);
			}
			else if (ParamDef.bOptional)
			{
				Arg.FloatVal = ParamDef.DefaultFloat;
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *ParamDef.Name));
				bHasError = true;
				continue;
			}
			break;
		}
		case EHktStoryParamType::String:
		{
			Step->TryGetStringField(ParamDef.Name, Arg.StrVal);
			break;
		}
		case EHktStoryParamType::Tag:
		{
			FString Val;
			if (!Step->TryGetStringField(ParamDef.Name, Val))
			{
				if (!ParamDef.bOptional)
				{
					OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *ParamDef.Name));
					bHasError = true;
					continue;
				}
			}
			else
			{
				Arg.TagVal = ResolveTag(Val, TagAliases);
				if (Arg.TagVal.IsValid())
				{
					OutReferencedTags.AddUnique(Arg.TagVal);
				}
			}
			break;
		}
		case EHktStoryParamType::PropertyId:
		{
			FString Val;
			if (!Step->TryGetStringField(ParamDef.Name, Val))
			{
				if (!ParamDef.bOptional)
				{
					OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *ParamDef.Name));
					bHasError = true;
					continue;
				}
			}
			else
			{
				Arg.PropId = FHktStoryOpRegistry::ParsePropertyId(Val);
				if (Arg.PropId == 0xFFFF)
				{
					OutErrors.Add(FString::Printf(TEXT("Step %d (%s): invalid PropertyId '%s'"), StepIndex, *Op, *Val));
					bHasError = true;
					continue;
				}
			}
			break;
		}
		}

		ParsedArgs.Add(ParamDef.Name, MoveTemp(Arg));
	}

	if (bHasError)
	{
		return false;
	}

	// 핸들러 호출
	OpDef->Handler(Builder, ParsedArgs);
	return true;
}

// ============================================================================
// CompileAndRegister
// ============================================================================

FHktStoryCompileResult FHktStoryJsonCompiler::CompileAndRegister(const FString& JsonStr)
{
	FHktStoryCompileResult Result;

	// Parse JSON
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Result.Errors.Add(TEXT("Invalid JSON syntax"));
		return Result;
	}

	// Story tag
	FString StoryTagStr;
	if (!Root->TryGetStringField(TEXT("storyTag"), StoryTagStr) || StoryTagStr.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing or empty 'storyTag' field"));
		return Result;
	}
	Result.StoryTag = StoryTagStr;
	EnsureTagRegistered(StoryTagStr);

	// Parse tag aliases
	TMap<FString, FGameplayTag> TagAliases;
	const TSharedPtr<FJsonObject>* TagsObj;
	if (Root->TryGetObjectField(TEXT("tags"), TagsObj))
	{
		for (const auto& Pair : (*TagsObj)->Values)
		{
			FString TagName = Pair.Value->AsString();
			FGameplayTag Tag = EnsureTagRegistered(TagName);
			if (!Tag.IsValid())
			{
				Result.Warnings.Add(FString::Printf(TEXT("Tag '%s' (%s) could not be registered"), *Pair.Key, *TagName));
			}
			TagAliases.Add(Pair.Key, Tag);
		}
	}

	// Create builder
	FHktStoryBuilder Builder = FHktStoryBuilder::Create(FName(*StoryTagStr));

	// CancelOnDuplicate policy
	bool bCancelOnDuplicate = false;
	if (Root->TryGetBoolField(TEXT("cancelOnDuplicate"), bCancelOnDuplicate) && bCancelOnDuplicate)
	{
		Builder.CancelOnDuplicate();
	}

	// Parse steps
	const TArray<TSharedPtr<FJsonValue>>* Steps;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps))
	{
		Result.Errors.Add(TEXT("Missing 'steps' array"));
		return Result;
	}

	for (int32 i = 0; i < Steps->Num(); ++i)
	{
		const TSharedPtr<FJsonObject>* StepObj;
		if (!(*Steps)[i]->TryGetObject(StepObj))
		{
			Result.Errors.Add(FString::Printf(TEXT("Step %d: not a JSON object"), i));
			continue;
		}

		ApplyStep(Builder, *StepObj, TagAliases, Result.ReferencedTags, Result.Errors, i);
	}

	if (Result.Errors.Num() > 0)
	{
		return Result;
	}

	// Build and register
	Builder.BuildAndRegister();
	Result.bSuccess = true;

	UE_LOG(LogHktStoryJsonCompiler, Log, TEXT("Story compiled and registered: %s (%d steps, %d tags referenced)"),
		*StoryTagStr, Steps->Num(), Result.ReferencedTags.Num());

	return Result;
}

// ============================================================================
// Validate — FHktStoryOpRegistry 기반 자동 검증
// ============================================================================

FHktStoryCompileResult FHktStoryJsonCompiler::Validate(const FString& JsonStr)
{
	FHktStoryCompileResult Result;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Result.Errors.Add(TEXT("Invalid JSON syntax"));
		return Result;
	}

	FString StoryTagStr;
	if (!Root->TryGetStringField(TEXT("storyTag"), StoryTagStr) || StoryTagStr.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing or empty 'storyTag'"));
		return Result;
	}
	Result.StoryTag = StoryTagStr;
	EnsureTagRegistered(StoryTagStr);

	// Parse tag aliases
	TMap<FString, FGameplayTag> TagAliases;
	const TSharedPtr<FJsonObject>* TagsObj;
	if (Root->TryGetObjectField(TEXT("tags"), TagsObj))
	{
		for (const auto& Pair : (*TagsObj)->Values)
		{
			TagAliases.Add(Pair.Key, EnsureTagRegistered(Pair.Value->AsString()));
		}
	}

	// Validate steps — 레지스트리에서 유효 op 목록 자동 생성
	const TArray<TSharedPtr<FJsonValue>>* Steps;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps))
	{
		Result.Errors.Add(TEXT("Missing 'steps' array"));
		return Result;
	}

	const TSet<FString> ValidOps = FHktStoryOpRegistry::Get().GetValidOpNames();

	for (int32 i = 0; i < Steps->Num(); ++i)
	{
		const TSharedPtr<FJsonObject>* StepObj;
		if (!(*Steps)[i]->TryGetObject(StepObj))
		{
			Result.Errors.Add(FString::Printf(TEXT("Step %d: not a JSON object"), i));
			continue;
		}

		FString Op;
		if (!(*StepObj)->TryGetStringField(TEXT("op"), Op))
		{
			Result.Errors.Add(FString::Printf(TEXT("Step %d: missing 'op' field"), i));
			continue;
		}

		if (!ValidOps.Contains(Op))
		{
			Result.Errors.Add(FString::Printf(TEXT("Step %d: unknown operation '%s'"), i, *Op));
		}

		// Collect referenced tags
		for (const FString& TagField : { TEXT("tag"), TEXT("classTag"), TEXT("effectTag"), TEXT("stanceTag"), TEXT("skillTag") })
		{
			FString TagStr;
			if ((*StepObj)->TryGetStringField(TagField, TagStr))
			{
				FGameplayTag Tag = ResolveTag(TagStr, TagAliases);
				if (Tag.IsValid())
				{
					Result.ReferencedTags.AddUnique(Tag);
				}
			}
		}
	}

	Result.bSuccess = Result.Errors.Num() == 0;
	return Result;
}

// ============================================================================
// 스키마 / 예제 — 레지스트리에서 자동 생성
// ============================================================================

FString FHktStoryJsonCompiler::GetStorySchema()
{
	return FHktStoryOpRegistry::Get().GenerateSchemaJson();
}

FString FHktStoryJsonCompiler::GetStoryExamples()
{
	return TEXT(R"([
  {
    "name": "BasicAttack",
    "description": "근접 공격 — 태그 기반 애니메이션 + 데미지",
    "story": {
      "storyTag": "Ability.Attack.Basic",
      "tags": {
        "AnimAttack": "Anim.UpperBody.Combat.Attack",
        "VFX_HitSpark": "VFX.HitSpark",
        "Sound_Hit": "Sound.Hit"
      },
      "steps": [
        { "op": "AddTag", "entity": "Self", "tag": "AnimAttack" },
        { "op": "WaitAnimEnd", "entity": "Self" },
        { "op": "GetDistance", "dst": "R0", "entity1": "Self", "entity2": "Target" },
        { "op": "LoadConst", "dst": "R1", "value": 200 },
        { "op": "CmpLe", "dst": "Flag", "src1": "R0", "src2": "R1" },
        { "op": "JumpIfNot", "cond": "Flag", "label": "miss" },
        { "op": "LoadEntityProperty", "dst": "R2", "entity": "Self", "property": "AttackPower" },
        { "op": "ApplyDamage", "target": "Target", "amount": "R2" },
        { "op": "PlayVFXAttached", "entity": "Target", "tag": "VFX_HitSpark" },
        { "op": "PlaySound", "tag": "Sound_Hit" },
        { "op": "Label", "name": "miss" },
        { "op": "RemoveTag", "entity": "Self", "tag": "AnimAttack" },
        { "op": "Halt" }
      ]
    }
  },
  {
    "name": "Fireball",
    "description": "투사체 스킬 — 스폰 + 충돌 + AoE",
    "story": {
      "storyTag": "Ability.Skill.Fireball",
      "tags": {
        "Projectile": "Entity.Projectile.Fireball",
        "AnimCast": "Anim.UpperBody.Cast.Fireball",
        "VFX_DirectHit": "VFX.DirectHit",
        "VFX_Explosion": "VFX.FireballExplosion",
        "Effect_Burn": "Effect.Burn",
        "Sound_Launch": "Sound.FireballLaunch",
        "Sound_Explode": "Sound.Explosion"
      },
      "steps": [
        { "op": "AddTag", "entity": "Self", "tag": "AnimCast" },
        { "op": "WaitSeconds", "seconds": 1.0 },
        { "op": "SpawnEntity", "classTag": "Projectile" },
        { "op": "GetPosition", "dst": "R0", "entity": "Self" },
        { "op": "SetPosition", "entity": "Spawned", "src": "R0" },
        { "op": "MoveForward", "entity": "Spawned", "force": 500 },
        { "op": "PlaySound", "tag": "Sound_Launch" },
        { "op": "WaitCollision", "entity": "Spawned" },
        { "op": "GetPosition", "dst": "R3", "entity": "Spawned" },
        { "op": "DestroyEntity", "entity": "Spawned" },
        { "op": "ApplyDamageConst", "target": "Hit", "amount": 100 },
        { "op": "PlayVFXAttached", "entity": "Hit", "tag": "VFX_DirectHit" },
        { "op": "PlayVFX", "pos": "R3", "tag": "VFX_Explosion" },
        { "op": "PlaySoundAtLocation", "pos": "R3", "tag": "Sound_Explode" },
        { "op": "ForEachInRadius", "center": "Hit", "radius": 300 },
        { "op": "Move", "dst": "Target", "src": "Iter" },
        { "op": "ApplyDamageConst", "target": "Target", "amount": 50 },
        { "op": "ApplyEffect", "target": "Target", "effectTag": "Effect_Burn" },
        { "op": "EndForEach" },
        { "op": "RemoveTag", "entity": "Self", "tag": "AnimCast" },
        { "op": "Halt" }
      ]
    }
  },
  {
    "name": "CharacterSpawn",
    "description": "캐릭터 입장 — 스폰 + 장비 + 애니메이션",
    "story": {
      "storyTag": "Event.Character.Spawn",
      "tags": {
        "PlayerEntity": "Entity.Character.Player",
        "AnimSpawn": "Anim.FullBody.Action.Spawn",
        "AnimIntro": "Anim.Montage.Intro",
        "AnimIdle": "Anim.FullBody.Locomotion.Idle",
        "VFX_Spawn": "VFX.SpawnEffect",
        "Sound_Spawn": "Sound.Spawn",
        "Weapon_Sword": "Entity.Item.Weapon.Sword",
        "Shield": "Entity.Item.Shield"
      },
      "steps": [
        { "op": "SpawnEntity", "classTag": "PlayerEntity" },
        { "op": "Move", "dst": "Self", "src": "Spawned" },
        { "op": "LoadStore", "dst": "R0", "property": "TargetPosX" },
        { "op": "LoadStore", "dst": "R1", "property": "TargetPosY" },
        { "op": "LoadStore", "dst": "R2", "property": "TargetPosZ" },
        { "op": "SetPosition", "entity": "Self", "src": "R0" },
        { "op": "PlayVFXAttached", "entity": "Self", "tag": "VFX_Spawn" },
        { "op": "PlaySound", "tag": "Sound_Spawn" },
        { "op": "AddTag", "entity": "Self", "tag": "AnimSpawn" },
        { "op": "WaitSeconds", "seconds": 0.5 },
        { "op": "SpawnEntity", "classTag": "Weapon_Sword" },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "OwnerEntity", "src": "Self" },
        { "op": "SaveConstEntity", "entity": "Spawned", "property": "BagSlot", "value": 0 },
        { "op": "SpawnEntity", "classTag": "Shield" },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "OwnerEntity", "src": "Self" },
        { "op": "SaveConstEntity", "entity": "Spawned", "property": "BagSlot", "value": 1 },
        { "op": "SaveConstEntity", "entity": "Self", "property": "Stance", "value": 2 },
        { "op": "AddTag", "entity": "Self", "tag": "AnimIntro" },
        { "op": "WaitAnimEnd", "entity": "Self" },
        { "op": "RemoveTag", "entity": "Self", "tag": "AnimSpawn" },
        { "op": "RemoveTag", "entity": "Self", "tag": "AnimIntro" },
        { "op": "AddTag", "entity": "Self", "tag": "AnimIdle" },
        { "op": "Halt" }
      ]
    }
  },
  {
    "name": "WaveSpawner",
    "description": "웨이브 스포너 — 반복 + 조건 + NPC 스폰",
    "story": {
      "storyTag": "Flow.Spawner.Wave.Arena",
      "tags": {
        "NPC_Goblin": "Entity.NPC.Goblin",
        "NPC_Tag": "Entity.NPC",
        "Hostile": "NPC.Hostile"
      },
      "steps": [
        { "op": "LoadConst", "dst": "R0", "value": 0 },
        { "op": "Label", "name": "wave1" },
        { "op": "LoadConst", "dst": "R1", "value": 3 },
        { "op": "LoadConst", "dst": "R2", "value": 0 },
        { "op": "Label", "name": "wave1_loop" },
        { "op": "CmpGe", "dst": "Flag", "src1": "R2", "src2": "R1" },
        { "op": "JumpIf", "cond": "Flag", "label": "wave1_wait" },
        { "op": "SpawnEntity", "classTag": "NPC_Goblin" },
        { "op": "SaveConstEntity", "entity": "Spawned", "property": "IsNPC", "value": 1 },
        { "op": "SaveConstEntity", "entity": "Spawned", "property": "Health", "value": 80 },
        { "op": "SaveConstEntity", "entity": "Spawned", "property": "MaxHealth", "value": 80 },
        { "op": "AddTag", "entity": "Spawned", "tag": "NPC_Tag" },
        { "op": "AddTag", "entity": "Spawned", "tag": "Hostile" },
        { "op": "GetPosition", "dst": "R3", "entity": "Self" },
        { "op": "SetPosition", "entity": "Spawned", "src": "R3" },
        { "op": "AddImm", "dst": "R2", "src": "R2", "imm": 1 },
        { "op": "Jump", "label": "wave1_loop" },
        { "op": "Label", "name": "wave1_wait" },
        { "op": "CountByTag", "dst": "R3", "tag": "NPC_Goblin" },
        { "op": "CmpEq", "dst": "Flag", "src1": "R3", "src2": "R0" },
        { "op": "JumpIf", "cond": "Flag", "label": "complete" },
        { "op": "WaitSeconds", "seconds": 2.0 },
        { "op": "Jump", "label": "wave1_wait" },
        { "op": "Label", "name": "complete" },
        { "op": "Log", "message": "All waves complete" },
        { "op": "Halt" }
      ]
    }
  }
])");
}
