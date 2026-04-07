#include "HktMcpEditorSubsystem.h"
#include "HktMcpBridgeEditorModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "Selection.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SLevelViewport.h"
#include "EditorViewportClient.h"
#include "IPythonScriptPlugin.h"
#include "UObject/SavePackage.h"
#include "Factories/DataAssetFactory.h"
#include "GameplayTagContainer.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"

void UHktMcpEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 에디터 시작 시 MCP Bridge 서버 자동 시작
	StartMcpServer();

	// Claude CLI 연결 검증도 자동 실행
	VerifyAgentConnection();

	UE_LOG(LogHktMcpEditor, Log, TEXT("HktMcpEditorSubsystem Initialized - MCP Server started, Agent verification in progress"));
}

void UHktMcpEditorSubsystem::Deinitialize()
{
	StopMcpServer();
	UE_LOG(LogHktMcpEditor, Log, TEXT("HktMcpEditorSubsystem Deinitialized"));
	Super::Deinitialize();
}

// ==================== Asset Tools ====================

TArray<FHktAssetInfo> UHktMcpEditorSubsystem::ListAssets(const FString& Path, const FString& ClassFilter)
{
	TArray<FHktAssetInfo> Result;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*Path), AssetDataList, true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (!ClassFilter.IsEmpty() && !AssetData.AssetClassPath.GetAssetName().ToString().Contains(ClassFilter))
		{
			continue;
		}

		FHktAssetInfo Info;
		Info.AssetPath = AssetData.GetObjectPathString();
		Info.AssetName = AssetData.AssetName.ToString();
		Info.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
		Info.PackagePath = AssetData.PackagePath.ToString();
		Result.Add(Info);
	}

	return Result;
}

FString UHktMcpEditorSubsystem::GetAssetDetails(const FString& AssetPath)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return TEXT("{\"error\": \"Asset not found\"}");
	}

	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("name"), Asset->GetName());
	JsonObject->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
	JsonObject->SetStringField(TEXT("path"), AssetPath);
	JsonObject->SetStringField(TEXT("package"), Asset->GetOutermost()->GetName());

	// 클래스별 추가 정보
	if (UBlueprint* BP = Cast<UBlueprint>(Asset))
	{
		JsonObject->SetStringField(TEXT("parent_class"), BP->ParentClass ? BP->ParentClass->GetName() : TEXT("None"));
		JsonObject->SetStringField(TEXT("blueprint_type"), UEnum::GetValueAsString(BP->BlueprintType));
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

TArray<FHktAssetInfo> UHktMcpEditorSubsystem::SearchAssets(const FString& SearchQuery, const FString& ClassFilter)
{
	TArray<FHktAssetInfo> Result;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName("/Game"));

	if (!ClassFilter.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), FName(*ClassFilter)));
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString().Contains(SearchQuery))
		{
			FHktAssetInfo Info;
			Info.AssetPath = AssetData.GetObjectPathString();
			Info.AssetName = AssetData.AssetName.ToString();
			Info.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
			Info.PackagePath = AssetData.PackagePath.ToString();
			Result.Add(Info);
		}
	}

	return Result;
}

bool UHktMcpEditorSubsystem::CreateDataAsset(const FString& AssetPath, const FString& ParentClassName)
{
	// 클래스 검색
	UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
	if (!ParentClass)
	{
		ParentClass = LoadObject<UClass>(nullptr, *ParentClassName);
	}
	if (!ParentClass)
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: Class not found: %s"), *ParentClassName);
		return false;
	}

	if (!ParentClass->IsChildOf(UDataAsset::StaticClass()))
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: %s is not a DataAsset subclass"), *ParentClassName);
		return false;
	}

	// 패키지 경로와 에셋 이름 분리
	FString PackagePath, AssetName;
	AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (AssetName.IsEmpty())
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: Invalid asset path: %s"), *AssetPath);
		return false;
	}

	// DataAssetFactory 사용
	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = ParentClass;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDataAsset::StaticClass(), Factory);
	if (!NewAsset)
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: Failed to create asset: %s"), *AssetPath);
		return false;
	}

	// 저장
	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);
	UE_LOG(LogHktMcpEditor, Log, TEXT("CreateDataAsset: Created %s (%s)"), *AssetPath, *ParentClassName);
	return true;
}

