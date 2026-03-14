#include "HktMcpFunctionLibrary.h"
#include "HktMcpEditorSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UHktMcpEditorSubsystem* UHktMcpFunctionLibrary::GetSubsystem()
{
    if (GEditor)
    {
        return GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
    }
    return nullptr;
}

// Helper to convert array to JSON string
template<typename T>
static FString ArrayToJsonString(const TArray<T>& Array, TFunction<TSharedPtr<FJsonObject>(const T&)> Converter)
{
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    for (const T& Item : Array)
    {
        JsonArray.Add(MakeShareable(new FJsonValueObject(Converter(Item))));
    }

    TSharedRef<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetNumberField(TEXT("count"), Array.Num());
    Result->SetArrayField(TEXT("items"), JsonArray);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Result, Writer);
    return Output;
}

// ==================== Asset Tools ====================

FString UHktMcpFunctionLibrary::McpListAssets(const FString& Path, const FString& ClassFilter)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    TArray<FHktAssetInfo> Assets = Subsystem->ListAssets(Path, ClassFilter);
   
    return ArrayToJsonString<FHktAssetInfo>(Assets, [](const FHktAssetInfo& Asset) {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("asset_path"), Asset.AssetPath);
        Obj->SetStringField(TEXT("asset_name"), Asset.AssetName);
        Obj->SetStringField(TEXT("asset_class"), Asset.AssetClass);
        Obj->SetStringField(TEXT("package_path"), Asset.PackagePath);
        return Obj;
    });
}

FString UHktMcpFunctionLibrary::McpSearchAssets(const FString& SearchQuery, const FString& ClassFilter)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    TArray<FHktAssetInfo> Assets = Subsystem->SearchAssets(SearchQuery, ClassFilter);
   
    return ArrayToJsonString<FHktAssetInfo>(Assets, [](const FHktAssetInfo& Asset) {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("asset_path"), Asset.AssetPath);
        Obj->SetStringField(TEXT("asset_name"), Asset.AssetName);
        Obj->SetStringField(TEXT("asset_class"), Asset.AssetClass);
        Obj->SetStringField(TEXT("package_path"), Asset.PackagePath);
        return Obj;
    });
}

FString UHktMcpFunctionLibrary::McpGetAssetDetails(const FString& AssetPath)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    return Subsystem->GetAssetDetails(AssetPath);
}

bool UHktMcpFunctionLibrary::McpModifyAssetProperty(const FString& AssetPath, const FString& PropertyName, const FString& NewValue)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->ModifyAssetProperty(AssetPath, PropertyName, NewValue);
}

// ==================== Level Tools ====================

FString UHktMcpFunctionLibrary::McpListActors(const FString& ClassFilter)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    TArray<FHktActorInfo> Actors = Subsystem->ListActors(ClassFilter);
   
    return ArrayToJsonString<FHktActorInfo>(Actors, [](const FHktActorInfo& Actor) {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("actor_name"), Actor.ActorName);
        Obj->SetStringField(TEXT("actor_label"), Actor.ActorLabel);
        Obj->SetStringField(TEXT("actor_class"), Actor.ActorClass);
        Obj->SetStringField(TEXT("actor_guid"), Actor.ActorGuid);
       
        TSharedPtr<FJsonObject> LocationObj = MakeShareable(new FJsonObject);
        LocationObj->SetNumberField(TEXT("x"), Actor.Location.X);
        LocationObj->SetNumberField(TEXT("y"), Actor.Location.Y);
        LocationObj->SetNumberField(TEXT("z"), Actor.Location.Z);
        Obj->SetObjectField(TEXT("location"), LocationObj);
       
        TSharedPtr<FJsonObject> RotationObj = MakeShareable(new FJsonObject);
        RotationObj->SetNumberField(TEXT("pitch"), Actor.Rotation.Pitch);
        RotationObj->SetNumberField(TEXT("yaw"), Actor.Rotation.Yaw);
        RotationObj->SetNumberField(TEXT("roll"), Actor.Rotation.Roll);
        Obj->SetObjectField(TEXT("rotation"), RotationObj);
       
        TSharedPtr<FJsonObject> ScaleObj = MakeShareable(new FJsonObject);
        ScaleObj->SetNumberField(TEXT("x"), Actor.Scale.X);
        ScaleObj->SetNumberField(TEXT("y"), Actor.Scale.Y);
        ScaleObj->SetNumberField(TEXT("z"), Actor.Scale.Z);
        Obj->SetObjectField(TEXT("scale"), ScaleObj);
       
        return Obj;
    });
}

FString UHktMcpFunctionLibrary::McpSpawnActor(const FString& BlueprintPath, FVector Location, FRotator Rotation, const FString& ActorLabel)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("");
    }

    return Subsystem->SpawnActor(BlueprintPath, Location, Rotation, ActorLabel);
}

FString UHktMcpFunctionLibrary::McpSpawnActorByClass(const FString& ClassName, FVector Location, FRotator Rotation)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("");
    }

    return Subsystem->SpawnActorByClass(ClassName, Location, Rotation);
}

bool UHktMcpFunctionLibrary::McpModifyActorTransform(const FString& ActorName, FVector NewLocation, FRotator NewRotation, FVector NewScale)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->ModifyActorTransform(ActorName, NewLocation, NewRotation, NewScale);
}

bool UHktMcpFunctionLibrary::McpDeleteActor(const FString& ActorName)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->DeleteActor(ActorName);
}

