/*	
* 第一人称角色，负责角色的输入、移动、视角、枪械等动作。
* 包括初始化第一人称角色、绑定输入、移动、视角、枪械等组件。
*/

#include "CustomFPSCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
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

	// 相机先挂在眼睛高度，构造完成后会再对齐到身体骨骼。
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	FPSView->SetupCamera(FirstPersonCameraComponent);

	// 手臂挂在相机下，跟随头部。
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCameraComponent);
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
	RefreshPresentation();
}

void ACustomFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
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
	if (FPSView)
	{
		FPSView->UpdateCameraAttachment();
	}
	if (FPSWeapon)
	{
		FPSWeapon->Refresh();
	}
}
