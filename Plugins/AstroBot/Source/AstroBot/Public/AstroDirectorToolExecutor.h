#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AstroDirectorTypes.h"
#include "AstroDirectorToolExecutor.generated.h"

UCLASS(BlueprintType)
class ASTROBOT_API UAstroDirectorToolExecutor : public UObject
{
	GENERATED_BODY()

public:
	// 导演系统的最小工具执行入口。当前版本先支持 SetWeather 和 TriggerMinorEvent 两类安全动作。
	UFUNCTION(BlueprintCallable, Category = "AstroBot|Director")
	FAstroToolExecutionResult ExecuteDirectorToolCall(const FAstroToolCall& ToolCall, FAstroRuntimeWorldOverlay& InOutWorldOverlay);
};
