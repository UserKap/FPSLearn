/*
* 后坐力数据。每把枪引用一份，同类枪可以共用。
* 真实上抬会移动准星，玩家要往下压；镜头冲击只动画面，不影响准星。
*/

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "CustomFPSRecoilData.generated.h"

/** 一把枪的后坐力：固定弹道、随机抖动、状态倍率、松手回落和镜头冲击。 */
UCLASS(BlueprintType, meta=(DisplayName="后坐力"),
	PrioritizeCategories=("FPS|弹道", "FPS|状态倍率", "FPS|上抬回落", "FPS|镜头冲击", "FPS|枪身冲击"))
class UE_LEARNFPS_API UCustomFPSRecoilData : public UDataAsset
{
	GENERATED_BODY()

public:
	UCustomFPSRecoilData();

	/**
	 * 连射第几发的固定上抬和横移，含随机抖动和状态倍率之前的值。
	 * 返回值 X 是俯仰（向上为正），Y 是偏航（向右为正），单位度。
	 */
	FVector2D EvaluatePattern(int32 ShotNumber) const;

	/** 每发的基础上抬，度。乘上上抬曲线在这一发的值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|弹道", meta=(DisplayName="每发上抬", ClampMin="0", ForceUnits="deg"))
	float VerticalRecoil = 0.55f;

	/** 横轴是连射第几发，从 1 开始。纵轴乘在每发上抬上。空曲线按 1 算。 */
	UPROPERTY(EditAnywhere, Category="FPS|弹道", meta=(DisplayName="上抬曲线"))
	FRuntimeFloatCurve VerticalPattern;

	/** 每发的基础横移，度。乘上横移曲线在这一发的值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|弹道", meta=(DisplayName="每发横移", ClampMin="0", ForceUnits="deg"))
	float HorizontalRecoil = 0.3f;

	/** 横轴是连射第几发，从 1 开始。纵轴在 -1 到 1 之间，正值向右。空曲线按 0 算，弹道只往上走。 */
	UPROPERTY(EditAnywhere, Category="FPS|弹道", meta=(DisplayName="横移曲线"))
	FRuntimeFloatCurve HorizontalPattern;

	/** 每发上抬再随机加减这么多，度。玩家没法把弹道完全背下来。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|弹道", meta=(DisplayName="上抬随机", ClampMin="0", ForceUnits="deg"))
	float VerticalRandom = 0.1f;

	/** 每发横移再随机加减这么多，度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|弹道", meta=(DisplayName="横移随机", ClampMin="0", ForceUnits="deg"))
	float HorizontalRandom = 0.12f;

	/** 两发间隔超过这个时间，弹道从第 1 发重新开始。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|弹道", meta=(DisplayName="弹道重置时间", ClampMin="0", ForceUnits="s"))
	float PatternResetTime = 0.35f;

	/** 开镜时上抬、横移和镜头冲击乘这个数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|状态倍率", meta=(DisplayName="开镜倍率", ClampMin="0"))
	float AimMultiplier = 0.75f;

	/** 下蹲时乘这个数。和开镜倍率相乘。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|状态倍率", meta=(DisplayName="下蹲倍率", ClampMin="0"))
	float CrouchMultiplier = 0.8f;

	/** 移动时乘这个数。和开镜、下蹲倍率相乘。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|状态倍率", meta=(DisplayName="移动倍率", ClampMin="0"))
	float MoveMultiplier = 1.2f;

	/** 每发的上抬在这段时间内平滑加完，避免画面跳一下。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|上抬回落", meta=(DisplayName="上抬时间", ClampMin="0", ForceUnits="s"))
	float RiseDuration = 0.06f;

	/** 停火后，准星回落已上抬部分的比例。玩家已经压下去的量会先扣掉。1 是回到开火前，0 是不回落。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|上抬回落", meta=(DisplayName="回落比例", ClampMin="0", ClampMax="1"))
	float RecoveryFraction = 0.7f;

	/** 最后一发之后等多久开始回落。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|上抬回落", meta=(DisplayName="回落延迟", ClampMin="0", ForceUnits="s"))
	float RecoveryDelay = 0.1f;

	/** 回落的快慢。越大越快。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|上抬回落", meta=(DisplayName="回落速度", ClampMin="0.1"))
	float RecoverySpeed = 8.f;

	/** 每发镜头往上顿的角度，度。只动画面，不动准星。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|镜头冲击", meta=(DisplayName="冲击上抬", ForceUnits="deg"))
	float CameraKickPitch = 0.6f;

	/** 每发镜头左右随机抖动的最大角度，度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|镜头冲击", meta=(DisplayName="冲击左右", ClampMin="0", ForceUnits="deg"))
	float CameraKickYaw = 0.25f;

	/** 每发镜头随机滚转的最大角度，度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|镜头冲击", meta=(DisplayName="冲击滚转", ClampMin="0", ForceUnits="deg"))
	float CameraKickRoll = 0.8f;

	/** 开镜时镜头冲击再乘这个数。开镜画面通常更稳。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|镜头冲击", meta=(DisplayName="开镜冲击倍率", ClampMin="0"))
	float AimCameraKickScale = 0.5f;

	/** 镜头冲向冲击角度的快慢。越大顿得越脆。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|镜头冲击", meta=(DisplayName="冲击速度", ClampMin="0.1"))
	float CameraKickSnapSpeed = 35.f;

	/** 镜头从冲击回到原位的快慢。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|镜头冲击", meta=(DisplayName="冲击回位速度", ClampMin="0.1"))
	float CameraKickReturnSpeed = 12.f;

	/** 每发手臂和枪一起往后退的距离，厘米。只动手臂锚点，不动准星。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="后退距离", ClampMin="0", ForceUnits="cm"))
	float WeaponKickBack = 1.6f;

	/** 每发枪口上翘的角度，度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="枪口上翘", ForceUnits="deg"))
	float WeaponKickPitch = 1.4f;

	/** 每发枪口左右随机摆动的最大角度，度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="左右摆动", ClampMin="0", ForceUnits="deg"))
	float WeaponKickYaw = 0.4f;

	/** 每发枪身随机滚转的最大角度，度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="滚转摆动", ClampMin="0", ForceUnits="deg"))
	float WeaponKickRoll = 1.2f;

	/**
	 * 开镜时后退距离再乘这个数。
	 * 开镜镜头贴着瞄准插槽，后退会带着镜头一起退，看起来像镜头往脸上撞，所以要小。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="开镜后退倍率", ClampMin="0"))
	float AimWeaponKickBackScale = 0.15f;

	/** 开镜时上翘、左右和滚转再乘这个数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="开镜旋转倍率", ClampMin="0"))
	float AimWeaponKickRotationScale = 0.6f;

	/** 枪身冲到后坐位置的快慢。越大越脆。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="冲击速度", ClampMin="0.1"))
	float WeaponKickSnapSpeed = 40.f;

	/** 枪身回到原位的快慢。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FPS|枪身冲击", meta=(DisplayName="回位速度", ClampMin="0.1"))
	float WeaponKickReturnSpeed = 14.f;
};
