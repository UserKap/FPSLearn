/*	
* 视角组件，负责角色的视角、瞄准、侧身等动作。
* 包括初始化视角组件、设置相机、移动、瞄准、侧身、更新相机附件等方法。
*/

#include "CustomFPSViewComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CustomFPSLocomotionComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ReferenceSkeleton.h"

UCustomFPSViewComponent::UCustomFPSViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 必须早于 PlayerCameraManager 取景，否则这一帧画面还是旧位置。
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
	Camera->SetUsingAbsoluteLocation(false);
	Camera->SetUsingAbsoluteRotation(false);
	Camera->bLockToHmd = false;
	Camera->bEnableFirstPersonFieldOfView = true;
	Camera->bEnableFirstPersonScale = true;
	Camera->FirstPersonFieldOfView = HipFieldOfView;
	Camera->FirstPersonScale = 0.6f;
	Camera->SetFieldOfView(HipFieldOfView);
}

void UCustomFPSViewComponent::SetupArmsAnchor(USceneComponent* InArmsAnchor)
{
	ArmsAnchor = InArmsAnchor;
}

void UCustomFPSViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateAnimationValues();
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

	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		EnsureViewIsCapturedAfterCameraUpdate();
		UpdateDrivenCamera(DeltaTime);
	}
}

void UCustomFPSViewComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = GetOwner<ACharacter>())
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			PrimaryComponentTick.AddPrerequisite(Movement, Movement->PrimaryComponentTick);
		}
	}

	UpdateCameraAttachment();
}

/** 确保视角在相机更新后被捕获 */
void UCustomFPSViewComponent::EnsureViewIsCapturedAfterCameraUpdate()
{
	if (bCameraManagerPrerequisiteSet)
	{
		return;
	}

	const ACharacter* Character = GetOwner<ACharacter>();
	const APlayerController* PlayerController = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (!CameraManager)
	{
		return;
	}

	CameraManager->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
	bCameraManagerPrerequisiteSet = true;
}

/** 更新视角动画值 */
void UCustomFPSViewComponent::UpdateAnimationValues()
{
	const APawn* Pawn = GetOwner<APawn>();
	if (!Pawn)
	{
		AimPitch = 0.f;
		AimYaw = 0.f;
		return;
	}

	AimPitch = FRotator::NormalizeAxis(Pawn->GetControlRotation().Pitch);
	AimYaw = FRotator::NormalizeAxis(Pawn->GetControlRotation().Yaw - Pawn->GetActorRotation().Yaw);
}

/** 移动视角 */
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

/** 侧身 */
void UCustomFPSViewComponent::Lean(float Side)
{
	TargetLeanRoll = FMath::Clamp(Side, -1.f, 1.f) * MaxLeanAngle;
}

/** 获取组件空间参考姿势 */
namespace
{
	FTransform GetComponentSpaceReferencePose(const USkeletalMeshComponent* BodyMesh, FName SocketOrBone)
	{
		const USkinnedAsset* SkinnedAsset = BodyMesh ? BodyMesh->GetSkinnedAsset() : nullptr;
		if (!SkinnedAsset)
		{
			return FTransform::Identity;
		}

		const FReferenceSkeleton& ReferenceSkeleton = SkinnedAsset->GetRefSkeleton();
		FName BoneName = SocketOrBone;
		FTransform SocketLocal = FTransform::Identity;
		if (const USkeletalMeshSocket* Socket = BodyMesh->GetSocketByName(SocketOrBone))
		{
			BoneName = Socket->BoneName;
			SocketLocal = FTransform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale);
		}

		const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return FTransform::Identity;
		}

		FTransform BoneTransform = ReferenceSkeleton.GetRefBonePose()[BoneIndex];
		for (int32 Index = ReferenceSkeleton.GetParentIndex(BoneIndex); Index != INDEX_NONE; Index = ReferenceSkeleton.GetParentIndex(Index))
		{
			BoneTransform = BoneTransform * ReferenceSkeleton.GetRefBonePose()[Index];
		}
		return SocketLocal * BoneTransform;
	}

	FName ResolveCameraBone(const USkeletalMeshComponent* BodyMesh, FName Requested)
	{
		if (!BodyMesh || Requested.IsNone() || !BodyMesh->GetSkinnedAsset())
		{
			return NAME_None;
		}

		if (BodyMesh->DoesSocketExist(Requested))
		{
			return Requested;
		}

		const FString Wanted = Requested.ToString();
		const FReferenceSkeleton& ReferenceSkeleton = BodyMesh->GetSkinnedAsset()->GetRefSkeleton();
		for (int32 Index = 0; Index < ReferenceSkeleton.GetNum(); ++Index)
		{
			const FName BoneName = ReferenceSkeleton.GetBoneName(Index);
			if (BoneName.ToString().Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return BoneName;
			}
		}

		return NAME_None;
	}

	USkeletalMeshComponent* FindMeshForCameraBone(ACharacter* Character, FName Requested, FName& OutBone)
	{
		OutBone = NAME_None;
		if (!Character || Requested.IsNone())
		{
			return nullptr;
		}

		auto TryMesh = [&OutBone, Requested](USkeletalMeshComponent* Mesh) -> bool
		{
			const FName Resolved = ResolveCameraBone(Mesh, Requested);
			if (Resolved.IsNone())
			{
				return false;
			}
			OutBone = Resolved;
			return true;
		};

		// 身体网格有这根骨骼时优先用它。蓝图经常把带骨骼的模型放在第一人称网格上。
		if (TryMesh(Character->GetMesh()))
		{
			return Character->GetMesh();
		}

		TInlineComponentArray<USkeletalMeshComponent*> Meshes;
		Character->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh != Character->GetMesh() && TryMesh(Mesh))
			{
				return Mesh;
			}
		}
		return nullptr;
	}
}

