#include "AstroNPCComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "AstroPromptBuilder.h"
#include "CharacterCardAsset.h"
#include "WorldBookAsset.h"

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
	// TODO: 在项目设置中接入真实 HTTP/SDK 请求实现。
	// 当前先用本地桩回复，保证 Runtime 链路可跑通。
	if (PromptText.IsEmpty())
	{
		return;
	}

	HandleModelReply(TEXT("[Stub Reply] Request pipeline is ready. Connect your LLM service in SendRequestToModel."));
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
