#include "AstroDirectorSubsystem.h"

#include "AstroDirectorPromptBuilder.h"
#include "AstroDirectorToolExecutor.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace AstroDirectorSubsystemInternal
{
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

	// 将完整会话、角色卡、玩家快照与当前 Overlay 组装成导演模型输入 JSON。
	static bool BuildDirectorRequestPayload(
		const FAstroNPCConversationSummary& Summary,
		const FAstroRuntimeCharacterOverlay& CurrentOverlay,
		const FAstroRuntimeWorldOverlay& CurrentWorldOverlay,
		FString& OutPayload)
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

		TSharedRef<FJsonObject> NPCObject = MakeShared<FJsonObject>();
		NPCObject->SetStringField(TEXT("id"), Summary.NPCId);
		NPCObject->SetStringField(TEXT("display_name"), Summary.NPCDisplayName);
		NPCObject->SetStringField(TEXT("base_persona"), Summary.BasePersonaText);
		NPCObject->SetStringField(TEXT("base_style"), Summary.BaseStyleText);
		NPCObject->SetStringField(TEXT("base_forbidden"), Summary.BaseForbiddenText);
		RootObject->SetObjectField(TEXT("npc"), NPCObject);

		TSharedRef<FJsonObject> PlayerObject = MakeShared<FJsonObject>();
		PlayerObject->SetStringField(TEXT("name"), Summary.PlayerName);
		PlayerObject->SetStringField(TEXT("level_name"), Summary.PlayerSnapshot.LevelName);
		PlayerObject->SetStringField(TEXT("current_quest"), Summary.PlayerSnapshot.CurrentQuest);
		PlayerObject->SetNumberField(TEXT("health"), Summary.PlayerSnapshot.Health);
		PlayerObject->SetNumberField(TEXT("max_health"), Summary.PlayerSnapshot.MaxHealth);
		PlayerObject->SetNumberField(TEXT("mana"), Summary.PlayerSnapshot.Mana);
		PlayerObject->SetNumberField(TEXT("max_mana"), Summary.PlayerSnapshot.MaxMana);
		PlayerObject->SetNumberField(TEXT("gold"), Summary.PlayerSnapshot.Gold);
		RootObject->SetObjectField(TEXT("player"), PlayerObject);

		TSharedRef<FJsonObject> OverlayObject = MakeShared<FJsonObject>();
		OverlayObject->SetNumberField(TEXT("affinity_score"), CurrentOverlay.AffinityScore);
		OverlayObject->SetStringField(TEXT("relationship_summary"), CurrentOverlay.RelationshipSummary);
		OverlayObject->SetStringField(TEXT("additional_persona_text"), CurrentOverlay.AdditionalPersonaText);
		OverlayObject->SetStringField(TEXT("additional_style_text"), CurrentOverlay.AdditionalStyleText);
		OverlayObject->SetStringField(TEXT("additional_forbidden_text"), CurrentOverlay.AdditionalForbiddenText);
		OverlayObject->SetStringField(TEXT("recent_memory_summary"), CurrentOverlay.RecentMemorySummary);
		RootObject->SetObjectField(TEXT("current_overlay"), OverlayObject);

		TSharedRef<FJsonObject> RegionObject = MakeShared<FJsonObject>();
		RegionObject->SetStringField(TEXT("region_id"), CurrentWorldOverlay.RegionId);
		RegionObject->SetStringField(TEXT("weather_state"), CurrentWorldOverlay.WeatherState);
		RegionObject->SetStringField(TEXT("active_event_summary"), CurrentWorldOverlay.ActiveEventSummary);
		RegionObject->SetStringField(TEXT("director_notes"), CurrentWorldOverlay.DirectorNotes);
		RootObject->SetObjectField(TEXT("region_context"), RegionObject);

		TArray<TSharedPtr<FJsonValue>> ConversationValues;
		for (const FAstroDialogueTurn& Turn : Summary.ConversationTurns)
		{
			TSharedRef<FJsonObject> TurnObject = MakeShared<FJsonObject>();
			TurnObject->SetStringField(TEXT("role"), DialogueRoleToJsonLabel(Turn.Role));
			TurnObject->SetStringField(TEXT("text"), Turn.Text);
			ConversationValues.Add(MakeShared<FJsonValueObject>(TurnObject));
		}
		RootObject->SetArrayField(TEXT("conversation"), ConversationValues);

		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutPayload);
		return FJsonSerializer::Serialize(RootObject, Writer);
	}

	// 解析导演模型返回的 JSON 内容，并提取运行时 Overlay 更新结果。
	static bool ParseDirectorOverlayResponse(const FString& ResponseContent, FAstroRuntimeCharacterOverlay& InOutOverlay, FString& OutSummaryText)
	{
		FString JsonText = ResponseContent.TrimStartAndEnd();
		if (JsonText.StartsWith(TEXT("```")))
		{
			JsonText = JsonText.Replace(TEXT("```json"), TEXT(""));
			JsonText = JsonText.Replace(TEXT("```"), TEXT(""));
			JsonText = JsonText.TrimStartAndEnd();
		}

		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			return false;
		}

		RootObject->TryGetStringField(TEXT("summary_text"), OutSummaryText);

		const TSharedPtr<FJsonObject>* OverlayUpdateObject = nullptr;
		if (!RootObject->TryGetObjectField(TEXT("character_overlay_update"), OverlayUpdateObject) || OverlayUpdateObject == nullptr)
		{
			return false;
		}

		bool bShouldUpdate = true;
		(*OverlayUpdateObject)->TryGetBoolField(TEXT("should_update"), bShouldUpdate);
		if (!bShouldUpdate)
		{
			return true;
		}

		int32 AffinityDelta = 0;
		(*OverlayUpdateObject)->TryGetNumberField(TEXT("affinity_score_delta"), AffinityDelta);
		InOutOverlay.AffinityScore += AffinityDelta;

		FString Value;
		if ((*OverlayUpdateObject)->TryGetStringField(TEXT("relationship_summary"), Value) && !Value.IsEmpty())
		{
			InOutOverlay.RelationshipSummary = Value;
		}
		if ((*OverlayUpdateObject)->TryGetStringField(TEXT("additional_persona_text"), Value) && !Value.IsEmpty())
		{
			InOutOverlay.AdditionalPersonaText = Value;
		}
		if ((*OverlayUpdateObject)->TryGetStringField(TEXT("additional_style_text"), Value) && !Value.IsEmpty())
		{
			InOutOverlay.AdditionalStyleText = Value;
		}
		if ((*OverlayUpdateObject)->TryGetStringField(TEXT("additional_forbidden_text"), Value) && !Value.IsEmpty())
		{
			InOutOverlay.AdditionalForbiddenText = Value;
		}
		if ((*OverlayUpdateObject)->TryGetStringField(TEXT("recent_memory_summary"), Value) && !Value.IsEmpty())
		{
			InOutOverlay.RecentMemorySummary = Value;
		}

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

void UAstroDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 初始化导演工具执行器，后续世界级工具调用统一由这里路由。
	ToolExecutor = NewObject<UAstroDirectorToolExecutor>(this);
	CurrentWorldOverlay.RegionId = TEXT("DefaultRegion");
	CurrentWorldOverlay.WeatherState = TEXT("Clear");
	CurrentWorldOverlay.ActiveEventSummary = TEXT("None");
}

void UAstroDirectorSubsystem::ConfigureDirectorAPI(const FString& InApiBaseUrl, const FString& InModelName, const FString& InApiKey)
{
	DirectorApiBaseUrl = InApiBaseUrl;
	DirectorModelName = InModelName;
	DirectorApiKey = InApiKey;
	bUseRealDirectorAPI = !DirectorApiBaseUrl.IsEmpty() && !DirectorModelName.IsEmpty() && !DirectorApiKey.IsEmpty();
}

void UAstroDirectorSubsystem::Deinitialize()
{
	CharacterOverlays.Empty();
	RecentConversationSummaries.Empty();
	ToolExecutor = nullptr;
	Super::Deinitialize();
}

void UAstroDirectorSubsystem::SubmitConversationSummary(const FAstroNPCConversationSummary& Summary)
{
	RecentConversationSummaries.Add(Summary);
	if (RecentConversationSummaries.Num() > 16)
	{
		RecentConversationSummaries.RemoveAt(0, RecentConversationSummaries.Num() - 16);
	}

	// 旧版本这里直接走启发式总结，保留如下作为回退参考：
	// ApplyHeuristicOverlayUpdate(Summary);
	// FAstroRuntimeCharacterOverlay Overlay = GetCharacterOverlayForNPC(Summary.NPCId);
	// LastDirectorDecision.SummaryText = FString::Printf(TEXT("Conversation summary processed for NPC: %s"), *Summary.NPCId);
	// OnCharacterOverlayUpdated.Broadcast(Overlay);

	if (!bUseRealDirectorAPI)
	{
		LastDirectorDecision.SummaryText = TEXT("Director API is not configured. Call ConfigureDirectorAPI before submitting conversation summaries.");
		return;
	}

	SendConversationSummaryToDirectorModel(Summary);
}

