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
