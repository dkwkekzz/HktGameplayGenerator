// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorSettings.h"

UHktMeshGeneratorSettings::UHktMeshGeneratorSettings()
{
	DefaultOutputDirectory = TEXT("/Game/Generated/Characters");
	CharacterStylePromptSuffix = TEXT("game-ready, stylized, T-pose, clean topology");
	StructureStylePromptSuffix = TEXT("game-ready, stylized, modular, clean geometry");
}