void UAstroDirectorSubsystem::SubmitPeriodicTick(const FAstroDirectorTickPayload& TickPayload)
{
	// 当前最小版本先把周期性信息整理成导演摘要文本，供调试面板或后续真实大模型接入使用。
	LastDirectorDecision.SummaryText = UAstroDirectorPromptBuilder::BuildDirectorPrompt(TickPayload, RecentConversationSummaries, CurrentWorldOverlay);
	LastDirectorDecision.WorldOverlay = CurrentWorldOverlay;
}

FAstroRuntimeCharacterOverlay UAstroDirectorSubsystem::GetCharacterOverlayForNPC(const FString& NPCId) const
{
	if (const FAstroRuntimeCharacterOverlay* FoundOverlay = CharacterOverlays.Find(NPCId))
	{
		return *FoundOverlay;
	}

	FAstroRuntimeCharacterOverlay EmptyOverlay;
	EmptyOverlay.NPCId = NPCId;
	return EmptyOverlay;
}

FAstroRuntimeWorldOverlay UAstroDirectorSubsystem::GetCurrentWorldOverlay() const
{
	return CurrentWorldOverlay;
}

FAstroToolExecutionResult UAstroDirectorSubsystem::ExecuteDirectorToolCall(const FAstroToolCall& ToolCall)
{
	if (ToolExecutor == nullptr)
	{
		FAstroToolExecutionResult Result;
		Result.bSuccess = false;
		Result.ResultMessage = TEXT("Director tool executor is missing.");
		Result.ResultJson = TEXT("{\"success\":false,\"reason\":\"missing_executor\"}");
		return Result;
	}

	FAstroToolExecutionResult Result = ToolExecutor->ExecuteDirectorToolCall(ToolCall, CurrentWorldOverlay);
	LastDirectorDecision.WorldOverlay = CurrentWorldOverlay;
	LastDirectorDecision.ToolCalls = { ToolCall };
	return Result;
}

void UAstroDirectorSubsystem::ApplyHeuristicOverlayUpdate(const FAstroNPCConversationSummary& Summary)
{
	FAstroRuntimeCharacterOverlay& Overlay = CharacterOverlays.FindOrAdd(Summary.NPCId);
	Overlay.NPCId = Summary.NPCId;

	// 当前最小版本用启发式规则模拟导演总结结果，先建立运行时人格补丁链路。
	const FString LowerPlayerMessage = Summary.LastPlayerMessage.ToLower();
	if (LowerPlayerMessage.Contains(TEXT("谢谢")) || LowerPlayerMessage.Contains(TEXT("thank")))
	{
		Overlay.AffinityScore += 1;
		Overlay.RelationshipSummary = TEXT("The player recently showed gratitude.");
		Overlay.AdditionalStyleText = TEXT("The NPC is slightly warmer toward the player than before.");
	}

	if (LowerPlayerMessage.Contains(TEXT("帮")) || LowerPlayerMessage.Contains(TEXT("help")))
	{
		Overlay.RecentMemorySummary = TEXT("The player recently asked for help or sought support from this NPC.");
		Overlay.AdditionalPersonaText = TEXT("Remember that the player sees this NPC as a potential source of guidance.");
	}

	if (Overlay.RelationshipSummary.IsEmpty() && Overlay.RecentMemorySummary.IsEmpty())
	{
		Overlay.RecentMemorySummary = TEXT("No major relationship change detected in the latest interaction.");
	}
}