bool UHktMcpFunctionLibrary::McpSelectActor(const FString& ActorName)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->SelectActor(ActorName);
}

FString UHktMcpFunctionLibrary::McpGetSelectedActors()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    TArray<FHktActorInfo> Actors = Subsystem->GetSelectedActors();
   
    return ArrayToJsonString<FHktActorInfo>(Actors, [](const FHktActorInfo& Actor) {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("actor_name"), Actor.ActorName);
        Obj->SetStringField(TEXT("actor_label"), Actor.ActorLabel);
        Obj->SetStringField(TEXT("actor_class"), Actor.ActorClass);
       
        TSharedPtr<FJsonObject> LocationObj = MakeShareable(new FJsonObject);
        LocationObj->SetNumberField(TEXT("x"), Actor.Location.X);
        LocationObj->SetNumberField(TEXT("y"), Actor.Location.Y);
        LocationObj->SetNumberField(TEXT("z"), Actor.Location.Z);
        Obj->SetObjectField(TEXT("location"), LocationObj);
       
        return Obj;
    });
}

// ==================== Query Tools ====================

FString UHktMcpFunctionLibrary::McpSearchClasses(const FString& SearchQuery, bool bBlueprintOnly)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    TArray<FString> Classes = Subsystem->SearchClasses(SearchQuery, bBlueprintOnly);
   
    TSharedRef<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetNumberField(TEXT("count"), Classes.Num());
   
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    for (const FString& Class : Classes)
    {
        JsonArray.Add(MakeShareable(new FJsonValueString(Class)));
    }
    Result->SetArrayField(TEXT("items"), JsonArray);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Result, Writer);
    return Output;
}

FString UHktMcpFunctionLibrary::McpGetClassProperties(const FString& ClassName)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    TArray<FHktPropertyInfo> Properties = Subsystem->GetClassProperties(ClassName);
   
    return ArrayToJsonString<FHktPropertyInfo>(Properties, [](const FHktPropertyInfo& Prop) {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("property_name"), Prop.PropertyName);
        Obj->SetStringField(TEXT("property_type"), Prop.PropertyType);
        Obj->SetStringField(TEXT("property_value"), Prop.PropertyValue);
        Obj->SetBoolField(TEXT("is_editable"), Prop.bIsEditable);
        return Obj;
    });
}

FString UHktMcpFunctionLibrary::McpGetProjectStructure(const FString& RootPath)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    return Subsystem->GetProjectStructure(RootPath);
}

FString UHktMcpFunctionLibrary::McpGetCurrentLevelInfo()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    return Subsystem->GetCurrentLevelInfo();
}

// ==================== Editor Control ====================

bool UHktMcpFunctionLibrary::McpOpenLevel(const FString& LevelPath)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->OpenLevel(LevelPath);
}

bool UHktMcpFunctionLibrary::McpSaveCurrentLevel()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->SaveCurrentLevel();
}

bool UHktMcpFunctionLibrary::McpStartPIE()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->StartPIE();
}

bool UHktMcpFunctionLibrary::McpStopPIE()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->StopPIE();
}

bool UHktMcpFunctionLibrary::McpIsPIERunning()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->IsPIERunning();
}

void UHktMcpFunctionLibrary::McpExecuteCommand(const FString& Command)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (Subsystem)
    {
        Subsystem->ExecuteEditorCommand(Command);
    }
}

// ==================== Utility ====================

void UHktMcpFunctionLibrary::McpShowNotification(const FString& Message, float Duration)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (Subsystem)
    {
        Subsystem->ShowNotification(Message, Duration);
    }
}

FString UHktMcpFunctionLibrary::McpPing()
{
    TSharedRef<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), TEXT("MCP Bridge is running"));
    Result->SetStringField(TEXT("version"), TEXT("1.0.0"));

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Result, Writer);
    return Output;
}

FString UHktMcpFunctionLibrary::McpGetViewportCamera()
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return TEXT("{\"error\": \"Subsystem not available\"}");
    }

    FVector Location;
    FRotator Rotation;
    
    if (Subsystem->GetViewportCameraTransform(Location, Rotation))
    {
        TSharedRef<FJsonObject> Result = MakeShareable(new FJsonObject);
        
        TSharedPtr<FJsonObject> LocationObj = MakeShareable(new FJsonObject);
        LocationObj->SetNumberField(TEXT("x"), Location.X);
        LocationObj->SetNumberField(TEXT("y"), Location.Y);
        LocationObj->SetNumberField(TEXT("z"), Location.Z);
        Result->SetObjectField(TEXT("location"), LocationObj);
        
        TSharedPtr<FJsonObject> RotationObj = MakeShareable(new FJsonObject);
        RotationObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
        RotationObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
        RotationObj->SetNumberField(TEXT("roll"), Rotation.Roll);
        Result->SetObjectField(TEXT("rotation"), RotationObj);
        
        Result->SetBoolField(TEXT("success"), true);

        FString Output;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
        FJsonSerializer::Serialize(Result, Writer);
        return Output;
    }
    
    return TEXT("{\"error\": \"Could not get viewport camera\"}");
}

bool UHktMcpFunctionLibrary::McpSetViewportCamera(FVector Location, FRotator Rotation)
{
    UHktMcpEditorSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        return false;
    }

    return Subsystem->SetViewportCameraTransform(Location, Rotation);
}
