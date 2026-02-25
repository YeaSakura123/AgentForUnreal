#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AstroBotTypes.h"
#include "AstroNPCComponent.generated.h"

class APlayerController;
class UCharacterCardAsset;
class UWorldBookAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAstroPromptBuiltSignature, const FString&, PromptText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAstroNPCReplySignature, const FString&, ReplyText);

// NPC 对话 Runtime 协调组件。
// 流程：收集上下文 -> 构建 Prompt -> 发请求 -> 收回复。
UCLASS(ClassGroup = (AstroBot), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ASTROBOT_API UAstroNPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAstroNPCComponent();

	UFUNCTION(BlueprintCallable, Category = "AstroBot|Interaction")
	void StartInteract(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "AstroBot|Interaction")
	void SendMessage(const FString& Text);

	UFUNCTION(BlueprintCallable, Category = "AstroBot|Interaction")
	void StopInteract();

	UPROPERTY(BlueprintAssignable, Category = "AstroBot|Interaction")
	FAstroPromptBuiltSignature OnPromptBuilt;

	// 模型回复可用时触发。
	UPROPERTY(BlueprintAssignable, Category = "AstroBot|Interaction")
	FAstroNPCReplySignature OnReplyReceived;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Config")
	UCharacterCardAsset* CharacterCard = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Config")
	UWorldBookAsset* WorldBook = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Config", meta = (ClampMin = "1", ClampMax = "50"))
	int32 MaxConversationTurns = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Config", meta = (ClampMin = "1", ClampMax = "20"))
	int32 RecallTopK = 3;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	bool bIsInteracting = false;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FString LastBuiltPrompt;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FString LastReceivedReply;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> CurrentPlayerController;

	UPROPERTY(Transient)
	TArray<FAstroDialogueTurn> ConversationHistory;

	// 采集当前可用的玩家字段，作为 Prompt 上下文。
	FAstroPlayerSnapshot CollectPlayerSnapshot() const;
	// 请求发送入口；当前为桩实现，后续替换为真实模型调用。
	void SendRequestToModel(const FString& PromptText);
	// 统一回复入口：更新状态并广播事件。
	void HandleModelReply(const FString& ReplyText);
};