bool UHktMcpEditorSubsystem::ModifyAssetProperty(const FString& AssetPath, const FString& PropertyName, const FString& NewValue)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Asset not found: %s"), *AssetPath);
		return false;
	}

	FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Property not found: %s on %s"), *PropertyName, *Asset->GetClass()->GetName());
		return false;
	}

	void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Asset);
	bool bSuccess = false;

	// FGameplayTag — "Entity.Character.Goblin" 단순 문자열
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == FGameplayTag::StaticStruct())
		{
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*NewValue), false);
			if (!Tag.IsValid())
			{
				UE_LOG(LogHktMcpEditor, Warning, TEXT("Invalid GameplayTag: %s"), *NewValue);
				return false;
			}
			*static_cast<FGameplayTag*>(PropertyValue) = Tag;
			bSuccess = true;
		}
		else if (StructProp->Struct == FGameplayTagContainer::StaticStruct())
		{
			// 쉼표 구분 태그 목록: "Tag.A, Tag.B, Tag.C"
			FGameplayTagContainer* Container = static_cast<FGameplayTagContainer*>(PropertyValue);
			Container->Reset();
			TArray<FString> TagStrings;
			NewValue.ParseIntoArray(TagStrings, TEXT(","));
			for (FString& TagStr : TagStrings)
			{
				TagStr.TrimStartAndEndInline();
				FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
				if (Tag.IsValid())
				{
					Container->AddTag(Tag);
				}
			}
			bSuccess = Container->Num() > 0;
			if (!bSuccess)
			{
				UE_LOG(LogHktMcpEditor, Warning, TEXT("No valid tags in container value: %s"), *NewValue);
				return false;
			}
		}
		else if (StructProp->Struct == TBaseStructure<FSoftObjectPath>::Get())
		{
			*static_cast<FSoftObjectPath*>(PropertyValue) = FSoftObjectPath(NewValue);
			bSuccess = true;
		}
		else if (StructProp->Struct == TBaseStructure<FSoftClassPath>::Get())
		{
			*static_cast<FSoftClassPath*>(PropertyValue) = FSoftClassPath(NewValue);
			bSuccess = true;
		}
	}

	// TSoftObjectPtr<T> / TSoftClassPtr<T> — ImportText로 경로 문자열 직접 처리
	if (!bSuccess)
	{
		if (CastField<FSoftObjectProperty>(Property) || CastField<FSoftClassProperty>(Property))
		{
			const TCHAR* Ret = Property->ImportText_Direct(*NewValue, PropertyValue, Asset, EPropertyPortFlags::PPF_None);
			if (Ret != nullptr)
			{
				bSuccess = true;
			}
			else
			{
				UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to set soft reference %s = %s"), *PropertyName, *NewValue);
				return false;
			}
		}
	}

	// 하드 오브젝트 참조 (UObject*, TObjectPtr) — 경로로 로드 후 설정
	if (!bSuccess)
	{
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* RefObj = UEditorAssetLibrary::LoadAsset(NewValue);
			if (!RefObj)
			{
				UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to load referenced object: %s"), *NewValue);
				return false;
			}
			ObjProp->SetObjectPropertyValue(PropertyValue, RefObj);
			bSuccess = true;
		}
	}

	// TArray — JSON 배열 문자열: ["a","b","c"] 또는 쉼표 구분 값
	if (!bSuccess)
	{
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			// ImportText로 시도 (UE5 내부 배열 형식 지원)
			const TCHAR* Ret = Property->ImportText_Direct(*NewValue, PropertyValue, Asset, EPropertyPortFlags::PPF_None);
			if (Ret != nullptr)
			{
				bSuccess = true;
			}
			else
			{
				UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to set array property %s (try UE5 text format like: (\"a\",\"b\",\"c\"))"), *PropertyName);
				return false;
			}
		}
	}

	// 기본: ImportText_Direct 사용 (int, float, bool, FString, enum 등)
	if (!bSuccess)
	{
		const TCHAR* Ret = Property->ImportText_Direct(*NewValue, PropertyValue, Asset, EPropertyPortFlags::PPF_None);
		if (Ret == nullptr)
		{
			UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to set property %s = %s (ImportText failed)"), *PropertyName, *NewValue);
			return false;
		}
		bSuccess = true;
	}

	// 수정 후 저장
	Asset->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(AssetPath, false);
	UE_LOG(LogHktMcpEditor, Log, TEXT("ModifyAssetProperty: %s.%s = %s"), *AssetPath, *PropertyName, *NewValue);
	return true;
}

