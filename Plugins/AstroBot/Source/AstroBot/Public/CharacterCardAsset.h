#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterCardAsset.generated.h"

UCLASS(BlueprintType)
class ASTROBOT_API UCharacterCardAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// NPC 的长期设定：身份、背景、世界观。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Character Card", meta = (MultiLine = "true"))
	FString PersonaText;

	// 说话语气与风格约束（如简洁、古风、活泼等）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Character Card", meta = (MultiLine = "true"))
	FString StyleText;

	// 禁忌与硬限制，用于安全与人设一致性。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Character Card", meta = (MultiLine = "true"))
	FString ForbiddenText;
};
