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

	// Config를 파싱하여 텍스처 생성 요청 수집
	FHktVFXNiagaraConfig ParsedConfig;
	FHktVFXNiagaraConfig::FromJson(JsonConfig, ParsedConfig);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("success"), true);
	Writer->WriteValue(TEXT("assetPath"), Result->GetPathName());

	// 텍스처 생성 요청이 있으면 포함
	bool bHasTextureRequests = false;
	for (const auto& E : ParsedConfig.Emitters)
	{
		if (!E.Render.TexturePrompt.IsEmpty())
		{
			bHasTextureRequests = true;
			break;
		}
	}
	if (bHasTextureRequests)
	{
		Writer->WriteArrayStart(TEXT("textureRequests"));
		for (const auto& E : ParsedConfig.Emitters)
		{
			if (E.Render.TexturePrompt.IsEmpty()) continue;
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("emitterName"), E.Name);
			Writer->WriteValue(TEXT("prompt"), E.Render.TexturePrompt);
			if (!E.Render.TextureNegativePrompt.IsEmpty())
				Writer->WriteValue(TEXT("negativePrompt"), E.Render.TextureNegativePrompt);
			Writer->WriteValue(TEXT("type"), E.Render.TextureType.IsEmpty() ? TEXT("particle_sprite") : E.Render.TextureType);
			Writer->WriteValue(TEXT("resolution"), E.Render.TextureResolution > 0 ? E.Render.TextureResolution : 256);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
	}

	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
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

	S += TEXT("=== HKT VFX Generator - Config Design Guide (Phase 2) ===\n\n");

	// ===================================================================
	S += TEXT("[OVERVIEW]\n");
	S += TEXT("Generate Niagara VFX by composing a JSON config with multiple emitter layers.\n");
	S += TEXT("Each emitter uses a TEMPLATE that determines available physics modules.\n");
	S += TEXT("Rich VFX = 3~6 emitters with different templates layered together.\n\n");

	// ===================================================================
	S += TEXT("[TEMPLATE REFERENCE]\n");
	S += TEXT("Set render.emitterTemplate to select template. This determines available modules.\n");
	S += TEXT("Config params for missing modules are silently ignored — safe to always set them.\n\n");

	// --- 14 Built-in Templates ---
	S += TEXT("=== BUILT-IN TEMPLATES (always available) ===\n\n");

	S += TEXT("  simple_sprite_burst  [BURST, Sprite]\n");
	S += TEXT("    Modules: InitializeParticle, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Basic sprite burst. NO gravity/drag/noise.\n");
	S += TEXT("    Best for: Simple flashes, shockwaves, basic one-shot effects\n\n");

	S += TEXT("  omnidirectional_burst  [BURST, Sprite]\n");
	S += TEXT("    Modules: InitializeParticle, GravityForce, Drag, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: All-direction explosion with physics. HAS gravity+drag.\n");
	S += TEXT("    Best for: Explosions, sparks, debris bursts\n\n");

	S += TEXT("  directional_burst  [BURST, Sprite]\n");
	S += TEXT("    Modules: InitializeParticle, GravityForce, Drag, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Cone-shaped directional burst with physics.\n");
	S += TEXT("    Best for: Muzzle sparks, directional impacts, water splashes\n\n");

	S += TEXT("  confetti_burst  [BURST, Sprite]\n");
	S += TEXT("    Modules: InitializeParticle, GravityForce, Drag, SpriteRotationRate, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Tumbling particles with rotation. HAS gravity+drag+rotation.\n");
	S += TEXT("    Best for: Confetti, leaf fall, debris with spin\n\n");

	S += TEXT("  fountain  [RATE, Sprite]\n");
	S += TEXT("    Modules: SpawnRate, InitializeParticle, GravityForce, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Continuous upward spray. spawn.mode='rate'. HAS gravity.\n");
	S += TEXT("    Best for: Fountains, fire columns, continuous sparks, rain\n\n");

	S += TEXT("  blowing_particles  [RATE, Sprite]\n");
	S += TEXT("    Modules: SpawnRate, InitializeParticle, CurlNoiseForce, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Wind-blown particles with noise. spawn.mode='rate'. HAS curl noise.\n");
	S += TEXT("    Best for: Dust, snow, leaves in wind, atmospheric particles\n\n");

	S += TEXT("  hanging_particulates  [RATE, Sprite]\n");
	S += TEXT("    Modules: SpawnRate, InitializeParticle, CurlNoiseForce, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Floating particles with gentle drift. spawn.mode='rate'. HAS curl noise.\n");
	S += TEXT("    Best for: Dust motes, fireflies, ambient magic sparkles\n\n");

	S += TEXT("  upward_mesh_burst  [BURST, Mesh]\n");
	S += TEXT("    Modules: InitializeParticle, GravityForce, ScaleMeshSize, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Mesh-based burst with gravity. Mesh renderer.\n");
	S += TEXT("    Best for: Rock debris, shell casings, 3D chunk ejection\n\n");

	S += TEXT("  single_looping_particle  [BURST(1), Sprite]\n");
	S += TEXT("    Modules: InitializeParticle, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Single particle that loops forever. burstCount=1.\n");
	S += TEXT("    Best for: Auras, status indicators, persistent glow markers\n\n");

	S += TEXT("  ribbon  [RATE, Ribbon]\n");
	S += TEXT("    Modules: SpawnRate, InitializeParticle, ScaleColor, SolveForcesAndVelocity\n");
	S += TEXT("    Use: Location-based ribbon trail. spawn.mode='rate'. Ribbon renderer.\n");
	S += TEXT("    Best for: Sword trails, projectile trails, energy beams, motion trails\n\n");

	S += TEXT("  dynamic_beam  [RATE, Ribbon]\n");
	S += TEXT("    Modules: Beam modules, Ribbon renderer\n");
	S += TEXT("    Use: Dynamic beam between two points.\n");
	S += TEXT("    Best for: Lightning, laser beams, tether effects\n\n");

	S += TEXT("  static_beam  [RATE, Ribbon]\n");
	S += TEXT("    Modules: Beam modules, Ribbon renderer\n");
	S += TEXT("    Use: Static beam effect.\n");
	S += TEXT("    Best for: Persistent beams, shield connections\n\n");

	S += TEXT("  minimal  [BURST, Sprite]\n");
	S += TEXT("    Modules: SolveForcesAndVelocity only\n");
	S += TEXT("    Use: Bare minimum. Good base for light renderer.\n");
	S += TEXT("    Best for: Dynamic lights, simple point effects\n\n");

	S += TEXT("  recycle_particles  [RATE, Sprite]\n");
	S += TEXT("    Modules: Camera-aware recycling, CurlNoiseForce\n");
	S += TEXT("    Use: Particles recycled to stay in camera view.\n");
	S += TEXT("    Best for: Environment particles that follow camera (fog, dust)\n\n");

	// --- NiagaraExamples Templates ---
	S += TEXT("=== RICH TEMPLATES (NiagaraExamples, pre-configured with materials+textures) ===\n\n");

	S += TEXT("  spark           - Stretched spark with SubUV. Gravity+Drag built-in.\n");
	S += TEXT("  spark_secondary - Smaller secondary sparks.\n");
	S += TEXT("  spark_debris    - Spark-like debris fragments.\n");
	S += TEXT("  smoke           - SubUV smoke with Noise+Rotation. Translucent.\n");
	S += TEXT("  explosion       - SubUV animated explosion burst.\n");
	S += TEXT("  core            - Bright emissive core/flare.\n");
	S += TEXT("  debris          - Gravity-affected debris chunks.\n");
	S += TEXT("  dust            - Dust cloud explosion.\n");
	S += TEXT("  ground_dust     - Ground-level dust ring.\n");
	S += TEXT("  impact          - Sprite impact effect.\n");
	S += TEXT("  impact_mesh     - Mesh-based impact chunks.\n");
	S += TEXT("  muzzle_flash    - Weapon muzzle flash.\n");
	S += TEXT("  arc             - Electric arc ribbon.\n\n");

	// ===================================================================
	S += TEXT("[DYNAMIC MODULE INJECTION]\n");
	S += TEXT("The builder automatically injects missing modules into the emitter graph.\n");
	S += TEXT("If a template does NOT have a module you need, the builder adds it dynamically.\n");
	S += TEXT("This means ANY template can use ANY module combination — no limitations!\n");
	S += TEXT("Example: fountain template + vortexStrength → VortexVelocity module is auto-injected.\n\n");

	S += TEXT("[MODULE-PARAMETER MAP]\n");
	S += TEXT("Which config fields apply to which modules:\n\n");

	S += TEXT("  spawn.mode='burst'  → SpawnBurst_Instantaneous (burstCount, burstDelay)\n");
	S += TEXT("  spawn.mode='rate'   → SpawnRate (rate)\n\n");

	S += TEXT("  init.*              → InitializeParticle (Lifetime, Uniform Sprite Size, Color)\n\n");

	S += TEXT("  update.gravity      → Gravity Force module (auto-injected if missing)\n");
	S += TEXT("  update.drag         → Drag module (auto-injected if missing)\n");
	S += TEXT("  update.noiseStrength→ Curl Noise Force module (auto-injected if missing)\n");
	S += TEXT("  update.rotationRate → Sprite Rotation Rate module (auto-injected if missing)\n");
	S += TEXT("  update.sizeScale*   → Scale Sprite Size / Scale Mesh Size module\n");
	S += TEXT("  update.opacity/color→ ScaleColor module (Scale RGBA, Scale RGB)\n");
	S += TEXT("  update.attraction*  → Point Attraction Force module (auto-injected if missing)\n");
	S += TEXT("  update.vortex*      → Vortex Velocity module (auto-injected if missing)\n");
	S += TEXT("  update.vortexAxis   → Vortex Velocity rotation axis (default Z-up)\n");
	S += TEXT("  update.windForce    → Wind Force module (auto-injected if missing)\n");
	S += TEXT("  update.accelerationForce → Acceleration Force module (constant accel, auto-injected)\n");
	S += TEXT("  update.speedLimit   → SolveForcesAndVelocity Speed Limit\n\n");

	// ===================================================================
	S += TEXT("[TEMPLATE SELECTION GUIDE]\n");
	S += TEXT("Match your effect type to the right template:\n\n");

	S += TEXT("  Need gravity+drag?      → omnidirectional_burst, directional_burst, confetti_burst, fountain\n");
	S += TEXT("  Need curl noise/wind?    → blowing_particles, hanging_particulates\n");
	S += TEXT("  Need rotation?           → confetti_burst\n");
	S += TEXT("  Need continuous spawn?   → fountain, blowing_particles, hanging_particulates\n");
	S += TEXT("  Need ribbon trail?       → ribbon\n");
	S += TEXT("  Need mesh debris?        → upward_mesh_burst\n");
	S += TEXT("  Need vortex/swirl?       → ANY template + vortexStrength (auto-injected)\n");
	S += TEXT("  Need point attraction?   → ANY template + attractionStrength (auto-injected)\n");
	S += TEXT("  Need constant accel?     → ANY template + accelerationForce (auto-injected)\n");
	S += TEXT("  Need light?              → minimal (rendererType='light')\n");
	S += TEXT("  Need rich visuals?       → spark, smoke, explosion, core, debris (NiagaraExamples)\n\n");

	// ===================================================================
	S += TEXT("[MATERIAL LIBRARY]\n");
	S += TEXT("Override material via render.materialPath (optional):\n\n");

	S += TEXT("  /Game/NiagaraExamples/Materials/MI_Sparks              - Stretched spark\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_Flare               - Soft lens flare\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_Flames              - Animated flame SubUV\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_BasicSprite         - Clean basic sprite\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_BasicSprite_Translucent - Translucent sprite\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_SmokePuff_8x8      - Smoke puff SubUV\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_SmokeRoil_8x8      - Turbulent smoke SubUV\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_Explosion_8x8      - Explosion SubUV\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_FireBall_8x8       - Fireball SubUV\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_FireRoil_8x8       - Fire roil SubUV\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_SimpleDebris        - Opaque debris\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_Distortion          - Heat distortion\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_Fireworks           - Firework sparks\n");
	S += TEXT("  /Game/NiagaraExamples/Materials/MI_ImpactFlash         - Impact flash\n\n");

	// ===================================================================
	S += TEXT("[VALUE RANGES]\n");
	S += TEXT("  Size: 1-10 (tiny sparks), 10-50 (normal), 50-200 (smoke/glow), 200+ (shockwave)\n");
	S += TEXT("  Velocity: 50-200 (slow drift), 200-500 (normal), 500-1500 (fast burst)\n");
	S += TEXT("  Lifetime: 0.05-0.3 (flash), 0.3-1.0 (sparks), 1.0-4.0 (smoke), 4.0+ (ambient)\n");
	S += TEXT("  Color RGB: 0-1 (normal), 1-5 (bright/emissive), 5-10+ (intense glow)\n");
	S += TEXT("  Gravity Z: -980 (earth), -490 (half), 0 (zero-g), +50~200 (rising)\n");
	S += TEXT("  Drag: 0 (none), 0.5-2 (light), 2-5 (heavy), 5+ (stops quickly)\n");
	S += TEXT("  NoiseStrength: 10-50 (subtle), 50-200 (turbulent), 200+ (chaotic)\n");
	S += TEXT("  VortexStrength: 50-200 (gentle swirl), 200-500 (tornado), 500+ (violent)\n");
	S += TEXT("  VortexRadius: 50-100 (tight), 100-300 (medium), 300+ (wide orbit)\n");
	S += TEXT("  AttractionStrength: 50-200 (gentle pull), 200-1000 (strong implosion)\n");
	S += TEXT("  AccelerationForce: like gravity but any direction. 100-500 (mild), 500+ (strong)\n");
	S += TEXT("  WindForce: 50-200 (breeze), 200-500 (gusty), 500+ (storm)\n");
	S += TEXT("  SortOrder: higher = renders on top. Light=15, Glow=10, Sparks=5, Smoke=0\n\n");

	// ===================================================================
	S += TEXT("[DESIGN TIPS]\n");
	S += TEXT("  - ALWAYS prefer templates with the modules you need (see Template Selection Guide).\n");
	S += TEXT("  - Layer 3-6 emitters with different roles for rich effects.\n");
	S += TEXT("  - velocity_aligned alignment makes sparks/debris look like streaks.\n");
	S += TEXT("  - burstDelay staggers layers (smoke 0.05s after explosion).\n");
	S += TEXT("  - colorOverLife: fire=orange->dark, magic=blue->purple->transparent.\n");
	S += TEXT("  - sizeScaleEnd>1 = particle grows. sizeScaleEnd<1 = shrinks.\n");
	S += TEXT("  - For looping: spawn.mode='rate' + looping=true + warmupTime.\n");
	S += TEXT("  - Only set non-default values. Omitted fields use sensible defaults.\n");
	S += TEXT("  - Use McpDumpAllTemplateParameters() to see actual module params at runtime.\n");
	S += TEXT("  - Combine vortex+attraction for spiral implosion (portals, black holes).\n");
	S += TEXT("  - accelerationForce for constant thrust (missiles, jets, ascending spirits).\n");
	S += TEXT("  - vortexAxis: default (0,0,1)=Z. Set (1,0,0) for horizontal tornado.\n\n");

	// ===================================================================
	S += TEXT("[TEXTURE GENERATION]\n");
	S += TEXT("When a VFX needs a custom texture (not available in NiagaraExamples), add:\n");
	S += TEXT("  render.texturePrompt = SD prompt for texture generation\n");
	S += TEXT("  render.textureNegativePrompt = negative prompt (optional)\n");
	S += TEXT("  render.textureType = particle_sprite | flipbook_4x4 | flipbook_8x8 | noise | gradient\n");
	S += TEXT("  render.textureResolution = 256 | 512 | 1024\n\n");

	S += TEXT("The builder returns textureRequests[] in the response. External tool generates them.\n");
	S += TEXT("SD Prompt tips by textureType:\n");
	S += TEXT("  particle_sprite: 'centered soft circle, alpha gradient edge, black background, VFX sprite'\n");
	S += TEXT("  flipbook_4x4: '4x4 grid animation, explosion sequence, black background, sprite sheet'\n");
	S += TEXT("  flipbook_8x8: '8x8 grid animation, smoke sequence, black background, sprite sheet'\n");
	S += TEXT("  noise: 'tileable perlin noise, seamless, grayscale'\n");
	S += TEXT("  gradient: 'horizontal gradient ramp, left white right black, smooth falloff'\n\n");

	S += TEXT("Negative prompt base: 'text, watermark, frame, border, realistic photo, face'\n");
	S += TEXT("Always add to negative: type-specific (e.g., 'grid lines' for flipbooks).\n\n");

	// ===================================================================
	S += TEXT("[JSON SCHEMA]\n");
	S += FHktVFXNiagaraConfig::GetSchemaJson();
	S += TEXT("\n");

	return S;
}