FString UHktMcpEditorSubsystem::CreateDataAssetWithProperties(const FString& AssetPath, const FString& ParentClassName, const FString& PropertiesJson)
{
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();

	// 1. DataAsset 생성
	if (!CreateDataAsset(AssetPath, ParentClassName))
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to create DataAsset: %s (%s)"), *AssetPath, *ParentClassName));
		FString Str;
		TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Str);
		FJsonSerializer::Serialize(Result, W);
		return Str;
	}

	// 2. 속성 일괄 설정 (PropertiesJson이 비어있으면 생성만)
	TArray<FString> FailedProps;
	if (!PropertiesJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> Props;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertiesJson);
		if (FJsonSerializer::Deserialize(Reader, Props) && Props.IsValid())
		{
			for (const auto& Pair : Props->Values)
			{
				FString Value;
				if (Pair.Value->TryGetString(Value))
				{
					if (!ModifyAssetProperty(AssetPath, Pair.Key, Value))
					{
						FailedProps.Add(Pair.Key);
					}
				}
				else
				{
					// 비-문자열 값 → 문자열 변환
					double NumVal;
					bool bSuccess;
					if (Pair.Value->TryGetNumber(NumVal))
					{
						Value = FString::SanitizeFloat(NumVal);
					}
					else if (Pair.Value->TryGetBool(bSuccess))
					{
						Value = bSuccess ? TEXT("true") : TEXT("false");
						bSuccess = false; // bSuccess 리셋 (위에서 bool 변환용으로만 사용)
					}
					else
					{
						UE_LOG(LogHktMcpEditor, Warning, TEXT("Unsupported JSON value type for property: %s"), *Pair.Key);
						FailedProps.Add(Pair.Key);
						continue;
					}

					if (!ModifyAssetProperty(AssetPath, Pair.Key, Value))
					{
						FailedProps.Add(Pair.Key);
					}
				}
			}
		}
		else
		{
			Result->SetBoolField(TEXT("success"), false);
			Result->SetStringField(TEXT("error"), TEXT("Invalid PropertiesJson format"));
			FString Str;
			TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Str);
			FJsonSerializer::Serialize(Result, W);
			return Str;
		}
	}

	// 3. 결과
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("class"), ParentClassName);

	if (FailedProps.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailedArr;
		for (const FString& P : FailedProps)
		{
			FailedArr.Add(MakeShared<FJsonValueString>(P));
		}
		Result->SetArrayField(TEXT("failed_properties"), FailedArr);
		Result->SetStringField(TEXT("warning"), TEXT("Some properties failed to set"));
	}

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

bool UHktMcpEditorSubsystem::DeleteAsset(const FString& AssetPath)
{
	return UEditorAssetLibrary::DeleteAsset(AssetPath);
}

bool UHktMcpEditorSubsystem::DuplicateAsset(const FString& SourcePath, const FString& DestinationPath)
{
	return UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath) != nullptr;
}

// ==================== Level Tools ====================

TArray<FHktActorInfo> UHktMcpEditorSubsystem::ListActors(const FString& ClassFilter)
{
	TArray<FHktActorInfo> Result;
	
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Result;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		if (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter))
		{
			continue;
		}

		FHktActorInfo Info;
		Info.ActorName = Actor->GetName();
		Info.ActorLabel = Actor->GetActorLabel();
		Info.ActorClass = Actor->GetClass()->GetName();
		Info.Location = Actor->GetActorLocation();
		Info.Rotation = Actor->GetActorRotation();
		Info.Scale = Actor->GetActorScale3D();
		Info.ActorGuid = Actor->GetActorGuid().ToString();
		Result.Add(Info);
	}

	return Result;
}

FString UHktMcpEditorSubsystem::SpawnActor(const FString& BlueprintPath, FVector Location, FRotator Rotation, const FString& ActorLabel)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return TEXT("");
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(BlueprintPath);
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Blueprint not found: %s"), *BlueprintPath);
		return TEXT("");
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Spawn Actor")));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
	if (NewActor)
	{
		if (!ActorLabel.IsEmpty())
		{
			NewActor->SetActorLabel(ActorLabel);
		}
		return NewActor->GetName();
	}

	return TEXT("");
}

FString UHktMcpEditorSubsystem::SpawnActorByClass(const FString& ClassName, FVector Location, FRotator Rotation)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return TEXT("");
	}

	UClass* ActorClass = FindObject<UClass>(nullptr, *ClassName);
	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Actor class not found: %s"), *ClassName);
		return TEXT("");
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Spawn Actor")));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (NewActor)
	{
		return NewActor->GetName();
	}

	return TEXT("");
}

bool UHktMcpEditorSubsystem::ModifyActorTransform(const FString& ActorName, FVector NewLocation, FRotator NewRotation, FVector NewScale)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Modify Actor Transform")));
	Actor->Modify();
	Actor->SetActorLocation(NewLocation);
	Actor->SetActorRotation(NewRotation);
	Actor->SetActorScale3D(NewScale);
	return true;
}

bool UHktMcpEditorSubsystem::ModifyActorProperty(const FString& ActorName, const FString& PropertyName, const FString& NewValue)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Property not found: %s"), *PropertyName);
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Modify Actor Property")));
	Actor->Modify();

	void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Actor);
	const TCHAR* Ret = Property->ImportText_Direct(*NewValue, PropertyValue, Actor, EPropertyPortFlags::PPF_None);
	return Ret != nullptr;
}

bool UHktMcpEditorSubsystem::DeleteActor(const FString& ActorName)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Delete Actor")));
	return Actor->Destroy();
}

