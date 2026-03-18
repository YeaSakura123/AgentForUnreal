#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstroJsonImportLibrary.generated.h"

class UCharacterCardAsset;
class UWorldBookAsset;

UCLASS()
class ASTROBOTEDITOR_API UAstroJsonImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 从外部 JSON 文件导入角色卡内容到现有 Asset。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Editor|Import")
	static bool ImportCharacterCardFromJsonFile(const FString& JsonFilePath, UCharacterCardAsset* TargetAsset, FString& OutMessage);

	// 从外部 JSON 文件导入世界书内容到现有 Asset。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Editor|Import")
	static bool ImportWorldBookFromJsonFile(const FString& JsonFilePath, UWorldBookAsset* TargetAsset, FString& OutMessage);
};
