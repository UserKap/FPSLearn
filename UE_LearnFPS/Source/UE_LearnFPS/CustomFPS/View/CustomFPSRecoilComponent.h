/*
* 后坐力组件。武器每发子弹交来一份后坐力，这里把它变成准星上抬、松手回落、镜头冲击和枪身冲击。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSRecoilComponent.generated.h"

class AController;
class UCustomFPSRecoilData;

/**
 * 准星上抬写进控制器旋转，会改变子弹方向，玩家要往下压。
 * 镜头冲击只交给视角组件叠在画面上，不改控制器。
 */
UCLASS(ClassGroup=(CustomFPS), meta=(DisplayName="后坐力"),
	hidecategories=(Object, ComponentTick, Tags, AssetUserData, Replication, ComponentReplication, Activation, Variable, Navigation, Cooking))
class UE_LEARNFPS_API UCustomFPSRecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSRecoilComponent();

	/**
	 * 叠加一发子弹的后坐力。
	 * ControlRise 的 Pitch 向上为正、Yaw 向右为正，会在上抬时间内平滑加到控制器上。
	 * CameraKick 只加在画面上。WeaponKickOffset、WeaponKickRotation 只加在手臂锚点上，枪和手一起动。
	 */
	void AddRecoil(const UCustomFPSRecoilData* Data, FRotator ControlRise, FRotator CameraKick, FVector WeaponKickOffset, FRotator WeaponKickRotation);

	/** 清掉还没加完的上抬、回落、镜头冲击和枪身冲击。死亡或换枪时用。 */
	void ResetRecoil();

	/** 当前叠在画面上的镜头冲击。视角组件每帧读取。 */
	FRotator GetCameraKick() const { return CameraKickCurrent; }

	/** 当前手臂锚点的后坐位移，瞄准方向局部空间，X 向前。 */
	FVector GetWeaponKickOffset() const { return WeaponKickOffsetCurrent; }

	/** 当前手臂锚点的后坐旋转，Pitch 为正时枪口上翘。 */
	FRotator GetWeaponKickRotation() const { return WeaponKickRotationCurrent; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 这一轮连射还没被压掉或回落掉的上抬。X 俯仰，Y 偏航。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|后坐力", meta=(DisplayName="累计上抬"))
	FVector2D AccumulatedRise = FVector2D::ZeroVector;

private:
	FVector2D ApplyControlDelta(AController* Controller, const FVector2D& Delta);
	void AbsorbPlayerCompensation(const FVector2D& PlayerDelta);

	FVector2D PendingRise = FVector2D::ZeroVector;
	FVector2D PendingRiseRate = FVector2D::ZeroVector;
	FVector2D RecoveryTarget = FVector2D::ZeroVector;
	bool bRecovering = false;
	float LastRecoilTime = -1.f;

	float RiseDuration = 0.06f;
	float RecoveryFraction = 0.7f;
	float RecoveryDelay = 0.1f;
	float RecoverySpeed = 8.f;
	float KickSnapSpeed = 35.f;
	float KickReturnSpeed = 12.f;

	FRotator CameraKickTarget = FRotator::ZeroRotator;
	FRotator CameraKickCurrent = FRotator::ZeroRotator;

	float WeaponKickSnapSpeed = 40.f;
	float WeaponKickReturnSpeed = 14.f;
	FVector WeaponKickOffsetTarget = FVector::ZeroVector;
	FVector WeaponKickOffsetCurrent = FVector::ZeroVector;
	FRotator WeaponKickRotationTarget = FRotator::ZeroRotator;
	FRotator WeaponKickRotationCurrent = FRotator::ZeroRotator;

	FRotator LastControlRotation = FRotator::ZeroRotator;
	bool bHasLastControlRotation = false;
};
