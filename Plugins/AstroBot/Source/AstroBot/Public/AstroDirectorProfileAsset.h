#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AstroBotTypes.h"
#include "AstroDirectorProfileAsset.generated.h"

// 导演 AI 的身份层与配置层资产。
// 用途：统一配置导演的人格、世界风格、难度倾向、干预策略以及可用工具。
UCLASS(BlueprintType)
class ASTROBOT_API UAstroDirectorProfileAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 导演名称，主要用于调试和多区域导演扩展。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile")
	FString DirectorName;

	// 导演人格，用于定义导演如何理解玩家、NPC 和世界。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile", meta = (MultiLine = "true"))
	FString DirectorPersonaText;

	// 世界风格，例如写实、冷峻、轻松、压抑等。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile", meta = (MultiLine = "true"))
	FString WorldStyleText;

	// 难度与压力倾向，用于指导导演在运行时如何塑造整体节奏。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile", meta = (MultiLine = "true"))
	FString DifficultyStyleText;

	// 干预策略，用于控制导演系统是偏保守还是偏积极地改变区域状态。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile", meta = (MultiLine = "true"))
	FString InterventionPolicyText;

	// 导演系统的禁忌与边界，避免模型做出破坏节奏或脱离风格的决策。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile", meta = (MultiLine = "true"))
	FString DirectorForbiddenText;

	// 导演系统允许使用的世界级工具集合。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile")
	TArray<FAstroToolDefinition> AvailableDirectorTools;

	// 周期性导演 Tick 的建议间隔秒数。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile", meta = (ClampMin = "1.0", ClampMax = "3600.0"))
	float DirectorTickIntervalSeconds = 60.0f;

	// 是否允许更新 NPC 运行时 Overlay。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile")
	bool bAllowNPCOverlayUpdates = true;

	// 是否允许更新世界 Overlay。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile")
	bool bAllowWorldOverlayUpdates = true;

	// 是否允许导演系统发起工具调用。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director Profile")
	bool bAllowDirectorToolCalls = true;
};
