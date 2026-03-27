#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstroDirectorTypes.h"
#include "AstroDirectorPromptBuilder.generated.h"

UCLASS()
class ASTROBOT_API UAstroDirectorPromptBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 为区域导演系统构建调试友好的文本 Prompt，当前版本先服务于最小可运行骨架。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	static FString BuildDirectorPrompt(
		const FAstroDirectorTickPayload& TickPayload,
		const TArray<FAstroNPCConversationSummary>& RecentSummaries,
		const FAstroRuntimeWorldOverlay& CurrentWorldOverlay);
};