bool UHktMcpEditorSubsystem::SelectActor(const FString& ActorName)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	GEditor->SelectNone(true, true);
	GEditor->SelectActor(Actor, true, true, true);
	return true;
}

TArray<FHktActorInfo> UHktMcpEditorSubsystem::GetSelectedActors()
{
	TArray<FHktActorInfo> Result;
	
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (Actor)
		{
			FHktActorInfo Info;
			Info.ActorName = Actor->GetName();
			Info.ActorLabel = Actor->GetActorLabel();
			Info.ActorClass = Actor->GetClass()->GetName();
			Info.Location = Actor->GetActorLocation();
			Info.Rotation = Actor->GetActorRotation();
			Info.Scale = Actor->GetActorScale3D();
			Info.ActorGuid = Actor->GetActorGuid().ToString();
			Result.Add(Info);
		}
	}

	return Result;
}

// ==================== Query Tools ====================

TArray<FString> UHktMcpEditorSubsystem::SearchClasses(const FString& SearchQuery, bool bBlueprintOnly)
{
	TArray<FString> Result;

	if (bBlueprintOnly)
	{
		// 블루프린트 클래스만 검색
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.AssetName.ToString().Contains(SearchQuery))
			{
				Result.Add(AssetData.GetObjectPathString());
			}
		}
	}
	else
	{
		// 네이티브 클래스 검색
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->GetName().Contains(SearchQuery))
			{
				Result.Add(Class->GetPathName());
			}
		}
	}

	return Result;
}

TArray<FHktPropertyInfo> UHktMcpEditorSubsystem::GetClassProperties(const FString& ClassName)
{
	TArray<FHktPropertyInfo> Result;

	UClass* Class = FindObject<UClass>(nullptr, *ClassName);
	if (!Class)
	{
		return Result;
	}

	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FProperty* Property = *It;
		
		FHktPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		
		Result.Add(Info);
	}

	return Result;
}

FString UHktMcpEditorSubsystem::GetProjectStructure(const FString& RootPath)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> SubPaths;
	AssetRegistry.GetSubPaths(RootPath, SubPaths, false);

	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("root"), RootPath);

	TArray<TSharedPtr<FJsonValue>> PathArray;
	for (const FString& SubPath : SubPaths)
	{
		PathArray.Add(MakeShareable(new FJsonValueString(SubPath)));
	}
	JsonObject->SetArrayField(TEXT("folders"), PathArray);

	// 에셋 수
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*RootPath), AssetDataList, false);
	JsonObject->SetNumberField(TEXT("asset_count"), AssetDataList.Num());

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

FString UHktMcpEditorSubsystem::GetCurrentLevelInfo()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return TEXT("{\"error\": \"No world\"}");
	}

	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("world_name"), World->GetName());
	JsonObject->SetStringField(TEXT("map_name"), World->GetMapName());
	
	// 레벨 정보
	if (World->PersistentLevel)
	{
		JsonObject->SetStringField(TEXT("level_name"), World->PersistentLevel->GetName());
	}

	// 액터 수
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCount++;
	}
	JsonObject->SetNumberField(TEXT("actor_count"), ActorCount);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

// ==================== Editor Control ====================

bool UHktMcpEditorSubsystem::OpenLevel(const FString& LevelPath)
{
	return FEditorFileUtils::LoadMap(LevelPath, false, true);
}

bool UHktMcpEditorSubsystem::SaveCurrentLevel()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return false;
	}

	return FEditorFileUtils::SaveCurrentLevel();
}

bool UHktMcpEditorSubsystem::CreateNewLevel(const FString& LevelPath)
{
	UWorld* NewWorld = GEditor->NewMap();
	if (NewWorld)
	{
		return FEditorFileUtils::SaveLevel(NewWorld->PersistentLevel, LevelPath);
	}
	return false;
}

bool UHktMcpEditorSubsystem::StartPIE()
{
	if (GUnrealEd && !GUnrealEd->IsPlayingSessionInEditor())
	{
		FRequestPlaySessionParams Params;
		Params.WorldType = EPlaySessionWorldType::PlayInEditor;
		GUnrealEd->RequestPlaySession(Params);
		return true;
	}
	return false;
}

bool UHktMcpEditorSubsystem::StopPIE()
{
	if (GUnrealEd && GUnrealEd->IsPlayingSessionInEditor())
	{
		GUnrealEd->RequestEndPlayMap();
		return true;
	}
	return false;
}

bool UHktMcpEditorSubsystem::IsPIERunning() const
{
	return GUnrealEd && GUnrealEd->IsPlayingSessionInEditor();
}

void UHktMcpEditorSubsystem::ExecuteEditorCommand(const FString& Command)
{
	GEngine->Exec(GetEditorWorld(), *Command);
}

// ==================== Utility ====================

