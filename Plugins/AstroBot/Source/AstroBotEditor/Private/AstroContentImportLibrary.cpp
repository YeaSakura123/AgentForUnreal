#include "AstroContentImportLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CharacterCardAsset.h"
#include "Dom/JsonObject.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"
#include "Serialization/Csv/CsvParser.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "WorldBookAsset.h"

namespace AstroContentImportLibraryInternal
{
	struct FImportedCharacterCardData
	{
		FString AssetName;
		FString PersonaText;
		FString StyleText;
		FString ForbiddenText;
	};

	struct FImportedWorldBookData
	{
		FString AssetName;
		TArray<FWorldBookEntry> Entries;
	};

	// 统一读取文本文件，便于 JSON 与 CSV 复用同一套错误处理。
	static bool LoadSourceText(const FString& SourceFilePath, FString& OutText, FString& OutMessage)
	{
		if (SourceFilePath.IsEmpty())
		{
			OutMessage = TEXT("源文件路径为空。");
			return false;
		}

		if (!FFileHelper::LoadFileToString(OutText, *SourceFilePath))
		{
			OutMessage = FString::Printf(TEXT("无法读取源文件：%s"), *SourceFilePath);
			return false;
		}

		return true;
	}

	// 统一读取 JSON 根对象，后续解析逻辑只关心字段结构。
	static bool LoadJsonRootObject(const FString& SourceFilePath, TSharedPtr<FJsonObject>& OutRootObject, FString& OutMessage)
	{
		FString SourceText;
		if (!LoadSourceText(SourceFilePath, SourceText, OutMessage))
		{
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SourceText);
		if (!FJsonSerializer::Deserialize(Reader, OutRootObject) || !OutRootObject.IsValid())
		{
			OutMessage = FString::Printf(TEXT("JSON 解析失败：%s"), *SourceFilePath);
			return false;
		}

		return true;
	}

	// 兼容 snake_case 与 camelCase，减少外部文件格式绑定。
	static FString GetStringFieldOrDefault(const TSharedPtr<FJsonObject>& JsonObject, const FString& SnakeCaseKey, const FString& CamelCaseKey)
	{
		if (!JsonObject.IsValid())
		{
			return TEXT("");
		}

		FString Value;
		if (JsonObject->TryGetStringField(SnakeCaseKey, Value))
		{
			return Value;
		}

		if (!CamelCaseKey.IsEmpty() && JsonObject->TryGetStringField(CamelCaseKey, Value))
		{
			return Value;
		}

		return TEXT("");
	}

	// 将 CSV 表头映射成列索引，方便后续按列名读取。
	static TMap<FString, int32> BuildCsvHeaderMap(const FCsvParser& Parser)
	{
		TMap<FString, int32> HeaderMap;
		const FCsvParser::FRows& Rows = Parser.GetRows();
		if (Rows.IsEmpty())
		{
			return HeaderMap;
		}

		const TArray<const TCHAR*>& HeaderRow = Rows[0];
		for (int32 ColumnIndex = 0; ColumnIndex < HeaderRow.Num(); ++ColumnIndex)
		{
			HeaderMap.Add(FString(HeaderRow[ColumnIndex]).TrimStartAndEnd(), ColumnIndex);
		}

		return HeaderMap;
	}

	// 按列名读取 CSV 单元格，缺列时返回空字符串。
	static FString GetCsvCell(const TArray<const TCHAR*>& Row, const TMap<FString, int32>& HeaderMap, const FString& ColumnName)
	{
		const int32* ColumnIndex = HeaderMap.Find(ColumnName);
		if (ColumnIndex == nullptr || !Row.IsValidIndex(*ColumnIndex))
		{
			return TEXT("");
		}

		return FString(Row[*ColumnIndex]).TrimStartAndEnd();
	}

	// 将 "a|b|c" 形式的关键词列拆成关键词数组。
	static TArray<FString> ParseKeywordList(const FString& KeywordsText)
	{
		TArray<FString> Keywords;
		KeywordsText.ParseIntoArray(Keywords, TEXT("|"), true);

		for (FString& Keyword : Keywords)
		{
			Keyword = Keyword.TrimStartAndEnd();
		}

		Keywords.RemoveAll([](const FString& Keyword)
		{
			return Keyword.IsEmpty();
		});

		return Keywords;
	}

