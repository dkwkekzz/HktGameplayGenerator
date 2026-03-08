// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSettings.h"

UHktVFXGeneratorSettings::UHktVFXGeneratorSettings()
{
	// UE5.6 Emitters/ 폴더 기준 기본값
	EmitterTemplates.Add(TEXT("sprite"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst")));
	EmitterTemplates.Add(TEXT("ribbon"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon")));
	EmitterTemplates.Add(TEXT("light"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal")));
	EmitterTemplates.Add(TEXT("mesh"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst")));

	FallbackEmitterTemplate =
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal"));
}
