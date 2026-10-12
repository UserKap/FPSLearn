/*
* 武器基类，负责自己的网格、部件，以及装到角色手上。
*/

#include "CustomFPSWeapon.h"

#include "CustomFPSWeaponHolder.h"
#include "UE_LearnFPS.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
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

/* 建造武器 */
void ACustomFPSWeapon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildParts();
}

void ACustomFPSWeapon::BeginPlay()
{
	Super::BeginPlay();
	AmmoInMagazine = MagazineSize;
	bChamberReady = true;
}

void ACustomFPSWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFireTimers();
	Super::EndPlay(EndPlayReason);
}

/* 是否可以开火 */
bool ACustomFPSWeapon::CanFire() const
{
	if (AmmoInMagazine <= 0 || (FireMode == EWeaponFireMode::Manual && !bChamberReady))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World || LastShotTime < 0.f)
	{
		return true;
	}

	const float Interval = 60.f / FMath::Max(FireRate, 1.f);
	return World->GetTimeSeconds() - LastShotTime + KINDA_SMALL_NUMBER >= Interval;
}

/* 换弹 */
bool ACustomFPSWeapon::Reload()
{
	const int32 Room = MagazineSize - AmmoInMagazine;
	if (Room <= 0 || ReserveAmmo <= 0)
	{
		return false;
	}

	const int32 Wanted = FireMode == EWeaponFireMode::Manual ? 1 : Room;
	const int32 Moved = FMath::Min(Wanted, ReserveAmmo);
	AmmoInMagazine += Moved;
	ReserveAmmo -= Moved;
	return true;
}

/* 循环开火 */
bool ACustomFPSWeapon::CycleBolt()
{
	if (FireMode != EWeaponFireMode::Manual || bChamberReady || bCyclingBolt)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || BoltDuration <= 0.f)
	{
		bChamberReady = true;
		return true;
	}

	bCyclingBolt = true;
	World->GetTimerManager().SetTimer(BoltTimer, this, &ACustomFPSWeapon::FinishBolt, BoltDuration, false);
	return true;
}

/* 完成开火 */
void ACustomFPSWeapon::FinishBolt()
{
	bCyclingBolt = false;
	bChamberReady = true;
}

/* 清除开火计时器 */
void ACustomFPSWeapon::ClearFireTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimer);
		World->GetTimerManager().ClearTimer(BoltTimer);
	}
}

/* 发射子弹 */
void ACustomFPSWeapon::FireRound(AActor* RequestedBy)
{
	const UWorld* World = GetWorld();
	LastShotTime = World ? World->GetTimeSeconds() : 0.f;
	--AmmoInMagazine;
	if (FireMode == EWeaponFireMode::Manual)
	{
		bChamberReady = false;
	}

	UE_LOG(LogUE_LearnFPS, Log, TEXT("True"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Green, TEXT("True"));
	}
	PlayFireAnimations();
	OnRoundFired(RequestedBy);
}

void ACustomFPSWeapon::PlayFireAnimations()
{
	AActor* HolderActor = GetOwner();
	const bool bHasHolder = HolderActor && HolderActor->GetClass()->ImplementsInterface(UCustomFPSWeaponHolder::StaticClass());
	if (bHasHolder && ICustomFPSWeaponHolder::Execute_IsHolderSprinting(HolderActor))
	{
		return;
	}

	UAnimMontage* CharacterMontage = IdleCharacterFire;
	UAnimMontage* WeaponMontage = IdleWeaponFire;
	if (bHasHolder && ICustomFPSWeaponHolder::Execute_IsHolderAiming(HolderActor))
	{
		CharacterMontage = AimCharacterFire;
		WeaponMontage = AimWeaponFire;
	}
	else if (bHasHolder && ICustomFPSWeaponHolder::Execute_IsHolderMoving(HolderActor))
	{
		CharacterMontage = MoveCharacterFire ? MoveCharacterFire.Get() : IdleCharacterFire.Get();
		WeaponMontage = MoveWeaponFire ? MoveWeaponFire.Get() : IdleWeaponFire.Get();
	}

	if (WeaponMesh && WeaponMontage)
	{
		if (UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(WeaponMontage);
		}
	}

	if (bHasHolder)
	{
		ICustomFPSWeaponHolder::Execute_PlayFireAnimation(HolderActor, CharacterMontage);
	}
}

/* 计划自动开火 */
void ACustomFPSWeapon::ScheduleAutoFire()
{
	UWorld* World = GetWorld();
	if (!World || FireMode != EWeaponFireMode::FullAuto || !bTriggerHeld || AmmoInMagazine <= 0)
	{
		return;
	}

	const float Interval = 60.f / FMath::Max(FireRate, 1.f);
	World->GetTimerManager().SetTimer(AutoFireTimer, this, &ACustomFPSWeapon::HandleAutoFire, Interval, false);
}

/* 处理自动开火 */
void ACustomFPSWeapon::HandleAutoFire()
{
	if (!bTriggerHeld || FireMode != EWeaponFireMode::FullAuto || AmmoInMagazine <= 0)
	{
		return;
	}

	FireRound(ShotRequester.Get());
	ScheduleAutoFire();
}

/* 开始开火 */
void ACustomFPSWeapon::StartFire_Implementation(AActor* RequestedBy)
{
	bTriggerHeld = true;
	ShotRequester = RequestedBy;
	if (!CanFire())
	{
		return;
	}

	FireRound(RequestedBy);
	if (FireMode == EWeaponFireMode::FullAuto)
	{
		ScheduleAutoFire();
	}
}

/* 停止开火 */
void ACustomFPSWeapon::StopFire_Implementation(AActor* /*RequestedBy*/)
{
	bTriggerHeld = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimer);
	}
}

/* 取得瞄准插槽位置 */
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
// AttachParent 是手臂网格的插槽，不是手臂网格组件。
void ACustomFPSWeapon::Equip(USceneComponent* AttachParent)
{
	if (!AttachParent)
	{
		return;
	}

	SetOwner(AttachParent->GetOwner());
	// 取得持枪者接口，持枪者是玩家、其他玩家或 AI。
	WeaponHolder = Cast<ICustomFPSWeaponHolder>(GetOwner());
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

	StopFire(GetOwner());
	bEquipped = false;
	WeaponHolder = nullptr;
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