	// 将任意文本转换成可创建 Asset 的安全命名。
	static FString SanitizeAssetName(const FString& RawName)
	{
		FString SafeName = RawName.TrimStartAndEnd();
		SafeName.ReplaceInline(TEXT(" "), TEXT("_"));
		SafeName.ReplaceInline(TEXT("-"), TEXT("_"));
		SafeName.ReplaceInline(TEXT("."), TEXT("_"));
		return ObjectTools::SanitizeObjectName(SafeName);
	}

	// 规范化目标目录，兼容用户输入的 "/Game" 路径与内容浏览器常见格式。
	static FString NormalizeTargetDirectory(const FString& TargetDirectory)
	{
		FString Normalized = TargetDirectory.TrimStartAndEnd();
		if (Normalized.IsEmpty())
		{
			return TEXT("/Game/AstroBot/Imported");
		}

		if (!Normalized.StartsWith(TEXT("/Game")))
		{
			if (Normalized.StartsWith(TEXT("Game")))
			{
				Normalized = TEXT("/") + Normalized;
			}
			else
			{
				Normalized = TEXT("/Game/") + Normalized;
			}
		}

		Normalized.RemoveFromEnd(TEXT("/"));
		return Normalized;
	}

	// 查找指定路径下是否已有同名 Asset，用于更新已有资源。
	static UObject* FindExistingAsset(const FString& PackagePath)
	{
		return StaticLoadObject(UObject::StaticClass(), nullptr, *PackagePath);
	}

	// 创建 DataAsset 时复用同一套工厂逻辑，降低不同资产类型的创建分支复杂度。
	static UObject* CreateDataAsset(UClass* AssetClass, const FString& TargetDirectory, const FString& AssetName, FString& OutMessage)
	{
		if (AssetClass == nullptr)
		{
			OutMessage = TEXT("创建资产失败：AssetClass 为空。");
			return nullptr;
		}

		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = AssetClass;

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		return AssetToolsModule.Get().CreateAsset(AssetName, TargetDirectory, AssetClass, Factory);
	}

	// 批量导入时优先读取 asset_name，缺失时再用文件名或默认命名兜底。
	static FString ResolveCharacterCardAssetName(const TSharedPtr<FJsonObject>& JsonObject, const FString& FallbackName)
	{
		const FString RawName = GetStringFieldOrDefault(JsonObject, TEXT("asset_name"), TEXT("assetName"));
		return SanitizeAssetName(RawName.IsEmpty() ? FallbackName : RawName);
	}

	static FString ResolveWorldBookAssetName(const TSharedPtr<FJsonObject>& JsonObject, const FString& FallbackName)
	{
		const FString RawName = GetStringFieldOrDefault(JsonObject, TEXT("asset_name"), TEXT("assetName"));
		return SanitizeAssetName(RawName.IsEmpty() ? FallbackName : RawName);
	}

	static bool ParseCharacterCardsFromJson(const FString& SourceFilePath, TArray<FImportedCharacterCardData>& OutItems, FString& OutMessage)
	{
		TSharedPtr<FJsonObject> RootObject;
		if (!LoadJsonRootObject(SourceFilePath, RootObject, OutMessage))
		{
			return false;
		}

		const FString FileBaseName = FPaths::GetBaseFilename(SourceFilePath);
		const TArray<TSharedPtr<FJsonValue>>* ItemValues = nullptr;
		if (RootObject->TryGetArrayField(TEXT("items"), ItemValues) && ItemValues != nullptr)
		{
			for (int32 ItemIndex = 0; ItemIndex < ItemValues->Num(); ++ItemIndex)
			{
				const TSharedPtr<FJsonObject>* ItemObject = nullptr;
				if (!(*ItemValues)[ItemIndex].IsValid() || !(*ItemValues)[ItemIndex]->TryGetObject(ItemObject) || ItemObject == nullptr)
				{
					OutMessage = FString::Printf(TEXT("角色卡 JSON 的第 %d 项不是合法对象。"), ItemIndex);
					return false;
				}

				FImportedCharacterCardData Item;
				Item.AssetName = ResolveCharacterCardAssetName(*ItemObject, FString::Printf(TEXT("%s_%d"), *FileBaseName, ItemIndex));
				Item.PersonaText = GetStringFieldOrDefault(*ItemObject, TEXT("persona_text"), TEXT("personaText"));
				Item.StyleText = GetStringFieldOrDefault(*ItemObject, TEXT("style_text"), TEXT("styleText"));
				Item.ForbiddenText = GetStringFieldOrDefault(*ItemObject, TEXT("forbidden_text"), TEXT("forbiddenText"));
				OutItems.Add(MoveTemp(Item));
			}

			return true;
		}

		FImportedCharacterCardData Item;
		Item.AssetName = ResolveCharacterCardAssetName(RootObject, FileBaseName);
		Item.PersonaText = GetStringFieldOrDefault(RootObject, TEXT("persona_text"), TEXT("personaText"));
		Item.StyleText = GetStringFieldOrDefault(RootObject, TEXT("style_text"), TEXT("styleText"));
		Item.ForbiddenText = GetStringFieldOrDefault(RootObject, TEXT("forbidden_text"), TEXT("forbiddenText"));
		OutItems.Add(MoveTemp(Item));
		return true;
	}

