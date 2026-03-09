#include "AstroNPCComponent.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "AstroPromptBuilder.h"
#include "CharacterCardAsset.h"
#include "WorldBookAsset.h"

namespace AstroNPCComponentInternal
{
	static FString ExtractAssistantReplyFromOpenAIResponse(const FString& ResponseText)
	{
		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseText);

		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			return TEXT("");
		}

		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("choices"), Choices) || Choices == nullptr || Choices->IsEmpty())
		{
			return TEXT("");
		}

		const TSharedPtr<FJsonObject>* FirstChoiceObject = nullptr;
		if (!(*Choices)[0].IsValid() || !(*Choices)[0]->TryGetObject(FirstChoiceObject) || FirstChoiceObject == nullptr)
		{
			return TEXT("");
		}

		const TSharedPtr<FJsonObject>* MessageObject = nullptr;
		if (!(*FirstChoiceObject)->TryGetObjectField(TEXT("message"), MessageObject) || MessageObject == nullptr)
		{
			return TEXT("");
		}

		FString Content;
		if (!(*MessageObject)->TryGetStringField(TEXT("content"), Content))
		{
			return TEXT("");
		}

		return Content.TrimStartAndEnd();
	}
}

UAstroNPCComponent::UAstroNPCComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAstroNPCComponent::StartInteract(APlayerController* PlayerController)
{
	CurrentPlayerController = PlayerController;
	bIsInteracting = CurrentPlayerController.IsValid();
}

void UAstroNPCComponent::SendMessage(const FString& Text)
{
	// 每条玩家消息触发一次完整的请求/回复流程。
	if (!bIsInteracting || Text.IsEmpty())
	{
		return;
	}

	// 单独记录最近一条用户输入，供 Mock 模式做稳定可控的分支测试。
	LastUserMessage = Text;

	FAstroDialogueTurn PlayerTurn;
	PlayerTurn.Role = EAstroDialogueRole::Player;
	PlayerTurn.Text = Text;
	ConversationHistory.Add(PlayerTurn);

	if (ConversationHistory.Num() > MaxConversationTurns)
	{
		const int32 NumToRemove = ConversationHistory.Num() - MaxConversationTurns;
		ConversationHistory.RemoveAt(0, NumToRemove);
	}

	TArray<FWorldBookEntry> RecalledEntries;
	if (WorldBook != nullptr)
	{
		RecalledEntries = WorldBook->RetrieveEntriesByKeyword(Text, RecallTopK);
	}

	const FAstroPlayerSnapshot Snapshot = CollectPlayerSnapshot();
	LastBuiltPrompt = UAstroPromptBuilder::BuildPrompt(CharacterCard, RecalledEntries, Snapshot, ConversationHistory);
	OnPromptBuilt.Broadcast(LastBuiltPrompt);

	SendRequestToModel(LastBuiltPrompt);
}

void UAstroNPCComponent::StopInteract()
{
	bIsInteracting = false;
	CurrentPlayerController.Reset();
}

FAstroPlayerSnapshot UAstroNPCComponent::CollectPlayerSnapshot() const
{
	FAstroPlayerSnapshot Snapshot;

	if (!CurrentPlayerController.IsValid())
	{
		return Snapshot;
	}

	const APlayerController* PlayerController = CurrentPlayerController.Get();
	Snapshot.PlayerName = PlayerController->PlayerState != nullptr ? PlayerController->PlayerState->GetPlayerName() : TEXT("Unknown");
	Snapshot.LevelName = GetWorld() != nullptr ? GetWorld()->GetMapName() : TEXT("Unknown");

	if (const APawn* Pawn = PlayerController->GetPawn())
	{
		Snapshot.Location = Pawn->GetActorLocation();
		Snapshot.Velocity = Pawn->GetVelocity();

		// TODO: 在编辑器数据绑定后，替换为你项目里的真实属性（体力等）。
		if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Snapshot.Stamina = Character->GetVelocity().Size();
			Snapshot.MaxStamina = 600.0f;
		}
	}

	// TODO: 从项目的战斗/任务/经济系统或蓝图变量中接入血量、法力、金币、任务。
	Snapshot.Health = 100.0f;
	Snapshot.MaxHealth = 100.0f;
	Snapshot.Mana = 100.0f;
	Snapshot.MaxMana = 100.0f;
	Snapshot.Gold = 0;
	Snapshot.CurrentQuest = TEXT("None");

	return Snapshot;
}

void UAstroNPCComponent::SendRequestToModel(const FString& PromptText)
{
	// 统一请求入口：保持原有上游流程不变，只在这里切换不同测试/运行模式。
	if (PromptText.IsEmpty())
	{
		return;
	}

	switch (RequestMode)
	{
	case EAstroModelRequestMode::Stub:
		HandleModelReply(StubReplyText.IsEmpty()
			? TEXT("[Stub Reply] Request pipeline is ready. Connect your LLM service in SendRequestToModel.")
			: StubReplyText);
		break;

	case EAstroModelRequestMode::Mock:
		HandleModelReply(BuildMockReply(PromptText));
		break;

	case EAstroModelRequestMode::RealAPI:
		SendHttpRequestToModel(PromptText);
		break;

	default:
		HandleModelReply(TEXT("[AstroBot] Unsupported request mode."));
		break;
	}
}

