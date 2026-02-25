#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldBookAsset.generated.h"

USTRUCT(BlueprintType)
struct ASTROBOT_API FWorldBookEntry
{
	GENERATED_BODY()

	// 用于简单召回匹配的关键词。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|World Book")
	TArray<FString> Keywords;

	// 命中后注入 Prompt 的世界设定内容。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|World Book", meta = (MultiLine = "true"))
	FString Content;
};

UCLASS(BlueprintType)
class ASTROBOT_API UWorldBookAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 可用于召回的全部世界书条目。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AstroBot|World Book")
	TArray<FWorldBookEntry> Entries;

	// 按关键词命中数排序，返回 TopK 条目。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|World Book")
	TArray<FWorldBookEntry> RetrieveEntriesByKeyword(const FString& QueryText, int32 TopK = 3) const;
};