	static bool ParseWorldBooksFromJson(const FString& SourceFilePath, TArray<FImportedWorldBookData>& OutItems, FString& OutMessage)
	{
		TSharedPtr<FJsonObject> RootObject;
		if (!LoadJsonRootObject(SourceFilePath, RootObject, OutMessage))
		{
			return false;
		}

		const FString FileBaseName = FPaths::GetBaseFilename(SourceFilePath);
		const TArray<TSharedPtr<FJsonValue>>* ItemValues = nullptr;
		TArray<TSharedPtr<FJsonObject>> WorldBookObjects;
		if (RootObject->TryGetArrayField(TEXT("items"), ItemValues) && ItemValues != nullptr)
		{
			for (int32 ItemIndex = 0; ItemIndex < ItemValues->Num(); ++ItemIndex)
			{
				const TSharedPtr<FJsonObject>* ItemObject = nullptr;
				if (!(*ItemValues)[ItemIndex].IsValid() || !(*ItemValues)[ItemIndex]->TryGetObject(ItemObject) || ItemObject == nullptr)
				{
					OutMessage = FString::Printf(TEXT("世界书 JSON 的第 %d 项不是合法对象。"), ItemIndex);
					return false;
				}
				WorldBookObjects.Add(*ItemObject);
			}
		}
		else
		{
			WorldBookObjects.Add(RootObject);
		}

		for (int32 ItemIndex = 0; ItemIndex < WorldBookObjects.Num(); ++ItemIndex)
		{
			const TSharedPtr<FJsonObject>& WorldBookObject = WorldBookObjects[ItemIndex];
			const TArray<TSharedPtr<FJsonValue>>* EntryValues = nullptr;
			if (!WorldBookObject->TryGetArrayField(TEXT("entries"), EntryValues) || EntryValues == nullptr)
			{
				OutMessage = FString::Printf(TEXT("世界书第 %d 项缺少 entries 数组。"), ItemIndex);
				return false;
			}

			FImportedWorldBookData Item;
			Item.AssetName = ResolveWorldBookAssetName(WorldBookObject, FString::Printf(TEXT("%s_%d"), *FileBaseName, ItemIndex));

			for (int32 EntryIndex = 0; EntryIndex < EntryValues->Num(); ++EntryIndex)
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				if (!(*EntryValues)[EntryIndex].IsValid() || !(*EntryValues)[EntryIndex]->TryGetObject(EntryObject) || EntryObject == nullptr)
				{
					OutMessage = FString::Printf(TEXT("世界书第 %d 项中的第 %d 条条目不是合法对象。"), ItemIndex, EntryIndex);
					return false;
				}

				const TArray<TSharedPtr<FJsonValue>>* KeywordValues = nullptr;
				if (!(*EntryObject)->TryGetArrayField(TEXT("keywords"), KeywordValues) || KeywordValues == nullptr)
				{
					OutMessage = FString::Printf(TEXT("世界书第 %d 项中的第 %d 条缺少 keywords 数组。"), ItemIndex, EntryIndex);
					return false;
				}

				FWorldBookEntry Entry;
				for (const TSharedPtr<FJsonValue>& KeywordValue : *KeywordValues)
				{
					if (KeywordValue.IsValid())
					{
						const FString Keyword = KeywordValue->AsString().TrimStartAndEnd();
						if (!Keyword.IsEmpty())
						{
							Entry.Keywords.Add(Keyword);
						}
					}
				}

				Entry.Content = GetStringFieldOrDefault(*EntryObject, TEXT("content"), TEXT("content"));
				Item.Entries.Add(MoveTemp(Entry));
			}

			OutItems.Add(MoveTemp(Item));
		}

