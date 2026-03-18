#include "AstroJsonImportLibrary.h"

#include "CharacterCardAsset.h"
#include "WorldBookAsset.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

namespace AstroJsonImportLibraryInternal
{
	// 统一读取并解析 JSON 文件，避免两个导入入口重复处理基础错误。
	static bool LoadJsonRootObjectFromFile(const FString& JsonFilePath, TSharedPtr<FJsonObject>& OutRootObject, FString& OutMessage)
	{
		if (JsonFilePath.IsEmpty())
		{
			OutMessage = TEXT("JSON 文件路径为空。");
			return false;
		}

		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *JsonFilePath))
		{
			OutMessage = FString::Printf(TEXT("无法读取 JSON 文件：%s"), *JsonFilePath);
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, OutRootObject) || !OutRootObject.IsValid())
		{
			OutMessage = FString::Printf(TEXT("JSON 解析失败：%s"), *JsonFilePath);
			return false;
		}

		return true;
	}

	// 兼容 snake_case 与 camelCase 两种键名，降低外部内容格式约束。
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
}

bool UAstroJsonImportLibrary::ImportCharacterCardFromJsonFile(const FString& JsonFilePath, UCharacterCardAsset* TargetAsset, FString& OutMessage)
{
	if (TargetAsset == nullptr)
	{
		OutMessage = TEXT("目标 CharacterCardAsset 为空。");
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	if (!AstroJsonImportLibraryInternal::LoadJsonRootObjectFromFile(JsonFilePath, RootObject, OutMessage))
	{
		return false;
	}

	// 导入前先调用 Modify，确保编辑器撤销/重做与脏标记流程正常工作。
	TargetAsset->Modify();
	TargetAsset->PersonaText = AstroJsonImportLibraryInternal::GetStringFieldOrDefault(RootObject, TEXT("persona_text"), TEXT("personaText"));
	TargetAsset->StyleText = AstroJsonImportLibraryInternal::GetStringFieldOrDefault(RootObject, TEXT("style_text"), TEXT("styleText"));
	TargetAsset->ForbiddenText = AstroJsonImportLibraryInternal::GetStringFieldOrDefault(RootObject, TEXT("forbidden_text"), TEXT("forbiddenText"));
	TargetAsset->MarkPackageDirty();

	OutMessage = FString::Printf(TEXT("角色卡导入完成：%s"), *TargetAsset->GetName());
	return true;
}

bool UAstroJsonImportLibrary::ImportWorldBookFromJsonFile(const FString& JsonFilePath, UWorldBookAsset* TargetAsset, FString& OutMessage)
{
	if (TargetAsset == nullptr)
	{
		OutMessage = TEXT("目标 WorldBookAsset 为空。");
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	if (!AstroJsonImportLibraryInternal::LoadJsonRootObjectFromFile(JsonFilePath, RootObject, OutMessage))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EntryValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("entries"), EntryValues) || EntryValues == nullptr)
	{
		OutMessage = TEXT("世界书 JSON 缺少 entries 数组。");
		return false;
	}

	TArray<FWorldBookEntry> ImportedEntries;
	ImportedEntries.Reserve(EntryValues->Num());

	for (int32 EntryIndex = 0; EntryIndex < EntryValues->Num(); ++EntryIndex)
	{
		const TSharedPtr<FJsonObject>* EntryObject = nullptr;
		if (!(*EntryValues)[EntryIndex].IsValid() || !(*EntryValues)[EntryIndex]->TryGetObject(EntryObject) || EntryObject == nullptr)
		{
			OutMessage = FString::Printf(TEXT("世界书第 %d 条条目不是合法对象。"), EntryIndex);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* KeywordValues = nullptr;
		if (!(*EntryObject)->TryGetArrayField(TEXT("keywords"), KeywordValues) || KeywordValues == nullptr)
		{
			OutMessage = FString::Printf(TEXT("世界书第 %d 条缺少 keywords 数组。"), EntryIndex);
			return false;
		}

		FWorldBookEntry ImportedEntry;
		for (const TSharedPtr<FJsonValue>& KeywordValue : *KeywordValues)
		{
			if (KeywordValue.IsValid())
			{
				const FString Keyword = KeywordValue->AsString().TrimStartAndEnd();
				if (!Keyword.IsEmpty())
				{
					ImportedEntry.Keywords.Add(Keyword);
				}
			}
		}

		ImportedEntry.Content = AstroJsonImportLibraryInternal::GetStringFieldOrDefault(*EntryObject, TEXT("content"), TEXT("content"));
		ImportedEntries.Add(ImportedEntry);
	}

	// 统一覆盖旧内容，保证导入结果与外部 JSON 完全一致。
	TargetAsset->Modify();
	TargetAsset->Entries = MoveTemp(ImportedEntries);
	TargetAsset->MarkPackageDirty();

	OutMessage = FString::Printf(TEXT("世界书导入完成：%s，共 %d 条。"), *TargetAsset->GetName(), TargetAsset->Entries.Num());
	return true;
}
