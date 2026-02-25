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
