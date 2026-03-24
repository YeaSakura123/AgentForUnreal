#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstroBotTypes.h"
#include "AstroDirectorTypes.h"
#include "WorldBookAsset.h"
#include "AstroPromptBuilder.generated.h"

class UCharacterCardAsset;

UCLASS()
class ASTROBOT_API UAstroPromptBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 基于角色卡、世界书召回、玩家快照与对话历史构建最终 Prompt。
	// 旧版本只接受基础角色卡与世界书内容，保留如下：
	// static FString BuildPrompt(const UCharacterCardAsset* CharacterCard, const TArray<FWorldBookEntry>& RecalledWorldBookEntries, const FAstroPlayerSnapshot& PlayerSnapshot, const TArray<FAstroDialogueTurn>& ConversationHistory);
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Prompt")
	static FString BuildPrompt(
		const UCharacterCardAsset* CharacterCard,
		const TArray<FWorldBookEntry>& RecalledWorldBookEntries,
		const FAstroRuntimeCharacterOverlay& RuntimeOverlay,
		const FAstroPlayerSnapshot& PlayerSnapshot,
		const TArray<FAstroDialogueTurn>& ConversationHistory);

	// 兼容旧调用点的重载入口，内部会自动使用空 Overlay。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Prompt")
	static FString BuildPromptWithoutOverlay(
		const UCharacterCardAsset* CharacterCard,
		const TArray<FWorldBookEntry>& RecalledWorldBookEntries,
		const FAstroPlayerSnapshot& PlayerSnapshot,
		const TArray<FAstroDialogueTurn>& ConversationHistory);
};
