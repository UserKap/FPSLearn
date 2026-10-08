/*	
* 视角组件，负责角色的视角、瞄准、侧身等动作。
* 包括初始化视角组件、设置相机、移动、瞄准、侧身、更新相机附件等方法。
*/

#include "CustomFPSViewComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CustomFPSLocomotionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UCustomFPSViewComponent::UCustomFPSViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCustomFPSViewComponent::SetupCamera(UCameraComponent* InCamera)
{
	Camera = InCamera;
	if (!Camera)
	{
		return;
	}

	Camera->bUsePawnControlRotation = true;
	Camera->bLockToHmd = false;
	Camera->bEnableFirstPersonFieldOfView = true;
	Camera->bEnableFirstPersonScale = true;
	Camera->FirstPersonFieldOfView = HipFieldOfView;
	Camera->FirstPersonScale = 0.6f;
	Camera->SetFieldOfView(HipFieldOfView);
}

void UCustomFPSViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Camera)
	{
		return;
	}

	ACharacter* Character = GetOwner<ACharacter>();
	const UCustomFPSLocomotionComponent* Locomotion = Character ? Character->FindComponentByClass<UCustomFPSLocomotionComponent>() : nullptr;
	const bool bAiming = Locomotion && Locomotion->IsAiming();
	const float TargetFOV = bAiming ? AimFieldOfView : HipFieldOfView;
	const float NewFOV = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed);
	Camera->SetFieldOfView(NewFOV);
	Camera->FirstPersonFieldOfView = NewFOV;

	CurrentLeanRoll = FMath::FInterpTo(CurrentLeanRoll, TargetLeanRoll, DeltaTime, LeanInterpSpeed);
	if (!Character)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Roll = CurrentLeanRoll;
		PlayerController->SetControlRotation(ControlRotation);
	}
}

void UCustomFPSViewComponent::Look(float Yaw, float Pitch)
{
	APawn* Pawn = GetOwner<APawn>();
	if (!Pawn || !Pawn->GetController())
	{
		return;
	}

	// 鼠标向上时 Pitch 为正，取反后视角向上。
	Pawn->AddControllerYawInput(Yaw * LookSensitivityYaw);
	Pawn->AddControllerPitchInput(-Pitch * LookSensitivityPitch);
}

void UCustomFPSViewComponent::Lean(float Side)
{
	TargetLeanRoll = FMath::Clamp(Side, -1.f, 1.f) * MaxLeanAngle;
}

void UCustomFPSViewComponent::UpdateCameraAttachment()
{
	ACharacter* Character = GetOwner<ACharacter>();
	if (!Character || !Camera)
	{
		return;
	}

	USkeletalMeshComponent* BodyMesh = Character->GetMesh();
	const bool bFollowBone = BodyMesh && !CameraAttachBone.IsNone() && BodyMesh->DoesSocketExist(CameraAttachBone);
	if (bFollowBone)
	{
		if (Camera->GetAttachParent() != BodyMesh || Camera->GetAttachSocketName() != CameraAttachBone)
		{
			Camera->AttachToComponent(BodyMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CameraAttachBone);
		}
		return;
	}

	if (Camera->GetAttachParent() != Character->GetCapsuleComponent())
	{
		Camera->AttachToComponent(Character->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	Camera->SetRelativeLocation(FVector(0.f, 0.f, Character->BaseEyeHeight));
}

#if WITH_EDITOR
void UCustomFPSViewComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UCustomFPSViewComponent, CameraAttachBone))
	{
		UpdateCameraAttachment();
	}
}
#endif
