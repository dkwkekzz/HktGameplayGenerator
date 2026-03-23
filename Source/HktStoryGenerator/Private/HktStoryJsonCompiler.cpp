// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryJsonCompiler.h"
#include "HktStoryJsonParser.h"
#include "HktStoryBuilder.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStoryJsonCompiler, Log, All);

// ============================================================================
// 태그 자동 등록 + INI 영속화 (에디터 전용)
// ============================================================================

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

FGameplayTag FHktStoryJsonCompiler::EnsureTagRegistered(const FString& TagName)
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
// CompileAndRegister — FHktStoryJsonParser에 위임
// ============================================================================

FHktStoryCompileResult FHktStoryJsonCompiler::CompileAndRegister(const FString& JsonStr)
{
	// 에디터 전용 태그 해석기: alias 해결 + 자동등록 + INI 영속화
	auto EditorResolveTag = [](const FString& TagStr) -> FGameplayTag
	{
		return EnsureTagRegistered(TagStr);
	};

	FHktStoryParseResult ParseResult = FHktStoryJsonParser::Get().ParseAndBuild(JsonStr, EditorResolveTag);

	// ParseResult → CompileResult 변환
	FHktStoryCompileResult Result;
	Result.bSuccess = ParseResult.bSuccess;
	Result.StoryTag = ParseResult.StoryTag;
	Result.Errors = MoveTemp(ParseResult.Errors);
	Result.Warnings = MoveTemp(ParseResult.Warnings);
	Result.ReferencedTags = MoveTemp(ParseResult.ReferencedTags);

	if (Result.bSuccess)
	{
		UE_LOG(LogHktStoryJsonCompiler, Log, TEXT("Story compiled and registered: %s (%d tags referenced)"),
			*Result.StoryTag, Result.ReferencedTags.Num());
	}

	return Result;
}

// ============================================================================
// Validate — FHktStoryJsonParser::GetValidOpNames 기반
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

	// Tag aliases
	TMap<FString, FGameplayTag> TagAliases;
	const TSharedPtr<FJsonObject>* TagsObj;
	if (Root->TryGetObjectField(TEXT("tags"), TagsObj))
	{
		for (const auto& Pair : (*TagsObj)->Values)
		{
			TagAliases.Add(Pair.Key, EnsureTagRegistered(Pair.Value->AsString()));
		}
	}

	const TSet<FString> ValidOps = FHktStoryJsonParser::Get().GetValidOpNames();

	// 공통 op 검증 람다
	auto ValidateOpArray = [&](const TArray<TSharedPtr<FJsonValue>>& OpArray, const TCHAR* SectionName, bool bReadOnlyOnly)
	{
		for (int32 i = 0; i < OpArray.Num(); ++i)
		{
			const TSharedPtr<FJsonObject>* StepObj;
			if (!OpArray[i]->TryGetObject(StepObj))
			{
				Result.Errors.Add(FString::Printf(TEXT("%s %d: not a JSON object"), SectionName, i));
				continue;
			}

			FString Op;
			if (!(*StepObj)->TryGetStringField(TEXT("op"), Op))
			{
				Result.Errors.Add(FString::Printf(TEXT("%s %d: missing 'op' field"), SectionName, i));
				continue;
			}

			if (!ValidOps.Contains(Op))
			{
				Result.Errors.Add(FString::Printf(TEXT("%s %d: unknown operation '%s'"), SectionName, i, *Op));
			}
			else if (bReadOnlyOnly && !FHktStoryJsonParser::IsReadOnlyOp(Op))
			{
				Result.Errors.Add(FString::Printf(TEXT("%s %d: operation '%s' is not allowed in preconditions"), SectionName, i, *Op));
			}

			// 참조 태그 수집
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
	};

	// Preconditions 검증 (선택)
	const TArray<TSharedPtr<FJsonValue>>* Preconditions;
	if (Root->TryGetArrayField(TEXT("preconditions"), Preconditions))
	{
		ValidateOpArray(*Preconditions, TEXT("Precondition"), /*bReadOnlyOnly=*/true);
	}

	// Steps 검증
	const TArray<TSharedPtr<FJsonValue>>* Steps;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps))
	{
		Result.Errors.Add(TEXT("Missing 'steps' array"));
		return Result;
	}

	ValidateOpArray(*Steps, TEXT("Step"), /*bReadOnlyOnly=*/false);

	Result.bSuccess = Result.Errors.Num() == 0;
	return Result;
}

// ============================================================================
// 스키마 / 예제
// ============================================================================

FString FHktStoryJsonCompiler::GetStorySchema()
{
	FString SchemaPath = FPaths::ProjectDir() / TEXT("../../HktGameplayGenerator/.claude/skills/story-gen/story_schema.json");
	FString SchemaStr;
	if (FFileHelper::LoadFileToString(SchemaStr, *SchemaPath))
	{
		return SchemaStr;
	}

	SchemaPath = FPaths::ProjectContentDir() / TEXT("HktStorySchema.json");
	if (FFileHelper::LoadFileToString(SchemaStr, *SchemaPath))
	{
		return SchemaStr;
	}

	UE_LOG(LogHktStoryJsonCompiler, Warning, TEXT("Story schema not found. Returning minimal schema."));
	return TEXT("{\"error\": \"Schema file not found.\"}");
}

FString FHktStoryJsonCompiler::GetStoryExamples()
{
	return TEXT(R"([
  {
    "name": "BasicAttackWithPreconditions",
    "description": "근접 공격 — precondition으로 쿨타임 검증 + 태그 기반 애니메이션 + 데미지",
    "story": {
      "storyTag": "Ability.Attack.Basic",
      "tags": {
        "AnimAttack": "Anim.UpperBody.Combat.Attack",
        "VFX_HitSpark": "VFX.HitSpark",
        "Sound_Hit": "Sound.Hit"
      },
      "preconditions": [
        { "op": "LoadEntityProperty", "dst": "R0", "entity": "Self", "property": "NextActionFrame" },
        { "op": "GetWorldTime", "dst": "R1" },
        { "op": "CmpGe", "dst": "Flag", "src1": "R1", "src2": "R0" }
      ],
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
