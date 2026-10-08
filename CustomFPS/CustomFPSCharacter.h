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
class USkeletalMeshComponent;

/**
 * 第一人称角色。
 * 身体和网格挂在角色上，能力拆到输入、移动、视角、枪械四个组件。
 */
UCLASS(Blueprintable)
class UE_LEARNFPS_API ACustomFPSCharacter : public ACharacter
{
	GENERATED_BODY()

	/** 映射上下文和按键绑定。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSInputComponent> FPSInput;

	/** 行走、跑步、下蹲、瞄准和移动速度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSLocomotionComponent> FPSLocomotion;

	/** 鼠标视角、开镜视野、侧身和相机骨骼。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSViewComponent> FPSView;

	/** 枪械插槽和静态网格部件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomFPSWeaponComponent> FPSWeapon;

	/** 第一人称手臂模型 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** 第一人称枪械模型，挂在手臂网格下 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** 第一人称相机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
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

	UFUNCTION(BlueprintPure, Category="FPS", meta=(DisplayName="是否瞄准"))
	bool IsAiming() const { return FPSLocomotion && FPSLocomotion->IsAiming(); }

	UFUNCTION(BlueprintPure, Category="FPS", meta=(DisplayName="是否跑步"))
	bool IsSprinting() const { return FPSLocomotion && FPSLocomotion->IsSprinting(); }

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	UCustomFPSInputComponent* GetFPSInput() const { return FPSInput; }
	UCustomFPSLocomotionComponent* GetLocomotion() const { return FPSLocomotion; }
	UCustomFPSViewComponent* GetView() const { return FPSView; }
	UCustomFPSWeaponComponent* GetWeapon() const { return FPSWeapon; }

private:
	void RefreshPresentation();
};
