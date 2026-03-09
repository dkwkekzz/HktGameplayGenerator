// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorFunctionLibrary.h"
#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "NiagaraSystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXMcp, Log, All);

namespace
{
	FString MakeSuccessResponse(const FString& AssetPath)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("success"), true);
		Writer->WriteValue(TEXT("assetPath"), AssetPath);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	FString MakeErrorResponse(const FString& Error)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("success"), false);
		Writer->WriteValue(TEXT("error"), Error);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	UHktVFXGeneratorSubsystem* GetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UHktVFXGeneratorSubsystem>() : nullptr;
	}
}

// ============================================================================
// 빌드
// ============================================================================

FString UHktVFXGeneratorFunctionLibrary::McpBuildNiagaraSystem(
	const FString& JsonConfig,
	const FString& OutputDir)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	UNiagaraSystem* Result = Subsystem->BuildNiagaraFromJson(JsonConfig, OutputDir);
	if (!Result)
	{
		return MakeErrorResponse(TEXT("Failed to build Niagara system from JSON config"));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Built Niagara system: %s"), *Result->GetPathName());
	return MakeSuccessResponse(Result->GetPathName());
}

FString UHktVFXGeneratorFunctionLibrary::McpBuildPresetExplosion(
	const FString& Name,
	float R, float G, float B,
	float Intensity,
	const FString& OutputDir)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	FLinearColor Color(R, G, B, 1.0f);
	UNiagaraSystem* Result = Subsystem->BuildPresetExplosion(Name, Color, Intensity, OutputDir);
	if (!Result)
	{
		return MakeErrorResponse(TEXT("Failed to build preset explosion"));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Built preset explosion: %s"), *Result->GetPathName());
	return MakeSuccessResponse(Result->GetPathName());
}

// ============================================================================
// LLM 학습용
// ============================================================================

FString UHktVFXGeneratorFunctionLibrary::McpGetVFXConfigSchema()
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return FHktVFXNiagaraConfig::GetSchemaJson();
	}

	return Subsystem->GetConfigSchemaJson();
}