FString UHktVFXGeneratorFunctionLibrary::McpGetVFXExampleConfigs()
{
	FString S;
	S += TEXT("[\n");

	// ===================================================================
	// Example 1: Explosion (rich templates - NiagaraExamples)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Explosion - rich templates with SubUV materials and textures\",\n");
	S += TEXT("  \"_templates_used\": \"core, explosion, spark, smoke, debris, ground_dust, minimal(light)\",\n");
	S += TEXT("  \"systemName\": \"Explosion_Fire\",\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"CoreFlash\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.05,\"lifetimeMax\":0.2,\"sizeMin\":80,\"sizeMax\":200,\"color\":{\"r\":5,\"g\":3,\"b\":0.5}},\n");
	S += TEXT("     \"update\":{\"sizeScaleEnd\":2.0,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"core\",\"sortOrder\":10}},\n");
	S += TEXT("    {\"name\":\"Burst\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":5},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":0.8,\"sizeMin\":60,\"sizeMax\":150,\"color\":{\"r\":2,\"g\":1,\"b\":0.3}},\n");
	S += TEXT("     \"update\":{\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"explosion\",\"sortOrder\":8}},\n");
	S += TEXT("    {\"name\":\"Sparks\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":80},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.4,\"lifetimeMax\":1.5,\"sizeMin\":3,\"sizeMax\":12,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-600,\"y\":-600,\"z\":-200},\"velocityMax\":{\"x\":600,\"y\":600,\"z\":800},\n");
	S += TEXT("            \"color\":{\"r\":1,\"g\":0.6,\"b\":0.1}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-980},\"drag\":2,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"spark\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"Smoke\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":15,\"burstDelay\":0.05},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":40,\"sizeMax\":120,\"color\":{\"r\":0.15,\"g\":0.12,\"b\":0.1,\"a\":0.5}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":50},\"drag\":2,\"sizeScaleEnd\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"smoke\",\"sortOrder\":0}},\n");
	S += TEXT("    {\"name\":\"Debris\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":10},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.5,\"lifetimeMax\":2,\"sizeMin\":5,\"sizeMax\":20,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-400,\"y\":-400,\"z\":100},\"velocityMax\":{\"x\":400,\"y\":400,\"z\":600}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-980},\"drag\":1},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"debris\",\"sortOrder\":3}},\n");
	S += TEXT("    {\"name\":\"Light\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":1},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.1,\"lifetimeMax\":0.5,\"color\":{\"r\":3,\"g\":2,\"b\":0.5}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":5,\"lightIntensity\":3}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 2: Water Fountain (built-in fountain template)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Water Fountain - continuous spray using fountain template (has SpawnRate+Gravity)\",\n");
	S += TEXT("  \"_templates_used\": \"fountain (built-in, has GravityForce)\",\n");
	S += TEXT("  \"systemName\": \"WaterFountain\",\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"warmupTime\": 1,\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"WaterDrops\",\"spawn\":{\"mode\":\"rate\",\"rate\":50},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":2,\"sizeMin\":3,\"sizeMax\":8,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-50,\"y\":-50,\"z\":400},\"velocityMax\":{\"x\":50,\"y\":50,\"z\":600},\n");
	S += TEXT("            \"color\":{\"r\":0.6,\"g\":0.8,\"b\":1.5,\"a\":0.7}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-980},\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"fountain\",\"blendMode\":\"translucent\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"Mist\",\"spawn\":{\"mode\":\"rate\",\"rate\":10},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":20,\"sizeMax\":50,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-30,\"y\":-30,\"z\":10},\"velocityMax\":{\"x\":30,\"y\":30,\"z\":50},\n");
	S += TEXT("            \"color\":{\"r\":0.8,\"g\":0.9,\"b\":1,\"a\":0.2}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":10},\"sizeScaleEnd\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"fountain\",\"blendMode\":\"translucent\",\"sortOrder\":0}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 3: Confetti Celebration (confetti_burst template)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Confetti - tumbling colored pieces using confetti_burst (has Gravity+Drag+Rotation)\",\n");
	S += TEXT("  \"_templates_used\": \"confetti_burst (built-in, has GravityForce+Drag+SpriteRotationRate)\",\n");
	S += TEXT("  \"systemName\": \"ConfettiCelebration\",\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"Confetti\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":200},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":2,\"lifetimeMax\":5,\"sizeMin\":5,\"sizeMax\":15,\n");
	S += TEXT("            \"spriteRotationMax\":360,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-300,\"y\":-300,\"z\":200},\"velocityMax\":{\"x\":300,\"y\":300,\"z\":800},\n");
	S += TEXT("            \"color\":{\"r\":1,\"g\":0.8,\"b\":0.2}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-400},\"drag\":2,\n");
	S += TEXT("              \"rotationRateMin\":-360,\"rotationRateMax\":360,\"opacityEnd\":0.5},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"confetti_burst\",\"sortOrder\":5}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 4: Dust Storm (blowing_particles template - has CurlNoise)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Dust Storm - wind-blown particles using blowing_particles (has CurlNoiseForce)\",\n");
	S += TEXT("  \"_templates_used\": \"blowing_particles (built-in, has CurlNoiseForce)\",\n");
	S += TEXT("  \"systemName\": \"DustStorm\",\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"warmupTime\": 3,\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"DustParticles\",\"spawn\":{\"mode\":\"rate\",\"rate\":30},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":2,\"lifetimeMax\":5,\"sizeMin\":5,\"sizeMax\":20,\n");
	S += TEXT("            \"velocityMin\":{\"x\":100,\"y\":-50,\"z\":-10},\"velocityMax\":{\"x\":300,\"y\":50,\"z\":30},\n");
	S += TEXT("            \"color\":{\"r\":0.6,\"g\":0.5,\"b\":0.3,\"a\":0.4}},\n");
	S += TEXT("     \"update\":{\"noiseStrength\":100,\"noiseFrequency\":2,\"opacityEnd\":0,\"sizeScaleEnd\":2},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"blowing_particles\",\"blendMode\":\"translucent\",\"sortOrder\":3}},\n");
	S += TEXT("    {\"name\":\"FineParticles\",\"spawn\":{\"mode\":\"rate\",\"rate\":15},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":3,\"lifetimeMax\":7,\"sizeMin\":1,\"sizeMax\":5,\n");
	S += TEXT("            \"velocityMin\":{\"x\":50,\"y\":-30,\"z\":-5},\"velocityMax\":{\"x\":200,\"y\":30,\"z\":20},\n");
	S += TEXT("            \"color\":{\"r\":0.7,\"g\":0.6,\"b\":0.4,\"a\":0.6}},\n");
	S += TEXT("     \"update\":{\"noiseStrength\":60,\"noiseFrequency\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"blowing_particles\",\"blendMode\":\"translucent\",\"sortOrder\":5}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 5: Firefly Ambiance (hanging_particulates - ambient noise)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Fireflies - floating light particles using hanging_particulates (has CurlNoiseForce)\",\n");
	S += TEXT("  \"_templates_used\": \"hanging_particulates (built-in, has CurlNoiseForce), minimal (light)\",\n");
	S += TEXT("  \"systemName\": \"FireflyAmbiance\",\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"warmupTime\": 5,\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"Fireflies\",\"spawn\":{\"mode\":\"rate\",\"rate\":5},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":3,\"lifetimeMax\":8,\"sizeMin\":2,\"sizeMax\":5,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-10,\"y\":-10,\"z\":-5},\"velocityMax\":{\"x\":10,\"y\":10,\"z\":5},\n");
	S += TEXT("            \"color\":{\"r\":1.5,\"g\":2,\"b\":0.3}},\n");
	S += TEXT("     \"update\":{\"noiseStrength\":30,\"noiseFrequency\":1,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"hanging_particulates\",\"blendMode\":\"additive\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"GlowLight\",\"spawn\":{\"mode\":\"rate\",\"rate\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"color\":{\"r\":0.8,\"g\":1,\"b\":0.2}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":2,\"lightIntensity\":1}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 6: Gunshot Impact (omnidirectional_burst + directional_burst)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Gunshot Impact - omnidirectional sparks + directional debris (built-in physics templates)\",\n");
	S += TEXT("  \"_templates_used\": \"omnidirectional_burst, directional_burst (both have GravityForce+Drag)\",\n");
	S += TEXT("  \"systemName\": \"GunImpact_Concrete\",\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"ImpactSparks\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":30},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.2,\"lifetimeMax\":0.8,\"sizeMin\":1,\"sizeMax\":4,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-300,\"y\":-300,\"z\":0},\"velocityMax\":{\"x\":300,\"y\":300,\"z\":500},\n");
	S += TEXT("            \"color\":{\"r\":2,\"g\":1.5,\"b\":0.5}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-980},\"drag\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"omnidirectional_burst\",\"blendMode\":\"additive\",\n");
	S += TEXT("              \"alignment\":\"velocity_aligned\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"ChipDebris\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":8},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.5,\"lifetimeMax\":1.5,\"sizeMin\":2,\"sizeMax\":8,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-150,\"y\":-150,\"z\":100},\"velocityMax\":{\"x\":150,\"y\":150,\"z\":400},\n");
	S += TEXT("            \"color\":{\"r\":0.5,\"g\":0.5,\"b\":0.4}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":-980},\"drag\":1},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"directional_burst\",\"blendMode\":\"translucent\",\"sortOrder\":3}},\n");
	S += TEXT("    {\"name\":\"DustPuff\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":5},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.5,\"lifetimeMax\":1.5,\"sizeMin\":10,\"sizeMax\":30,\n");
	S += TEXT("            \"color\":{\"r\":0.4,\"g\":0.35,\"b\":0.3,\"a\":0.4}},\n");
	S += TEXT("     \"update\":{\"sizeScaleEnd\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"simple_sprite_burst\",\"blendMode\":\"translucent\",\"sortOrder\":0}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 7: Campfire (rich templates + material override)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Campfire - flames+embers+smoke using rich templates with material overrides\",\n");
	S += TEXT("  \"_templates_used\": \"fountain (gravity), spark (gravity+drag), smoke, minimal (light)\",\n");
	S += TEXT("  \"systemName\": \"Campfire\",\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"warmupTime\": 2,\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"Flames\",\"spawn\":{\"mode\":\"rate\",\"rate\":20},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":0.8,\"sizeMin\":15,\"sizeMax\":40,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-30,\"y\":-30,\"z\":100},\"velocityMax\":{\"x\":30,\"y\":30,\"z\":250},\n");
	S += TEXT("            \"color\":{\"r\":3,\"g\":1.5,\"b\":0.2}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":100},\"sizeScaleEnd\":0.1,\"opacityEnd\":0,\n");
	S += TEXT("              \"useColorOverLife\":true,\"colorEnd\":{\"r\":1,\"g\":0.1,\"b\":0}},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"fountain\",\n");
	S += TEXT("              \"materialPath\":\"/Game/NiagaraExamples/Materials/MI_Flames\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"Embers\",\"spawn\":{\"mode\":\"rate\",\"rate\":8},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":1,\"sizeMax\":4,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-20,\"y\":-20,\"z\":50},\"velocityMax\":{\"x\":20,\"y\":20,\"z\":150},\n");
	S += TEXT("            \"color\":{\"r\":2,\"g\":0.8,\"b\":0.1}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":30},\"drag\":0.5,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"spark\",\"sortOrder\":7}},\n");
	S += TEXT("    {\"name\":\"Smoke\",\"spawn\":{\"mode\":\"rate\",\"rate\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":2,\"lifetimeMax\":5,\"sizeMin\":20,\"sizeMax\":50,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-15,\"y\":-15,\"z\":40},\"velocityMax\":{\"x\":15,\"y\":15,\"z\":80},\n");
	S += TEXT("            \"color\":{\"r\":0.1,\"g\":0.08,\"b\":0.06,\"a\":0.3}},\n");
	S += TEXT("     \"update\":{\"gravity\":{\"x\":0,\"y\":0,\"z\":20},\"drag\":1,\n");
	S += TEXT("              \"sizeScaleEnd\":4,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"smoke\",\"sortOrder\":0}},\n");
	S += TEXT("    {\"name\":\"WarmLight\",\"spawn\":{\"mode\":\"rate\",\"rate\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.2,\"lifetimeMax\":0.5,\"color\":{\"r\":2,\"g\":1,\"b\":0.3}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":3,\"lightIntensity\":2}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 8: Magic Portal (Vortex + Point Attraction + Curl Noise)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Magic Portal - swirling particles pulled inward with vortex + attraction\",\n");
	S += TEXT("  \"_templates_used\": \"hanging_particulates (all modules auto-injected), core, minimal(light)\",\n");
	S += TEXT("  \"systemName\": \"MagicPortal_Arcane\",\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"warmupTime\": 3,\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"VortexRing\",\"spawn\":{\"mode\":\"rate\",\"rate\":40},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":3,\"sizeMax\":8,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-100,\"y\":-100,\"z\":-20},\"velocityMax\":{\"x\":100,\"y\":100,\"z\":20},\n");
	S += TEXT("            \"color\":{\"r\":0.5,\"g\":0.2,\"b\":3}},\n");
	S += TEXT("     \"update\":{\"vortexStrength\":300,\"vortexRadius\":150,\n");
	S += TEXT("              \"attractionStrength\":200,\"attractionRadius\":250,\n");
	S += TEXT("              \"noiseStrength\":30,\"noiseFrequency\":2,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"hanging_particulates\",\"blendMode\":\"additive\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"CoreGlow\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":2},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.5,\"lifetimeMax\":1,\"sizeMin\":50,\"sizeMax\":100,\"color\":{\"r\":2,\"g\":0.5,\"b\":5}},\n");
	S += TEXT("     \"update\":{\"sizeScaleEnd\":1.5,\"opacityEnd\":0.5},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"core\",\"sortOrder\":10}},\n");
	S += TEXT("    {\"name\":\"PortalLight\",\"spawn\":{\"mode\":\"rate\",\"rate\":2},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":0.8,\"color\":{\"r\":0.5,\"g\":0.2,\"b\":3}},\n");
	S += TEXT("     \"render\":{\"rendererType\":\"light\",\"lightRadiusScale\":4,\"lightIntensity\":2}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 9: Tornado (Vortex + Wind + Acceleration)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Tornado - strong vortex with upward acceleration and wind-blown debris\",\n");
	S += TEXT("  \"_templates_used\": \"fountain (all force modules auto-injected)\",\n");
	S += TEXT("  \"systemName\": \"Tornado_Dust\",\n");
	S += TEXT("  \"looping\": true,\n");
	S += TEXT("  \"warmupTime\": 3,\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"MainFunnel\",\"spawn\":{\"mode\":\"rate\",\"rate\":80},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":2,\"lifetimeMax\":5,\"sizeMin\":5,\"sizeMax\":25,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-50,\"y\":-50,\"z\":20},\"velocityMax\":{\"x\":50,\"y\":50,\"z\":80},\n");
	S += TEXT("            \"color\":{\"r\":0.5,\"g\":0.4,\"b\":0.3,\"a\":0.6}},\n");
	S += TEXT("     \"update\":{\"vortexStrength\":500,\"vortexRadius\":200,\n");
	S += TEXT("              \"accelerationForce\":{\"x\":0,\"y\":0,\"z\":150},\n");
	S += TEXT("              \"drag\":0.5,\"sizeScaleEnd\":2,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"fountain\",\"blendMode\":\"translucent\",\"sortOrder\":3}},\n");
	S += TEXT("    {\"name\":\"FlyingDebris\",\"spawn\":{\"mode\":\"rate\",\"rate\":10},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":3,\"sizeMin\":3,\"sizeMax\":15,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-200,\"y\":-200,\"z\":0},\"velocityMax\":{\"x\":200,\"y\":200,\"z\":100},\n");
	S += TEXT("            \"color\":{\"r\":0.4,\"g\":0.35,\"b\":0.25}},\n");
	S += TEXT("     \"update\":{\"vortexStrength\":400,\"vortexRadius\":250,\n");
	S += TEXT("              \"gravity\":{\"x\":0,\"y\":0,\"z\":-300},\"drag\":1},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"fountain\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"DustCloud\",\"spawn\":{\"mode\":\"rate\",\"rate\":15},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":2,\"lifetimeMax\":6,\"sizeMin\":30,\"sizeMax\":80,\n");
	S += TEXT("            \"color\":{\"r\":0.4,\"g\":0.35,\"b\":0.25,\"a\":0.3}},\n");
	S += TEXT("     \"update\":{\"windForce\":{\"x\":100,\"y\":50,\"z\":0},\"noiseStrength\":80,\"noiseFrequency\":1,\n");
	S += TEXT("              \"sizeScaleEnd\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"fountain\",\"blendMode\":\"translucent\",\"sortOrder\":0}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 10: Energy Implosion (Attraction + Vortex → Burst)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Energy Implosion - particles gather inward then burst outward\",\n");
	S += TEXT("  \"_templates_used\": \"hanging_particulates (gather phase), omnidirectional_burst (burst phase), core\",\n");
	S += TEXT("  \"systemName\": \"EnergyImplosion_Holy\",\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"GatherPhase\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":60},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.8,\"lifetimeMax\":1.5,\"sizeMin\":2,\"sizeMax\":6,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-300,\"y\":-300,\"z\":-300},\"velocityMax\":{\"x\":300,\"y\":300,\"z\":300},\n");
	S += TEXT("            \"color\":{\"r\":2,\"g\":2,\"b\":0.5}},\n");
	S += TEXT("     \"update\":{\"attractionStrength\":800,\"attractionRadius\":300,\n");
	S += TEXT("              \"vortexStrength\":150,\"vortexRadius\":100,\n");
	S += TEXT("              \"drag\":2,\"opacityEnd\":0.3},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"hanging_particulates\",\"blendMode\":\"additive\",\"sortOrder\":5}},\n");
	S += TEXT("    {\"name\":\"BurstPhase\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":40,\"burstDelay\":1.0},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.3,\"lifetimeMax\":1,\"sizeMin\":3,\"sizeMax\":10,\n");
	S += TEXT("            \"velocityMin\":{\"x\":-600,\"y\":-600,\"z\":-600},\"velocityMax\":{\"x\":600,\"y\":600,\"z\":600},\n");
	S += TEXT("            \"color\":{\"r\":5,\"g\":4,\"b\":1}},\n");
	S += TEXT("     \"update\":{\"drag\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"omnidirectional_burst\",\"blendMode\":\"additive\",\"sortOrder\":7}},\n");
	S += TEXT("    {\"name\":\"FlashCore\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":2,\"burstDelay\":0.9},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.1,\"lifetimeMax\":0.4,\"sizeMin\":100,\"sizeMax\":200,\n");
	S += TEXT("            \"color\":{\"r\":8,\"g\":6,\"b\":2}},\n");
	S += TEXT("     \"update\":{\"sizeScaleEnd\":3,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"core\",\"sortOrder\":10}}\n");
	S += TEXT("  ]\n");
	S += TEXT("},\n");

	// ===================================================================
	// Example 11: Custom Texture VFX (SD texture generation request)
	// ===================================================================
	S += TEXT("{\n");
	S += TEXT("  \"_description\": \"Custom magic rune effect with SD-generated texture\",\n");
	S += TEXT("  \"_templates_used\": \"simple_sprite_burst, core. Includes texturePrompt for SD generation.\",\n");
	S += TEXT("  \"systemName\": \"MagicRune_Custom\",\n");
	S += TEXT("  \"emitters\": [\n");
	S += TEXT("    {\"name\":\"RuneSymbol\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":1},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":1,\"lifetimeMax\":2,\"sizeMin\":80,\"sizeMax\":120,\n");
	S += TEXT("            \"color\":{\"r\":1,\"g\":0.3,\"b\":2}},\n");
	S += TEXT("     \"update\":{\"sizeScaleStart\":0.5,\"sizeScaleEnd\":1.5,\"opacityEnd\":0,\n");
	S += TEXT("              \"rotationRateMin\":30,\"rotationRateMax\":60},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"simple_sprite_burst\",\"blendMode\":\"additive\",\"sortOrder\":8,\n");
	S += TEXT("              \"texturePrompt\":\"magical runic circle, glowing arcane symbols, circular mandala pattern, centered, black background, VFX game sprite, high contrast\",\n");
	S += TEXT("              \"textureNegativePrompt\":\"text, watermark, realistic, photo, frame, blurry\",\n");
	S += TEXT("              \"textureType\":\"particle_sprite\",\"textureResolution\":512}},\n");
	S += TEXT("    {\"name\":\"RuneGlow\",\"spawn\":{\"mode\":\"burst\",\"burstCount\":3},\n");
	S += TEXT("     \"init\":{\"lifetimeMin\":0.1,\"lifetimeMax\":0.5,\"sizeMin\":60,\"sizeMax\":150,\n");
	S += TEXT("            \"color\":{\"r\":3,\"g\":0.5,\"b\":5}},\n");
	S += TEXT("     \"update\":{\"sizeScaleEnd\":2,\"opacityEnd\":0},\n");
	S += TEXT("     \"render\":{\"emitterTemplate\":\"core\",\"sortOrder\":10}}\n");
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

FString UHktVFXGeneratorFunctionLibrary::McpDumpTemplateParameters(const FString& RendererType)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return TEXT("ERROR: VFXGenerator subsystem not available");
	}

	return Subsystem->DumpTemplateParameters(RendererType);
}

FString UHktVFXGeneratorFunctionLibrary::McpDumpAllTemplateParameters()
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return TEXT("ERROR: VFXGenerator subsystem not available");
	}

	return Subsystem->DumpAllTemplateParameters();
}
