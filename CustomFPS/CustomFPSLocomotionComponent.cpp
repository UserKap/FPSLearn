/*	
* 移动组件，负责角色的移动、跑步、蹲下、瞄准等动作。
* 包括初始化移动组件、移动、开始瞄准、停止瞄准、开始跑步、停止跑步、开始蹲下、停止蹲下、更新移动速度等方法。
*/

#include "CustomFPSLocomotionComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
	UpdateCrouch(DeltaTime);
	UpdateMovementSpeed();
	UpdateAnimationValues();
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
	// 跑步键按住时每帧都会再进一次。瞄准中直接忽略，避免刚开镜就被取消。
	if (bAiming)
	{
		return;
	}

	bWantsCrouch = false;

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

	bWantsCrouch = true;
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::StopCrouch()
{
	bWantsCrouch = false;
	UpdateMovementSpeed();
}

void UCustomFPSLocomotionComponent::UpdateCrouch(float DeltaTime)
{
	ACharacter* Character = GetOwner<ACharacter>();
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Capsule || !Movement)
	{
		return;
	}

	if (!bCrouchDefaultsCached)
	{
		StandingCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		CrouchedCapsuleHalfHeight = Movement->GetCrouchedHalfHeight();
		StandingEyeHeight = Character->BaseEyeHeight;
		CrouchedEyeHeightValue = Character->CrouchedEyeHeight;
		if (const USkeletalMeshComponent* BodyMesh = Character->GetMesh())
		{
			DefaultMeshRelativeLocation = BodyMesh->GetRelativeLocation();
		}
		bCrouchDefaultsCached = true;
	}

	const float TargetHalfHeight = bWantsCrouch ? CrouchedCapsuleHalfHeight : StandingCapsuleHalfHeight;
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	float NewHalfHeight = CrouchInterpSpeed <= 0.f
		? TargetHalfHeight
		: FMath::FInterpTo(OldHalfHeight, TargetHalfHeight, DeltaTime, CrouchInterpSpeed);
	if (FMath::IsNearlyEqual(NewHalfHeight, TargetHalfHeight, 0.1f))
	{
		NewHalfHeight = TargetHalfHeight;
	}
	if (FMath::IsNearlyEqual(OldHalfHeight, NewHalfHeight, 0.01f))
	{
		return;
	}

	const float HalfHeightDelta = (NewHalfHeight - OldHalfHeight) * Capsule->GetShapeScale();
	const FVector UpOffset = -Movement->GetGravityDirection() * HalfHeightDelta;
	if (NewHalfHeight > OldHalfHeight)
	{
		UWorld* World = GetWorld();
		const FCollisionShape StandingShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), NewHalfHeight * Capsule->GetShapeScale() - 1.f);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CustomFPSCrouch), false, Character);
		if (!World || World->OverlapBlockingTestByChannel(Capsule->GetComponentLocation() + UpOffset, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), StandingShape, QueryParams))
		{
			return;
		}
	}

	Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), NewHalfHeight);
	Character->AddActorWorldOffset(UpOffset, true);

	const float HeightSpan = FMath::Max(StandingCapsuleHalfHeight - CrouchedCapsuleHalfHeight, 1.f);
	const float CrouchAlpha = FMath::Clamp((StandingCapsuleHalfHeight - NewHalfHeight) / HeightSpan, 0.f, 1.f);
	Character->BaseEyeHeight = FMath::Lerp(StandingEyeHeight, CrouchedEyeHeightValue, CrouchAlpha);
	if (USkeletalMeshComponent* BodyMesh = Character->GetMesh())
	{
		FVector MeshLocation = DefaultMeshRelativeLocation;
		MeshLocation.Z += StandingCapsuleHalfHeight - NewHalfHeight;
		BodyMesh->SetRelativeLocation(MeshLocation);
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

	Movement->MaxWalkSpeedCrouched = bAiming ? AimCrouchSpeed : CrouchSpeed;
	if (bWantsCrouch)
	{
		Movement->MaxWalkSpeed = bAiming ? AimCrouchSpeed : CrouchSpeed;
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

void UCustomFPSLocomotionComponent::UpdateAnimationValues()
{
	ACharacter* Character = GetOwner<ACharacter>();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Movement)
	{
		MoveX = 0.f;
		MoveY = 0.f;
		Speed = 0.f;
		VerticalSpeed = 0.f;
		bIsCrouching = false;
		bIsFalling = false;
		return;
	}

	const FVector HorizontalVelocity(Movement->Velocity.X, Movement->Velocity.Y, 0.f);
	Speed = HorizontalVelocity.Size();
	VerticalSpeed = Movement->Velocity.Z;
	bIsCrouching = bWantsCrouch;
	bIsFalling = Movement->IsFalling();

	const float MaxSpeed = FMath::Max(Movement->GetMaxSpeed(), 1.f);
	const FRotator YawRotation(0.f, Character->GetActorRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	MoveY = FMath::Clamp(FVector::DotProduct(HorizontalVelocity, Forward) / MaxSpeed, -1.f, 1.f);
	MoveX = FMath::Clamp(FVector::DotProduct(HorizontalVelocity, Right) / MaxSpeed, -1.f, 1.f);
}
