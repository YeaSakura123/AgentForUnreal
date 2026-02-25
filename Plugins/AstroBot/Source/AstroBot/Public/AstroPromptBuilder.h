#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstroBotTypes.h"
#include "WorldBookAsset.h"
#include "AstroPromptBuilder.generated.h"

class UCharacterCardAsset;

UCLASS()
class ASTROBOT_API UAstroPromptBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 基于角色卡、世界书召回、玩家快照与对话历史构建最终 Prompt。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Prompt")
	static FString BuildPrompt(
		const UCharacterCardAsset* CharacterCard,
		const TArray<FWorldBookEntry>& RecalledWorldBookEntries,
		const FAstroPlayerSnapshot& PlayerSnapshot,
		const TArray<FAstroDialogueTurn>& ConversationHistory);
};
