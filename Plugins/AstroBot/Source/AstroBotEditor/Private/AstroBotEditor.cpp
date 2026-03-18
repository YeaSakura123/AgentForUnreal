#include "AstroBotEditor.h"

#include "Logging/LogMacros.h"

#define LOCTEXT_NAMESPACE "FAstroBotEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogAstroBotEditor, Log, All);

void FAstroBotEditorModule::StartupModule()
{
	// 先输出模块启动日志，便于确认 Editor 模块已经正常加载。
	UE_LOG(LogAstroBotEditor, Log, TEXT("AstroBotEditor module started."));
}

void FAstroBotEditorModule::ShutdownModule()
{
	UE_LOG(LogAstroBotEditor, Log, TEXT("AstroBotEditor module shutdown."));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FAstroBotEditorModule, AstroBotEditor)
