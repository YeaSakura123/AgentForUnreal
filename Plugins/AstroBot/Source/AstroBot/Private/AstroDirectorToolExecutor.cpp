#include "AstroDirectorToolExecutor.h"

FAstroToolExecutionResult UAstroDirectorToolExecutor::ExecuteDirectorToolCall(const FAstroToolCall& ToolCall, FAstroRuntimeWorldOverlay& InOutWorldOverlay)
{
	FAstroToolExecutionResult Result;

	// 当前最小版本不直接操作项目具体系统，而是先修改运行时世界覆盖层，便于后续再接入天气/事件真实实现。
	if (ToolCall.ToolName.Equals(TEXT("SetWeather"), ESearchCase::IgnoreCase))
	{
		InOutWorldOverlay.WeatherState = ToolCall.ArgumentsJson;
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Director tool SetWeather executed: %s"), *ToolCall.ArgumentsJson);
		Result.ResultJson = FString::Printf(TEXT("{\"success\":true,\"weather\":\"%s\"}"), *ToolCall.ArgumentsJson.ReplaceCharWithEscapedChar());
		return Result;
	}

	if (ToolCall.ToolName.Equals(TEXT("TriggerMinorEvent"), ESearchCase::IgnoreCase))
	{
		InOutWorldOverlay.ActiveEventSummary = ToolCall.ArgumentsJson;
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Director tool TriggerMinorEvent executed: %s"), *ToolCall.ArgumentsJson);
		Result.ResultJson = FString::Printf(TEXT("{\"success\":true,\"event\":\"%s\"}"), *ToolCall.ArgumentsJson.ReplaceCharWithEscapedChar());
		return Result;
	}

	Result.bSuccess = false;
	Result.ResultMessage = FString::Printf(TEXT("Director tool is not supported yet: %s"), *ToolCall.ToolName);
	Result.ResultJson = FString::Printf(TEXT("{\"success\":false,\"tool_name\":\"%s\"}"), *ToolCall.ToolName.ReplaceCharWithEscapedChar());
	return Result;
}
