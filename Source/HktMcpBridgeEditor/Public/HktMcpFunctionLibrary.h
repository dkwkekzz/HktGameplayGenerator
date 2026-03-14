#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktMcpEditorSubsystem.h"
#include "HktMcpFunctionLibrary.generated.h"

/**
 * MCP Function Library
 *
 * Static functions exposed via Remote Control API.
 * These functions can be called from external tools via:
 *   POST http://localhost:30010/remote/object/call
 *   {
 *     "objectPath": "/Script/HktMcpBridgeEditor.Default__HktMcpFunctionLibrary",
 *     "functionName": "McpListAssets",
 *     "parameters": { "Path": "/Game", "ClassFilter": "" }
 *   }
 */
UCLASS()
class HKTMCPBRIDGEEDITOR_API UHktMcpFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ==================== Asset Tools ====================
   
    /** List assets in specified path */
    UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
    static FString McpListAssets(const FString& Path, const FString& ClassFilter = TEXT(""));

    /** Search assets by name */
    UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
    static FString McpSearchAssets(const FString& SearchQuery, const FString& ClassFilter = TEXT(""));

    /** Get detailed asset information */
    UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
    static FString McpGetAssetDetails(const FString& AssetPath);

    /** Modify asset property */
    UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
    static bool McpModifyAssetProperty(const FString& AssetPath, const FString& PropertyName, const FString& NewValue);

    // ==================== Level Tools ====================

    /** List actors in current level */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static FString McpListActors(const FString& ClassFilter = TEXT(""));

    /** Spawn actor from blueprint */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static FString McpSpawnActor(const FString& BlueprintPath, FVector Location, FRotator Rotation, const FString& ActorLabel = TEXT(""));

    /** Spawn actor by class name */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static FString McpSpawnActorByClass(const FString& ClassName, FVector Location, FRotator Rotation);

    /** Modify actor transform */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static bool McpModifyActorTransform(const FString& ActorName, FVector NewLocation, FRotator NewRotation, FVector NewScale);

    /** Delete actor */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static bool McpDeleteActor(const FString& ActorName);

    /** Select actor in editor */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static bool McpSelectActor(const FString& ActorName);

    /** Get selected actors info */
    UFUNCTION(BlueprintCallable, Category = "MCP|Level")
    static FString McpGetSelectedActors();

    // ==================== Query Tools ====================

    /** Search classes by name */
    UFUNCTION(BlueprintCallable, Category = "MCP|Query")
    static FString McpSearchClasses(const FString& SearchQuery, bool bBlueprintOnly = false);

    /** Get class properties */
    UFUNCTION(BlueprintCallable, Category = "MCP|Query")
    static FString McpGetClassProperties(const FString& ClassName);

    /** Get project folder structure */
    UFUNCTION(BlueprintCallable, Category = "MCP|Query")
    static FString McpGetProjectStructure(const FString& RootPath = TEXT("/Game"));

    /** Get current level info */
    UFUNCTION(BlueprintCallable, Category = "MCP|Query")
    static FString McpGetCurrentLevelInfo();

    // ==================== Editor Control ====================

    /** Open level */
    UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
    static bool McpOpenLevel(const FString& LevelPath);

    /** Save current level */
    UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
    static bool McpSaveCurrentLevel();

    /** Start PIE */
    UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
    static bool McpStartPIE();

    /** Stop PIE */
    UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
    static bool McpStopPIE();

    /** Check if PIE is running */
    UFUNCTION(BlueprintPure, Category = "MCP|Editor")
    static bool McpIsPIERunning();

    /** Execute console command */
    UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
    static void McpExecuteCommand(const FString& Command);

    // ==================== Utility ====================

    /** Show editor notification */
    UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
    static void McpShowNotification(const FString& Message, float Duration = 3.0f);

    /** Ping - check if MCP is available */
    UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
    static FString McpPing();

    /** Get viewport camera transform */
    UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
    static FString McpGetViewportCamera();

    /** Set viewport camera transform */
    UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
    static bool McpSetViewportCamera(FVector Location, FRotator Rotation);

private:
    static UHktMcpEditorSubsystem* GetSubsystem();
};
