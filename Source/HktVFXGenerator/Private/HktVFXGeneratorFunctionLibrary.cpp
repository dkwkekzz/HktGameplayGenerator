// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorFunctionLibrary.h"
#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "HktTextureIntent.h"
#include "HktTextureGeneratorSubsystem.h"
#include "NiagaraSystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"

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

	// Config를 파싱하여 텍스처 생성 요청 수집 → HktTextureGenerator에 위임
	FHktVFXNiagaraConfig ParsedConfig;
	FHktVFXNiagaraConfig::FromJson(JsonConfig, ParsedConfig);

	// VFX 에미터의 TexturePrompt → FHktTextureRequest 변환
	TArray<FHktTextureRequest> TextureRequests;
	for (const auto& E : ParsedConfig.Emitters)
	{
		if (E.Render.TexturePrompt.IsEmpty()) continue;

		FHktTextureRequest Req;
		Req.Name = E.Name;

		// TextureType → EHktTextureUsage 변환
		if (E.Render.TextureType == TEXT("flipbook_4x4"))
			Req.Intent.Usage = EHktTextureUsage::Flipbook4x4;
		else if (E.Render.TextureType == TEXT("flipbook_8x8"))
			Req.Intent.Usage = EHktTextureUsage::Flipbook8x8;
		else if (E.Render.TextureType == TEXT("noise"))
			Req.Intent.Usage = EHktTextureUsage::Noise;
		else if (E.Render.TextureType == TEXT("gradient"))
			Req.Intent.Usage = EHktTextureUsage::Gradient;
		else
			Req.Intent.Usage = EHktTextureUsage::ParticleSprite;

		Req.Intent.Prompt = E.Render.TexturePrompt;
		Req.Intent.NegativePrompt = E.Render.TextureNegativePrompt;
		Req.Intent.Resolution = E.Render.TextureResolution > 0 ? E.Render.TextureResolution : 256;
		Req.Intent.bAlphaChannel = true;

		TextureRequests.Add(MoveTemp(Req));
	}

	// HktTextureGenerator에 배치 생성 위임
	TArray<FHktTextureResult> TextureResults;
	if (TextureRequests.Num() > 0)
	{
		UHktTextureGeneratorSubsystem* TexSubsystem = GEditor
			? GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>()
			: nullptr;

		if (TexSubsystem)
		{
			TextureResults = TexSubsystem->GenerateBatch(TextureRequests, OutputDir);
		}
	}

	// 응답 JSON 구성
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("success"), true);
	Writer->WriteValue(TEXT("assetPath"), Result->GetPathName());

	// 텍스처 결과 포함 (생성 완료 + pending 모두)
	if (TextureRequests.Num() > 0)
	{
		Writer->WriteArrayStart(TEXT("textureRequests"));
		for (int32 i = 0; i < TextureRequests.Num(); ++i)
		{
			const FHktTextureRequest& Req = TextureRequests[i];
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("emitterName"), Req.Name);
			Writer->WriteValue(TEXT("assetKey"), Req.Intent.GetAssetKey());

			// 이미 생성 완료되었는지 확인
			const FHktTextureResult* TexResult = (i < TextureResults.Num()) ? &TextureResults[i] : nullptr;
			if (TexResult && TexResult->IsSuccess())
			{
				Writer->WriteValue(TEXT("status"), TEXT("ready"));
				Writer->WriteValue(TEXT("texturePath"), TexResult->AssetPath);
			}
			else
			{
				Writer->WriteValue(TEXT("status"), TEXT("pending"));
				Writer->WriteValue(TEXT("prompt"), Req.Intent.Prompt);
				if (!Req.Intent.NegativePrompt.IsEmpty())
					Writer->WriteValue(TEXT("negativePrompt"), Req.Intent.NegativePrompt);
				Writer->WriteValue(TEXT("type"), Req.Intent.Usage == EHktTextureUsage::Flipbook4x4 ? TEXT("flipbook_4x4")
					: Req.Intent.Usage == EHktTextureUsage::Flipbook8x8 ? TEXT("flipbook_8x8")
					: Req.Intent.Usage == EHktTextureUsage::Noise ? TEXT("noise")
					: Req.Intent.Usage == EHktTextureUsage::Gradient ? TEXT("gradient")
					: TEXT("particle_sprite"));
				Writer->WriteValue(TEXT("resolution"), Req.Intent.GetEffectiveResolution());
			}
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

	S += TEXT("  init.*              → InitializeParticle (Lifetime, Uniform Sprite Size, Color, Velocity)\n");
	S += TEXT("                        Min/Max values create RANDOM DISTRIBUTIONS per particle.\n");
	S += TEXT("  shapeLocation.*    → ShapeLocation module (sphere, box, cone, ring, torus, cylinder, plane)\n\n");

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
	S += TEXT("[SHAPE LOCATION — Emission Shape]\n");
	S += TEXT("Use shapeLocation to control WHERE particles spawn (replaces point-spawn):\n\n");
	S += TEXT("  \"sphere\"   — Spherical emission. Params: sphereRadius (default 100)\n");
	S += TEXT("               Use for: Explosions, magic orbs, shockwaves, auras\n");
	S += TEXT("  \"box\"      — Box-shaped emission. Params: boxSize {x,y,z}\n");
	S += TEXT("               Use for: Rain, snow, environment particles, area effects\n");
	S += TEXT("  \"cone\"     — Cone-shaped emission. Params: coneAngle (degrees), coneLength\n");
	S += TEXT("               Use for: Muzzle flash, directional spray, spotlight particles\n");
	S += TEXT("  \"cylinder\" — Cylindrical emission. Params: cylinderRadius, cylinderHeight\n");
	S += TEXT("               Use for: Pillars, portals, beam origins, ground dust\n");
	S += TEXT("  \"ring\"     — Ring emission. Params: ringRadius, ringWidth\n");
	S += TEXT("               Use for: Shockwaves, halos, orbital effects, portals\n");
	S += TEXT("  \"torus\"    — Torus/donut emission. Params: torusRadius, torusSectionRadius\n");
	S += TEXT("               Use for: Orbital rings, portal edges, Saturn-like effects\n");
	S += TEXT("  \"plane\"    — Flat plane emission.\n");
	S += TEXT("               Use for: Ground effects, floor sparkles\n\n");
	S += TEXT("  Common: offset {x,y,z}, surfaceOnly (bool)\n");
	S += TEXT("  surfaceOnly=true → particles only on shape surface (rings, shells)\n");
	S += TEXT("  surfaceOnly=false → particles fill the volume (clouds, orbs)\n\n");

	// ===================================================================
	S += TEXT("[SPRITE FACING MODE]\n");
	S += TEXT("Controls how sprite particles orient relative to camera:\n\n");
	S += TEXT("  \"default\"          — Standard billboarding (always face camera)\n");
	S += TEXT("  \"velocity\"         — Face velocity direction (sparks, debris, streaks)\n");
	S += TEXT("  \"camera_position\"  — Face toward camera position (perspective-correct for close-ups)\n");
	S += TEXT("  \"camera_plane\"     — Parallel to camera plane (uniform facing, UI particles)\n");
	S += TEXT("  \"custom_axis\"      — Custom facing vector\n\n");

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
	S += TEXT("[COLLISION — Surface Interaction]\n");
	S += TEXT("Add collision to make particles interact with world geometry:\n\n");
	S += TEXT("  \"collision\": { \"enabled\": true, \"response\": \"bounce\", \"restitution\": 0.5, \"friction\": 0.2 }\n\n");
	S += TEXT("  response modes:\n");
	S += TEXT("    \"bounce\"  — Reflect off surfaces (debris, sparks hitting ground)\n");
	S += TEXT("    \"kill\"    — Destroy on contact (raindrops, snowflakes)\n");
	S += TEXT("    \"stick\"   — Stop at collision point (blood splatter, paint)\n\n");
	S += TEXT("  restitution: 0.0=no bounce, 0.5=moderate, 1.0=full elastic (bounce only)\n");
	S += TEXT("  friction: 0.0=slippery, 0.5=moderate, 1.0=rough surface\n");
	S += TEXT("  traceDistance: GPU ray trace depth (0=default, increase for fast particles)\n\n");

	// ===================================================================
	S += TEXT("[EVENT-BASED SECONDARY SPAWN]\n");
	S += TEXT("Trigger secondary particles from particle events:\n\n");
	S += TEXT("  \"eventSpawn\": { \"triggerEvent\": \"death\", \"spawnCount\": 5, \"targetEmitter\": \"Smoke\" }\n\n");
	S += TEXT("  triggerEvent:\n");
	S += TEXT("    \"death\"     — When particle dies → spawn secondary (sparks→smoke, fire→embers)\n");
	S += TEXT("    \"collision\" — When particle hits surface → spawn secondary (bullet→dust)\n\n");
	S += TEXT("  spawnCount: Number of secondary particles per event (1-20)\n");
	S += TEXT("  targetEmitter: Name of target emitter in same system (receives location event)\n");
	S += TEXT("  velocityScale: Inherited velocity multiplier (0.5=half speed, 0=stationary)\n\n");
	S += TEXT("  Workflow: Source emitter (eventSpawn) → GenerateLocationEvent → Target emitter receives\n");
	S += TEXT("  Both emitters must exist in same system. Target gets particles at source particle's position.\n\n");

	// ===================================================================
	S += TEXT("[SPAWN PER UNIT — Distance-based Spawning]\n");
	S += TEXT("Spawn particles based on emitter movement distance (for trails, afterimages):\n\n");
	S += TEXT("  \"spawnPerUnit\": { \"enabled\": true, \"spawnPerUnit\": 5, \"maxFrameSpawn\": 100 }\n\n");
	S += TEXT("  spawnPerUnit: Particles per distance unit moved (higher=denser trail)\n");
	S += TEXT("  maxFrameSpawn: Safety cap per frame (prevent teleport-spawning thousands)\n");
	S += TEXT("  movementTolerance: Minimum movement to trigger spawn (filter jitter)\n\n");
	S += TEXT("  Best with: ribbon renderers, velocity_aligned sprites for trails.\n");
	S += TEXT("  Combine with: looping=true, no burst/rate spawn needed.\n\n");

	// ===================================================================
	S += TEXT("[GPU SIMULATION]\n");
	S += TEXT("Enable GPU simulation for massive particle counts:\n\n");
	S += TEXT("  \"gpuSim\": true\n\n");
	S += TEXT("  When to use:\n");
	S += TEXT("    - Particle count > 1000 (storms, galaxy, dense smoke)\n");
	S += TEXT("    - Heavy physics (collision + noise + attraction combined)\n");
	S += TEXT("    - Large-scale environment effects (snow, rain, fireflies)\n\n");
	S += TEXT("  Limitations:\n");
	S += TEXT("    - No CPU readback (cannot drive audio/gameplay from GPU particles)\n");
	S += TEXT("    - Some modules may not support GPU (check template compatibility)\n");
	S += TEXT("    - Ribbon renderer does NOT work with GPU sim\n\n");

	// ===================================================================
	S += TEXT("[DESIGN TIPS]\n");
	S += TEXT("  - ALWAYS prefer templates with the modules you need (see Template Selection Guide).\n");
	S += TEXT("  - Layer 3-6 emitters with different roles for rich effects.\n");
	S += TEXT("  - Min/Max values create RANDOM DISTRIBUTIONS — each particle gets unique random values.\n");
	S += TEXT("    Wide ranges = organic/natural, narrow ranges = uniform/artificial.\n");
	S += TEXT("  - Use shapeLocation to give particles a spawn SHAPE (sphere, cone, ring, etc.).\n");
	S += TEXT("    Combine shape + velocity for complex emission: ring + upward velocity = rising halo.\n");
	S += TEXT("  - velocity_aligned alignment OR facingMode='velocity' makes sparks/debris look like streaks.\n");
	S += TEXT("  - facingMode='camera_position' for perspective-correct billboarding on close-up effects.\n");
	S += TEXT("  - burstDelay staggers layers (smoke 0.05s after explosion).\n");
	S += TEXT("  - colorOverLife: fire=orange->dark, magic=blue->purple->transparent.\n");
	S += TEXT("  - sizeScaleEnd>1 = particle grows. sizeScaleEnd<1 = shrinks.\n");
	S += TEXT("  - For looping: spawn.mode='rate' + looping=true + warmupTime.\n");
	S += TEXT("  - Only set non-default values. Omitted fields use sensible defaults.\n");
	S += TEXT("  - Use McpDumpAllTemplateParameters() to see actual module params at runtime.\n");
	S += TEXT("  - Combine vortex+attraction for spiral implosion (portals, black holes).\n");
	S += TEXT("  - accelerationForce for constant thrust (missiles, jets, ascending spirits).\n");
	S += TEXT("  - vortexAxis: default (0,0,1)=Z. Set (1,0,0) for horizontal tornado.\n");
	S += TEXT("  - Use collision + eventSpawn together: sparks hit ground → spawn dust/scorch marks.\n");
	S += TEXT("  - spawnPerUnit for movement trails (swords, projectiles, vehicles).\n");
	S += TEXT("  - gpuSim for >1000 particles or heavy physics combos.\n");
	S += TEXT("  - SubUV flipbook: set subImageRows/Columns + subUVPlayRate for animated sprites.\n");
	S += TEXT("  - Soft particle (bSoftParticle) prevents hard edges where particles intersect geometry.\n");
	S += TEXT("  - Ribbon taper: ribbonWidthScaleStart=1, ribbonWidthScaleEnd=0 for natural trail fade.\n");
	S += TEXT("  - Mesh orientation='velocity' for directional debris; 'camera' for billboard mesh.\n");
	S += TEXT("  - cameraOffset > 0 pushes sprites toward camera (prevents z-fighting with surfaces).\n\n");

	// ===================================================================
	S += TEXT("[RENDERING QUALITY]\n");
	S += TEXT("SubUV Animation:\n");
	S += TEXT("  render.subImageRows/subImageColumns = flipbook grid dimensions\n");
	S += TEXT("  render.subUVPlayRate = animation speed multiplier (default 1.0)\n");
	S += TEXT("  render.bSubUVRandomStartFrame = true → each particle starts at a random frame\n");
	S += TEXT("  Best for: fire, smoke, explosions, energy effects with animated textures\n\n");
	S += TEXT("Soft Particle / Depth Fade:\n");
	S += TEXT("  render.bSoftParticle = true → enables soft edges where particles intersect geometry\n");
	S += TEXT("  render.softParticleFadeDistance = fade range in cm (default 100)\n");
	S += TEXT("  render.cameraOffset = push toward camera to prevent z-fighting (0=none)\n");
	S += TEXT("  Best for: smoke, fog, volumetric clouds, ground effects\n\n");
	S += TEXT("Ribbon Extensions:\n");
	S += TEXT("  render.ribbonUVMode = stretch | tile_distance | tile_lifetime | distribute\n");
	S += TEXT("  render.ribbonTessellation = int (0=auto, higher=smoother curves)\n");
	S += TEXT("  render.ribbonWidthScaleStart / ribbonWidthScaleEnd = taper width (1→0 for trail)\n");
	S += TEXT("  Best for: sword trails, energy beams, smoke trails, magic ribbons\n\n");
	S += TEXT("Mesh Renderer Extensions:\n");
	S += TEXT("  render.meshPath = /Engine/BasicShapes/Cube.Cube (or custom asset path)\n");
	S += TEXT("  render.meshOrientation = default | velocity | camera\n");
	S += TEXT("  Best for: debris, shrapnel, crystalline effects, geometric particles\n\n");
	S += TEXT("Light Renderer Extensions:\n");
	S += TEXT("  render.lightExponent = falloff exponent (1=default, higher=sharper)\n");
	S += TEXT("  render.bLightVolumetricScattering = true → affects volumetric fog\n");
	S += TEXT("  Best for: fireflies, magic orbs, muzzle flash illumination\n\n");

	// ===================================================================
	S += TEXT("[MULTI-POINT CURVES]\n");
	S += TEXT("Color Over Life (multi-point):\n");
	S += TEXT("  update.colorCurve = [{\"time\":0,\"color\":{\"r\":1,\"g\":0.8,\"b\":0}}, {\"time\":0.5,\"color\":{\"r\":1,\"g\":0.2,\"b\":0}}, {\"time\":1,\"color\":{\"r\":0.1,\"g\":0,\"b\":0}}]\n");
	S += TEXT("  Replaces useColorOverLife+colorEnd for richer color transitions.\n");
	S += TEXT("  Currently maps first→last keyframe to ScaleColor start/end.\n");
	S += TEXT("  For true multi-phase color: layer emitters with different lifetimes+colors.\n");
	S += TEXT("  Best for: fire (orange→red→black), magic (blue→purple→white)\n\n");
	S += TEXT("Size Over Life (multi-point):\n");
	S += TEXT("  update.sizeCurve = [{\"time\":0,\"scale\":0.5}, {\"time\":0.3,\"scale\":2.0}, {\"time\":1,\"scale\":0.1}]\n");
	S += TEXT("  Replaces sizeScaleStart/End for more detailed size animation.\n");
	S += TEXT("  Currently maps first→last keyframe to ScaleSpriteSize start/end.\n");
	S += TEXT("  Best for: explosions (small→big→shrink), heartbeat pulse\n\n");

	// ===================================================================
	S += TEXT("[MULTI-WAVE BURST]\n");
	S += TEXT("Spawn multiple bursts at different times from one emitter:\n\n");
	S += TEXT("  \"spawn\": { \"mode\": \"burst\", \"burstWaves\": [\n");
	S += TEXT("    {\"count\": 20, \"delay\": 0},\n");
	S += TEXT("    {\"count\": 15, \"delay\": 0.2},\n");
	S += TEXT("    {\"count\": 10, \"delay\": 0.5}\n");
	S += TEXT("  ]}\n\n");
	S += TEXT("  When burstWaves is set, burstCount/burstDelay are ignored.\n");
	S += TEXT("  Each wave spawns 'count' particles at 'delay' seconds.\n");
	S += TEXT("  Best for: staged explosions, fireworks, multi-phase bursts\n\n");

	// ===================================================================
	S += TEXT("[CAMERA DISTANCE FADE]\n");
	S += TEXT("Fade particles based on camera distance (LOD alternative):\n\n");
	S += TEXT("  update.cameraDistanceFadeNear = 500   (start fading at 500 units)\n");
	S += TEXT("  update.cameraDistanceFadeFar = 2000   (fully invisible at 2000 units)\n\n");
	S += TEXT("  Auto-injects CameraDistanceFade module.\n");
	S += TEXT("  Best for: ambient effects (fireflies, dust, snow) that should disappear at distance\n\n");

	// ===================================================================
	S += TEXT("[EFFECT COMPLEXITY TIERS]\n");
	S += TEXT("Match your design to the right complexity level:\n\n");
	S += TEXT("  Tier 1 — Simple (1 emitter)\n");
	S += TEXT("    Single burst/rate, one renderer. E.g.: muzzle flash, simple spark\n");
	S += TEXT("  Tier 2 — Standard (2-3 emitters)\n");
	S += TEXT("    Mixed spawn modes, basic physics. E.g.: campfire (flame+embers+smoke)\n");
	S += TEXT("  Tier 3 — Rich (3-5 emitters)\n");
	S += TEXT("    Collision, eventSpawn, shapeLocation. E.g.: explosion with debris+dust+light\n");
	S += TEXT("  Tier 4 — Complex (5-8 emitters)\n");
	S += TEXT("    Multi-layer with vortex, attraction, GPU sim. E.g.: portal, tornado\n\n");
	S += TEXT("  Rule: Start with the LOWEST tier that achieves the effect.\n");
	S += TEXT("  More emitters = more cost. 3-5 emitters covers 90% of production VFX.\n\n");

	// ===================================================================
	S += TEXT("[LIMITATIONS — DO NOT ATTEMPT]\n");
	S += TEXT("The system CANNOT do these — do NOT include in your config:\n\n");
	S += TEXT("  ✗ Static mesh surface spawning (no StaticMesh data interface)\n");
	S += TEXT("  ✗ Physics field interaction (no Chaos physics binding)\n");
	S += TEXT("  ✗ Audio-driven parameters or audio event triggers\n");
	S += TEXT("  ✗ Custom HLSL or blueprint logic inside modules\n");
	S += TEXT("  ✗ LOD / distance-based quality scaling\n");
	S += TEXT("  ✗ Procedural mesh generation\n");
	S += TEXT("  ✗ Dynamic material parameter curves (only fixed material override)\n");
	S += TEXT("  ✗ Multi-point color/size curves (only start→end linear interpolation)\n");
	S += TEXT("  ✗ Volume renderer or 2D renderer\n");
	S += TEXT("  ✗ Ribbon renderer + GPU sim (incompatible)\n\n");
	S += TEXT("  Workarounds:\n");
	S += TEXT("    - Need ground spawn? Use shapeLocation=plane or low cone instead\n");
	S += TEXT("    - Need mesh surface spawn? Use skeletalMesh data interface\n");
	S += TEXT("    - Need complex color transitions? Layer multiple emitters with different colors\n");
	S += TEXT("    - Need oscillation? Use vortex + attraction combo\n\n");

	// ===================================================================
	S += TEXT("[ANTI-PATTERNS — COMMON MISTAKES]\n");
	S += TEXT("  ✗ Circular eventSpawn: A→B and B→A causes infinite spawn loop\n");
	S += TEXT("  ✗ eventSpawn targetEmitter referencing non-existent emitter name\n");
	S += TEXT("  ✗ gpuSim=true with rendererType='ribbon' (GPU sim breaks ribbons)\n");
	S += TEXT("  ✗ spawnPerUnit without looping=true (needs continuous spawning)\n");
	S += TEXT("  ✗ collision without gravity or velocity (particles won't reach surfaces)\n");
	S += TEXT("  ✗ SubUV rows/columns without matching flipbook material/texture\n");
	S += TEXT("  ✗ meshPath pointing to non-existent asset (silently fails)\n");
	S += TEXT("  ✗ Setting burstCount=0 with mode='burst' (no particles spawn)\n");
	S += TEXT("  ✗ Very high burstCount (>500) without gpuSim=true (CPU bottleneck)\n");
	S += TEXT("  ✗ opacityStart=0 without changing it later (invisible particles)\n");
	S += TEXT("  ✗ colorOverLife without visible color difference (wasteful computation)\n\n");

	// ===================================================================
	S += TEXT("[TEMPLATE SELECTION MATRIX]\n");
	S += TEXT("Quick-pick guide — choose the BEST template for your effect:\n\n");
	S += TEXT("  Effect Type          → Best Template         | Built-in Modules\n");
	S += TEXT("  ──────────────────────────────────────────────────────────────────\n");
	S += TEXT("  Explosion flash      → core                  | SubUV, ScaleColor\n");
	S += TEXT("  Explosion burst      → explosion             | SubUV, ScaleColor\n");
	S += TEXT("  Spark/ember          → spark                 | Velocity, ScaleColor\n");
	S += TEXT("  Bouncing debris      → spark + collision     | Velocity, ScaleColor\n");
	S += TEXT("  Smoke puff           → smoke                 | SubUV, Scale, Curl\n");
	S += TEXT("  Muzzle flash         → muzzle_flash          | SubUV, ScaleColor\n");
	S += TEXT("  Water spray          → fountain              | SpawnRate, Gravity\n");
	S += TEXT("  Floating dust        → hanging_particulates  | SpawnRate, CurlNoise\n");
	S += TEXT("  Wind-blown leaves    → blowing_particles     | SpawnRate, CurlNoise\n");
	S += TEXT("  Confetti             → confetti_burst        | Gravity, Rotation, Drag\n");
	S += TEXT("  Mesh debris          → upward_mesh_burst     | Gravity, MeshScale\n");
	S += TEXT("  Energy trail         → ribbon                | SpawnRate, Ribbon\n");
	S += TEXT("  Electric arc         → arc                   | Beam, Arc\n");
	S += TEXT("  Point light          → minimal + light       | (none, pure light)\n");
	S += TEXT("  Any + custom forces  → ANY + auto-inject     | VortexVelocity, etc.\n\n");

	// ===================================================================
	S += TEXT("[PARAMETER SAFE RANGES]\n");
	S += TEXT("Recommended value ranges for natural-looking effects:\n\n");
	S += TEXT("  Parameter              | Low (subtle)  | Medium       | High (dramatic) | Extreme\n");
	S += TEXT("  ─────────────────────────────────────────────────────────────────────────────────\n");
	S += TEXT("  burstCount             | 5-15          | 20-50        | 50-200          | 500+ (gpuSim!)\n");
	S += TEXT("  rate                   | 5-15          | 15-50        | 50-200          | 500+ (gpuSim!)\n");
	S += TEXT("  lifetime               | 0.05-0.3      | 0.3-1.5      | 1.5-4.0         | 5.0+\n");
	S += TEXT("  size                   | 1-10          | 10-50        | 50-200          | 200+\n");
	S += TEXT("  velocity               | 20-100        | 100-400      | 400-1000        | 1000+\n");
	S += TEXT("  gravity.z              | -300          | -600         | -980            | -1500\n");
	S += TEXT("  drag                   | 0.2-0.5       | 1-3          | 3-8             | 10+\n");
	S += TEXT("  noiseStrength          | 5-20          | 20-80        | 80-300          | 500+\n");
	S += TEXT("  vortexStrength         | 30-100        | 100-300      | 300-800         | 1000+\n");
	S += TEXT("  attractionStrength     | 20-80         | 80-300       | 300-1000        | 2000+\n");
	S += TEXT("  ribbonWidth            | 2-5           | 5-20         | 20-60           | 80+\n");
	S += TEXT("  ribbonTessellation     | 2-4           | 4-8          | 8-16            | 16+\n");
	S += TEXT("  subUVPlayRate          | 0.5           | 1.0          | 2.0-4.0         | 8.0+\n");
	S += TEXT("  cameraOffset           | 1-3           | 3-10         | 10-30           | 50+\n");
	S += TEXT("  softParticleFadeDistance| 20-50         | 50-100       | 100-300         | 500+\n");
	S += TEXT("  lightExponent          | 0.5           | 1.0          | 2.0-4.0         | 8.0+\n\n");

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
	// JSON 파일 경로: {PluginBaseDir}/Source/HktVFXGenerator/Python/VFXProductionExamples.json
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("HktGameplayGenerator"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogHktVFXMcp, Error, TEXT("HktGameplayGenerator plugin not found"));
		return TEXT("[]");
	}

	const FString JsonPath = Plugin->GetBaseDir()
		/ TEXT("Source/HktVFXGenerator/Python/VFXProductionExamples.json");

	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *JsonPath))
	{
		UE_LOG(LogHktVFXMcp, Error, TEXT("Failed to load VFX examples JSON: %s"), *JsonPath);
		return TEXT("[]");
	}

	// JSON 파싱 → examples 배열에서 config만 추출
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogHktVFXMcp, Error, TEXT("Failed to parse VFX examples JSON"));
		return TEXT("[]");
	}

	const TArray<TSharedPtr<FJsonValue>>* Examples = nullptr;
	if (!Root->TryGetArrayField(TEXT("examples"), Examples))
	{
		UE_LOG(LogHktVFXMcp, Error, TEXT("No 'examples' array in JSON"));
		return TEXT("[]");
	}

	// config 객체만 모아서 JSON 배열로 반환
	TArray<TSharedPtr<FJsonValue>> Configs;
	for (const TSharedPtr<FJsonValue>& ExampleVal : *Examples)
	{
		const TSharedPtr<FJsonObject>* ExampleObj = nullptr;
		if (ExampleVal->TryGetObject(ExampleObj) && (*ExampleObj)->HasField(TEXT("config")))
		{
			Configs.Add((*ExampleObj)->TryGetField(TEXT("config")));
		}
	}

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Configs, Writer);

	UE_LOG(LogHktVFXMcp, Log, TEXT("Loaded %d VFX example configs from JSON"), Configs.Num());
	return Output;
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