		return true;
	}

	static bool ParseCharacterCardsFromCsv(const FString& SourceFilePath, TArray<FImportedCharacterCardData>& OutItems, FString& OutMessage)
	{
		FString SourceText;
		if (!LoadSourceText(SourceFilePath, SourceText, OutMessage))
		{
			return false;
		}

		FCsvParser Parser(SourceText);
		const FCsvParser::FRows& Rows = Parser.GetRows();
		if (Rows.Num() <= 1)
		{
			OutMessage = TEXT("角色卡 CSV 至少需要表头和一行数据。");
			return false;
		}

		const TMap<FString, int32> HeaderMap = BuildCsvHeaderMap(Parser);
		for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
		{
			const TArray<const TCHAR*>& Row = Rows[RowIndex];
			FImportedCharacterCardData Item;
			Item.AssetName = SanitizeAssetName(GetCsvCell(Row, HeaderMap, TEXT("asset_name")));
			if (Item.AssetName.IsEmpty())
			{
				Item.AssetName = FString::Printf(TEXT("CharacterCard_%d"), RowIndex);
			}

			Item.PersonaText = GetCsvCell(Row, HeaderMap, TEXT("persona_text"));
			Item.StyleText = GetCsvCell(Row, HeaderMap, TEXT("style_text"));
			Item.ForbiddenText = GetCsvCell(Row, HeaderMap, TEXT("forbidden_text"));
			OutItems.Add(MoveTemp(Item));
		}

		return true;
	}

	static bool ParseWorldBooksFromCsv(const FString& SourceFilePath, TArray<FImportedWorldBookData>& OutItems, FString& OutMessage)
	{
		FString SourceText;
		if (!LoadSourceText(SourceFilePath, SourceText, OutMessage))
		{
			return false;
		}

		FCsvParser Parser(SourceText);
		const FCsvParser::FRows& Rows = Parser.GetRows();
		if (Rows.Num() <= 1)
		{
			OutMessage = TEXT("世界书 CSV 至少需要表头和一行数据。");
			return false;
		}

		const TMap<FString, int32> HeaderMap = BuildCsvHeaderMap(Parser);
		TMap<FString, int32> AssetNameToIndex;

		for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
		{
			const TArray<const TCHAR*>& Row = Rows[RowIndex];
			FString AssetName = SanitizeAssetName(GetCsvCell(Row, HeaderMap, TEXT("asset_name")));
			if (AssetName.IsEmpty())
			{
				AssetName = TEXT("WorldBook_Default");
			}

			int32* ExistingIndex = AssetNameToIndex.Find(AssetName);
			if (ExistingIndex == nullptr)
			{
				FImportedWorldBookData NewItem;
				NewItem.AssetName = AssetName;
				AssetNameToIndex.Add(AssetName, OutItems.Add(MoveTemp(NewItem)));
				ExistingIndex = AssetNameToIndex.Find(AssetName);
			}

			FWorldBookEntry Entry;
			Entry.Keywords = ParseKeywordList(GetCsvCell(Row, HeaderMap, TEXT("keywords")));
			Entry.Content = GetCsvCell(Row, HeaderMap, TEXT("content"));
			OutItems[*ExistingIndex].Entries.Add(MoveTemp(Entry));
		}

		return true;
	}

	static bool ParseCharacterCards(const FString& SourceFilePath, const EAstroImportFileFormat FileFormat, TArray<FImportedCharacterCardData>& OutItems, FString& OutMessage)
	{
		return FileFormat == EAstroImportFileFormat::Json
			? ParseCharacterCardsFromJson(SourceFilePath, OutItems, OutMessage)
			: ParseCharacterCardsFromCsv(SourceFilePath, OutItems, OutMessage);
	}

	static bool ParseWorldBooks(const FString& SourceFilePath, const EAstroImportFileFormat FileFormat, TArray<FImportedWorldBookData>& OutItems, FString& OutMessage)
	{
		return FileFormat == EAstroImportFileFormat::Json
			? ParseWorldBooksFromJson(SourceFilePath, OutItems, OutMessage)
			: ParseWorldBooksFromCsv(SourceFilePath, OutItems, OutMessage);
	}

	static void ApplyCharacterCardData(UCharacterCardAsset* TargetAsset, const FImportedCharacterCardData& Data)
	{
		TargetAsset->Modify();
		TargetAsset->PersonaText = Data.PersonaText;
		TargetAsset->StyleText = Data.StyleText;
		TargetAsset->ForbiddenText = Data.ForbiddenText;
		TargetAsset->MarkPackageDirty();
	}

	static void ApplyWorldBookData(UWorldBookAsset* TargetAsset, const FImportedWorldBookData& Data)
	{
		TargetAsset->Modify();
		TargetAsset->Entries = Data.Entries;
		TargetAsset->MarkPackageDirty();
	}

	static bool SaveAssetIfNeeded(UObject* Asset, const bool bSaveAssets, FString& OutMessage)
	{
		if (!bSaveAssets || Asset == nullptr)
		{
			return true;
		}

		UPackage* Package = Asset->GetOutermost();
		if (Package == nullptr)
		{
			OutMessage = TEXT("保存资产失败：Package 为空。");
			return false;
		}

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

		// 新版 UE 的 SavePackage 使用 FSavePackageArgs 传递保存选项。
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
	}

	static UCharacterCardAsset* FindOrCreateCharacterCardAsset(const FString& TargetDirectory, const FString& AssetName, FString& OutMessage)
	{
		const FString PackagePath = TargetDirectory / AssetName;
		if (UCharacterCardAsset* ExistingAsset = Cast<UCharacterCardAsset>(FindExistingAsset(PackagePath)))
		{
			return ExistingAsset;
		}

		return Cast<UCharacterCardAsset>(CreateDataAsset(UCharacterCardAsset::StaticClass(), TargetDirectory, AssetName, OutMessage));
	}

	static UWorldBookAsset* FindOrCreateWorldBookAsset(const FString& TargetDirectory, const FString& AssetName, FString& OutMessage)
	{
		const FString PackagePath = TargetDirectory / AssetName;
		if (UWorldBookAsset* ExistingAsset = Cast<UWorldBookAsset>(FindExistingAsset(PackagePath)))
		{
			return ExistingAsset;
		}

		return Cast<UWorldBookAsset>(CreateDataAsset(UWorldBookAsset::StaticClass(), TargetDirectory, AssetName, OutMessage));
	}

	static bool BatchCreateOrUpdateCharacterCards(const TArray<FImportedCharacterCardData>& Items, const FString& TargetDirectory, const bool bSaveAssets, FString& OutMessage)
	{
		for (const FImportedCharacterCardData& Item : Items)
		{
			FString ErrorMessage;
			UCharacterCardAsset* TargetAsset = FindOrCreateCharacterCardAsset(TargetDirectory, Item.AssetName, ErrorMessage);
			if (TargetAsset == nullptr)
			{
				OutMessage = ErrorMessage.IsEmpty()
					? FString::Printf(TEXT("无法创建或更新角色卡：%s"), *Item.AssetName)
					: ErrorMessage;
				return false;
			}

			ApplyCharacterCardData(TargetAsset, Item);
			if (!SaveAssetIfNeeded(TargetAsset, bSaveAssets, ErrorMessage))
			{
				OutMessage = ErrorMessage.IsEmpty()
					? FString::Printf(TEXT("角色卡保存失败：%s"), *Item.AssetName)
					: ErrorMessage;
				return false;
			}
		}

		OutMessage = FString::Printf(TEXT("角色卡批量导入完成，共 %d 个。"), Items.Num());
		return true;
	}

	static bool BatchCreateOrUpdateWorldBooks(const TArray<FImportedWorldBookData>& Items, const FString& TargetDirectory, const bool bSaveAssets, FString& OutMessage)
	{
		for (const FImportedWorldBookData& Item : Items)
		{
			FString ErrorMessage;
			UWorldBookAsset* TargetAsset = FindOrCreateWorldBookAsset(TargetDirectory, Item.AssetName, ErrorMessage);
			if (TargetAsset == nullptr)
			{
				OutMessage = ErrorMessage.IsEmpty()
					? FString::Printf(TEXT("无法创建或更新世界书：%s"), *Item.AssetName)
					: ErrorMessage;
				return false;
			}

			ApplyWorldBookData(TargetAsset, Item);
			if (!SaveAssetIfNeeded(TargetAsset, bSaveAssets, ErrorMessage))
			{
				OutMessage = ErrorMessage.IsEmpty()
					? FString::Printf(TEXT("世界书保存失败：%s"), *Item.AssetName)
					: ErrorMessage;
				return false;
			}
		}

		OutMessage = FString::Printf(TEXT("世界书批量导入完成，共 %d 个。"), Items.Num());
		return true;
	}
}

