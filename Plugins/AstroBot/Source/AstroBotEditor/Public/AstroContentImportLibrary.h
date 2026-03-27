#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstroContentImportLibrary.generated.h"

UENUM(BlueprintType)
enum class EAstroImportFileFormat : uint8
{
	Json,
	Csv
};

UENUM(BlueprintType)
enum class EAstroImportContentType : uint8
{
	CharacterCard,
	WorldBook
};

UCLASS()
class ASTROBOTEDITOR_API UAstroContentImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 从单个外部文件导入角色卡到现有 Asset，支持 JSON 与 CSV。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Editor|Import")
	static bool ImportCharacterCardFromFile(const FString& SourceFilePath, EAstroImportFileFormat FileFormat, class UCharacterCardAsset* TargetAsset, FString& OutMessage);

	// 从单个外部文件导入世界书到现有 Asset，支持 JSON 与 CSV。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Editor|Import")
	static bool ImportWorldBookFromFile(const FString& SourceFilePath, EAstroImportFileFormat FileFormat, class UWorldBookAsset* TargetAsset, FString& OutMessage);

	// 从单个文件批量创建或更新多个角色卡/世界书 Asset。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Editor|Import")
	static bool BatchCreateOrUpdateAssetsFromFile(const FString& SourceFilePath, EAstroImportFileFormat FileFormat, EAstroImportContentType ContentType, const FString& TargetDirectory, bool bSaveAssets, FString& OutMessage);
};
