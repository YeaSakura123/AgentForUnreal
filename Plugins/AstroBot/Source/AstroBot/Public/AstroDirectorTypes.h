#pragma once

#include "CoreMinimal.h"
#include "AstroBotTypes.h"
#include "AstroDirectorTypes.generated.h"

// 单次 NPC 会话结束后，上报给导演系统的摘要结构。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroNPCConversationSummary
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString NPCDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FAstroPlayerSnapshot PlayerSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString BasePersonaText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString BaseStyleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString BaseForbiddenText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	TArray<FAstroDialogueTurn> ConversationTurns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString LastPlayerMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString LastNPCReply;
};

// 导演系统对单个 NPC 的运行时补丁，只在本次运行中有效，不改动原始角色卡资产。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroRuntimeCharacterOverlay
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	int32 AffinityScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString RelationshipSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString AdditionalPersonaText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString AdditionalStyleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString AdditionalForbiddenText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString RecentMemorySummary;
};

// 区域级运行时状态，后续可继续扩展天气、事件与区域危险度等内容。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroRuntimeWorldOverlay
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString WeatherState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString ActiveEventSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString DirectorNotes;
};

// 周期性发送给导演系统的运行时信息，避免玩家长时间不交互时区域导演失去上下文。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroDirectorTickPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FAstroPlayerSnapshot PlayerSnapshot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString StoryProgressSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString ExtraRuntimeSummary;
};

// 导演系统当前生成的结果快照，便于蓝图观察与调试。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroDirectorDecision
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FString SummaryText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	FAstroRuntimeWorldOverlay WorldOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Director")
	TArray<FAstroToolCall> ToolCalls;
};