bool UHktMcpEditorSubsystem::GetViewportCameraTransform(FVector& OutLocation, FRotator& OutRotation)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	
	if (ActiveLevelViewport.IsValid())
	{
		FEditorViewportClient& ViewportClient = ActiveLevelViewport->GetLevelViewportClient();
		OutLocation = ViewportClient.GetViewLocation();
		OutRotation = ViewportClient.GetViewRotation();
		return true;
	}
	return false;
}

bool UHktMcpEditorSubsystem::SetViewportCameraTransform(FVector Location, FRotator Rotation)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	
	if (ActiveLevelViewport.IsValid())
	{
		FEditorViewportClient& ViewportClient = ActiveLevelViewport->GetLevelViewportClient();
		ViewportClient.SetViewLocation(Location);
		ViewportClient.SetViewRotation(Rotation);
		return true;
	}
	return false;
}

void UHktMcpEditorSubsystem::ShowNotification(const FString& Message, float Duration)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = Duration;
	Info.bUseSuccessFailIcons = false;
	FSlateNotificationManager::Get().AddNotification(Info);
}

// ==================== Python Script Executor ====================

/** Helper to escape a string for embedding inside a Python triple-quoted string */
static FString EscapePythonTripleQuote(const FString& Input)
{
	// Replace backslashes first, then triple-quotes
	FString Out = Input.Replace(TEXT("\\"), TEXT("\\\\"));
	Out = Out.Replace(TEXT("\"\"\""), TEXT("\\\"\\\"\\\""));
	return Out;
}

FString UHktMcpEditorSubsystem::ExecutePythonScript(const FString& ScriptCode, float TimeoutSeconds)
{
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		return TEXT("{\"success\":false,\"output\":\"\",\"error\":\"Python plugin not available\"}");
	}

	// Wrap user script to capture stdout/stderr and report results via unreal.log()
	// The wrapper stores results in a known format that we parse from LogOutput
	FString EscapedCode = EscapePythonTripleQuote(ScriptCode);

	FString WrappedScript = FString::Printf(TEXT(
		"import sys as _sys, io as _io, json as _json, traceback as _tb\n"
		"_hkt_stdout = _io.StringIO()\n"
		"_hkt_stderr = _io.StringIO()\n"
		"_hkt_old_stdout, _hkt_old_stderr = _sys.stdout, _sys.stderr\n"
		"_sys.stdout, _sys.stderr = _hkt_stdout, _hkt_stderr\n"
		"_hkt_success = True\n"
		"_hkt_error = ''\n"
		"try:\n"
		"    exec(compile(\"\"\"%s\"\"\", '<mcp_script>', 'exec'))\n"
		"except Exception as _e:\n"
		"    _hkt_success = False\n"
		"    _hkt_error = _tb.format_exc()\n"
		"finally:\n"
		"    _sys.stdout, _sys.stderr = _hkt_old_stdout, _hkt_old_stderr\n"
		"_hkt_out = _hkt_stdout.getvalue()\n"
		"_hkt_err_out = _hkt_stderr.getvalue()\n"
		"if _hkt_err_out and not _hkt_error:\n"
		"    _hkt_error = _hkt_err_out\n"
		"import unreal as _ue\n"
		"_ue.log('__HKT_PYRESULT__' + _json.dumps({'success': _hkt_success, 'output': _hkt_out, 'error': _hkt_error}))\n"
	), *EscapedCode);

	FPythonCommandEx PythonCommand;
	PythonCommand.Command = WrappedScript;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;

	PythonPlugin->ExecPythonCommandEx(PythonCommand);

	// Search LogOutput for our result marker
	FString ResultJson;
	const FString Marker = TEXT("__HKT_PYRESULT__");
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		int32 MarkerIdx = Entry.Output.Find(Marker);
		if (MarkerIdx != INDEX_NONE)
		{
			ResultJson = Entry.Output.Mid(MarkerIdx + Marker.Len());
			break;
		}
	}

	if (!ResultJson.IsEmpty())
	{
		return ResultJson;
	}

	// Fallback: collect all log output
	FString AllOutput;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		if (!AllOutput.IsEmpty())
		{
			AllOutput += TEXT("\n");
		}
		AllOutput += Entry.Output;
	}

	TSharedRef<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), !AllOutput.Contains(TEXT("Error")));
	Result->SetStringField(TEXT("output"), AllOutput);
	Result->SetStringField(TEXT("error"), TEXT(""));

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Result, Writer);
	return Output;
}

// ==================== Generation Request Queue ====================

