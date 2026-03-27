#include "AstroDirectorPromptBuilder.h"

FString UAstroDirectorPromptBuilder::BuildDirectorPrompt(
	const FAstroDirectorTickPayload& TickPayload,
	const TArray<FAstroNPCConversationSummary>& RecentSummaries,
	const FAstroRuntimeWorldOverlay& CurrentWorldOverlay)
{
	// 当前阶段先构建人可读的导演输入，后续如果接真实大模型，可直接复用或升级成消息数组结构。
	FString Prompt;
	Prompt += TEXT("# Director Runtime State\n");
	Prompt += FString::Printf(TEXT("RegionId: %s\n"), *TickPayload.RegionId);
	Prompt += FString::Printf(TEXT("StoryProgress: %s\n"), *TickPayload.StoryProgressSummary);
	Prompt += FString::Printf(TEXT("ExtraRuntimeSummary: %s\n\n"), *TickPayload.ExtraRuntimeSummary);

	Prompt += TEXT("# Player Snapshot\n");
	Prompt += FString::Printf(TEXT("PlayerName: %s\n"), *TickPayload.PlayerSnapshot.PlayerName);
	Prompt += FString::Printf(TEXT("LevelName: %s\n"), *TickPayload.PlayerSnapshot.LevelName);
	Prompt += FString::Printf(TEXT("CurrentQuest: %s\n\n"), *TickPayload.PlayerSnapshot.CurrentQuest);

	Prompt += TEXT("# Current World Overlay\n");
	Prompt += FString::Printf(TEXT("WeatherState: %s\n"), *CurrentWorldOverlay.WeatherState);
	Prompt += FString::Printf(TEXT("ActiveEventSummary: %s\n"), *CurrentWorldOverlay.ActiveEventSummary);
	Prompt += FString::Printf(TEXT("DirectorNotes: %s\n\n"), *CurrentWorldOverlay.DirectorNotes);

	Prompt += TEXT("# Recent NPC Interaction Summaries\n");
	if (RecentSummaries.IsEmpty())
	{
		Prompt += TEXT("No recent NPC summaries.\n");
	}
	else
	{
		for (const FAstroNPCConversationSummary& Summary : RecentSummaries)
		{
			Prompt += FString::Printf(TEXT("NPCId: %s\n"), *Summary.NPCId);
			Prompt += FString::Printf(TEXT("NPCDisplayName: %s\n"), *Summary.NPCDisplayName);
			Prompt += FString::Printf(TEXT("LastPlayerMessage: %s\n"), *Summary.LastPlayerMessage);
			Prompt += FString::Printf(TEXT("LastNPCReply: %s\n\n"), *Summary.LastNPCReply);
		}
	}

	return Prompt;
}
