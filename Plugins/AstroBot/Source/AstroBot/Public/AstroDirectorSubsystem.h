#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AstroDirectorTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "AstroDirectorSubsystem.generated.h"

class UAstroDirectorToolExecutor;
class IHttpRequest;
class IHttpResponse;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAstroDirectorOverlayUpdatedSignature, const FAstroRuntimeCharacterOverlay&, Overlay);

UCLASS()
class ASTROBOT_API UAstroDirectorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 配置导演模型的真实 HTTP 参数。当前版本使用 OpenAI 兼容聊天补全接口。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	void ConfigureDirectorAPI(const FString& InApiBaseUrl, const FString& InModelName, const FString& InApiKey);

	// 接收单次 NPC 会话摘要并更新运行时角色补丁。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	void SubmitConversationSummary(const FAstroNPCConversationSummary& Summary);

	// 提交周期性导演 Tick 信息，当前最小版本会生成导演调试摘要并维护世界覆盖层。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	void SubmitPeriodicTick(const FAstroDirectorTickPayload& TickPayload);

	// 获取指定 NPC 的运行时补丁；若不存在则返回空补丁。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	FAstroRuntimeCharacterOverlay GetCharacterOverlayForNPC(const FString& NPCId) const;

	// 获取当前世界覆盖层，供后续天气/事件系统读取。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	FAstroRuntimeWorldOverlay GetCurrentWorldOverlay() const;

	// 手动执行一个导演工具调用，便于蓝图或调试面板验证区域导演能力。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	FAstroToolExecutionResult ExecuteDirectorToolCall(const FAstroToolCall& ToolCall);

	UPROPERTY(BlueprintAssignable, Category = "AstroBot|Director")
	FAstroDirectorOverlayUpdatedSignature OnCharacterOverlayUpdated;

	UPROPERTY(BlueprintReadOnly, Category = "AstroBot|Director")
	FAstroDirectorDecision LastDirectorDecision;

protected:
	// 导演系统真实请求配置。Subsystem 不方便像组件那样直接在关卡中编辑，因此先提供运行时配置入口。
	UPROPERTY(Transient)
	FString DirectorApiBaseUrl = TEXT("https://api.openai.com/v1/chat/completions");

	UPROPERTY(Transient)
	FString DirectorModelName = TEXT("gpt-4o-mini");

	UPROPERTY(Transient)
	FString DirectorApiKey;

	UPROPERTY(Transient)
	bool bUseRealDirectorAPI = false;

	// 维护每个 NPC 的运行时补丁，不改动原始 CharacterCardAsset。
	UPROPERTY(Transient)
	TMap<FString, FAstroRuntimeCharacterOverlay> CharacterOverlays;

	// 保存最近若干条会话摘要，供周期性导演 Prompt 使用。
	UPROPERTY(Transient)
	TArray<FAstroNPCConversationSummary> RecentConversationSummaries;

	UPROPERTY(Transient)
	FAstroRuntimeWorldOverlay CurrentWorldOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UAstroDirectorToolExecutor> ToolExecutor;

	// 旧版本启发式总结入口保留如下，方便后续回退参考：
	// void ApplyHeuristicOverlayUpdate(const FAstroNPCConversationSummary& Summary);
	void ApplyHeuristicOverlayUpdate(const FAstroNPCConversationSummary& Summary);
	void SendConversationSummaryToDirectorModel(const FAstroNPCConversationSummary& Summary);
	void OnDirectorHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