FString UHktMcpEditorSubsystem::SubmitGenerationRequest(const FHktGenerationRequest& InRequest)
{
	FHktGenerationRequest Request = InRequest;

	// RequestId 생성 (비어 있으면)
	if (Request.RequestId.IsEmpty())
	{
		Request.RequestId = FString::Printf(TEXT("gen-%d-%lld"), NextRequestId++, FDateTime::Now().GetTicks());
	}

	Request.Status = EHktGenerationStatus::Pending;
	GenerationRequests.Add(Request.RequestId, Request);
	PendingRequestQueue.Add(Request.RequestId);

	UE_LOG(LogHktMcpEditor, Log, TEXT("Generation request submitted: %s (skill: %s, project: %s)"),
		*Request.RequestId, *Request.SkillName, *Request.ProjectId);

	return Request.RequestId;
}

FHktGenerationRequest UHktMcpEditorSubsystem::PollGenerationRequest()
{
	if (PendingRequestQueue.Num() == 0)
	{
		return FHktGenerationRequest();
	}

	FString RequestId = PendingRequestQueue[0];
	PendingRequestQueue.RemoveAt(0);

	FHktGenerationRequest* Request = GenerationRequests.Find(RequestId);
	if (!Request)
	{
		return FHktGenerationRequest();
	}

	Request->Status = EHktGenerationStatus::InProgress;

	UE_LOG(LogHktMcpEditor, Log, TEXT("Generation request polled: %s"), *RequestId);
	return *Request;
}

void UHktMcpEditorSubsystem::SendGenerationEvent(const FHktGenerationEvent& Event)
{
	// Remote Control API 등 비-게임스레드에서 호출될 수 있으므로 게임스레드로 디스패치
	if (!IsInGameThread())
	{
		FHktGenerationEvent EventCopy = Event;
		TWeakObjectPtr<UHktMcpEditorSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, EventCopy]()
		{
			if (UHktMcpEditorSubsystem* Self = WeakThis.Get())
			{
				Self->SendGenerationEvent(EventCopy);
			}
		});
		return;
	}

	// 요청 상태 업데이트
	FHktGenerationRequest* Request = GenerationRequests.Find(Event.RequestId);
	if (Request)
	{
		if (Event.EventType == TEXT("complete"))
		{
			Request->Status = (Event.ExitCode == 0)
				? EHktGenerationStatus::Completed
				: EHktGenerationStatus::Failed;
		}
		else if (Event.EventType == TEXT("error"))
		{
			Request->Status = EHktGenerationStatus::Failed;
		}
	}

	// UI에 브로드캐스트
	OnGenerationEvent.Broadcast(Event);
}

void UHktMcpEditorSubsystem::CancelGenerationRequest(const FString& RequestId)
{
	// Pending 큐에서 제거
	PendingRequestQueue.Remove(RequestId);

	FHktGenerationRequest* Request = GenerationRequests.Find(RequestId);
	if (Request)
	{
		Request->Status = EHktGenerationStatus::Cancelled;

		// 취소 이벤트 브로드캐스트
		FHktGenerationEvent CancelEvent;
		CancelEvent.RequestId = RequestId;
		CancelEvent.EventType = TEXT("error");
		CancelEvent.Content = TEXT("Cancelled by user");
		OnGenerationEvent.Broadcast(CancelEvent);
	}

	UE_LOG(LogHktMcpEditor, Log, TEXT("Generation request cancelled: %s"), *RequestId);
}

EHktGenerationStatus UHktMcpEditorSubsystem::GetGenerationStatus(const FString& RequestId) const
{
	const FHktGenerationRequest* Request = GenerationRequests.Find(RequestId);
	return Request ? Request->Status : EHktGenerationStatus::Failed;
}

// ==================== External Agent Connection ====================

void UHktMcpEditorSubsystem::ConnectAgent(const FHktAgentInfo& AgentInfo)
{
	FString Id = AgentInfo.AgentId;
	if (Id.IsEmpty())
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("ConnectAgent: empty AgentId"));
		return;
	}

	bool bNew = !ConnectedAgents.Contains(Id);

	FAgentConnection& Conn = ConnectedAgents.FindOrAdd(Id);
	Conn.Info = AgentInfo;
	Conn.LastHeartbeat = FPlatformTime::Seconds();

	if (bNew)
	{
		UE_LOG(LogHktMcpEditor, Log, TEXT("Agent connected: %s (provider: %s, name: %s)"),
			*Id, *AgentInfo.Provider, *AgentInfo.DisplayName);
		OnAgentConnectionChanged.Broadcast(true, AgentInfo);
	}
	else
	{
		UE_LOG(LogHktMcpEditor, Log, TEXT("Agent reconnected: %s"), *Id);
	}
}

bool UHktMcpEditorSubsystem::Heartbeat(const FString& AgentId)
{
	FAgentConnection* Conn = ConnectedAgents.Find(AgentId);
	if (!Conn)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Heartbeat from unknown agent: %s — call ConnectAgent first"), *AgentId);
		return false;
	}

	Conn->LastHeartbeat = FPlatformTime::Seconds();

	// 타임아웃된 다른 에이전트 정리
	CleanupTimedOutAgents();

	return PendingRequestQueue.Num() > 0;
}

