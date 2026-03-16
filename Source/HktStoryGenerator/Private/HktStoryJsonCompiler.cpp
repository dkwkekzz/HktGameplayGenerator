// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryJsonCompiler.h"
#include "HktStoryBuilder.h"
#include "HktStoryTypes.h"
#include "HktCoreProperties.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStoryJsonCompiler, Log, All);

int32 FHktStoryJsonCompiler::ParseRegister(const FString& RegStr)
{
	if (RegStr == TEXT("Self")) return Reg::Self;
	if (RegStr == TEXT("Target")) return Reg::Target;
	if (RegStr == TEXT("Spawned")) return Reg::Spawned;
	if (RegStr == TEXT("Hit")) return Reg::Hit;
	if (RegStr == TEXT("Iter")) return Reg::Iter;
	if (RegStr == TEXT("Flag")) return Reg::Flag;
	if (RegStr == TEXT("Count")) return Reg::Count;
	if (RegStr == TEXT("Temp")) return Reg::Temp;

	// R0-R9
	if (RegStr.StartsWith(TEXT("R")) && RegStr.Len() <= 3)
	{
		int32 Idx = FCString::Atoi(*RegStr.Mid(1));
		if (Idx >= 0 && Idx <= 9) return Idx;
	}

	return -1;
}

/**
 * 프로젝트 DefaultGameplayTags.ini에 태그를 영속화.
 * 에디터 재시작 시에도 태그가 유지되도록 보장.
 *
 * UE Config 시스템의 +Array= 형식을 사용하여 기존 엔트리에 추가.
 */
static void PersistTagToProjectConfig(const FString& TagName)
{
	const FString IniPath = FPaths::ProjectConfigDir() / TEXT("DefaultGameplayTags.ini");
	const FString Section = TEXT("/Script/GameplayTags.GameplayTagsSettings");
	const FString Key = TEXT("GameplayTagList");
	const FString Entry = FString::Printf(TEXT("(Tag=\"%s\",DevComment=\"Auto-registered by HktStoryGenerator\")"), *TagName);

	// 이미 등록되어 있는지 확인 (중복 방지)
	TArray<FString> Existing;
	GConfig->GetArray(*Section, *Key, Existing, IniPath);
	for (const FString& E : Existing)
	{
		if (E.Contains(TagName))
		{
			return; // 이미 존재
		}
	}

	// 배열에 추가 후 INI에 기록
	Existing.Add(Entry);
	GConfig->SetArray(*Section, *Key, Existing, IniPath);
	GConfig->Flush(false, IniPath);

	UE_LOG(LogHktStoryJsonCompiler, Log, TEXT("Persisted tag to %s: %s"), *IniPath, *TagName);
}

/**
 * GameplayTag가 유효한지 확인하고, 없으면 동적 등록 + INI 영속화.
 *
 * 동작 방식:
 * 1. RequestGameplayTag로 기존 태그 조회
 * 2. 없으면 AddNativeGameplayTag로 현재 세션에 등록
 *    - DoneAddingNativeTags 이후에도 태그 트리에 추가됨 (valid tag 반환)
 *    - NetIndex 미할당 (에디터 전용이므로 리플리케이션 무관)
 * 3. DefaultGameplayTags.ini에 기록하여 에디터 재시작 시에도 유지
 */
static FGameplayTag EnsureTagRegistered(const FString& TagName)
{
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
	if (Tag.IsValid())
	{
		return Tag;
	}

	// 현재 세션에 등록 — AddNativeGameplayTag는 DoneAddingNativeTags 이후에도
	// AddTagTableRow를 통해 트리에 노드를 생성하고 valid tag를 반환함.
	// NetIndex는 할당되지 않으나 에디터 전용 플러그인이므로 리플리케이션 불필요.
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	Tag = Manager.AddNativeGameplayTag(FName(*TagName), TEXT("Auto-registered by StoryJsonCompiler"));

	if (Tag.IsValid())
	{
		UE_LOG(LogHktStoryJsonCompiler, Log, TEXT("Auto-registered GameplayTag: %s"), *TagName);

		// INI에 영속화 — 에디터 재시작 시에도 태그가 유지됨
		PersistTagToProjectConfig(TagName);
	}
	else
	{
		UE_LOG(LogHktStoryJsonCompiler, Warning, TEXT("Failed to auto-register GameplayTag: %s"), *TagName);
	}

	return Tag;
}

