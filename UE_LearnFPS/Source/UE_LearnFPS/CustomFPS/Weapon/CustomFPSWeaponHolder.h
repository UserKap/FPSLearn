/*
* 持枪者接口。玩家、其他玩家和 AI 都能实现它，武器只通过它和持有者沟通。
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "UObject/Interface.h"
#include "CustomFPSWeaponHolder.generated.h"

class AActor;
class UCustomFPSRecoilData;

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

	/** 持枪者是否下蹲。下蹲时后坐力乘下蹲倍率。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="持枪者是否下蹲"))
	bool IsHolderCrouching();

	/**
	 * 武器每射出一发交来的后坐力。已经乘过状态倍率。
	 * 准星上抬的 Pitch 向上为正、Yaw 向右为正，持枪者应把它加到瞄准方向上。镜头冲击只用于画面。
	 * 枪身位移在瞄准方向的局部空间，X 向前；枪身旋转 Pitch 为正时枪口上翘。两者都只动手和枪，不动准星。
	 * AI 没有镜头，可以只用准星上抬，或者什么都不做。
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="承受后坐力"))
	void ApplyWeaponRecoil(
		UPARAM(meta=(DisplayName="后坐力数据")) UCustomFPSRecoilData* RecoilData,
		UPARAM(meta=(DisplayName="准星上抬")) FRotator ControlRise,
		UPARAM(meta=(DisplayName="镜头冲击")) FRotator CameraKick,
		UPARAM(meta=(DisplayName="枪身位移")) FVector WeaponKickOffset,
		UPARAM(meta=(DisplayName="枪身旋转")) FRotator WeaponKickRotation);
};
