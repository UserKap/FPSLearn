/*	
* 第一人称角色，负责角色的输入、移动、视角、枪械等动作。
* 包括初始化第一人称角色、绑定输入、移动、视角、枪械等组件。
*/

#include "CustomFPSCharacter.h"

#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

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

	// 枪挂在手臂插槽下。
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(FirstPersonMesh, FPSWeapon->GetWeaponAttachSocket());
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->SetCollisionProfileName(FName("NoCollision"));
	WeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	WeaponMesh->CastShadow = false;
	WeaponMesh->bCastDynamicShadow = false;
	FPSWeapon->SetMeshes(FirstPersonMesh, WeaponMesh);

	// 第三人称身体，自己看不见。
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
}

void ACustomFPSCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 属性面板的 PostEditChange 会重跑构造。当时再改组件会嵌套 PreEditChange 并触发断言。
	QueuePresentationRefresh();
}

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
	// 自己看不见身体，但镜头要读它的骨骼。不能因为没被渲染就停掉动画。
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	RefreshPresentation();
}

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

void ACustomFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (FPSInput)
	{
		FPSInput->BindInput(PlayerInputComponent);
	}
}

void ACustomFPSCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (FPSView)
	{
		FPSView->UpdateCameraAttachment();
	}
	if (FPSLocomotion)
	{
		FPSLocomotion->UpdateMovementSpeed();
	}
}

void ACustomFPSCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (FPSView)
	{
		FPSView->UpdateCameraAttachment();
	}
	if (FPSLocomotion)
	{
		FPSLocomotion->UpdateMovementSpeed();
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
