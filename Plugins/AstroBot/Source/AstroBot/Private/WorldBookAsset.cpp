#include "WorldBookAsset.h"

#include "Algo/Sort.h"

namespace AstroBotWorldBook
{
	// 统计单条目在查询文本中的关键词命中数。
	static int32 CountKeywordMatches(const FWorldBookEntry& Entry, const FString& QueryLower)
	{
		int32 Score = 0;
		for (const FString& Keyword : Entry.Keywords)
		{
			if (!Keyword.IsEmpty() && QueryLower.Contains(Keyword.ToLower()))
			{
				++Score;
			}
		}

		return Score;
	}
}

TArray<FWorldBookEntry> UWorldBookAsset::RetrieveEntriesByKeyword(const FString& QueryText, int32 TopK) const
{
	// 先使用简单策略：按关键词命中数排序。
	if (TopK <= 0 || Entries.IsEmpty())
	{
		return {};
	}

	const FString QueryLower = QueryText.ToLower();
	TArray<TPair<int32, int32>> ScoredEntryIndexes;
	ScoredEntryIndexes.Reserve(Entries.Num());

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const int32 Score = AstroBotWorldBook::CountKeywordMatches(Entries[EntryIndex], QueryLower);
		if (Score > 0)
		{
			ScoredEntryIndexes.Emplace(EntryIndex, Score);
		}
	}

	Algo::Sort(ScoredEntryIndexes, [](const TPair<int32, int32>& Left, const TPair<int32, int32>& Right)
	{
		return Left.Value > Right.Value;
	});

	const int32 ResultCount = FMath::Min(TopK, ScoredEntryIndexes.Num());
	TArray<FWorldBookEntry> Result;
	Result.Reserve(ResultCount);

	for (int32 i = 0; i < ResultCount; ++i)
	{
		Result.Add(Entries[ScoredEntryIndexes[i].Key]);
	}

	return Result;
}
