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
#include "CustomFPS/Locomotion/CustomFPSLocomotionComponent.h"
#include "CustomFPS/Weapon/CustomFPSWeaponComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/RichCurve.h"
#include "GameFramework/PlayerController.h"
#include "ReferenceSkeleton.h"

UCustomFPSViewComponent::UCustomFPSViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 必须早于 PlayerCameraManager 取景，否则这一帧画面还是旧位置。
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// 横轴 0 到 1，实际秒数是开镜速度。
	FRichCurve* KickCurve = AimKickCurve.GetRichCurve();
	auto AddLinearKey = [KickCurve](float Time, float Value)
	{
		const FKeyHandle Key = KickCurve->AddKey(Time, Value);
		KickCurve->SetKeyInterpMode(Key, RCIM_Linear, false);
	};
	AddLinearKey(0.f, 0.f);
	AddLinearKey(0.22f, 1.f);
	AddLinearKey(0.5f, -0.35f);
	AddLinearKey(1.f, 0.f);
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
	const float Duration = FMath::Max(AimDuration, 0.01f);
	AimSwayAlpha = FMath::Clamp(AimSwayAlpha + (bAiming ? DeltaTime : -DeltaTime) / Duration, 0.f, 1.f);
	const float NewFOV = FMath::Lerp(HipFieldOfView, AimFieldOfView, AimSwayAlpha);
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

