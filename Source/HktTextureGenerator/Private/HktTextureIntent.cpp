// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTextureIntent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* UsageToString(EHktTextureUsage Usage)
	{
		switch (Usage)
		{
		case EHktTextureUsage::ParticleSprite: return TEXT("particle_sprite");
		case EHktTextureUsage::Flipbook4x4:    return TEXT("flipbook_4x4");
		case EHktTextureUsage::Flipbook8x8:    return TEXT("flipbook_8x8");
		case EHktTextureUsage::Noise:           return TEXT("noise");
		case EHktTextureUsage::Gradient:        return TEXT("gradient");
		case EHktTextureUsage::ItemIcon:        return TEXT("item_icon");
		case EHktTextureUsage::MaterialBase:    return TEXT("material_base");
		case EHktTextureUsage::MaterialNormal:  return TEXT("material_normal");
		case EHktTextureUsage::MaterialMask:    return TEXT("material_mask");
		default:                                return TEXT("particle_sprite");
		}
	}

	EHktTextureUsage StringToUsage(const FString& Str)
	{
		if (Str == TEXT("particle_sprite"))  return EHktTextureUsage::ParticleSprite;
		if (Str == TEXT("flipbook_4x4"))     return EHktTextureUsage::Flipbook4x4;
		if (Str == TEXT("flipbook_8x8"))     return EHktTextureUsage::Flipbook8x8;
		if (Str == TEXT("noise"))            return EHktTextureUsage::Noise;
		if (Str == TEXT("gradient"))         return EHktTextureUsage::Gradient;
		if (Str == TEXT("item_icon"))        return EHktTextureUsage::ItemIcon;
		if (Str == TEXT("material_base"))    return EHktTextureUsage::MaterialBase;
		if (Str == TEXT("material_normal"))  return EHktTextureUsage::MaterialNormal;
		if (Str == TEXT("material_mask"))    return EHktTextureUsage::MaterialMask;
		return EHktTextureUsage::ParticleSprite;
	}
}

FString FHktTextureIntent::ToJson() const
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("usage"), UsageToString(Usage));
	Writer->WriteValue(TEXT("prompt"), Prompt);
	if (!NegativePrompt.IsEmpty())
	{
		Writer->WriteValue(TEXT("negativePrompt"), NegativePrompt);
	}
	Writer->WriteValue(TEXT("resolution"), Resolution);
	Writer->WriteValue(TEXT("alphaChannel"), bAlphaChannel);
	Writer->WriteValue(TEXT("tileable"), bTileable);
	if (StyleKeywords.Num() > 0)
	{
		Writer->WriteArrayStart(TEXT("styleKeywords"));
		for (const FString& Kw : StyleKeywords)
		{
			Writer->WriteValue(Kw);
		}
		Writer->WriteArrayEnd();
	}
	Writer->WriteObjectEnd();

	Writer->Close();
	return Output;
}

bool FHktTextureIntent::FromJson(const FString& JsonStr, FHktTextureIntent& OutIntent)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return false;
	}

	FString UsageStr;
	if (JsonObj->TryGetStringField(TEXT("usage"), UsageStr))
	{
		OutIntent.Usage = StringToUsage(UsageStr);
	}

	JsonObj->TryGetStringField(TEXT("prompt"), OutIntent.Prompt);
	JsonObj->TryGetStringField(TEXT("negativePrompt"), OutIntent.NegativePrompt);
	JsonObj->TryGetNumberField(TEXT("resolution"), OutIntent.Resolution);
	JsonObj->TryGetBoolField(TEXT("alphaChannel"), OutIntent.bAlphaChannel);
	JsonObj->TryGetBoolField(TEXT("tileable"), OutIntent.bTileable);

	const TArray<TSharedPtr<FJsonValue>>* Keywords = nullptr;
	if (JsonObj->TryGetArrayField(TEXT("styleKeywords"), Keywords))
	{
		OutIntent.StyleKeywords.Reset();
		for (const auto& Val : *Keywords)
		{
			FString Kw;
			if (Val->TryGetString(Kw))
			{
				OutIntent.StyleKeywords.Add(MoveTemp(Kw));
			}
		}
	}

	return true;
}
