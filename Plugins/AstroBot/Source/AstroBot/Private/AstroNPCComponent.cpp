#include "AstroNPCComponent.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/GameInstance.h"
#include "Logging/LogMacros.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "AstroDirectorSubsystem.h"
#include "AstroPromptBuilder.h"
#include "CharacterCardAsset.h"
#include "WorldBookAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroNPCComponent, Log, All);

namespace AstroNPCComponentInternal
{
	static const FString PlayNPCAnimationToolName = TEXT("PlayNPCAnimation");

	static FString DialogueRoleToJsonLabel(const EAstroDialogueRole Role)
	{
		switch (Role)
		{
		case EAstroDialogueRole::System:
			return TEXT("system");
		case EAstroDialogueRole::NPC:
			return TEXT("npc");
		case EAstroDialogueRole::Player:
		default:
			return TEXT("player");
		}
	}

	static void AddChatMessageJson(TArray<TSharedPtr<FJsonValue>>& MessageValues, const FAstroOpenAIChatMessage& Message)
	{
		TSharedRef<FJsonObject> MessageObject = MakeShared<FJsonObject>();
		MessageObject->SetStringField(TEXT("role"), Message.Role);

		if (!Message.Content.IsEmpty())
		{
			MessageObject->SetStringField(TEXT("content"), Message.Content);
		}
		else
		{
			MessageObject->SetStringField(TEXT("content"), TEXT(""));
		}

		if (!Message.Name.IsEmpty())
		{
			MessageObject->SetStringField(TEXT("name"), Message.Name);
		}

		if (!Message.ToolCallId.IsEmpty())
		{
			MessageObject->SetStringField(TEXT("tool_call_id"), Message.ToolCallId);
		}

		if (!Message.ToolCalls.IsEmpty())
		{
			TArray<TSharedPtr<FJsonValue>> ToolCallValues;
			for (const FAstroToolCall& ToolCall : Message.ToolCalls)
			{
				TSharedRef<FJsonObject> ToolCallObject = MakeShared<FJsonObject>();
				ToolCallObject->SetStringField(TEXT("id"), ToolCall.ToolCallId);
				ToolCallObject->SetStringField(TEXT("type"), TEXT("function"));

				TSharedRef<FJsonObject> FunctionObject = MakeShared<FJsonObject>();
				FunctionObject->SetStringField(TEXT("name"), ToolCall.ToolName);
				FunctionObject->SetStringField(TEXT("arguments"), ToolCall.ArgumentsJson);
				ToolCallObject->SetObjectField(TEXT("function"), FunctionObject);

				ToolCallValues.Add(MakeShared<FJsonValueObject>(ToolCallObject));
			}

			MessageObject->SetArrayField(TEXT("tool_calls"), ToolCallValues);
		}

		MessageValues.Add(MakeShared<FJsonValueObject>(MessageObject));
	}

	static void AddToolDefinitionsJson(TSharedRef<FJsonObject> RootObject, const TArray<FAstroToolDefinition>& ToolDefinitions)
	{
		TArray<TSharedPtr<FJsonValue>> ToolValues;
		for (const FAstroToolDefinition& ToolDefinition : ToolDefinitions)
		{
			TSharedRef<FJsonObject> ToolObject = MakeShared<FJsonObject>();
			ToolObject->SetStringField(TEXT("type"), TEXT("function"));

			TSharedRef<FJsonObject> FunctionObject = MakeShared<FJsonObject>();
			FunctionObject->SetStringField(TEXT("name"), ToolDefinition.ToolName);
			FunctionObject->SetStringField(TEXT("description"), ToolDefinition.Description);

			TSharedRef<FJsonObject> ParametersObject = MakeShared<FJsonObject>();
			ParametersObject->SetStringField(TEXT("type"), TEXT("object"));

			TSharedRef<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> RequiredValues;

			for (const FAstroToolParameter& Parameter : ToolDefinition.Parameters)
			{
				TSharedRef<FJsonObject> PropertyObject = MakeShared<FJsonObject>();
				PropertyObject->SetStringField(TEXT("type"), Parameter.Type.IsEmpty() ? TEXT("string") : Parameter.Type);
				PropertyObject->SetStringField(TEXT("description"), Parameter.Description);
				PropertiesObject->SetObjectField(Parameter.Name, PropertyObject);

				if (Parameter.bRequired)
				{
					RequiredValues.Add(MakeShared<FJsonValueString>(Parameter.Name));
				}
			}

			ParametersObject->SetObjectField(TEXT("properties"), PropertiesObject);
			ParametersObject->SetArrayField(TEXT("required"), RequiredValues);
			FunctionObject->SetObjectField(TEXT("parameters"), ParametersObject);
			ToolObject->SetObjectField(TEXT("function"), FunctionObject);

			ToolValues.Add(MakeShared<FJsonValueObject>(ToolObject));
		}

		if (!ToolValues.IsEmpty())
		{
			RootObject->SetArrayField(TEXT("tools"), ToolValues);
		}
	}

