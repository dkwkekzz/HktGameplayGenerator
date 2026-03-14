// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemGeneratorSettings.h"

UHktItemGeneratorSettings::UHktItemGeneratorSettings()
{
	DefaultOutputDirectory = TEXT("/Game/Generated/Items");
	IconOutputDirectory = TEXT("/Game/Generated/Textures/Icons");
	ItemMeshPromptSuffix = TEXT("game-ready, low-poly, stylized, clean topology, single mesh");
	ItemIconPromptSuffix = TEXT("item icon, RPG style, dark background, centered, high contrast");

	// 기본 소켓 매핑
	SocketMappings = {
		{ TEXT("Weapon"), FName("hand_r"), TEXT("socket") },
		{ TEXT("Shield"), FName("hand_l"), TEXT("socket") },
		{ TEXT("Armor"), FName("spine_03"), TEXT("overlay") },
		{ TEXT("Helmet"), FName("head"), TEXT("socket") },
		{ TEXT("Accessory"), FName("spine_02"), TEXT("vfx") },
	};
}
