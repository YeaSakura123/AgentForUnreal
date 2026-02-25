#include "AstroPromptBuilder.h"

#include "CharacterCardAsset.h"

namespace AstroPromptBuilderInternal
{
	static FString ToRoleLabel(const EAstroDialogueRole Role)
	{
		switch (Role)
		{
		case EAstroDialogueRole::System:
			return TEXT("System");
		case EAstroDialogueRole::NPC:
			return TEXT("NPC");
		case EAstroDialogueRole::Player:
		default:
			return TEXT("Player");
		}
	}
}

FString UAstroPromptBuilder::BuildPrompt(
	const UCharacterCardAsset* CharacterCard,
	const TArray<FWorldBookEntry>& RecalledWorldBookEntries,
	const FAstroPlayerSnapshot& PlayerSnapshot,
	const TArray<FAstroDialogueTurn>& ConversationHistory)
{
	// 使用可读性高的分段文本格式，方便后续调 Prompt。
	FString Prompt;
	Prompt.Reserve(4096);

	Prompt += TEXT("# Role Setup\n");
	if (CharacterCard != nullptr)
	{
		Prompt += TEXT("## Persona\n") + CharacterCard->PersonaText + TEXT("\n\n");
		Prompt += TEXT("## Style\n") + CharacterCard->StyleText + TEXT("\n\n");
		Prompt += TEXT("## Forbidden\n") + CharacterCard->ForbiddenText + TEXT("\n\n");
	}
	else
	{
		Prompt += TEXT("CharacterCard is empty. Keep a neutral assistant behavior.\n\n");
	}

	Prompt += TEXT("# World Context\n");
	if (RecalledWorldBookEntries.IsEmpty())
	{
		Prompt += TEXT("No matched world book entries.\n\n");
	}
	else
	{
		for (int32 i = 0; i < RecalledWorldBookEntries.Num(); ++i)
		{
			Prompt += FString::Printf(TEXT("[%d] %s\n"), i + 1, *RecalledWorldBookEntries[i].Content);
		}
		Prompt += TEXT("\n");
	}

	Prompt += TEXT("# Player Snapshot\n");
	Prompt += FString::Printf(TEXT("PlayerName: %s\n"), *PlayerSnapshot.PlayerName);
	Prompt += FString::Printf(TEXT("LevelName: %s\n"), *PlayerSnapshot.LevelName);
	Prompt += FString::Printf(TEXT("Health: %.2f / %.2f\n"), PlayerSnapshot.Health, PlayerSnapshot.MaxHealth);
	Prompt += FString::Printf(TEXT("Stamina: %.2f / %.2f\n"), PlayerSnapshot.Stamina, PlayerSnapshot.MaxStamina);
	Prompt += FString::Printf(TEXT("Mana: %.2f / %.2f\n"), PlayerSnapshot.Mana, PlayerSnapshot.MaxMana);
	Prompt += FString::Printf(TEXT("Location: X=%.2f Y=%.2f Z=%.2f\n"), PlayerSnapshot.Location.X, PlayerSnapshot.Location.Y, PlayerSnapshot.Location.Z);
	Prompt += FString::Printf(TEXT("Velocity: X=%.2f Y=%.2f Z=%.2f\n"), PlayerSnapshot.Velocity.X, PlayerSnapshot.Velocity.Y, PlayerSnapshot.Velocity.Z);
	Prompt += FString::Printf(TEXT("Gold: %d\n"), PlayerSnapshot.Gold);
	Prompt += FString::Printf(TEXT("CurrentQuest: %s\n\n"), *PlayerSnapshot.CurrentQuest);

	Prompt += TEXT("# Conversation History\n");
	if (ConversationHistory.IsEmpty())
	{
		Prompt += TEXT("No conversation yet.\n");
	}
	else
	{
		for (const FAstroDialogueTurn& Turn : ConversationHistory)
		{
			Prompt += FString::Printf(TEXT("%s: %s\n"), *AstroPromptBuilderInternal::ToRoleLabel(Turn.Role), *Turn.Text);
		}
	}

	return Prompt;
}