// ============================================================================
// Phase 2: 머티리얼 / 텍스처
// ============================================================================

FString UHktVFXGeneratorFunctionLibrary::McpCreateParticleMaterial(
	const FString& MaterialName,
	const FString& TexturePath,
	const FString& BlendMode,
	float EmissiveIntensity,
	const FString& OutputDir)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	FString ResultPath = Subsystem->CreateParticleMaterial(
		MaterialName, TexturePath, BlendMode, EmissiveIntensity, OutputDir);
	if (ResultPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("Failed to create particle material"));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Created particle material: %s"), *ResultPath);
	return MakeSuccessResponse(ResultPath);
}

FString UHktVFXGeneratorFunctionLibrary::McpAssignVFXMaterial(
	const FString& NiagaraSystemPath,
	const FString& EmitterName,
	const FString& MaterialPath)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	bool bSuccess = Subsystem->AssignMaterialToEmitter(NiagaraSystemPath, EmitterName, MaterialPath);
	if (!bSuccess)
	{
		return MakeErrorResponse(
			FString::Printf(TEXT("Failed to assign material '%s' to emitter '%s' in '%s'"),
				*MaterialPath, *EmitterName, *NiagaraSystemPath));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Assigned material '%s' to emitter '%s'"), *MaterialPath, *EmitterName);
	return MakeSuccessResponse(NiagaraSystemPath);
}

