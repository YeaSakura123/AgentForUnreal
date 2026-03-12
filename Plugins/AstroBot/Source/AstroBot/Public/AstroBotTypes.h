#pragma once

#include "CoreMinimal.h"
#include "AstroBotTypes.generated.h"

UENUM(BlueprintType)
enum class EAstroDialogueRole : uint8
{
	// 系统级指令或上下文。
	System,
	// 玩家发言。
	Player,
	// NPC 模型回复。
	NPC
};

// 模型请求运行模式。
// 目的：让流程测试与真实 API 调用共用同一入口，避免重复实现多套发送逻辑。
UENUM(BlueprintType)
enum class EAstroModelRequestMode : uint8
{
	// 固定返回一段桩回复，用于确认整条链路是否打通。
	Stub,
	// 按输入关键词走确定性回复，便于验证不同交互分支。
	Mock,
	// 使用 OpenAI 兼容 HTTP 接口发起真实请求。
	RealAPI
};

// 单条对话消息，包含说话角色与文本。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroDialogueTurn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot")
	EAstroDialogueRole Role = EAstroDialogueRole::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot")
	FString Text;
};

// 轻量玩家状态快照，用于给 Prompt 提供上下文。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	FString PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	FString LevelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	float Stamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	float Mana = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	float MaxMana = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Player")
	FString CurrentQuest;
};

// 工具参数描述，用于向模型声明可调用能力。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroToolParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString Type = TEXT("string");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	bool bRequired = true;
};

// 单个工具声明。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroToolDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString ToolName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	TArray<FAstroToolParameter> Parameters;
};

// 模型发起的单次工具调用。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroToolCall
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString ToolCallId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString ToolName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString ArgumentsJson;
};

// UE 侧执行工具后的统一结果。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroToolExecutionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString ResultJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|Tools")
	FString ResultMessage;
};

// 发送给 OpenAI 兼容聊天接口的消息结构。
USTRUCT(BlueprintType)
struct ASTROBOT_API FAstroOpenAIChatMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|LLM")
	FString Role;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|LLM")
	FString Content;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|LLM")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|LLM")
	FString ToolCallId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AstroBot|LLM")
	TArray<FAstroToolCall> ToolCalls;
};