FString UHktVFXGeneratorFunctionLibrary::McpGetVFXPromptGuide()
{
	FString S;

	// --- 개요 ---
	S += TEXT("=== HKT VFX Generator - Config Design Guide ===\n\n");

	S += TEXT("[OVERVIEW]\n");
	S += TEXT("Generate Niagara VFX by composing a JSON config with multiple emitter layers.\n");
	S += TEXT("Each emitter is a particle group with its own spawn/init/update/render settings.\n");
	S += TEXT("Rich VFX = 3~6 emitters with different roles layered together.\n\n");

	// --- 에미터 레이어 패턴 ---
	S += TEXT("[EMITTER LAYER PATTERNS]\n");
	S += TEXT("Compose these roles to build visually rich effects:\n\n");

	S += TEXT("  CoreGlow     - Central bright flash. Few particles, large size, short life, high color intensity (r/g/b > 2.0).\n");
	S += TEXT("                 sizeScaleStart=0.3, sizeScaleEnd=2.0, opacityEnd=0, additive blend.\n\n");

	S += TEXT("  Sparks       - Fast small particles flying outward. Many particles, small size, velocity_aligned.\n");
	S += TEXT("                 High velocity (300-1000), gravity, drag, colorOverLife (bright->dark), opacityEnd=0.\n\n");

	S += TEXT("  Smoke/Cloud  - Slow expanding translucent particles. Large final size via sizeScaleEnd=2~4.\n");
	S += TEXT("                 Low velocity, upward gravity(+Z), noise for organic motion. translucent blend.\n\n");

	S += TEXT("  Shockwave    - Single particle expanding rapidly. 1 burstCount, sizeScaleEnd=10~30.\n");
	S += TEXT("                 Short life (0.2-0.5s), opacityEnd=0, additive blend.\n\n");

	S += TEXT("  Trail/Ribbon - Continuous particles for trails. rate mode, ribbon renderer.\n");
	S += TEXT("                 ribbonWidth for thickness. Works with movement.\n\n");

	S += TEXT("  Light        - Dynamic lighting. light renderer, 1 particle.\n");
	S += TEXT("                 lightRadiusScale=2~10, lightIntensity=1~5. Short life for flash, long for ambient.\n\n");

	S += TEXT("  Debris       - Gravity-affected chunks. Medium count, random rotation (spriteRotationMax=360),\n");
	S += TEXT("                 high rotationRate, strong gravity (-980), drag=1~3.\n\n");

	S += TEXT("  Ambient      - Persistent floating particles. rate mode (5-20/sec), looping=true.\n");
	S += TEXT("                 Small size, low velocity, noise for drift. warmupTime=2~5.\n\n");

	// --- 값 범위 가이드 ---
	S += TEXT("[VALUE RANGES]\n");
	S += TEXT("  Size: 1-10 (tiny sparks), 10-50 (normal), 50-200 (smoke/glow), 200+ (shockwave)\n");
	S += TEXT("  Velocity: 50-200 (slow drift), 200-500 (normal), 500-1500 (fast burst)\n");
	S += TEXT("  Lifetime: 0.05-0.3 (flash), 0.3-1.0 (sparks), 1.0-4.0 (smoke), 4.0+ (ambient)\n");
	S += TEXT("  Color RGB: 0-1 (normal), 1-5 (bright/emissive), 5-10+ (intense glow, additive only)\n");
	S += TEXT("  Gravity Z: -980 (earth), -490 (half), 0 (zero-g), +50~200 (rising smoke/fire)\n");
	S += TEXT("  Drag: 0 (none), 0.5-2 (light), 2-5 (heavy), 5+ (stops quickly)\n");
	S += TEXT("  NoiseStrength: 10-50 (subtle), 50-200 (turbulent), 200+ (chaotic)\n");
	S += TEXT("  SortOrder: higher = renders on top. Light=15, Glow=10, Sparks=5, Smoke=0\n\n");

	// --- 디자인 팁 ---
	S += TEXT("[DESIGN TIPS]\n");
	S += TEXT("  - Use additive blend for glowing/fire/energy. translucent for smoke/dust/clouds.\n");
	S += TEXT("  - velocity_aligned alignment makes sparks/debris look like streaks.\n");
	S += TEXT("  - burstDelay staggers layers (e.g., smoke 0.05s after explosion).\n");
	S += TEXT("  - colorOverLife: fire = orange->red->black. magic = blue->purple->transparent.\n");
	S += TEXT("  - sizeScaleStart < 1 + sizeScaleEnd > 1 = particle grows. Reverse = shrinks.\n");
	S += TEXT("  - Combine vortex + attraction for swirling effects (tornado, portal).\n");
	S += TEXT("  - For looping effects, use rate mode + set system looping=true + warmupTime.\n");
	S += TEXT("  - Only set non-default values. Omitted fields use sensible defaults.\n\n");

	// --- 스키마 ---
	S += TEXT("[JSON SCHEMA]\n");
	S += FHktVFXNiagaraConfig::GetSchemaJson();
	S += TEXT("\n");

	return S;
}