float UCustomFPSViewComponent::GetAimKickAlpha() const
{
	const ACharacter* Character = GetOwner<ACharacter>();
	const UCustomFPSLocomotionComponent* Locomotion = Character ? Character->FindComponentByClass<UCustomFPSLocomotionComponent>() : nullptr;
	if (!Locomotion || !Locomotion->IsAiming())
	{
		return 0.f;
	}

	const FRichCurve* KickCurve = AimKickCurve.GetRichCurveConst();
	if (!KickCurve || KickCurve->GetNumKeys() == 0)
	{
		return 0.f;
	}

	return KickCurve->Eval(FMath::Clamp(AimSwayAlpha, 0.f, 1.f));
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

void UCustomFPSViewComponent::WaitForMeshPose(USkeletalMeshComponent* Mesh, TWeakObjectPtr<USkeletalMeshComponent>& Registered)
{
	if (!Mesh)
	{
		return;
	}

	// 手臂和枪先算完这一帧的走路姿势，镜头再读 AimSocket。读到的是上一帧插槽时，枪会比准星快一拍。
	// 这里只等姿势结束。锚点移动时子组件已经跟着更新，不再另算一次世界坐标。
	if (Registered.Get() != Mesh)
	{
		PrimaryComponentTick.AddPrerequisite(Mesh, Mesh->PrimaryComponentTick);
		Registered = Mesh;
	}
	Mesh->HandleExistingParallelEvaluationTask(true, true);
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
FRotator UCustomFPSViewComponent::ComputeBoneSway(const USkeletalMeshComponent* BodyMesh, float SwayScale, float MaxAngle) const
{
	if (!BodyMesh || ActiveCameraBone.IsNone() || SwayScale <= 0.f || MaxAngle <= 0.f || !BodyMesh->DoesSocketExist(ActiveCameraBone))
	{
		return FRotator::ZeroRotator;
	}

	const FQuat ReferenceRotation = GetComponentSpaceReferencePose(BodyMesh, ActiveCameraBone).GetRotation();
	const FQuat CurrentRotation = BodyMesh->GetSocketTransform(ActiveCameraBone, RTS_Component).GetRotation();
	const FQuat ComponentDelta = CurrentRotation * ReferenceRotation.Inverse();
	const FQuat MeshRotation = BodyMesh->GetComponentQuat();
	const FRotator WorldDelta = (MeshRotation * ComponentDelta * MeshRotation.Inverse()).Rotator();

	return FRotator(
		FMath::Clamp(FRotator::NormalizeAxis(WorldDelta.Pitch) * SwayScale, -MaxAngle, MaxAngle),
		FMath::Clamp(FRotator::NormalizeAxis(WorldDelta.Yaw) * SwayScale, -MaxAngle, MaxAngle),
		FMath::Clamp(FRotator::NormalizeAxis(WorldDelta.Roll) * SwayScale, -MaxAngle, MaxAngle));
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
	FVector ReferenceWorld = EyeLocation;
	FVector LiveDeltaWorld = FVector::ZeroVector;
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
			LiveDeltaWorld = AnimatedWorld - ReferenceWorld;
			const float RotationScale = FMath::Lerp(BoneSwayScale, AimBoneSwayScale, AimSwayAlpha);
			const float RotationLimit = FMath::Lerp(MaxBoneSwayAngle, AimMaxBoneSwayAngle, AimSwayAlpha);
			DesiredSway = ComputeBoneSway(BodyMesh, RotationScale, RotationLimit);
		}
	}

	const FRotator YawRotation(0.f, Character->GetActorRotation().Yaw, 0.f);
	const FRotator LookRotation = Character->GetControlRotation();
	// Head 被拉向 AimSocket 时，这段位移不是身体晃动。瞄准过程中冻结开镜前的差值，避免枪和镜头追着瞄点往前冲。
	const FVector LiveDeltaLocal = YawRotation.UnrotateVector(LiveDeltaWorld);
	if (AimSwayAlpha <= KINDA_SMALL_NUMBER)
	{
		CachedLocalBoneDelta = LiveDeltaLocal;
	}
	const FVector BlendedLocalDelta = FMath::Lerp(CachedLocalBoneDelta, FVector::ZeroVector, AimSwayAlpha);
	const float LocationScale = FMath::Lerp(BoneLocationSwayScale, AimBoneLocationSwayScale, AimSwayAlpha);
	const FVector LocalSway = BlendedLocalDelta * LocationScale;

	if (ArmsAnchor)
	{
		ArmsAnchor->SetUsingAbsoluteLocation(true);
		ArmsAnchor->SetUsingAbsoluteRotation(true);
		ArmsAnchor->SetWorldLocation(ReferenceWorld + YawRotation.RotateVector(BlendedLocalDelta));
		ArmsAnchor->SetWorldRotation(LookRotation);
	}

	// 平滑只作用在骨骼晃动上。参考位置每帧跟着角色走，跑步不会把镜头落在枪托后面。
	const float LocationLag = FMath::Lerp(CameraLocationLagSpeed, AimCameraLocationLagSpeed, AimSwayAlpha);
	if (!bHasSmoothedLocalSway || LocationLag <= 0.f)
	{
		SmoothedLocalSway = LocalSway;
		bHasSmoothedLocalSway = true;
	}
	else
	{
		SmoothedLocalSway = FMath::VInterpTo(SmoothedLocalSway, LocalSway, DeltaTime, LocationLag);
	}
	// 腰射停在参考姿势加偏移。开镜时镜头移到武器 AimSocket，插槽就是瞄准位置。
	// 手臂锚点不跟着这个点走，枪不会被镜头带着跑。
	const float HipWeight = 1.f - AimSwayAlpha;
	const FVector HipLocation = ReferenceWorld
		+ YawRotation.RotateVector(SmoothedLocalSway)
		+ FRotationMatrix(YawRotation).TransformVector(CameraAttachOffset * HipWeight);
	const UCustomFPSWeaponComponent* Weapons = Character->FindComponentByClass<UCustomFPSWeaponComponent>();
	const ACustomFPSWeapon* Weapon = Weapons ? Weapons->GetEquippedWeapon() : nullptr;
	if (Weapon)
	{
		if (USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh())
		{
			if (USkeletalMeshComponent* ArmsMesh = Cast<USkeletalMeshComponent>(WeaponMesh->GetAttachParent()))
			{
				WaitForMeshPose(ArmsMesh, PosePrerequisiteArms);
				if (PosePrerequisiteWeapon.Get() != WeaponMesh)
				{
					WeaponMesh->PrimaryComponentTick.AddPrerequisite(ArmsMesh, ArmsMesh->PrimaryComponentTick);
				}
			}
			WaitForMeshPose(WeaponMesh, PosePrerequisiteWeapon);
		}
	}
	const FVector AimPoint = Weapon ? Weapon->GetAimSocketLocation() : HipLocation;
	FVector DesiredLocation = FMath::Lerp(HipLocation, AimPoint, AimSwayAlpha);

	const float RotationLag = FMath::Lerp(CameraRotationLagSpeed, AimCameraRotationLagSpeed, AimSwayAlpha);
	CurrentBoneSway = RotationLag <= 0.f
		? DesiredSway
		: FMath::RInterpTo(CurrentBoneSway, DesiredSway, DeltaTime, RotationLag);

	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->bUsePawnControlRotation = false;

	FRotator ViewRotation = Character->GetControlRotation();
	const FRotator HipRotationOffset = CameraAttachRotation * HipWeight;
	const float KickAlpha = GetAimKickAlpha();
	const FRotator KickRotation = AimKickRotation * KickAlpha;
	ViewRotation.Pitch = FRotator::NormalizeAxis(ViewRotation.Pitch + CurrentBoneSway.Pitch + HipRotationOffset.Pitch + KickRotation.Pitch);
	ViewRotation.Yaw = FRotator::NormalizeAxis(ViewRotation.Yaw + CurrentBoneSway.Yaw + HipRotationOffset.Yaw + KickRotation.Yaw);
	ViewRotation.Roll = FRotator::NormalizeAxis(ViewRotation.Roll + CurrentBoneSway.Roll + HipRotationOffset.Roll + KickRotation.Roll);
	DesiredLocation += LookRotation.RotateVector(AimKickLocation * KickAlpha);
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
		CachedLocalBoneDelta = FVector::ZeroVector;
		AimSwayAlpha = 0.f;
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