/** 计算骨骼晃动 */
FRotator UCustomFPSViewComponent::ComputeBoneSway(const USkeletalMeshComponent* BodyMesh) const
{
	if (!BodyMesh || ActiveCameraBone.IsNone() || BoneSwayScale <= 0.f || !BodyMesh->DoesSocketExist(ActiveCameraBone))
	{
		return FRotator::ZeroRotator;
	}

	const FQuat ReferenceRotation = GetComponentSpaceReferencePose(BodyMesh, ActiveCameraBone).GetRotation();
	const FQuat CurrentRotation = BodyMesh->GetSocketTransform(ActiveCameraBone, RTS_Component).GetRotation();
	const FQuat ComponentDelta = CurrentRotation * ReferenceRotation.Inverse();
	const FQuat MeshRotation = BodyMesh->GetComponentQuat();
	const FRotator WorldDelta = (MeshRotation * ComponentDelta * MeshRotation.Inverse()).Rotator();

	const float Limit = MaxBoneSwayAngle;
	return FRotator(
		FMath::Clamp(FRotator::NormalizeAxis(WorldDelta.Pitch) * BoneSwayScale, -Limit, Limit),
		FMath::Clamp(FRotator::NormalizeAxis(WorldDelta.Yaw) * BoneSwayScale, -Limit, Limit),
		FMath::Clamp(FRotator::NormalizeAxis(WorldDelta.Roll) * BoneSwayScale, -Limit, Limit));
}