bool UAstroContentImportLibrary::ImportCharacterCardFromFile(const FString& SourceFilePath, const EAstroImportFileFormat FileFormat, UCharacterCardAsset* TargetAsset, FString& OutMessage)
{
	if (TargetAsset == nullptr)
	{
		OutMessage = TEXT("目标 CharacterCardAsset 为空。");
		return false;
	}

	TArray<AstroContentImportLibraryInternal::FImportedCharacterCardData> Items;
	if (!AstroContentImportLibraryInternal::ParseCharacterCards(SourceFilePath, FileFormat, Items, OutMessage))
	{
		return false;
	}

	if (Items.IsEmpty())
	{
		OutMessage = TEXT("角色卡导入失败：未解析到有效数据。");
		return false;
	}

	// 单资产导入只使用第一条记录，便于兼容现有编辑器工具的使用方式。
	AstroContentImportLibraryInternal::ApplyCharacterCardData(TargetAsset, Items[0]);
	OutMessage = FString::Printf(TEXT("角色卡导入完成：%s"), *TargetAsset->GetName());
	return true;
}

bool UAstroContentImportLibrary::ImportWorldBookFromFile(const FString& SourceFilePath, const EAstroImportFileFormat FileFormat, UWorldBookAsset* TargetAsset, FString& OutMessage)
{
	if (TargetAsset == nullptr)
	{
		OutMessage = TEXT("目标 WorldBookAsset 为空。");
		return false;
	}

	TArray<AstroContentImportLibraryInternal::FImportedWorldBookData> Items;
	if (!AstroContentImportLibraryInternal::ParseWorldBooks(SourceFilePath, FileFormat, Items, OutMessage))
	{
		return false;
	}

	if (Items.IsEmpty())
	{
		OutMessage = TEXT("世界书导入失败：未解析到有效数据。");
		return false;
	}

	AstroContentImportLibraryInternal::ApplyWorldBookData(TargetAsset, Items[0]);
	OutMessage = FString::Printf(TEXT("世界书导入完成：%s，共 %d 条。"), *TargetAsset->GetName(), TargetAsset->Entries.Num());
	return true;
}

