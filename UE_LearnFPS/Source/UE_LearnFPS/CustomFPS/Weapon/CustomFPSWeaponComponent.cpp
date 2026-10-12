/*
* 角色上的装备槽。只负责生成武器并交给武器自己装到手臂上。
*/

#include "CustomFPSWeaponComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

/* 构造函数 */
UCustomFPSWeaponComponent::UCustomFPSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* 设置手臂 */
void UCustomFPSWeaponComponent::SetArms(USkeletalMeshComponent* InArms)
{
	ArmsMesh = InArms;
}

/* 结束播放 */
void UCustomFPSWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearEquippedWeapon();
	Super::EndPlay(EndPlayReason);
}

/* 刷新武器 */
void UCustomFPSWeaponComponent::Refresh()
{
	if (!WeaponClass)
	{
		ClearEquippedWeapon();
		return;
	}

	EquipWeaponActor();
}

/* 生成武器Actor并装备到手臂上 */
void UCustomFPSWeaponComponent::EquipWeaponActor()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!WeaponClass || !ArmsMesh || !Owner || !World || Owner->HasAnyFlags(RF_ClassDefaultObject) || !World->IsGameWorld())
	{
		return;
	}

	if (EquippedWeapon && EquippedWeapon->GetClass() != WeaponClass)
	{
		ClearEquippedWeapon();
	}

	if (!EquippedWeapon)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.Instigator = Cast<APawn>(Owner);
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		EquippedWeapon = World->SpawnActor<ACustomFPSWeapon>(WeaponClass, SpawnParams);
	}

	if (EquippedWeapon)
	{
		EquippedWeapon->Equip(ArmsMesh);
	}
}

/* 卸下武器 */
void UCustomFPSWeaponComponent::ClearEquippedWeapon()
{
	if (!EquippedWeapon)
	{
		return;
	}

	EquippedWeapon->Unequip();
	EquippedWeapon->Destroy();
	EquippedWeapon = nullptr;
}

/* 编辑器下刷新武器 */
#if WITH_EDITOR
void UCustomFPSWeaponComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UCustomFPSWeaponComponent, WeaponClass))
	{
		Refresh();
	}
}
#endif