void UAstroDirectorSubsystem::SendConversationSummaryToDirectorModel(const FAstroNPCConversationSummary& Summary)
{
	FAstroRuntimeCharacterOverlay CurrentOverlay = GetCharacterOverlayForNPC(Summary.NPCId);
	CurrentOverlay.NPCId = Summary.NPCId;

	FString PayloadJson;
	if (!AstroDirectorSubsystemInternal::BuildDirectorRequestPayload(Summary, CurrentOverlay, CurrentWorldOverlay, PayloadJson))
	{
		LastDirectorDecision.SummaryText = TEXT("Failed to build director request payload.");
		return;
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), DirectorModelName);

	TArray<TSharedPtr<FJsonValue>> Messages;
	{
		TSharedRef<FJsonObject> SystemMessage = MakeShared<FJsonObject>();
		SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
		SystemMessage->SetStringField(TEXT("content"), TEXT("You are a game director AI. Analyze the provided NPC interaction data and decide whether the NPC's runtime relationship overlay should change. Always respond with strict JSON only. Use the field 'character_overlay_update' to describe changes. If no update is needed, return should_update=false."));
		Messages.Add(MakeShared<FJsonValueObject>(SystemMessage));

		TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
		UserMessage->SetStringField(TEXT("role"), TEXT("user"));
		UserMessage->SetStringField(TEXT("content"), PayloadJson);
		Messages.Add(MakeShared<FJsonValueObject>(UserMessage));
	}
	RootObject->SetArrayField(TEXT("messages"), Messages);
	RootObject->SetNumberField(TEXT("temperature"), 0.2);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		LastDirectorDecision.SummaryText = TEXT("Failed to serialize director request body.");
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(DirectorApiBaseUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *DirectorApiKey));
	Request->SetHeader(TEXT("X-Astro-NPCId"), Summary.NPCId);
	Request->SetContentAsString(RequestBody);
	Request->OnProcessRequestComplete().BindUObject(this, &UAstroDirectorSubsystem::OnDirectorHttpResponseReceived);

	if (!Request->ProcessRequest())
	{
		LastDirectorDecision.SummaryText = TEXT("Failed to start director HTTP request.");
	}
}

void UAstroDirectorSubsystem::OnDirectorHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		LastDirectorDecision.SummaryText = TEXT("Director HTTP request failed or response is invalid.");
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		LastDirectorDecision.SummaryText = FString::Printf(TEXT("Director HTTP error. Status=%d Body=%s"), Response->GetResponseCode(), *Response->GetContentAsString());
		return;
	}

	const FString NPCId = Request.IsValid() ? Request->GetHeader(TEXT("X-Astro-NPCId")) : TEXT("UnknownNPC");
	FAstroRuntimeCharacterOverlay Overlay = GetCharacterOverlayForNPC(NPCId);
	Overlay.NPCId = NPCId;

	const FString AssistantContent = AstroDirectorSubsystemInternal::ExtractAssistantReplyFromOpenAIResponse(Response->GetContentAsString());
	FString SummaryText;
	if (!AstroDirectorSubsystemInternal::ParseDirectorOverlayResponse(AssistantContent, Overlay, SummaryText))
	{
		LastDirectorDecision.SummaryText = FString::Printf(TEXT("Director response parse failed. RawContent=%s"), *AssistantContent);
		return;
	}

	CharacterOverlays.Add(NPCId, Overlay);
	LastDirectorDecision.SummaryText = SummaryText.IsEmpty()
		? FString::Printf(TEXT("Director summary processed for NPC: %s"), *NPCId)
		: SummaryText;
	OnCharacterOverlayUpdated.Broadcast(Overlay);
}
