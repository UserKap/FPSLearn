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
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="移动"))
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

	bool IsAiming() const { return bAiming; }
	bool IsSprinting() const { return bSprinting; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 非瞄准时移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0", ForceUnits="cm/s"))
	float WalkSpeed = 250.f;

	/** 非瞄准时跑步移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0", ForceUnits="cm/s"))
	float SprintSpeed = 300.f;

	/** 非瞄准时蹲下移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0", ForceUnits="cm/s"))
	float CrouchSpeed = 60.f;

	/** 瞄准时行走速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0", ForceUnits="cm/s"))
	float AimWalkSpeed = 100.f;

	/** 瞄准时蹲下移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0", ForceUnits="cm/s"))
	float AimCrouchSpeed = 20.f;

	/** 是否瞄准 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS")
	bool bAiming = false;

	/** 是否跑步 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS")
	bool bSprinting = false;
};
