/*	
* 移动组件，负责角色的移动、跑步、蹲下、瞄准等动作。
* 包括初始化移动组件、移动、开始瞄准、停止瞄准、开始跑步、停止跑步、开始蹲下、停止蹲下、更新移动速度等方法。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSLocomotionComponent.generated.h"

class UCharacterMovementComponent;

/** 行走、跑步、下蹲、瞄准，以及它们对应的移动速度。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="移动"),
	hidecategories=(Object, ComponentTick, Tags, AssetUserData, Replication, ComponentReplication, Activation, Variable, Navigation, Cooking),
	PrioritizeCategories=("FPS|速度", "FPS|状态", "FPS|动画"))
class UE_LEARNFPS_API UCustomFPSLocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSLocomotionComponent();

	void InitializeMovement(UCharacterMovementComponent* Movement) const;
	void Move(float Right, float Forward);
	void StartAim();
	void StopAim();
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void UpdateMovementSpeed();
	void UpdateAnimationValues();
	void UpdateCrouch(float DeltaTime);

	bool IsAiming() const { return bAiming; }
	bool IsSprinting() const { return bSprinting; }

	/** 本地左右速度。向右为正，大约在 -1 到 1。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="MoveX", BlueprintThreadSafe))
	float GetMoveX() const { return MoveX; }

	/** 本地前后速度。向前为正，大约在 -1 到 1。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="MoveY", BlueprintThreadSafe))
	float GetMoveY() const { return MoveY; }

	/** 水平速度，厘米/秒。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="Speed", BlueprintThreadSafe))
	float GetSpeed() const { return Speed; }

	/** 垂直速度，厘米/秒。向上为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="VerticalSpeed", BlueprintThreadSafe))
	float GetVerticalSpeed() const { return VerticalSpeed; }

	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="是否下蹲", BlueprintThreadSafe))
	bool IsCrouching() const { return bIsCrouching; }

	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="是否在空中", BlueprintThreadSafe))
	bool IsFalling() const { return bIsFalling; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 非瞄准时移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|速度", meta=(DisplayName="行走速度", ClampMin="0", ForceUnits="cm/s"))
	float WalkSpeed = 250.f;

	/** 非瞄准时跑步移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|速度", meta=(DisplayName="跑步速度", ClampMin="0", ForceUnits="cm/s"))
	float SprintSpeed = 500.f;

	/** 非瞄准时蹲下移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|速度", meta=(DisplayName="下蹲速度", ClampMin="0", ForceUnits="cm/s"))
	float CrouchSpeed = 60.f;

	/** 胶囊和眼睛高度过渡到下蹲或站立的速度。0 表示立刻切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|速度", meta=(DisplayName="下蹲插值", ClampMin="0"))
	float CrouchInterpSpeed = 8.f;

	/** 瞄准时行走速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|速度", meta=(DisplayName="瞄准行走速度", ClampMin="0", ForceUnits="cm/s"))
	float AimWalkSpeed = 100.f;

	/** 瞄准时蹲下移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|速度", meta=(DisplayName="瞄准下蹲速度", ClampMin="0", ForceUnits="cm/s"))
	float AimCrouchSpeed = 20.f;

	/** 是否瞄准 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|状态", meta=(DisplayName="是否瞄准"))
	bool bAiming = false;

	/** 是否跑步 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|状态", meta=(DisplayName="是否跑步"))
	bool bSprinting = false;

	/** 本地左右速度。向右为正，大约在 -1 到 1。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="MoveX"))
	float MoveX = 0.f;

	/** 本地前后速度。向前为正，大约在 -1 到 1。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="MoveY"))
	float MoveY = 0.f;

	/** 水平速度，厘米/秒。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="Speed", ForceUnits="cm/s"))
	float Speed = 0.f;

	/** 垂直速度，厘米/秒。向上为正。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="VerticalSpeed", ForceUnits="cm/s"))
	float VerticalSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="是否下蹲"))
	bool bIsCrouching = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="是否在空中"))
	bool bIsFalling = false;

	bool bWantsCrouch = false;
	bool bCrouchDefaultsCached = false;
	float StandingCapsuleHalfHeight = 96.f;
	float CrouchedCapsuleHalfHeight = 56.f;
	float StandingEyeHeight = 64.f;
	float CrouchedEyeHeightValue = 44.f;
	FVector DefaultMeshRelativeLocation = FVector(0.f, 0.f, -96.f);
};
