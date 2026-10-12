/*
* 持枪者接口。玩家、其他玩家和 AI 都能实现它，武器只通过它和持有者沟通。
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "UObject/Interface.h"
#include "CustomFPSWeaponHolder.generated.h"

class AActor;

UINTERFACE(MinimalAPI, Blueprintable, meta=(DisplayName="持枪者"))
class UCustomFPSWeaponHolder : public UInterface
{
	GENERATED_BODY()
};

/**
 * 任何能拿 CustomFPS 武器的角色都实现这个接口。
 * 输入、AI 或其他玩家都调用这里，不直接依赖某一个角色类。
 */
class UE_LEARNFPS_API ICustomFPSWeaponHolder
{
	GENERATED_BODY()

public:
	/** 开始开火。请求者是要求开枪的蓝图 Actor，玩家、其他玩家或 AI 把自己传进来。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="开始持枪开火"))
	void StartWeaponFire(UPARAM(meta=(DisplayName="请求者")) AActor* RequestedBy);

	/** 停止开火。请求者是要求停火的蓝图 Actor。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="停止持枪开火"))
	void StopWeaponFire(UPARAM(meta=(DisplayName="请求者")) AActor* RequestedBy);

	/** 在持枪者自己的骨骼网格上播放开火蒙太奇。武器网格由武器自己播。槽位写在蒙太奇资源里。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="播放开火动画"))
	void PlayFireAnimation(UAnimMontage* Montage);

	/** 持枪者是否正在瞄准。瞄准开火优先于走路和待机。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="持枪者是否瞄准"))
	bool IsHolderAiming();

	/** 持枪者是否正在跑步。跑步时不播放开火动画。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="持枪者是否跑步"))
	bool IsHolderSprinting();

	/** 持枪者是否正在移动。用于在待机开火和走路开火之间选择。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="持枪者是否移动"))
	bool IsHolderMoving();
};
