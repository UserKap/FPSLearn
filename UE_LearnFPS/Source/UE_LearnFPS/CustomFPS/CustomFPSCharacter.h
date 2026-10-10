/*	
* 第一人称角色，负责角色的输入、移动、视角、枪械等动作。
* 包括初始化第一人称角色、绑定输入、移动、视角、枪械等组件。
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CustomFPSInputComponent.h"
#include "CustomFPSLocomotionComponent.h"
#include "CustomFPSViewComponent.h"
#include "CustomFPSWeaponComponent.h"
#include "CustomFPSCharacter.generated.h"

class UCameraComponent;
class USceneComponent;
class USkeletalMeshComponent;

/**
 * 第一人称角色。
 * 身体和网格挂在角色上，能力拆到输入、移动、视角、枪械四个组件。
 */
UCLASS(Blueprintable, PrioritizeCategories=("FPS|组件"))
class UE_LEARNFPS_API ACustomFPSCharacter : public ACharacter
{
	GENERATED_BODY()

	/** 映射上下文和按键绑定。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSInputComponent> FPSInput;

	/** 行走、跑步、下蹲、瞄准和移动速度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSLocomotionComponent> FPSLocomotion;

	/** 鼠标视角、开镜视野、侧身和相机骨骼。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSViewComponent> FPSView;

	/** 武器装备槽。只保存要生成的武器类。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSWeaponComponent> FPSWeapon;

	/** 手臂锚点。跟着瞄准和骨骼节点，不吃相机晃动。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ArmsAnchor;

	/** 第一人称手臂模型，挂在手臂锚点下，不是相机的子物体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** 第一人称相机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|组件", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;

public:
	ACustomFPSCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** 是否瞄准 */
	UFUNCTION(BlueprintPure, Category="FPS", meta=(DisplayName="是否瞄准"))
	bool IsAiming() const { return FPSLocomotion && FPSLocomotion->IsAiming(); }

	/** 是否跑步 */
	UFUNCTION(BlueprintPure, Category="FPS", meta=(DisplayName="是否跑步"))
	bool IsSprinting() const { return FPSLocomotion && FPSLocomotion->IsSprinting(); }

	/** 本地左右速度。向右为正，大约在 -1 到 1。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="MoveX", BlueprintThreadSafe))
	float GetMoveX() const { return FPSLocomotion ? FPSLocomotion->GetMoveX() : 0.f; }

	/** 本地前后速度。向前为正，大约在 -1 到 1。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="MoveY", BlueprintThreadSafe))
	float GetMoveY() const { return FPSLocomotion ? FPSLocomotion->GetMoveY() : 0.f; }

	/** 水平速度，厘米/秒。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="Speed", BlueprintThreadSafe))
	float GetSpeed() const { return FPSLocomotion ? FPSLocomotion->GetSpeed() : 0.f; }

	/** 垂直速度，厘米/秒。向上为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="VerticalSpeed", BlueprintThreadSafe))
	float GetVerticalSpeed() const { return FPSLocomotion ? FPSLocomotion->GetVerticalSpeed() : 0.f; }

	/** 是否下蹲 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="是否下蹲", BlueprintThreadSafe))
	bool IsCrouching() const { return FPSLocomotion && FPSLocomotion->IsCrouching(); }

	/** 是否在空中 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="是否在空中", BlueprintThreadSafe))
	bool IsFalling() const { return FPSLocomotion && FPSLocomotion->IsFalling(); }

	/** 控制器俯仰角。向上为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimPitch", BlueprintThreadSafe))
	float GetAimPitch() const { return FPSView ? FPSView->GetAimPitch() : 0.f; }

	/** 控制器偏航相对角色朝向的差。向右为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimYaw", BlueprintThreadSafe))
	float GetAimYaw() const { return FPSView ? FPSView->GetAimYaw() : 0.f; }

	/** 当前侧身滚转角。向右为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="LeanAngle", BlueprintThreadSafe))
	float GetLeanAngle() const { return FPSView ? FPSView->GetLeanAngle() : 0.f; }

	/** 当前武器瞄准插槽的世界坐标。没有武器时为零。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimSocketLocation", BlueprintThreadSafe))
	FVector GetAimSocketLocation() const
	{
		const ACustomFPSWeapon* Weapon = FPSWeapon ? FPSWeapon->GetEquippedWeapon() : nullptr;
		return Weapon ? Weapon->GetAimSocketLocation() : FVector::ZeroVector;
	}

	/** 获取第一人称手臂模型 */	
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** 获取当前装备的武器网格。没有装备武器时为空。 */
	USkeletalMeshComponent* GetWeaponMesh() const { return FPSWeapon && FPSWeapon->GetEquippedWeapon() ? FPSWeapon->GetEquippedWeapon()->GetWeaponMesh() : nullptr; }

	/** 获取第一人称相机 */
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** 获取输入组件 */
	UCustomFPSInputComponent* GetFPSInput() const { return FPSInput; }
	UCustomFPSLocomotionComponent* GetLocomotion() const { return FPSLocomotion; }
	UCustomFPSViewComponent* GetView() const { return FPSView; }
	UCustomFPSWeaponComponent* GetWeapon() const { return FPSWeapon; }

private:
	void RefreshPresentation();
	void QueuePresentationRefresh();

	bool bPresentationRefreshQueued = false;
};
