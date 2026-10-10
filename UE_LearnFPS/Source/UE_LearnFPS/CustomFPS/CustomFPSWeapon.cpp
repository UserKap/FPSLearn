/*
* 武器基类，负责自己的网格、部件，以及装到角色手上。
*/

#include "CustomFPSWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ACustomFPSWeapon::ACustomFPSWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionProfileName(FName("NoCollision"));
	WeaponMesh->CastShadow = false;
	WeaponMesh->bCastDynamicShadow = false;

	FWeaponPartDesc Barrel;
	Barrel.PartName = TEXT("Barrel");
	Barrel.AttachSocket = TEXT("Barrel");
	WeaponParts.Add(Barrel);

	FWeaponPartDesc Stock;
	Stock.PartName = TEXT("Stock");
	Stock.AttachSocket = TEXT("Stock");
	WeaponParts.Add(Stock);

	FWeaponPartDesc Muzzle;
	Muzzle.PartName = TEXT("Muzzle");
	Muzzle.AttachSocket = TEXT("Muzzle");
	WeaponParts.Add(Muzzle);
}

void ACustomFPSWeapon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildParts();
}

FVector ACustomFPSWeapon::GetAimSocketLocation() const
{
	if (!WeaponMesh)
	{
		return GetActorLocation();
	}

	if (!AimSocket.IsNone() && WeaponMesh->DoesSocketExist(AimSocket))
	{
		return WeaponMesh->GetSocketLocation(AimSocket);
	}

	return WeaponMesh->GetComponentLocation();
}

/* 装备武器 */
void ACustomFPSWeapon::Equip(USceneComponent* AttachParent)
{
	if (!AttachParent)
	{
		return;
	}

	SetOwner(AttachParent->GetOwner());
	// 含缩放贴到插槽，相对缩放保持 1。手臂网格如果是 0.25，不含缩放会把世界缩放 1 反算成相对缩放 4。
	AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
	ApplyFirstPersonView();
	RebuildParts();
	bEquipped = true;
	OnEquipped();
}

/* 卸下武器 */
void ACustomFPSWeapon::Unequip()
{
	if (!bEquipped)
	{
		return;
	}

	bEquipped = false;
	OnUnequipped();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

/* 应用第一人称视角 */
void ACustomFPSWeapon::ApplyFirstPersonView()
{
	if (!WeaponMesh)
	{
		return;
	}

	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	WeaponMesh->SetCollisionProfileName(FName("NoCollision"));
	WeaponMesh->CastShadow = false;
	WeaponMesh->bCastDynamicShadow = false;

	for (UStaticMeshComponent* Part : WeaponPartComponents)
	{
		if (!IsValid(Part))
		{
			continue;
		}

		Part->SetOnlyOwnerSee(true);
		Part->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		Part->SetCollisionProfileName(FName("NoCollision"));
		Part->CastShadow = false;
		Part->bCastDynamicShadow = false;
	}
}

/* 重建部件 */
void ACustomFPSWeapon::RebuildParts()
{
	if (!WeaponMesh || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	for (UStaticMeshComponent* Part : WeaponPartComponents)
	{
		if (IsValid(Part))
		{
			Part->DestroyComponent();
		}
	}
	WeaponPartComponents.Reset();

	for (const FWeaponPartDesc& Desc : WeaponParts)
	{
		if (Desc.PartName.IsNone() && Desc.AttachSocket.IsNone() && !Desc.Mesh)
		{
			continue;
		}

		const FName ComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), Desc.PartName.IsNone() ? TEXT("WeaponPart") : Desc.PartName);
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, ComponentName);
		Part->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Part->SetCollisionProfileName(FName("NoCollision"));
		Part->CastShadow = false;
		Part->bCastDynamicShadow = false;
		if (bEquipped)
		{
			Part->SetOnlyOwnerSee(true);
			Part->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		}
		Part->SetStaticMesh(Desc.Mesh);
		Part->SetupAttachment(WeaponMesh, Desc.AttachSocket);
		Part->RegisterComponent();
		WeaponPartComponents.Add(Part);
	}
}