void UHktMcpEditorSubsystem::DisconnectAgent(const FString& AgentId)
{
	FAgentConnection* Conn = ConnectedAgents.Find(AgentId);
	if (Conn)
	{
		FHktAgentInfo Info = Conn->Info;
		ConnectedAgents.Remove(AgentId);

		UE_LOG(LogHktMcpEditor, Log, TEXT("Agent disconnected: %s (%s)"), *AgentId, *Info.DisplayName);
		OnAgentConnectionChanged.Broadcast(false, Info);
	}
}

bool UHktMcpEditorSubsystem::IsExternalAgentConnected() const
{
	double Now = FPlatformTime::Seconds();
	for (const auto& Pair : ConnectedAgents)
	{
		if ((Now - Pair.Value.LastHeartbeat) < AgentTimeoutSeconds)
		{
			return true;
		}
	}
	return false;
}

FHktAgentInfo UHktMcpEditorSubsystem::GetConnectedAgentInfo() const
{
	double Now = FPlatformTime::Seconds();
	// 가장 최근 Heartbeat인 에이전트 반환
	const FAgentConnection* Best = nullptr;
	for (const auto& Pair : ConnectedAgents)
	{
		if ((Now - Pair.Value.LastHeartbeat) < AgentTimeoutSeconds)
		{
			if (!Best || Pair.Value.LastHeartbeat > Best->LastHeartbeat)
			{
				Best = &Pair.Value;
			}
		}
	}
	return Best ? Best->Info : FHktAgentInfo();
}

int32 UHktMcpEditorSubsystem::GetConnectedAgentCount() const
{
	double Now = FPlatformTime::Seconds();
	int32 Count = 0;
	for (const auto& Pair : ConnectedAgents)
	{
		if ((Now - Pair.Value.LastHeartbeat) < AgentTimeoutSeconds)
		{
			Count++;
		}
	}
	return Count;
}

void UHktMcpEditorSubsystem::CleanupTimedOutAgents()
{
	double Now = FPlatformTime::Seconds();
	TArray<FString> TimedOut;

	for (const auto& Pair : ConnectedAgents)
	{
		if ((Now - Pair.Value.LastHeartbeat) >= AgentTimeoutSeconds)
		{
			TimedOut.Add(Pair.Key);
		}
	}

	for (const FString& Id : TimedOut)
	{
		FAgentConnection Conn = ConnectedAgents.FindAndRemoveChecked(Id);
		UE_LOG(LogHktMcpEditor, Log, TEXT("Agent timed out: %s (%s)"), *Id, *Conn.Info.DisplayName);
		OnAgentConnectionChanged.Broadcast(false, Conn.Info);
	}
}

// ==================== MCP Bridge Server (Editor-level) ====================

bool UHktMcpEditorSubsystem::StartMcpServer(int32 Port)
{
	if (bMcpServerRunning)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("MCP Bridge server already running on port %d"), McpServerPort);
		return false;
	}

	// Port 0이면 기본 포트 사용
	if (Port == 0)
	{
		Port = 9876;
	}

	McpServerPort = Port;

	// TODO: 실제 WebSocket 서버 구현
	// UE5의 WebSocket 모듈은 클라이언트만 지원하므로
	// 서버 기능은 별도 구현 필요 (libwebsocket 또는 TCP 서버 + WS 프로토콜)
	// 현재는 placeholder로 서버 상태를 관리

	bMcpServerRunning = true;
	UE_LOG(LogHktMcpEditor, Log, TEXT("MCP Bridge server started on port %d (editor-level)"), McpServerPort);

	OnMcpServerStateChanged.Broadcast(true);
	return true;
}

void UHktMcpEditorSubsystem::StopMcpServer()
{
	if (!bMcpServerRunning)
	{
		return;
	}

	McpClientCount = 0;
	bMcpServerRunning = false;

	UE_LOG(LogHktMcpEditor, Log, TEXT("MCP Bridge server stopped"));
	OnMcpServerStateChanged.Broadcast(false);
}

bool UHktMcpEditorSubsystem::ReconnectMcpServer()
{
	UE_LOG(LogHktMcpEditor, Log, TEXT("Reconnecting MCP Bridge server..."));
	StopMcpServer();
	return StartMcpServer();
}

// ==================== Agent Connection Verification ====================

