// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorSettings.h"

UHktAnimGeneratorSettings::UHktAnimGeneratorSettings()
{
	DefaultOutputDirectory = TEXT("/Game/Generated/Animations");
	AnimStylePromptSuffix = TEXT("game animation, 30fps, loopable if locomotion, clear keyframes");
	bGenerateBlendSpaceForLocomotion = true;
}