	static bool ParseToolCallFromResponse(const FString& ResponseText, FAstroToolCall& OutToolCall, FString& OutAssistantContent)
	{
		OutToolCall = FAstroToolCall();
		OutAssistantContent.Reset();

		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseText);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("choices"), Choices) || Choices == nullptr || Choices->IsEmpty())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* FirstChoiceObject = nullptr;
		if (!(*Choices)[0].IsValid() || !(*Choices)[0]->TryGetObject(FirstChoiceObject) || FirstChoiceObject == nullptr)
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* MessageObject = nullptr;
		if (!(*FirstChoiceObject)->TryGetObjectField(TEXT("message"), MessageObject) || MessageObject == nullptr)
		{
			return false;
		}

		(*MessageObject)->TryGetStringField(TEXT("content"), OutAssistantContent);
		OutAssistantContent = OutAssistantContent.TrimStartAndEnd();

		const TArray<TSharedPtr<FJsonValue>>* ToolCalls = nullptr;
		if (!(*MessageObject)->TryGetArrayField(TEXT("tool_calls"), ToolCalls) || ToolCalls == nullptr || ToolCalls->IsEmpty())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* FirstToolCallObject = nullptr;
		if (!(*ToolCalls)[0].IsValid() || !(*ToolCalls)[0]->TryGetObject(FirstToolCallObject) || FirstToolCallObject == nullptr)
		{
			return false;
		}

		(*FirstToolCallObject)->TryGetStringField(TEXT("id"), OutToolCall.ToolCallId);

		const TSharedPtr<FJsonObject>* FunctionObject = nullptr;
		if (!(*FirstToolCallObject)->TryGetObjectField(TEXT("function"), FunctionObject) || FunctionObject == nullptr)
		{
			return false;
		}

		if (!(*FunctionObject)->TryGetStringField(TEXT("name"), OutToolCall.ToolName))
		{
			return false;
		}

		(*FunctionObject)->TryGetStringField(TEXT("arguments"), OutToolCall.ArgumentsJson);
		return true;
	}

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
	RegisterDefaultTools();
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
	// 旧版本 Prompt 构建只传 CharacterCard / WorldBook / Snapshot / History，保留如下：
	// LastBuiltPrompt = UAstroPromptBuilder::BuildPrompt(CharacterCard, RecalledEntries, Snapshot, ConversationHistory);
	LastBuiltPrompt = UAstroPromptBuilder::BuildPrompt(CharacterCard, RecalledEntries, RuntimeCharacterOverlay, Snapshot, ConversationHistory);
	OnPromptBuilt.Broadcast(LastBuiltPrompt);

	SendRequestToModel(LastBuiltPrompt);
}

