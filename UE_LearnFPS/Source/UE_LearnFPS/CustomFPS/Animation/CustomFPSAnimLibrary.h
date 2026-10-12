/*
* 动画蓝图用的查询。序列评估器的显式时间用秒，这里直接给出播放长度。
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CustomFPSAnimLibrary.generated.h"

class UAnimSequenceBase;

UCLASS()
class UE_LEARNFPS_API UCustomFPSAnimLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 动画序列的播放长度，单位秒。序列为空时为 0。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="动画秒数", BlueprintThreadSafe))
	static float GetAnimPlayLength(const UAnimSequenceBase* Sequence);
};
