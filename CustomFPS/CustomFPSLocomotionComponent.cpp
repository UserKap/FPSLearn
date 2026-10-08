/*	
* 移动组件，负责角色的移动、跑步、蹲下、瞄准等动作。
* 包括初始化移动组件、移动、开始瞄准、停止瞄准、开始跑步、停止跑步、开始蹲下、停止蹲下、更新移动速度等方法。
*/

#include "CustomFPSLocomotionComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"

UCustomFPSLocomotionComponent::UCustomFPSLocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCustomFPSLocomotionComponent::InitializeMovement(UCharacterMovementComponent* Movement) const
{
	if (!Movement)
	{
		return;
	}

	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->BrakingDecelerationWalking = 1800.f;
	Movement->BrakingDecelerationFalling = 1500.f;
	Movement->AirControl = 0.35f;
	Movement->JumpZVelocity = 420.f;
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(56.f);
}

void UCustomFPSLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::Move(float Right, float Forward)
{
	ACharacter* Character = GetOwner<ACharacter>();
	if (!Character || !Character->GetController())
	{
		return;
	}

	const FRotator YawRotation(0.f, Character->GetControlRotation().Yaw, 0.f);
	// 前后走角色右方向，左右走角色前方向，和当前输入轴一致。
	Character->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Forward);
	Character->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Right);
}

void UCustomFPSLocomotionComponent::StartAim()
{
	if (bSprinting)
	{
		StopSprint();
	}

	bAiming = true;
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::StopAim()
{
	bAiming = false;
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::StartSprint()
{
	if (bAiming)
	{
		StopAim();
	}

	if (ACharacter* Character = GetOwner<ACharacter>())
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (Movement->IsCrouching())
			{
				Character->UnCrouch();
			}
		}
	}

	bSprinting = true;
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::StopSprint()
{
	bSprinting = false;
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::StartCrouch()
{
	if (bSprinting)
	{
		StopSprint();
	}

	if (ACharacter* Character = GetOwner<ACharacter>())
	{
		Character->Crouch();
	}
}

void UCustomFPSLocomotionComponent::StopCrouch()
{
	if (ACharacter* Character = GetOwner<ACharacter>())
	{
		Character->UnCrouch();
	}
}

void UCustomFPSLocomotionComponent::UpdateMovementSpeed()
{
	ACharacter* Character = GetOwner<ACharacter>();
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const bool bCrouching = Movement->IsCrouching();
	Movement->MaxWalkSpeedCrouched = bAiming ? AimCrouchSpeed : CrouchSpeed;
	if (bCrouching)
	{
		return;
	}

	if (bSprinting)
	{
		Movement->MaxWalkSpeed = SprintSpeed;
	}
	else if (bAiming)
	{
		Movement->MaxWalkSpeed = AimWalkSpeed;
	}
	else
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}
}
