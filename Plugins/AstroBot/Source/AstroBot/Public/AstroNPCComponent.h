#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "AstroBotTypes.h"
#include "AstroDirectorTypes.h"
#include "AstroNPCComponent.generated.h"

class APlayerController;
class UCharacterCardAsset;
class UWorldBookAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAstroPromptBuiltSignature, const FString&, PromptText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAstroNPCReplySignature, const FString&, ReplyText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAstroToolExecutedSignature, const FAstroToolCall&, ToolCall, const FAstroToolExecutionResult&, Result);

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

	// 项目侧可用来播放 NPC 动作动画的默认工具实现。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AstroBot|Tools")
	FAstroToolExecutionResult HandleAstroToolCall(const FAstroToolCall& ToolCall);
	
	
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

	// 控制请求发送模式：本地桩、本地规则 Mock、真实 HTTP。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|LLM")
	EAstroModelRequestMode RequestMode = EAstroModelRequestMode::Stub;

	// 本地桩模式固定返回的文本，适合先验证蓝图事件与会话历史。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|LLM", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::Stub", MultiLine = "true"))
	FString StubReplyText = TEXT("[Stub Reply] Request pipeline is ready. Connect your LLM service in SendRequestToModel.");

	// Mock 模式下，当输入无法命中特定规则时使用该兜底回复。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|LLM", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::Mock", MultiLine = "true"))
	FString MockFallbackReply = TEXT("[Mock Reply] No specific keyword matched. This is the deterministic fallback response.");

	// OpenAI 兼容接口地址，默认使用聊天补全端点。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|LLM", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::RealAPI"))
	FString ApiBaseUrl = TEXT("https://api.openai.com/v1/chat/completions");

	// 真实请求使用的模型名称，可替换为兼容服务商模型名。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|LLM", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::RealAPI"))
	FString ModelName = TEXT("gpt-4o-mini");

	// 鉴权 Key。建议后续迁移到项目设置或更安全的配置来源。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|LLM", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::RealAPI"))
	FString ApiKey;

	// 是否向真实模型声明并执行工具调用。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Tools", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::RealAPI"))
	bool bEnableToolCalling = true;

	// 每次玩家输入允许的最大工具调用轮数，避免模型陷入无限调用。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Tools", meta = (ClampMin = "0", ClampMax = "8", EditCondition = "RequestMode == EAstroModelRequestMode::RealAPI && bEnableToolCalling"))
	int32 MaxToolCallsPerMessage = 2;

	// 当前组件对模型声明的可用工具列表。默认会注册 PlayNPCAnimation。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Tools", meta = (EditCondition = "RequestMode == EAstroModelRequestMode::RealAPI && bEnableToolCalling"))
	TArray<FAstroToolDefinition> AvailableTools;

	// 是否在交互结束时向导演系统上报会话摘要。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|Director")
	bool bReportConversationToDirector = true;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	bool bIsInteracting = false;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FString LastBuiltPrompt;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FString LastReceivedReply;

	// 保存最近一次发送给请求层的用户文本，便于 Mock 模式做稳定分支判断。
	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FString LastUserMessage;

	// 最近一次被模型触发的工具调用。
	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FAstroToolCall LastToolCall;

	// 最近一次工具执行结果。
	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|State")
	FAstroToolExecutionResult LastToolExecutionResult;

	// 工具执行完成时触发，便于蓝图调试工具链路。
	UPROPERTY(BlueprintAssignable, Category = "AstroBot|Tools")
	FAstroToolExecutedSignature OnToolExecuted;

	// 当前 NPC 从导演系统拿到的运行时补丁，仅在本次运行期间生效。
	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|Director")
	FAstroRuntimeCharacterOverlay RuntimeCharacterOverlay;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> CurrentPlayerController;

	UPROPERTY(Transient)
	TArray<FAstroDialogueTurn> ConversationHistory;

	// 当前这一次打开对话框到关闭对话框之间的会话历史，仅用于提交给导演系统做本次会话总结。
	UPROPERTY(Transient)
	TArray<FAstroDialogueTurn> CurrentSessionConversationHistory;

	// 采集当前可用的玩家字段，作为 Prompt 上下文。
	FAstroPlayerSnapshot CollectPlayerSnapshot() const;
	void RefreshRuntimeOverlayFromDirector();
	FAstroNPCConversationSummary BuildConversationSummaryForDirector() const;
	FString BuildConversationHistoryJson() const;
	FString BuildCurrentRuntimeCharacterCardDebugText() const;
	FString BuildCharacterCardDebugText() const;
	void LogConversationForDirectorDebug() const;
	void SubmitConversationSummaryToDirector();
	// 请求发送入口；内部根据模式切换到 Stub / Mock / RealAPI，避免破坏现有上游流程。
	void SendRequestToModel(const FString& PromptText);
	// 为 Mock 模式生成可预测回复，保证测试时不依赖网络与计费接口。
	FString BuildMockReply(const FString& PromptText) const;
	// 发起 OpenAI 兼容 HTTP 请求。
	void SendHttpRequestToModel(const FString& PromptText);
	void SendHttpRequestToModelWithMessages(const TArray<struct FAstroOpenAIChatMessage>& Messages, bool bIncludeTools, int32 ToolCallDepth);
	// 解析 HTTP 结果并统一转交回复入口。
	void OnModelHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnToolFollowUpHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	// 统一回复入口：更新状态并广播事件。
	void HandleModelReply(const FString& ReplyText);
	void RegisterDefaultTools();
	FAstroToolExecutionResult ExecuteToolCall(const FAstroToolCall& ToolCall);
	virtual FAstroToolExecutionResult HandleAstroToolCall_Implementation(const FAstroToolCall& ToolCall);
};