FGameplayTag FHktStoryJsonCompiler::ResolveTag(const FString& TagStr, const TMap<FString, FGameplayTag>& TagAliases)
{
	// First check aliases
	if (const FGameplayTag* Found = TagAliases.Find(TagStr))
	{
		return *Found;
	}
	// Direct tag name — auto-register if missing
	return EnsureTagRegistered(TagStr);
}

uint16 FHktStoryJsonCompiler::ParsePropertyId(const FString& PropStr)
{
	// Map property names to PropertyId constants via X-macro match
	#define HKT_PROP_PARSE(Name) if (PropStr == TEXT(#Name)) return PropertyId::Name;
	HKT_PROPERTY_LIST(HKT_PROP_PARSE)
	#undef HKT_PROP_PARSE

	return 0xFFFF; // Invalid
}

bool FHktStoryJsonCompiler::ApplyStep(
	FHktStoryBuilder& Builder,
	const TSharedPtr<FJsonObject>& Step,
	const TMap<FString, FGameplayTag>& TagAliases,
	TArray<FGameplayTag>& OutReferencedTags,
	TArray<FString>& OutErrors,
	int32 StepIndex)
{
	FString Op = Step->GetStringField(TEXT("op"));

	auto GetReg = [&](const FString& Field) -> int32
	{
		FString Val;
		if (!Step->TryGetStringField(Field, Val))
		{
			OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *Field));
			return -1;
		}
		int32 R = ParseRegister(Val);
		if (R < 0)
		{
			OutErrors.Add(FString::Printf(TEXT("Step %d (%s): invalid register '%s'"), StepIndex, *Op, *Val));
		}
		return R;
	};

	auto GetTag = [&](const FString& Field) -> FGameplayTag
	{
		FString Val;
		if (!Step->TryGetStringField(Field, Val))
		{
			OutErrors.Add(FString::Printf(TEXT("Step %d (%s): missing field '%s'"), StepIndex, *Op, *Field));
			return FGameplayTag();
		}
		FGameplayTag Tag = ResolveTag(Val, TagAliases);
		if (Tag.IsValid())
		{
			OutReferencedTags.AddUnique(Tag);
		}
		return Tag;
	};

	auto GetInt = [&](const FString& Field, int32 Default = 0) -> int32
	{
		double Val;
		if (Step->TryGetNumberField(Field, Val)) return static_cast<int32>(Val);
		return Default;
	};

	auto GetFloat = [&](const FString& Field, float Default = 0.f) -> float
	{
		double Val;
		if (Step->TryGetNumberField(Field, Val)) return static_cast<float>(Val);
		return Default;
	};

	auto GetStr = [&](const FString& Field) -> FString
	{
		FString Val;
		Step->TryGetStringField(Field, Val);
		return Val;
	};

	// ========== Control Flow ==========
	if (Op == TEXT("Label")) { Builder.Label(GetStr(TEXT("name"))); return true; }
	if (Op == TEXT("Jump")) { Builder.Jump(GetStr(TEXT("label"))); return true; }
	if (Op == TEXT("JumpIf")) { Builder.JumpIf(GetReg(TEXT("cond")), GetStr(TEXT("label"))); return true; }
	if (Op == TEXT("JumpIfNot")) { Builder.JumpIfNot(GetReg(TEXT("cond")), GetStr(TEXT("label"))); return true; }
	if (Op == TEXT("Yield")) { Builder.Yield(GetInt(TEXT("frames"), 1)); return true; }
	if (Op == TEXT("WaitSeconds")) { Builder.WaitSeconds(GetFloat(TEXT("seconds"))); return true; }
	if (Op == TEXT("Halt")) { Builder.Halt(); return true; }

	// ========== Event Wait ==========
	if (Op == TEXT("WaitCollision")) { Builder.WaitCollision(GetReg(TEXT("entity"))); return true; }
	if (Op == TEXT("WaitAnimEnd")) { Builder.WaitAnimEnd(GetReg(TEXT("entity"))); return true; }
	if (Op == TEXT("WaitMoveEnd")) { Builder.WaitMoveEnd(GetReg(TEXT("entity"))); return true; }

	// ========== Data ==========
	if (Op == TEXT("LoadConst")) { Builder.LoadConst(GetReg(TEXT("dst")), GetInt(TEXT("value"))); return true; }
	if (Op == TEXT("LoadStore")) { Builder.LoadStore(GetReg(TEXT("dst")), ParsePropertyId(GetStr(TEXT("property")))); return true; }
	if (Op == TEXT("LoadEntityProperty")) { Builder.LoadEntityProperty(GetReg(TEXT("dst")), GetReg(TEXT("entity")), ParsePropertyId(GetStr(TEXT("property")))); return true; }
	if (Op == TEXT("SaveStore")) { Builder.SaveStore(ParsePropertyId(GetStr(TEXT("property"))), GetReg(TEXT("src"))); return true; }
	if (Op == TEXT("SaveEntityProperty")) { Builder.SaveEntityProperty(GetReg(TEXT("entity")), ParsePropertyId(GetStr(TEXT("property"))), GetReg(TEXT("src"))); return true; }
	if (Op == TEXT("Move")) { Builder.Move(GetReg(TEXT("dst")), GetReg(TEXT("src"))); return true; }

	// ========== Arithmetic ==========
	if (Op == TEXT("Add")) { Builder.Add(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("Sub")) { Builder.Sub(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("Mul")) { Builder.Mul(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("Div")) { Builder.Div(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("AddImm")) { Builder.AddImm(GetReg(TEXT("dst")), GetReg(TEXT("src")), GetInt(TEXT("imm"))); return true; }

	// ========== Comparison ==========
	if (Op == TEXT("CmpEq")) { Builder.CmpEq(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("CmpNe")) { Builder.CmpNe(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("CmpLt")) { Builder.CmpLt(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("CmpLe")) { Builder.CmpLe(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("CmpGt")) { Builder.CmpGt(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }
	if (Op == TEXT("CmpGe")) { Builder.CmpGe(GetReg(TEXT("dst")), GetReg(TEXT("src1")), GetReg(TEXT("src2"))); return true; }

	// ========== Entity ==========
	if (Op == TEXT("SpawnEntity")) { Builder.SpawnEntity(GetTag(TEXT("classTag"))); return true; }
	if (Op == TEXT("DestroyEntity")) { Builder.DestroyEntity(GetReg(TEXT("entity"))); return true; }

	// ========== Position & Movement ==========
	if (Op == TEXT("GetPosition")) { Builder.GetPosition(GetReg(TEXT("dst")), GetReg(TEXT("entity"))); return true; }
	if (Op == TEXT("SetPosition")) { Builder.SetPosition(GetReg(TEXT("entity")), GetReg(TEXT("src"))); return true; }
	if (Op == TEXT("MoveToward")) { Builder.MoveToward(GetReg(TEXT("entity")), GetReg(TEXT("targetPos")), GetInt(TEXT("force"))); return true; }
	if (Op == TEXT("MoveForward")) { Builder.MoveForward(GetReg(TEXT("entity")), GetInt(TEXT("force"))); return true; }
	if (Op == TEXT("StopMovement")) { Builder.StopMovement(GetReg(TEXT("entity"))); return true; }
	if (Op == TEXT("GetDistance")) { Builder.GetDistance(GetReg(TEXT("dst")), GetReg(TEXT("entity1")), GetReg(TEXT("entity2"))); return true; }

	// ========== Spatial ==========
	if (Op == TEXT("FindInRadius")) { Builder.FindInRadius(GetReg(TEXT("center")), GetInt(TEXT("radius"))); return true; }
	if (Op == TEXT("NextFound")) { Builder.NextFound(); return true; }
	if (Op == TEXT("ForEachInRadius")) { Builder.ForEachInRadius(GetReg(TEXT("center")), GetInt(TEXT("radius"))); return true; }
	if (Op == TEXT("EndForEach")) { Builder.EndForEach(); return true; }

	// ========== Combat ==========
	if (Op == TEXT("ApplyDamage")) { Builder.ApplyDamage(GetReg(TEXT("target")), GetReg(TEXT("amount"))); return true; }
	if (Op == TEXT("ApplyDamageConst")) { Builder.ApplyDamageConst(GetReg(TEXT("target")), GetInt(TEXT("amount"))); return true; }
	if (Op == TEXT("ApplyEffect")) { Builder.ApplyEffect(GetReg(TEXT("target")), GetTag(TEXT("effectTag"))); return true; }
	if (Op == TEXT("RemoveEffect")) { Builder.RemoveEffect(GetReg(TEXT("target")), GetTag(TEXT("effectTag"))); return true; }

	// ========== VFX ==========
	if (Op == TEXT("PlayVFX")) { Builder.PlayVFX(GetReg(TEXT("pos")), GetTag(TEXT("tag"))); return true; }
	if (Op == TEXT("PlayVFXAttached")) { Builder.PlayVFXAttached(GetReg(TEXT("entity")), GetTag(TEXT("tag"))); return true; }

	// ========== Audio ==========
	if (Op == TEXT("PlaySound")) { Builder.PlaySound(GetTag(TEXT("tag"))); return true; }
	if (Op == TEXT("PlaySoundAtLocation")) { Builder.PlaySoundAtLocation(GetReg(TEXT("pos")), GetTag(TEXT("tag"))); return true; }

	// ========== Tags ==========
	if (Op == TEXT("AddTag")) { Builder.AddTag(GetReg(TEXT("entity")), GetTag(TEXT("tag"))); return true; }
	if (Op == TEXT("RemoveTag")) { Builder.RemoveTag(GetReg(TEXT("entity")), GetTag(TEXT("tag"))); return true; }
	if (Op == TEXT("HasTag")) { Builder.HasTag(GetReg(TEXT("dst")), GetReg(TEXT("entity")), GetTag(TEXT("tag"))); return true; }
	if (Op == TEXT("CountByTag")) { Builder.CountByTag(GetReg(TEXT("dst")), GetTag(TEXT("tag"))); return true; }

	// ========== Query ==========
	if (Op == TEXT("GetWorldTime")) { Builder.GetWorldTime(GetReg(TEXT("dst"))); return true; }
	if (Op == TEXT("RandomInt")) { Builder.RandomInt(GetReg(TEXT("dst")), GetReg(TEXT("modulus"))); return true; }
	if (Op == TEXT("HasPlayerInGroup")) { Builder.HasPlayerInGroup(GetReg(TEXT("dst"))); return true; }
	if (Op == TEXT("CountByOwner")) { Builder.CountByOwner(GetReg(TEXT("dst")), GetReg(TEXT("owner")), GetTag(TEXT("tag"))); return true; }
	if (Op == TEXT("FindByOwner")) { Builder.FindByOwner(GetReg(TEXT("owner")), GetTag(TEXT("tag"))); return true; }

	// ========== Utility ==========
	if (Op == TEXT("Log")) { Builder.Log(GetStr(TEXT("message"))); return true; }

	OutErrors.Add(FString::Printf(TEXT("Step %d: unknown operation '%s'"), StepIndex, *Op));
	return false;
}

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

	// Story tag — auto-register if missing
	FString StoryTagStr;
	if (!Root->TryGetStringField(TEXT("storyTag"), StoryTagStr) || StoryTagStr.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing or empty 'storyTag' field"));
		return Result;
	}
	Result.StoryTag = StoryTagStr;
	EnsureTagRegistered(StoryTagStr);

	// Parse tag aliases — auto-register missing tags
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

	// Parse tag aliases for validation — auto-register missing tags
	TMap<FString, FGameplayTag> TagAliases;
	const TSharedPtr<FJsonObject>* TagsObj;
	if (Root->TryGetObjectField(TEXT("tags"), TagsObj))
	{
		for (const auto& Pair : (*TagsObj)->Values)
		{
			TagAliases.Add(Pair.Key, EnsureTagRegistered(Pair.Value->AsString()));
		}
	}

	// Validate steps without building
	const TArray<TSharedPtr<FJsonValue>>* Steps;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps))
	{
		Result.Errors.Add(TEXT("Missing 'steps' array"));
		return Result;
	}

	// Check each step has valid op
	static const TSet<FString> ValidOps = {
		TEXT("Label"), TEXT("Jump"), TEXT("JumpIf"), TEXT("JumpIfNot"), TEXT("Yield"), TEXT("WaitSeconds"), TEXT("Halt"),
		TEXT("WaitCollision"), TEXT("WaitAnimEnd"), TEXT("WaitMoveEnd"),
		TEXT("LoadConst"), TEXT("LoadStore"), TEXT("LoadEntityProperty"), TEXT("SaveStore"), TEXT("SaveEntityProperty"), TEXT("Move"),
		TEXT("Add"), TEXT("Sub"), TEXT("Mul"), TEXT("Div"), TEXT("AddImm"),
		TEXT("CmpEq"), TEXT("CmpNe"), TEXT("CmpLt"), TEXT("CmpLe"), TEXT("CmpGt"), TEXT("CmpGe"),
		TEXT("SpawnEntity"), TEXT("DestroyEntity"),
		TEXT("GetPosition"), TEXT("SetPosition"), TEXT("MoveToward"), TEXT("MoveForward"), TEXT("StopMovement"), TEXT("GetDistance"),
		TEXT("FindInRadius"), TEXT("NextFound"), TEXT("ForEachInRadius"), TEXT("EndForEach"),
		TEXT("ApplyDamage"), TEXT("ApplyDamageConst"), TEXT("ApplyEffect"), TEXT("RemoveEffect"),
		TEXT("PlayVFX"), TEXT("PlayVFXAttached"),
		TEXT("PlaySound"), TEXT("PlaySoundAtLocation"),
		TEXT("AddTag"), TEXT("RemoveTag"), TEXT("HasTag"), TEXT("CountByTag"),
		TEXT("GetWorldTime"), TEXT("RandomInt"), TEXT("HasPlayerInGroup"), TEXT("CountByOwner"), TEXT("FindByOwner"),
		TEXT("Log"),
	};

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
		for (const FString& TagField : { TEXT("tag"), TEXT("classTag"), TEXT("effectTag") })
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

FString FHktStoryJsonCompiler::GetStorySchema()
{
	return TEXT(R"JSON({
  "description": "HktStory JSON Format — AI Agent가 게임 Story/Flow를 정의하는 형식",
  "format": {
    "storyTag": "GameplayTag (이벤트 이름, e.g. 'Ability.Skill.IceBlast')",
    "description": "Story 설명 (선택)",
    "cancelOnDuplicate": "bool — 같은 엔티티에 동일 이벤트 중복 시 기존 취소 (선택, 기본 false)",
    "tags": {
      "alias": "Full.GameplayTag.Name (단축 이름 → 전체 태그 매핑)"
    },
    "steps": [
      { "op": "OperationName", "...params": "..." }
    ]
  },
  "registers": {
    "R0-R9": "범용 레지스터 (R9=Temp)",
    "Self": "이벤트 소스 엔티티 (R10)",
    "Target": "이벤트 타겟 엔티티 (R11)",
    "Spawned": "마지막 스폰된 엔티티 (R12)",
    "Hit": "충돌 대상 (R13)",
    "Iter": "ForEach 순회 대상 (R14)",
    "Flag": "조건 결과 / Count (R15)"
  },
  "operations": {
    "control": {
      "Label": { "name": "string" },
      "Jump": { "label": "string" },
      "JumpIf": { "cond": "register", "label": "string" },
      "JumpIfNot": { "cond": "register", "label": "string" },
      "Yield": { "frames": "int (default 1)" },
      "WaitSeconds": { "seconds": "float" },
      "Halt": {}
    },
    "wait": {
      "WaitCollision": { "entity": "register" },
      "WaitAnimEnd": { "entity": "register" },
      "WaitMoveEnd": { "entity": "register" }
    },
    "entity": {
      "SpawnEntity": { "classTag": "tag — Entity type tag" },
      "DestroyEntity": { "entity": "register" }
    },
    "position": {
      "GetPosition": { "dst": "register (3 consecutive: dst, dst+1, dst+2)", "entity": "register" },
      "SetPosition": { "entity": "register", "src": "register (3 consecutive)" },
      "MoveToward": { "entity": "register", "targetPos": "register (3 consecutive)", "force": "int" },
      "MoveForward": { "entity": "register", "force": "int (cm/s)" },
      "StopMovement": { "entity": "register" },
      "GetDistance": { "dst": "register", "entity1": "register", "entity2": "register" }
    },
    "combat": {
      "ApplyDamage": { "target": "register", "amount": "register" },
      "ApplyDamageConst": { "target": "register", "amount": "int" },
      "ApplyEffect": { "target": "register", "effectTag": "tag" },
      "RemoveEffect": { "target": "register", "effectTag": "tag" }
    },
    "vfx": {
      "PlayVFX": { "pos": "register (3 consecutive)", "tag": "tag — VFX tag" },
      "PlayVFXAttached": { "entity": "register", "tag": "tag — VFX tag" }
    },
    "audio": {
      "PlaySound": { "tag": "tag — Sound tag" },
      "PlaySoundAtLocation": { "pos": "register (3 consecutive)", "tag": "tag" }
    },
    "tags": {
      "AddTag": { "entity": "register", "tag": "tag" },
      "RemoveTag": { "entity": "register", "tag": "tag" },
      "HasTag": { "dst": "register (1/0)", "entity": "register", "tag": "tag" },
      "CountByTag": { "dst": "register", "tag": "tag" }
    },
    "spatial": {
      "FindInRadius": { "center": "register", "radius": "int (cm)" },
      "NextFound": {},
      "ForEachInRadius": { "center": "register", "radius": "int (cm)" },
      "EndForEach": {}
    },
    "data": {
      "LoadConst": { "dst": "register", "value": "int" },
      "LoadStore": { "dst": "register", "property": "PropertyId name" },
      "LoadEntityProperty": { "dst": "register", "entity": "register", "property": "PropertyId name" },
      "SaveStore": { "property": "PropertyId name", "src": "register" },
      "SaveEntityProperty": { "entity": "register", "property": "PropertyId name", "src": "register" },
      "Move": { "dst": "register", "src": "register" }
    },
    "arithmetic": {
      "Add": { "dst": "register", "src1": "register", "src2": "register" },
      "Sub": { "dst": "register", "src1": "register", "src2": "register" },
      "Mul": { "dst": "register", "src1": "register", "src2": "register" },
      "Div": { "dst": "register", "src1": "register", "src2": "register" },
      "AddImm": { "dst": "register", "src": "register", "imm": "int" }
    },
    "comparison": {
      "CmpEq": { "dst": "register (1/0)", "src1": "register", "src2": "register" },
      "CmpNe": { "dst": "register (1/0)", "src1": "register", "src2": "register" },
      "CmpLt": { "dst": "register (1/0)", "src1": "register", "src2": "register" },
      "CmpLe": { "dst": "register (1/0)", "src1": "register", "src2": "register" },
      "CmpGt": { "dst": "register (1/0)", "src1": "register", "src2": "register" },
      "CmpGe": { "dst": "register (1/0)", "src1": "register", "src2": "register" }
    },
    "query": {
      "GetWorldTime": { "dst": "register" },
      "RandomInt": { "dst": "register", "modulus": "register" },
      "HasPlayerInGroup": { "dst": "register (1/0)" },
      "CountByOwner": { "dst": "register", "owner": "register", "tag": "tag" },
      "FindByOwner": { "owner": "register", "tag": "tag" }
    },
    "utility": {
      "Log": { "message": "string" }
    }
  },
  "propertyIds": [
    "PosX", "PosY", "PosZ", "RotYaw",
    "Health", "MaxHealth", "AttackPower", "Defense", "Team",
    "MoveTargetX", "MoveTargetY", "MoveTargetZ", "MoveForce", "IsMoving", "MaxSpeed",
    "Mana", "MaxMana", "OwnerEntity", "EntitySpawnTag",
    "TargetPosX", "TargetPosY", "TargetPosZ",
    "Param0", "Param1", "Param2", "Param3",
    "AnimState", "VisualState", "AnimStateUpper",
    "VelX", "VelY", "VelZ", "Mass", "CollisionRadius",
    "ItemState", "ItemId", "BagSlot", "ActionSlot",
    "IsNPC", "SpawnFlowTag"
  ],
  "tagPrefixes": {
    "VFX.*": "VFXGenerator에서 자동 생성 가능",
    "Entity.*": "MeshGenerator에서 자동 생성 가능",
    "Anim.*": "AnimGenerator에서 자동 생성 가능",
    "Entity.Item.*": "ItemGenerator에서 자동 생성 가능",
    "Sound.*": "사운드 에셋 (수동 등록 필요)",
    "Effect.*": "게임플레이 이펙트 (수동 등록 필요)"
  }
})JSON");
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
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "Owner", "src": "Self" },
        { "op": "LoadConst", "dst": "R3", "value": 0 },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "Slot", "src": "R3" },
        { "op": "SpawnEntity", "classTag": "Shield" },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "Owner", "src": "Self" },
        { "op": "LoadConst", "dst": "R3", "value": 1 },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "Slot", "src": "R3" },
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
        { "op": "LoadConst", "dst": "R3", "value": 1 },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "IsNPC", "src": "R3" },
        { "op": "LoadConst", "dst": "R3", "value": 80 },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "Health", "src": "R3" },
        { "op": "SaveEntityProperty", "entity": "Spawned", "property": "MaxHealth", "src": "R3" },
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