void UAstroNPCComponent::StopInteract()
{
	// 旧版本 StopInteract 仅负责关闭交互：
	// bIsInteracting = false;
	// CurrentPlayerController.Reset();
	LogConversationForDirectorDebug();

	if (bReportConversationToDirector)
	{
		SubmitConversationSummaryToDirector();
	}

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

FAstroNPCConversationSummary UAstroNPCComponent::BuildConversationSummaryForDirector() const
{
	FAstroNPCConversationSummary Summary;
	Summary.NPCId = GetOwner() != nullptr ? GetOwner()->GetName() : TEXT("UnknownNPC");
	Summary.NPCDisplayName = GetOwner() != nullptr ? GetOwner()->GetActorNameOrLabel() : TEXT("UnknownNPC");
	Summary.PlayerName = CurrentPlayerController.IsValid() && CurrentPlayerController->PlayerState != nullptr
		? CurrentPlayerController->PlayerState->GetPlayerName()
		: TEXT("UnknownPlayer");
	Summary.PlayerSnapshot = CollectPlayerSnapshot();
	Summary.BasePersonaText = CharacterCard != nullptr ? CharacterCard->PersonaText : TEXT("");
	Summary.BaseStyleText = CharacterCard != nullptr ? CharacterCard->StyleText : TEXT("");
	Summary.BaseForbiddenText = CharacterCard != nullptr ? CharacterCard->ForbiddenText : TEXT("");
	Summary.ConversationTurns = ConversationHistory;
	Summary.LastPlayerMessage = LastUserMessage;
	Summary.LastNPCReply = LastReceivedReply;
	return Summary;
}

FString UAstroNPCComponent::BuildConversationHistoryJson() const
{
	TArray<TSharedPtr<FJsonValue>> ConversationValues;
	for (const FAstroDialogueTurn& Turn : ConversationHistory)
	{
		TSharedRef<FJsonObject> TurnObject = MakeShared<FJsonObject>();
		TurnObject->SetStringField(TEXT("role"), AstroNPCComponentInternal::DialogueRoleToJsonLabel(Turn.Role));
		TurnObject->SetStringField(TEXT("text"), Turn.Text);
		ConversationValues.Add(MakeShared<FJsonValueObject>(TurnObject));
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetArrayField(TEXT("conversation"), ConversationValues);

	FString JsonOutput;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		return TEXT("{\"conversation\":[]}");
	}

	return JsonOutput;
}

FString UAstroNPCComponent::BuildCharacterCardDebugText() const
{
	if (CharacterCard == nullptr)
	{
		return TEXT("CharacterCard: None");
	}

	// 当前阶段只做调试日志用途，因此直接输出角色卡核心字段，便于后续接导演系统真实总结。
	FString DebugText;
	DebugText += TEXT("CharacterCard\n");
	DebugText += TEXT("Persona:\n") + CharacterCard->PersonaText + TEXT("\n\n");
	DebugText += TEXT("Style:\n") + CharacterCard->StyleText + TEXT("\n\n");
	DebugText += TEXT("Forbidden:\n") + CharacterCard->ForbiddenText;
	return DebugText;
}

void UAstroNPCComponent::LogConversationForDirectorDebug() const
{
	// 结束对话时输出角色卡文本与完整对话 JSON，便于你在切真实导演模型前确认发送素材是否正确。
	UE_LOG(LogAstroNPCComponent, Log, TEXT("[AstroBot Director Debug] %s"), *BuildCharacterCardDebugText());
	UE_LOG(LogAstroNPCComponent, Log, TEXT("[AstroBot Director Debug] ConversationJson=%s"), *BuildConversationHistoryJson());
}

void UAstroNPCComponent::SubmitConversationSummaryToDirector()
{
	if (GetWorld() == nullptr || GetWorld()->GetGameInstance() == nullptr)
	{
		return;
	}

	if (UAstroDirectorSubsystem* DirectorSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UAstroDirectorSubsystem>())
	{
		DirectorSubsystem->SubmitConversationSummary(BuildConversationSummaryForDirector());
		RuntimeCharacterOverlay = DirectorSubsystem->GetCharacterOverlayForNPC(GetOwner() != nullptr ? GetOwner()->GetName() : TEXT("UnknownNPC"));
	}
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
	TArray<FAstroOpenAIChatMessage> Messages;
	FAstroOpenAIChatMessage UserMessage;
	UserMessage.Role = TEXT("user");
	UserMessage.Content = PromptText;
	Messages.Add(UserMessage);

	SendHttpRequestToModelWithMessages(Messages, bEnableToolCalling, 0);
}

void UAstroNPCComponent::SendHttpRequestToModelWithMessages(const TArray<FAstroOpenAIChatMessage>& Messages, const bool bIncludeTools, const int32 ToolCallDepth)
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

	TArray<TSharedPtr<FJsonValue>> MessageValues;
	for (const FAstroOpenAIChatMessage& Message : Messages)
	{
		AstroNPCComponentInternal::AddChatMessageJson(MessageValues, Message);
	}
	RootObject->SetArrayField(TEXT("messages"), MessageValues);

	if (bIncludeTools && !AvailableTools.IsEmpty())
	{
		AstroNPCComponentInternal::AddToolDefinitionsJson(RootObject, AvailableTools);
	}

	RootObject->SetNumberField(TEXT("temperature"), 0.7);

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
	Request->SetHeader(TEXT("X-AstroBot-ToolDepth"), LexToString(ToolCallDepth));
	Request->SetContentAsString(RequestBody);

	if (ToolCallDepth <= 0)
	{
		Request->OnProcessRequestComplete().BindUObject(this, &UAstroNPCComponent::OnModelHttpResponseReceived);
	}
	else
	{
		Request->OnProcessRequestComplete().BindUObject(this, &UAstroNPCComponent::OnToolFollowUpHttpResponseReceived);
	}

	if (!Request->ProcessRequest())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] Failed to start HTTP request."));
	}
}