void UHktMcpEditorSubsystem::VerifyAgentConnection(const FString& CLIPathOverride)
{
	if (bAgentVerifying)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Agent verification already in progress"));
		return;
	}

	bAgentVerifying = true;
	bAgentVerified = false;
	AgentVersionString.Empty();

	// CLIPathOverride가 있으면 해당 경로 사용, 없으면 자동 탐색
	FString CLIPath;
	if (!CLIPathOverride.IsEmpty() && FPaths::FileExists(CLIPathOverride))
	{
		CLIPath = CLIPathOverride;
	}
	else
	{
		// 환경변수 → 알려진 경로 → PATH 순서로 탐색
		FString EnvCLI = FPlatformMisc::GetEnvironmentVariable(TEXT("HKT_CLAUDE_CLI"));
		if (!EnvCLI.IsEmpty() && FPaths::FileExists(EnvCLI))
		{
			CLIPath = EnvCLI;
		}
		else
		{
			// 간단한 알려진 경로 확인
			TArray<FString> SearchPaths;
#if PLATFORM_WINDOWS
			FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
			FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
			FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
			SearchPaths.Add(FPaths::Combine(UserProfile, TEXT(".claude"), TEXT("local"), TEXT("claude.exe")));
			SearchPaths.Add(FPaths::Combine(LocalAppData, TEXT("Programs"), TEXT("claude"), TEXT("claude.exe")));
			SearchPaths.Add(FPaths::Combine(AppData, TEXT("npm"), TEXT("claude.cmd")));
#else
			FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
			SearchPaths.Add(FPaths::Combine(Home, TEXT(".claude"), TEXT("local"), TEXT("claude")));
			SearchPaths.Add(TEXT("/usr/local/bin/claude"));
			SearchPaths.Add(TEXT("/usr/bin/claude"));
			SearchPaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("bin"), TEXT("claude")));
#endif
			for (const FString& Path : SearchPaths)
			{
				if (FPaths::FileExists(Path))
				{
					CLIPath = Path;
					break;
				}
			}

			// PATH에서 which/where로 검색
			if (CLIPath.IsEmpty())
			{
#if PLATFORM_WINDOWS
				FString WhichExe = TEXT("C:\\Windows\\System32\\where.exe");
#else
				FString WhichExe = TEXT("/usr/bin/which");
#endif
				FString WhichOutput;
				int32 WhichReturnCode = -1;
				if (FPaths::FileExists(WhichExe))
				{
					FPlatformProcess::ExecProcess(*WhichExe, TEXT("claude"), &WhichReturnCode, &WhichOutput, nullptr);
					if (WhichReturnCode == 0)
					{
						FString FoundPath = WhichOutput.TrimStartAndEnd();
						int32 NewlineIdx;
						if (FoundPath.FindChar(TEXT('\n'), NewlineIdx))
						{
							FoundPath.LeftInline(NewlineIdx);
							FoundPath.TrimEndInline();
						}
						if (!FoundPath.IsEmpty() && FPaths::FileExists(FoundPath))
						{
							CLIPath = FoundPath;
						}
					}
				}
			}
		}
	}

	if (CLIPath.IsEmpty())
	{
		bAgentVerifying = false;
		bAgentVerified = false;
		AgentCLIPath.Empty();
		AgentVersionString = TEXT("CLI not found");
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Agent verification failed: Claude CLI not found"));
		OnAgentVerified.Broadcast(false, AgentVersionString);
		return;
	}

	AgentCLIPath = CLIPath;

	// 백그라운드 스레드에서 `claude --version` 실행
	TWeakObjectPtr<UHktMcpEditorSubsystem> WeakThis(this);
	FString CapturedCLIPath = CLIPath;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, CapturedCLIPath]()
	{
		FString StdOut;
		FString StdErr;
		int32 ReturnCode = -1;

		FPlatformProcess::ExecProcess(*CapturedCLIPath, TEXT("--version"), &ReturnCode, &StdOut, &StdErr);

		FString VersionStr = StdOut.TrimStartAndEnd();
		bool bSuccess = (ReturnCode == 0 && !VersionStr.IsEmpty());

		if (!bSuccess && !StdErr.IsEmpty())
		{
			VersionStr = StdErr.TrimStartAndEnd();
		}

		// GameThread로 결과 전달
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, VersionStr]()
		{
			if (UHktMcpEditorSubsystem* Self = WeakThis.Get())
			{
				Self->bAgentVerifying = false;
				Self->bAgentVerified = bSuccess;
				Self->AgentVersionString = bSuccess ? VersionStr : FString::Printf(TEXT("Failed: %s"), *VersionStr);

				UE_LOG(LogHktMcpEditor, Log, TEXT("Agent verification %s: %s"),
					bSuccess ? TEXT("succeeded") : TEXT("failed"), *Self->AgentVersionString);

				Self->OnAgentVerified.Broadcast(bSuccess, Self->AgentVersionString);
			}
		});
	});
}

// ==================== Helper Functions ====================

AActor* UHktMcpEditorSubsystem::FindActorByName(const FString& ActorName)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName)
		{
			return Actor;
		}
	}

	return nullptr;
}

UWorld* UHktMcpEditorSubsystem::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}