/** 更新驱动相机 */
void UCustomFPSViewComponent::UpdateDrivenCamera(float DeltaTime)
{
	ACharacter* Character = GetOwner<ACharacter>();
	if (!Character)
	{
		ActiveCameraBone = NAME_None;
		bDrivingCameraFromBone = false;
		return;
	}

	USkeletalMeshComponent* BodyMesh = FindMeshForCameraBone(Character, CameraAttachBone, ActiveCameraBone);
	const bool bHasBone = BodyMesh != nullptr;
	bDrivingCameraFromBone = bHasBone;
	if (!Camera)
	{
		return;
	}

	const FVector EyeLocation = Character->GetActorLocation() + FVector(0.f, 0.f, Character->BaseEyeHeight);
	FVector NodeLocation = EyeLocation;
	FVector ReferenceWorld = EyeLocation;
	FVector SwayWorld = FVector::ZeroVector;
	FRotator DesiredSway = FRotator::ZeroRotator;
	if (bHasBone)
	{
		// 等这帧并行计算结束再读骨骼。直接结束任务会把组件空间变换清成 0，下一帧就会在 140 根骨骼上断言。
		BodyMesh->HandleExistingParallelEvaluationTask(true, true);

		const int32 BoneIndex = BodyMesh->GetBoneIndex(ActiveCameraBone);
		const bool bPoseReady = BoneIndex != INDEX_NONE && BoneIndex < BodyMesh->GetNumComponentSpaceTransforms();
		if (bPoseReady)
		{
			const FTransform SocketLocal = BodyMesh->GetSocketTransform(ActiveCameraBone, RTS_Component);
			const FTransform ReferenceLocal = GetComponentSpaceReferencePose(BodyMesh, ActiveCameraBone);
			// 第一人称网格挂在我们每帧都会挪动的锚点下。用它当前的世界坐标会把骨骼偏移再叠一次。
			const bool bMeshMovesWithView = (ArmsAnchor && BodyMesh->IsAttachedTo(ArmsAnchor)) || (Camera && BodyMesh->IsAttachedTo(Camera));
			const FTransform MeshWorld = bMeshMovesWithView
				? BodyMesh->GetRelativeTransform() * FTransform(Character->GetControlRotation(), EyeLocation)
				: BodyMesh->GetComponentTransform();
			const FVector AnimatedWorld = MeshWorld.TransformPosition(SocketLocal.GetLocation());
			ReferenceWorld = MeshWorld.TransformPosition(ReferenceLocal.GetLocation());
			// 手臂贴骨骼节点本身。位置晃动倍数只放大相机离开节点的那一段。
			NodeLocation = AnimatedWorld;
			SwayWorld = (AnimatedWorld - ReferenceWorld) * BoneLocationSwayScale;
			DesiredSway = ComputeBoneSway(BodyMesh);
		}
	}

	const FRotator YawRotation(0.f, Character->GetActorRotation().Yaw, 0.f);
	const FRotator LookRotation = Character->GetControlRotation();
	const FVector LocalSway = YawRotation.UnrotateVector(SwayWorld);

	if (ArmsAnchor)
	{
		ArmsAnchor->SetUsingAbsoluteLocation(true);
		ArmsAnchor->SetUsingAbsoluteRotation(true);
		ArmsAnchor->SetWorldLocation(NodeLocation);
		ArmsAnchor->SetWorldRotation(LookRotation);
	}

	// 平滑只作用在骨骼晃动上。参考位置每帧跟着角色走，跑步不会把镜头落在枪托后面。
	if (!bHasSmoothedLocalSway || CameraLocationLagSpeed <= 0.f)
	{
		SmoothedLocalSway = LocalSway;
		bHasSmoothedLocalSway = true;
	}
	else
	{
		SmoothedLocalSway = FMath::VInterpTo(SmoothedLocalSway, LocalSway, DeltaTime, CameraLocationLagSpeed);
	}
	const FVector DesiredLocation = ReferenceWorld
		+ YawRotation.RotateVector(SmoothedLocalSway)
		+ FRotationMatrix(YawRotation).TransformVector(CameraAttachOffset);

	CurrentBoneSway = CameraRotationLagSpeed <= 0.f
		? DesiredSway
		: FMath::RInterpTo(CurrentBoneSway, DesiredSway, DeltaTime, CameraRotationLagSpeed);

	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->bUsePawnControlRotation = false;

	FRotator ViewRotation = Character->GetControlRotation();
	ViewRotation.Pitch = FRotator::NormalizeAxis(ViewRotation.Pitch + CurrentBoneSway.Pitch + CameraAttachRotation.Pitch);
	ViewRotation.Yaw = FRotator::NormalizeAxis(ViewRotation.Yaw + CurrentBoneSway.Yaw + CameraAttachRotation.Yaw);
	ViewRotation.Roll = FRotator::NormalizeAxis(ViewRotation.Roll + CurrentBoneSway.Roll + CameraAttachRotation.Roll);
	Camera->SetWorldLocation(DesiredLocation);
	Camera->SetWorldRotation(ViewRotation);
}

/** 更新相机附件 */
void UCustomFPSViewComponent::UpdateCameraAttachment()
{
	ACharacter* Character = GetOwner<ACharacter>();
	if (!Character || !Camera)
	{
		return;
	}

	if (Camera->GetAttachParent() != Character->GetCapsuleComponent())
	{
		Camera->AttachToComponent(Character->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	if (ArmsAnchor && ArmsAnchor->GetAttachParent() != Character->GetCapsuleComponent())
	{
		ArmsAnchor->AttachToComponent(Character->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	FName ResolvedBone = NAME_None;
	USkeletalMeshComponent* BodyMesh = FindMeshForCameraBone(Character, CameraAttachBone, ResolvedBone);
	ActiveCameraBone = ResolvedBone;
	bDrivingCameraFromBone = BodyMesh != nullptr;

	const UWorld* World = Character->GetWorld();
	if (World && World->IsGameWorld())
	{
		bHasSmoothedLocalSway = false;
		return;
	}

	const FVector EyeLocation(CameraAttachOffset.X, CameraAttachOffset.Y, Character->BaseEyeHeight);
	if (ArmsAnchor)
	{
		ArmsAnchor->SetUsingAbsoluteLocation(false);
		ArmsAnchor->SetUsingAbsoluteRotation(false);
		ArmsAnchor->SetRelativeLocation(FVector(0.f, 0.f, Character->BaseEyeHeight));
		ArmsAnchor->SetRelativeRotation(FRotator::ZeroRotator);
	}

	// 没运行时，偏移只出现在相机上，手臂锚点留在眼睛高度。
	Camera->SetUsingAbsoluteLocation(false);
	Camera->SetUsingAbsoluteRotation(false);
	Camera->bUsePawnControlRotation = true;
	Camera->SetRelativeLocation(EyeLocation + FVector(0.f, 0.f, CameraAttachOffset.Z));
	Camera->SetRelativeRotation(CameraAttachRotation);
}