void UAstroNPCComponent::OnModelHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
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

	const int32 ToolCallDepth = Request.IsValid()
		? FCString::Atoi(*Request->GetHeader(TEXT("X-AstroBot-ToolDepth")))
		: 0;

	if (bEnableToolCalling && ToolCallDepth < MaxToolCallsPerMessage)
	{
		FAstroToolCall ToolCall;
		FString AssistantContent;
		if (AstroNPCComponentInternal::ParseToolCallFromResponse(Response->GetContentAsString(), ToolCall, AssistantContent))
		{
			LastToolCall = ToolCall;
			LastToolExecutionResult = ExecuteToolCall(ToolCall);
			OnToolExecuted.Broadcast(LastToolCall, LastToolExecutionResult);

			if (!LastToolExecutionResult.bSuccess)
			{
				HandleModelReply(LastToolExecutionResult.ResultMessage.IsEmpty()
					? TEXT("[AstroBot Tool Error] Tool execution failed.")
					: LastToolExecutionResult.ResultMessage);
				return;
			}

			TArray<FAstroOpenAIChatMessage> FollowUpMessages;
			FAstroOpenAIChatMessage UserMessage;
			UserMessage.Role = TEXT("user");
			UserMessage.Content = LastBuiltPrompt;
			FollowUpMessages.Add(UserMessage);

			FAstroOpenAIChatMessage AssistantMessage;
			AssistantMessage.Role = TEXT("assistant");
			AssistantMessage.Content = AssistantContent;
			AssistantMessage.ToolCalls.Add(ToolCall);
			FollowUpMessages.Add(AssistantMessage);

			FAstroOpenAIChatMessage ToolMessage;
			ToolMessage.Role = TEXT("tool");
			ToolMessage.Name = ToolCall.ToolName;
			ToolMessage.ToolCallId = ToolCall.ToolCallId;
			ToolMessage.Content = LastToolExecutionResult.ResultJson.IsEmpty()
				? LastToolExecutionResult.ResultMessage
				: LastToolExecutionResult.ResultJson;
			FollowUpMessages.Add(ToolMessage);

			SendHttpRequestToModelWithMessages(FollowUpMessages, false, ToolCallDepth + 1);
			return;
		}
	}

	const FString ReplyText = AstroNPCComponentInternal::ExtractAssistantReplyFromOpenAIResponse(Response->GetContentAsString());
	if (ReplyText.IsEmpty())
	{
		HandleModelReply(TEXT("[AstroBot HTTP Error] Response parsed successfully, but no assistant content was found."));
		return;
	}

	HandleModelReply(ReplyText);
}

void UAstroNPCComponent::OnToolFollowUpHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	OnModelHttpResponseReceived(Request, Response, bWasSuccessful);
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

void UAstroNPCComponent::RegisterDefaultTools()
{
	if (!AvailableTools.IsEmpty())
	{
		return;
	}

	FAstroToolDefinition PlayAnimationTool;
	PlayAnimationTool.ToolName = AstroNPCComponentInternal::PlayNPCAnimationToolName;
	PlayAnimationTool.Description = TEXT("Play a semantic animation on the current NPC. Use short animation tags such as Greet, Nod, Wave, Point, Laugh, or IdleTalk.");

	FAstroToolParameter AnimationTagParameter;
	AnimationTagParameter.Name = TEXT("animation_tag");
	AnimationTagParameter.Type = TEXT("string");
	AnimationTagParameter.Description = TEXT("Semantic animation tag to play on the NPC, for example Greet or Nod.");
	AnimationTagParameter.bRequired = true;
	PlayAnimationTool.Parameters.Add(AnimationTagParameter);

	AvailableTools.Add(PlayAnimationTool);
}

FAstroToolExecutionResult UAstroNPCComponent::ExecuteToolCall(const FAstroToolCall& ToolCall)
{
	FAstroToolExecutionResult Result;

	if (!IsValid(this))
	{
		Result.ResultMessage = TEXT("[AstroBot Tool Error] Component is invalid.");
		return Result;
	}

	Result = HandleAstroToolCall(ToolCall);
	if (Result.ResultJson.IsEmpty())
	{
		Result.ResultJson = FString::Printf(TEXT("{\"success\":%s,\"message\":\"%s\"}"), Result.bSuccess ? TEXT("true") : TEXT("false"), *Result.ResultMessage.ReplaceCharWithEscapedChar());
	}

	return Result;
}

FAstroToolExecutionResult UAstroNPCComponent::HandleAstroToolCall_Implementation(const FAstroToolCall& ToolCall)
{
	FAstroToolExecutionResult Result;
	Result.bSuccess = false;
	Result.ResultMessage = FString::Printf(TEXT("[AstroBot Tool Error] Tool '%s' is not implemented on this component. Override HandleAstroToolCall in Blueprint or C++."), *ToolCall.ToolName);
	Result.ResultJson = FString::Printf(TEXT("{\"success\":false,\"tool_name\":\"%s\",\"message\":\"%s\"}"), *ToolCall.ToolName.ReplaceCharWithEscapedChar(), *Result.ResultMessage.ReplaceCharWithEscapedChar());
	return Result;
}