bool UAstroContentImportLibrary::BatchCreateOrUpdateAssetsFromFile(const FString& SourceFilePath, const EAstroImportFileFormat FileFormat, const EAstroImportContentType ContentType, const FString& TargetDirectory, const bool bSaveAssets, FString& OutMessage)
{
	const FString NormalizedDirectory = AstroContentImportLibraryInternal::NormalizeTargetDirectory(TargetDirectory);

	if (ContentType == EAstroImportContentType::CharacterCard)
	{
		TArray<AstroContentImportLibraryInternal::FImportedCharacterCardData> Items;
		if (!AstroContentImportLibraryInternal::ParseCharacterCards(SourceFilePath, FileFormat, Items, OutMessage))
		{
			return false;
		}

		return AstroContentImportLibraryInternal::BatchCreateOrUpdateCharacterCards(Items, NormalizedDirectory, bSaveAssets, OutMessage);
	}

	TArray<AstroContentImportLibraryInternal::FImportedWorldBookData> Items;
	if (!AstroContentImportLibraryInternal::ParseWorldBooks(SourceFilePath, FileFormat, Items, OutMessage))
	{
		return false;
	}

	return AstroContentImportLibraryInternal::BatchCreateOrUpdateWorldBooks(Items, NormalizedDirectory, bSaveAssets, OutMessage);
}
