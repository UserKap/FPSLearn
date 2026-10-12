/*	
* 第一人称角色，负责角色的输入、移动、视角、枪械等动作。
* 包括初始化第一人称角色、绑定输入、移动、视角、枪械等组件。
*/

#include "CustomFPSCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UObjectIterator.h"

ACustomFPSCharacter::ACustomFPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	BaseEyeHeight = 64.f;
	CrouchedEyeHeight = 44.f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	FPSInput = CreateDefaultSubobject<UCustomFPSInputComponent>(TEXT("FPSInput"));
	FPSLocomotion = CreateDefaultSubobject<UCustomFPSLocomotionComponent>(TEXT("FPSLocomotion"));
	FPSView = CreateDefaultSubobject<UCustomFPSViewComponent>(TEXT("FPSView"));
	FPSWeapon = CreateDefaultSubobject<UCustomFPSWeaponComponent>(TEXT("FPSWeapon"));
	FPSLocomotion->InitializeMovement(GetCharacterMovement());

	// 相机独立挂在胶囊体上，运行时再对齐到骨骼节点，晃动只加在它自己身上。
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	FPSView->SetupCamera(FirstPersonCameraComponent);

	// 手臂锚点和相机同级。瞄准跟着锚点，相机晃动不会带动枪。
	ArmsAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ArmsAnchor"));
	ArmsAnchor->SetupAttachment(GetCapsuleComponent());
	ArmsAnchor->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	FPSView->SetupArmsAnchor(ArmsAnchor);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(ArmsAnchor);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->bCastDynamicShadow = false;
	FPSWeapon->SetArms(FirstPersonMesh);

	// 第三人称身体
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
}

void ACustomFPSCharacter::RepairViewComponent()
{
	UCustomFPSViewComponent* Found = FindObject<UCustomFPSViewComponent>(this, TEXT("FPSView"));
	if (!Found)
	{
		TInlineComponentArray<UCustomFPSViewComponent*> Views(this);
		Found = Views.Num() > 0 ? Views[0] : nullptr;
	}
	if (!Found)
	{
		return;
	}
	if (FPSView != Found)
	{
		FPSView = Found;
	}
}

void ACustomFPSCharacter::RepairAllViewReferences()
{
	for (TObjectIterator<ACustomFPSCharacter> It; It; ++It)
	{
		It->RepairViewComponent();
	}
}

static FDelegateHandle GRepairViewReloadHandle = FCoreUObjectDelegates::ReloadCompleteDelegate.AddLambda(
	[](EReloadCompleteReason) { ACustomFPSCharacter::RepairAllViewReferences(); });
static FDelegateHandle GRepairViewReinstanceHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddLambda(
	[](const FCoreUObjectDelegates::FReplacementObjectMap&) { ACustomFPSCharacter::RepairAllViewReferences(); });

void ACustomFPSCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RepairViewComponent();
	QueuePresentationRefresh();
}

void ACustomFPSCharacter::PostLoad()
{
	Super::PostLoad();
	RepairViewComponent();
}

#if WITH_EDITOR
void ACustomFPSCharacter::PostCDOCompiled(const FPostCDOCompiledContext& Context)
{
	Super::PostCDOCompiled(Context);
	RepairViewComponent();
}
#endif

void ACustomFPSCharacter::QueuePresentationRefresh()
{
	if (bPresentationRefreshQueued)
	{
		return;
	}

	bPresentationRefreshQueued = true;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		bPresentationRefreshQueued = false;
		RefreshPresentation();
		return false;
	}));
}

void ACustomFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	RepairViewComponent();
	// 自己看不见身体，但镜头要读它的骨骼。不能因为没被渲染就停掉动画。
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	RefreshPresentation();
}

/* 客户端重连 */
void ACustomFPSCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (FPSInput)
	{
		FPSInput->AddMappingContext();
	}
}

void ACustomFPSCharacter::UnPossessed()
{
	if (FPSInput)
	{
		FPSInput->RemoveMappingContext();
	}
	Super::UnPossessed();
}

/* 开始持枪开火 */
void ACustomFPSCharacter::StartWeaponFire_Implementation(AActor* RequestedBy)
{
	StartFire();
	if (!bIsFiring)
	{
		return;
	}

	if (ACustomFPSWeapon* Weapon = GetEquippedWeapon())
	{
		Weapon->StartFire(RequestedBy ? RequestedBy : this);
	}
}

void ACustomFPSCharacter::PlayFireAnimation_Implementation(UAnimMontage* Montage)
{
	if (!Montage || !FirstPersonMesh)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
	{
		AnimInstance->Montage_Play(Montage);
	}
}

bool ACustomFPSCharacter::IsHolderAiming_Implementation()
{
	return IsAiming();
}

bool ACustomFPSCharacter::IsHolderSprinting_Implementation()
{
	return FPSLocomotion && FPSLocomotion->IsSprinting();
}

bool ACustomFPSCharacter::IsHolderMoving_Implementation()
{
	return GetSpeed() > 10.f;
}

void ACustomFPSCharacter::StopWeaponFire_Implementation(AActor* RequestedBy)
{
	if (ACustomFPSWeapon* Weapon = GetEquippedWeapon())
	{
		Weapon->StopFire(RequestedBy ? RequestedBy : this);
	}

	StopFire();
}

void ACustomFPSCharacter::StartFire()
{
	if (bIsDead)
	{
		return;
	}

	bIsFiring = true;
}

void ACustomFPSCharacter::StopFire()
{
	bIsFiring = false;
}

void ACustomFPSCharacter::SetDead(bool bDead)
{
	if (bIsDead == bDead)
	{
		return;
	}

	bIsDead = bDead;
	if (!bIsDead)
	{
		return;
	}

	ICustomFPSWeaponHolder::Execute_StopWeaponFire(this, this);
	if (FPSLocomotion)
	{
		FPSLocomotion->StopAim();
		FPSLocomotion->StopSprint();
		FPSLocomotion->StopCrouch();
	}
	if (FPSView)
	{
		FPSView->Lean(0.f);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

void ACustomFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (FPSInput)
	{
		FPSInput->BindInput(PlayerInputComponent);
	}
}

void ACustomFPSCharacter::RefreshPresentation()
{
	if (FirstPersonMesh && ArmsAnchor && FirstPersonMesh->GetAttachParent() != ArmsAnchor)
	{
		FirstPersonMesh->AttachToComponent(ArmsAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	if (FPSView)
	{
		FPSView->UpdateCameraAttachment();
	}
	if (FPSWeapon)
	{
		FPSWeapon->Refresh();
	}
}