FString UAstroNPCComponent::BuildMockReply(const FString& PromptText) const
{
	// Mock 模式的目标是返回确定性结果，便于你稳定验证不同流程分支。
	const FString InputLower = LastUserMessage.ToLower();
	const FString PromptLower = PromptText.ToLower();

	if (InputLower.Contains(TEXT("你好")) || InputLower.Contains(TEXT("hello")) || InputLower.Contains(TEXT("hi")))
	{
		return TEXT("[Mock Reply] 你好，当前问候分支已命中，说明 NPC 对话链路和基础事件广播正常。");
	}

	if (InputLower.Contains(TEXT("任务")) || InputLower.Contains(TEXT("quest")))
	{
		return TEXT("[Mock Reply] 任务分支已命中。你可以继续检查 Prompt 中的 CurrentQuest 字段与历史对话是否符合预期。");
	}

	if (InputLower.Contains(TEXT("商店")) || InputLower.Contains(TEXT("交易")) || InputLower.Contains(TEXT("shop")))
	{
		return TEXT("[Mock Reply] 交易分支已命中。此结果可用于验证经济类 NPC 的固定业务流程。");
	}

	if (PromptLower.Contains(TEXT("no matched world book entries")))
	{
		return MockFallbackReply.IsEmpty()
			? TEXT("[Mock Reply] No rule matched and no world book entry was recalled.")
			: MockFallbackReply;
	}

	return MockFallbackReply.IsEmpty()
		? TEXT("[Mock Reply] Deterministic fallback response.")
		: MockFallbackReply;
}

void UAstroNPCComponent::SendHttpRequestToModel(const FString& PromptText)
{
	// 真实接口只在必要配置齐全时发送，避免测试阶段误打外部接口。
	if (ApiBaseUrl.IsEmpty())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] ApiBaseUrl is empty."));
		return;
	}

	if (ModelName.IsEmpty())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] ModelName is empty."));
		return;
	}

	if (ApiKey.IsEmpty())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] ApiKey is empty. Switch to Stub/Mock mode for local testing or fill in the key for RealAPI mode."));
		return;
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), ModelName);

	TArray<TSharedPtr<FJsonValue>> Messages;
	{
		TSharedRef<FJsonObject> UserMessageObject = MakeShared<FJsonObject>();
		// 这里直接发送已经构建好的 Prompt，保持与当前插件 Prompt 架构一致。
		UserMessageObject->SetStringField(TEXT("role"), TEXT("user"));
		UserMessageObject->SetStringField(TEXT("content"), PromptText);
		Messages.Add(MakeShared<FJsonValueObject>(UserMessageObject));
	}
	RootObject->SetArrayField(TEXT("messages"), Messages);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] Failed to serialize request body."));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ApiBaseUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetContentAsString(RequestBody);
	Request->OnProcessRequestComplete().BindUObject(this, &UAstroNPCComponent::OnModelHttpResponseReceived);

	if (!Request->ProcessRequest())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] Failed to start HTTP request."));
	}
}

void UAstroNPCComponent::OnModelHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// 当前回调不需要直接读取 Request，但保留参数以兼容 UE HTTP 回调签名。
	(void)Request;

	// 统一在回调里做错误折叠，避免把失败状态散落到多个分支里。
	if (!bWasSuccessful || !Response.IsValid())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] Request failed or response is invalid."));
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		HandleModelReply(FString::Printf(
			TEXT("[AstroBot HTTP Error] Status=%d Body=%s"),
			Response->GetResponseCode(),
			*Response->GetContentAsString()));
		return;
	}

	const FString ReplyText = AstroNPCComponentInternal::ExtractAssistantReplyFromOpenAIResponse(Response->GetContentAsString());
	if (ReplyText.IsEmpty())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] Response parsed successfully, but no assistant content was found."));
		return;
	}

	HandleModelReply(ReplyText);
}

void UAstroNPCComponent::HandleModelReply(const FString& ReplyText)
{
	// 统一在这里处理回复，确保历史记录和事件行为一致。
	LastReceivedReply = ReplyText;

	FAstroDialogueTurn NPCTurn;
	NPCTurn.Role = EAstroDialogueRole::NPC;
	NPCTurn.Text = ReplyText;
	ConversationHistory.Add(NPCTurn);

	if (ConversationHistory.Num() > MaxConversationTurns)
	{
		const int32 NumToRemove = ConversationHistory.Num() - MaxConversationTurns;
		ConversationHistory.RemoveAt(0, NumToRemove);
	}

	OnReplyReceived.Broadcast(ReplyText);
}
