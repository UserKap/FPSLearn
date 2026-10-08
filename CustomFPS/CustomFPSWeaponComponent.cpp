/*	
* 枪械组件，负责枪械的部件、枪械的附件、枪械的动画等动作。
* 包括初始化枪械组件、设置枪械部件、设置枪械附件、刷新枪械等方法。
*/

#include "CustomFPSWeaponComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

UCustomFPSWeaponComponent::UCustomFPSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

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

void UCustomFPSWeaponComponent::SetMeshes(USkeletalMeshComponent* InArms, USkeletalMeshComponent* InWeapon)
{
	ArmsMesh = InArms;
	WeaponMesh = InWeapon;
}

void UCustomFPSWeaponComponent::Refresh()
{
	AttachToArms();
	RebuildParts();
}

void UCustomFPSWeaponComponent::AttachToArms()
{
	if (!WeaponMesh || !ArmsMesh)
	{
		return;
	}

	if (WeaponMesh->GetAttachParent() == ArmsMesh && WeaponMesh->GetAttachSocketName() == WeaponAttachSocket)
	{
		return;
	}

	WeaponMesh->AttachToComponent(ArmsMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocket);
}

void UCustomFPSWeaponComponent::RebuildParts()
{
	AActor* Owner = GetOwner();
	if (!WeaponMesh || !Owner || Owner->HasAnyFlags(RF_ClassDefaultObject))
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

		const FName ComponentName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), Desc.PartName.IsNone() ? TEXT("WeaponPart") : Desc.PartName);
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, ComponentName);
		Part->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Part->SetOnlyOwnerSee(true);
		Part->SetCollisionProfileName(FName("NoCollision"));
		Part->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		Part->CastShadow = false;
		Part->bCastDynamicShadow = false;
		Part->SetStaticMesh(Desc.Mesh);
		Part->SetupAttachment(WeaponMesh, Desc.AttachSocket);
		Part->RegisterComponent();
		WeaponPartComponents.Add(Part);
	}
}

#if WITH_EDITOR
void UCustomFPSWeaponComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	if (MemberName == GET_MEMBER_NAME_CHECKED(UCustomFPSWeaponComponent, WeaponParts)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UCustomFPSWeaponComponent, WeaponAttachSocket))
	{
		Refresh();
	}
}
#endif