FString UHktVFXGeneratorFunctionLibrary::McpGetVFXExampleConfigs()
{
	FString S;
	S += TEXT("[\n");

	// === Example 1: Explosion ===
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Explosion - burst fire with sparks, smoke, shockwave, light\",\n");
	S += TEXT("  \"systemName\": \"Explosion_Fire\",\n");
	S += TEXT("  \"emitters\": [\n");
	// CoreFlash
	S += TEXT("    {\"name\":\"CoreFlash\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":5},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.05,\"lifetimeMax\":0.2,\"sizeMin\":80,\"sizeMax\":200,\n");
	S += TEXT("            \"color\":{\"r\":5,\"g\":3,\"b\":0.5}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0},\"drag\":0,\n");
	S += TEXT("              \"sizeScaleStart\":0.3,\"sizeScaleEnd\":2.0,\"opacityStart\":1,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":10}},\n");
	// Sparks
	S += TEXT("    {\"name\":\"Sparks\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":80},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.4,\"lifetimeMax\":1.5,\"sizeMin\":3,\"sizeMax\":12,\n");
	S += TEXT("            \"spriteRotationMax\":360,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-600,\"y\":-600,\"z\":-200},\n");
	S += TEXT("            \"velocityMax\":{\"x\":600,\"y\":600,\"z\":800},\n");
	S += TEXT("            \"color\":{\"r\":1,\"g\":0.6,\"b\":0.1}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-980},\"drag\":2,\n");
	S += TEXT("              \"rotationRateMin\":-180,\"rotationRateMax\":180,\n");
	S += TEXT("              \"sizeScaleEnd\":0.1,\"opacityEnd\":0,\n");
	S += TEXT("              \"useColorOverLife\":true,\"colorEnd\":{\"r\":0.3,\"g\":0.05,\"b\":0}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"alignment\":\"velocity_aligned\",\"sortOrder\":5}},\n");
	// Smoke
	S += TEXT("    {\"name\":\"Smoke\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":15,\"burstDelay\":0.05},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":40,\"sizeMax\":120,\n");
	S += TEXT("            \"spriteRotationMax\":360,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-100,\"y\":-100,\"z\":0},\n");
	S += TEXT("            \"velocityMax\":{\"x\":100,\"y\":100,\"z\":200},\n");
	S += TEXT("            \"color\":{\"r\":0.15,\"g\":0.12,\"b\":0.1,\"a\":0.5}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":50},\"drag\":2,\n");
	S += TEXT("              \"rotationRateMin\":-30,\"rotationRateMax\":30,\n");
	S += TEXT("              \"sizeScaleStart\":0.5,\"sizeScaleEnd\":3,\"opacityStart\":0.5,\"opacityEnd\":0,\n");
	S += TEXT("              \"noiseStrength\":50,\"noiseFrequency\":2},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"translucent\",\"sortOrder\":0}},\n");
	// Shockwave
	S += TEXT("    {\"name\":\"Shockwave\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":1},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.2,\"lifetimeMax\":0.4,\"sizeMin\":20,\"sizeMax\":30,\n");
	S += TEXT("            \"color\":{\"r\":3,\"g\":2,\"b\":0.5,\"a\":0.8}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0},\n");
	S += TEXT("              \"sizeScaleStart\":1,\"sizeScaleEnd\":20,\"opacityStart\":0.8,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":2}},\n");
	// Light
	S += TEXT("    {\"name\":\"Light\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":1},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.1,\"lifetimeMax\":0.5,\"color\":{\"r\":3,\"g\":2,\"b\":0.5}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":5,\"lightIntensity\":3}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// === Example 2: Magic Portal (looping) ===
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Magic Portal - looping swirl with ambient particles and glow\",\n");
	S += TEXT("  \"systemName\": \"MagicPortal\",\n");
	S += TEXT("  \"warmupTime\": 3,\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"emitters\": [\n");
	// Swirl particles
	S += TEXT("    {\"name\":\"SwirlParticles\",\"spawn\":{\"mode\":\"rate\",\"rate\":30},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":2,\"sizeMin\":5,\"sizeMax\":15,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-50,\"y\":-50,\"z\":-20},\n");
	S += TEXT("            \"velocityMax\":{\"x\":50,\"y\":50,\"z\":20},\n");
	S += TEXT("            \"color\":{\"r\":0.3,\"g\":0.5,\"b\":3}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0},\"drag\":1,\n");
	S += TEXT("              \"opacityEnd\":0,\"sizeScaleEnd\":0.3,\n");
	S += TEXT("              \"vortexStrength\":200,\"vortexRadius\":80,\n");
	S += TEXT("              \"attractionStrength\":50,\"attractionRadius\":150},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":5}},\n");
	// Core glow
	S += TEXT("    {\"name\":\"CoreGlow\",\"spawn\":{\"mode\":\"rate\",\"rate\":5},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.5,\"lifetimeMax\":1,\"sizeMin\":30,\"sizeMax\":60,\n");
	S += TEXT("            \"color\":{\"r\":0.5,\"g\":0.8,\"b\":5}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0},\n");
	S += TEXT("              \"sizeScaleStart\":0.5,\"sizeScaleEnd\":1.5,\"opacityEnd\":0,\n");
	S += TEXT("              \"noiseStrength\":20,\"noiseFrequency\":3},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":8}},\n");
	// Ambient sparkles
	S += TEXT("    {\"name\":\"Sparkles\",\"spawn\":{\"mode\":\"rate\",\"rate\":10},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":0.8,\"sizeMin\":2,\"sizeMax\":6,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-30,\"y\":-30,\"z\":-30},\n");
	S += TEXT("            \"velocityMax\":{\"x\":30,\"y\":30,\"z\":30},\n");
	S += TEXT("            \"color\":{\"r\":1,\"g\":1,\"b\":3}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0},\n");
	S += TEXT("              \"opacityEnd\":0,\"noiseStrength\":30,\"noiseFrequency\":5},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":3}},\n");
	// Light
	S += TEXT("    {\"name\":\"PortalLight\",\"spawn\":{\"mode\":\"rate\",\"rate\":2},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":0.6,\"color\":{\"r\":0.5,\"g\":0.7,\"b\":3}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":4,\"lightIntensity\":2}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// === Example 3: Campfire (looping) ===
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Campfire - rising flames, embers, smoke, warm light\",\n");
	S += TEXT("  \"systemName\": \"Campfire\",\n");
	S += TEXT("  \"warmupTime\": 2,\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"emitters\": [\n");
	// Flames
	S += TEXT("    {\"name\":\"Flames\",\"spawn\":{\"mode\":\"rate\",\"rate\":20},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":0.8,\"sizeMin\":15,\"sizeMax\":40,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-30,\"y\":-30,\"z\":100},\n");
	S += TEXT("            \"velocityMax\":{\"x\":30,\"y\":30,\"z\":250},\n");
	S += TEXT("            \"color\":{\"r\":3,\"g\":1.5,\"b\":0.2}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":100},\"drag\":3,\n");
	S += TEXT("              \"sizeScaleStart\":1,\"sizeScaleEnd\":0.1,\"opacityEnd\":0,\n");
	S += TEXT("              \"useColorOverLife\":true,\"colorEnd\":{\"r\":1,\"g\":0.1,\"b\":0},\n");
	S += TEXT("              \"noiseStrength\":80,\"noiseFrequency\":4},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":5}},\n");
	// Embers
	S += TEXT("    {\"name\":\"Embers\",\"spawn\":{\"mode\":\"rate\",\"rate\":8},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":1,\"sizeMax\":4,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-20,\"y\":-20,\"z\":50},\n");
	S += TEXT("            \"velocityMax\":{\"x\":20,\"y\":20,\"z\":150},\n");
	S += TEXT("            \"color\":{\"r\":2,\"g\":0.8,\"b\":0.1}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":30},\"drag\":0.5,\n");
	S += TEXT("              \"opacityEnd\":0,\n");
	S += TEXT("              \"noiseStrength\":40,\"noiseFrequency\":2},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"additive\",\"sortOrder\":7}},\n");
	// Smoke
	S += TEXT("    {\"name\":\"Smoke\",\"spawn\":{\"mode\":\"rate\",\"rate\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":2,\"lifetimeMax\":5,\"sizeMin\":20,\"sizeMax\":50,\n");
	S += TEXT("            \"spriteRotationMax\":360,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-15,\"y\":-15,\"z\":40},\n");
	S += TEXT("            \"velocityMax\":{\"x\":15,\"y\":15,\"z\":80},\n");
	S += TEXT("            \"color\":{\"r\":0.1,\"g\":0.08,\"b\":0.06,\"a\":0.3}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":20},\"drag\":1,\n");
	S += TEXT("              \"rotationRateMin\":-20,\"rotationRateMax\":20,\n");
	S += TEXT("              \"sizeScaleStart\":0.5,\"sizeScaleEnd\":4,\"opacityStart\":0.3,\"opacityEnd\":0,\n");
	S += TEXT("              \"noiseStrength\":30,\"noiseFrequency\":1},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"sprite\",\"blendMode\":\"translucent\",\"sortOrder\":0}},\n");
	// Warm light
	S += TEXT("    {\"name\":\"WarmLight\",\"spawn\":{\"mode\":\"rate\",\"rate\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.2,\"lifetimeMax\":0.5,\"color\":{\"r\":2,\"g\":1,\"b\":0.3}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":0}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":3,\"lightIntensity\":2}}\n");
	S += TEXT("  ]\n");
	S += TEXT("}\n");

	S += TEXT("]");
	return S;
}

// ============================================================================
// 조회
// ============================================================================

FString UHktVFXGeneratorFunctionLibrary::McpListGeneratedVFX(const FString& Directory)
{
	const FString ResolvedDir = Directory.IsEmpty()
		? UHktVFXGeneratorSettings::Get()->DefaultOutputDirectory
		: Directory;

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*ResolvedDir), Assets, true);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("assets"));

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath.GetAssetName() == TEXT("NiagaraSystem"))
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Asset.AssetName.ToString());
			Writer->WriteValue(TEXT("path"), Asset.GetObjectPathString());
			Writer->WriteObjectEnd();
		}
	}

	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	return Output;
}