// ============================================================================
// Phase 4: 프리뷰 / 튜닝
// ============================================================================

FString UHktVFXGeneratorFunctionLibrary::McpPreviewVFX(
	const FString& NiagaraSystemPath,
	float Duration,
	const FString& ScreenshotPath)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	FString ResultScreenshotPath = Subsystem->PreviewVFX(NiagaraSystemPath, Duration, ScreenshotPath);
	if (ResultScreenshotPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("Failed to preview VFX"));
	}

	// 스크린샷 경로 포함 응답
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("success"), true);
	Writer->WriteValue(TEXT("assetPath"), NiagaraSystemPath);
	Writer->WriteValue(TEXT("screenshotPath"), ResultScreenshotPath);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

FString UHktVFXGeneratorFunctionLibrary::McpUpdateVFXEmitter(
	const FString& NiagaraSystemPath,
	const FString& EmitterName,
	const FString& JsonOverrides)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	bool bSuccess = Subsystem->UpdateEmitterParameters(NiagaraSystemPath, EmitterName, JsonOverrides);
	if (!bSuccess)
	{
		return MakeErrorResponse(
			FString::Printf(TEXT("Failed to update emitter '%s' in '%s'"),
				*EmitterName, *NiagaraSystemPath));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Updated emitter '%s' in '%s'"), *EmitterName, *NiagaraSystemPath);
	return MakeSuccessResponse(NiagaraSystemPath);
}
