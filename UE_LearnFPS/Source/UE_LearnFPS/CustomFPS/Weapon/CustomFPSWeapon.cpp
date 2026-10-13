/*
* 武器基类，负责自己的网格、部件，以及装到角色手上。
*/

#include "CustomFPSWeapon.h"

#include "CustomFPSWeaponHolder.h"
#include "CustomFPS/Weapon/CustomFPSRecoilData.h"
#include "UE_LearnFPS.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MetasoundSource.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FObjectFinder<UMetaSoundSource> FireSoundAsset(TEXT("/Game/MetaSound/Weapon/00_Mcx/MS_00_MCX.MS_00_MCX"));
	if (FireSoundAsset.Succeeded())
	{
		FireSound = FireSoundAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FireEffectAsset(TEXT("/Game/ShotSystem/Shot_VFX/VIRTUS_MuzzleFlash.VIRTUS_MuzzleFlash"));
	if (FireEffectAsset.Succeeded())
	{
		FireEffect = FireEffectAsset.Object;
	}
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

	// 先按这一帧的枪口打射线，再播动画和加后坐，弹道不受这一发自己的后坐影响。
	FireShot();
	PlayFireAnimations();
	PlayFireSound();
	PlayFireEffect();
	ApplyRecoil();
	OnRoundFired(RequestedBy);
	EjectSpentCasing();
}

/* 枪管位置和朝向 */
bool ACustomFPSWeapon::ResolveBore(FVector& OutLocation, FVector& OutDirection) const
{
	if (!WeaponMesh)
	{
		return false;
	}

	// 枪管部件的枪口插槽 X 轴就是枪管方向，会跟着骨骼动画动。
	USceneComponent* MuzzleComponent = nullptr;
	FName MuzzleSocket = NAME_None;
	if (ResolveMuzzle(MuzzleComponent, MuzzleSocket))
	{
		const FTransform Muzzle = MuzzleComponent->GetSocketTransform(MuzzleSocket, RTS_World);
		OutLocation = Muzzle.GetLocation();
		OutDirection = Muzzle.GetUnitAxis(EAxis::X);
		return true;
	}

	// 没有枪管部件时退回武器网格。网格自己的 Muzzle 插槽轴向朝后，只取位置，方向用网格前方。
	OutLocation = !FireSoundSocket.IsNone() && WeaponMesh->DoesSocketExist(FireSoundSocket)
		? WeaponMesh->GetSocketLocation(FireSoundSocket)
		: WeaponMesh->GetComponentLocation();
	OutDirection = WeaponMesh->GetForwardVector();
	return true;
}

/* 枪口瞄准 */
bool ACustomFPSWeapon::GetShotAim(FVector& OutMuzzle, FVector& OutDirection) const
{
	if (!ResolveBore(OutMuzzle, OutDirection))
	{
		OutMuzzle = GetActorLocation();
		OutDirection = GetActorForwardVector();
		return false;
	}
	return true;
}

/* 瞄准线 */
bool ACustomFPSWeapon::GetSightLine(FVector& OutEye, FVector& OutDirection) const
{
	FVector Up;
	return ResolveSightLine(OutEye, OutDirection, Up);
}

bool ACustomFPSWeapon::ResolveSightLine(FVector& OutEye, FVector& OutDirection, FVector& OutUp) const
{
	// 瞄准插槽的轴向不可靠，只取位置。
	OutEye = GetAimSocketLocation();
	FVector Muzzle;
	FVector Bore;
	if (!ResolveBore(Muzzle, Bore))
	{
		OutDirection = GetActorForwardVector();
		OutUp = GetActorUpVector();
		return false;
	}

	FVector Sight = (Muzzle + Bore * ZeroDistance - OutEye).GetSafeNormal();
	if (Sight.IsNearlyZero())
	{
		Sight = Bore;
	}

	// 瞄具在枪管正上方，用它和枪口的偏移定出枪的上方，不依赖插槽轴向。
	OutUp = FVector::VectorPlaneProject(OutEye - Muzzle, Bore).GetSafeNormal();
	if (OutUp.IsNearlyZero())
	{
		OutUp = WeaponMesh ? WeaponMesh->GetUpVector() : FVector::UpVector;
	}
	const FVector Right = FVector::CrossProduct(OutUp, Bore).GetSafeNormal();

	// 弹着点往上调等于红点往下指：举枪把红点对上目标时，枪管就抬高了。
	const float ElevationTan = FMath::Tan(FMath::DegreesToRadians(SightElevationMOA / 60.f));
	const float WindageTan = FMath::Tan(FMath::DegreesToRadians(SightWindageMOA / 60.f));
	OutDirection = (Sight - OutUp * ElevationTan - Right * WindageTan).GetSafeNormal();
	return true;
}

/* 更新红点 */
void ACustomFPSWeapon::UpdateReticle()
{
	UStaticMeshComponent* Part = FindWeaponPart(ReticlePart);
	if (!Part)
	{
		return;
	}

	FVector Eye;
	FVector Direction;
	FVector Up;
	ResolveSightLine(Eye, Direction, Up);
	// 下标要和镜片材质里 ReticleDirection、ReticleUp 的图元数据下标一致。
	Part->SetCustomPrimitiveDataVector4(0, FVector4(Direction, 0.f));
	Part->SetCustomPrimitiveDataVector4(4, FVector4(Up, 0.f));
}

float ACustomFPSWeapon::GetDecayedBloom() const
{
	const UWorld* World = GetWorld();
	if (!World || SpreadBloomTime < 0.f)
	{
		return 0.f;
	}
	return FMath::Max(0.f, SpreadBloom - BloomRecoveryRate * (World->GetTimeSeconds() - SpreadBloomTime));
}

/* 当前散布 */
float ACustomFPSWeapon::GetCurrentSpread() const
{
	const ACustomFPSBullet* Bullet = GetBulletDefaults();
	// 角分是散布直径，换成度再取一半当半角。
	float Spread = WeaponMOA * (Bullet ? Bullet->GetSpreadMultiplier() : 1.f) / 60.f * 0.5f + GetDecayedBloom();

	AActor* HolderActor = GetOwner();
	if (!HolderActor)
	{
		return Spread;
	}

	if (HolderActor->GetClass()->ImplementsInterface(UCustomFPSWeaponHolder::StaticClass()) && !ICustomFPSWeaponHolder::Execute_IsHolderAiming(HolderActor))
	{
		Spread += HipSpreadAngle;
	}

	const FVector Velocity = HolderActor->GetVelocity();
	Spread += MoveSpreadAngle * FMath::Clamp(FVector(Velocity.X, Velocity.Y, 0.f).Size() / FMath::Max(MoveSpreadFullSpeed, 1.f), 0.f, 1.f);

	const APawn* Pawn = Cast<APawn>(HolderActor);
	const UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	if (Movement && Movement->IsFalling())
	{
		Spread += AirSpreadAngle;
	}
	return Spread;
}

FVector ACustomFPSWeapon::ApplySpread(const FVector& Direction, float HalfAngleDegrees) const
{
	if (HalfAngleDegrees <= 0.f)
	{
		return Direction;
	}

	// 正态分布，标准差取半角的一半，再夹进半角：越靠中心越密，和真实弹着分布一样。
	const float Uniform = FMath::Max(FMath::FRand(), UE_SMALL_NUMBER);
	const float Radius = FMath::Min(FMath::Sqrt(-2.f * FMath::Loge(Uniform)) * 0.5f, 1.f) * HalfAngleDegrees;
	const float Angle = FMath::FRand() * 2.f * UE_PI;
	FVector AxisY;
	FVector AxisZ;
	Direction.FindBestAxisVectors(AxisY, AxisZ);
	const FVector Offset = (AxisY * FMath::Cos(Angle) + AxisZ * FMath::Sin(Angle)) * FMath::Tan(FMath::DegreesToRadians(Radius));
	return (Direction + Offset).GetSafeNormal();
}

/* 射线 */
void ACustomFPSWeapon::FireShot()
{
	UWorld* World = GetWorld();
	FVector Muzzle;
	FVector AimDirection;
	if (!World || !GetShotAim(Muzzle, AimDirection))
	{
		return;
	}

	// 先按开枪前的状态算散布，这一发自己的连射散布从下一发开始算。
	const float Spread = GetCurrentSpread();
	const FVector Direction = ApplySpread(AimDirection, Spread);
	SpreadBloom = FMath::Min(GetDecayedBloom() + BloomPerShot, BloomMax);
	SpreadBloomTime = World->GetTimeSeconds();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CustomFPSShot), false, this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	FVector TraceStart = Muzzle;
	const FVector TraceEnd = Muzzle + Direction * MaxRange;
	// 贴墙时枪口可能已经插进墙里。先从持枪者眼睛连到枪口，被挡住就算打在挡住的地方，子弹不穿墙。
	bool bHit = false;
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		bHit = World->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), Muzzle, ShotTraceChannel, QueryParams);
		if (bHit)
		{
			TraceStart = Hit.TraceStart;
		}
	}
	if (!bHit)
	{
		bHit = World->LineTraceSingleByChannel(Hit, Muzzle, TraceEnd, ShotTraceChannel, QueryParams);
	}

	if (bDrawShotDebug)
	{
		const FVector LineEnd = bHit ? Hit.ImpactPoint : TraceEnd;
		DrawDebugLine(World, TraceStart, LineEnd, bHit ? FColor::Red : FColor::Green, false, ShotDebugDuration, 0, 0.3f);
		if (bHit)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 10.f, FColor::Yellow, false, ShotDebugDuration);
		}
		if (GEngine)
		{
			const FString Message = bHit
				? FString::Printf(TEXT("命中 %s / %s  骨骼 %s  %.1f 米  散布 %.2f°"),
					*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.BoneName.ToString(), Hit.Distance / 100.f, Spread)
				: FString::Printf(TEXT("未命中  散布 %.2f°"), Spread);
			GEngine->AddOnScreenDebugMessage(-1, ShotDebugDuration, bHit ? FColor::Yellow : FColor::Green, Message);
		}
	}

	if (bHit)
	{
		OnShotHit(Hit);
	}
}

/* 后坐力 */
void ACustomFPSWeapon::ApplyRecoil()
{
	if (!RecoilData)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (RecoilLastShotTime < 0.f || Now - RecoilLastShotTime > RecoilData->PatternResetTime)
	{
		RecoilShotNumber = 0;
	}
	RecoilLastShotTime = Now;
	++RecoilShotNumber;

	AActor* HolderActor = GetOwner();
	if (!HolderActor || !HolderActor->GetClass()->ImplementsInterface(UCustomFPSWeaponHolder::StaticClass()))
	{
		return;
	}

	const bool bAiming = ICustomFPSWeaponHolder::Execute_IsHolderAiming(HolderActor);
	float Multiplier = 1.f;
	if (bAiming)
	{
		Multiplier *= RecoilData->AimMultiplier;
	}
	if (ICustomFPSWeaponHolder::Execute_IsHolderCrouching(HolderActor))
	{
		Multiplier *= RecoilData->CrouchMultiplier;
	}
	if (ICustomFPSWeaponHolder::Execute_IsHolderMoving(HolderActor))
	{
		Multiplier *= RecoilData->MoveMultiplier;
	}

	const FVector2D Pattern = RecoilData->EvaluatePattern(RecoilShotNumber);
	const FRotator ControlRise(
		(Pattern.X + FMath::FRandRange(-RecoilData->VerticalRandom, RecoilData->VerticalRandom)) * Multiplier,
		(Pattern.Y + FMath::FRandRange(-RecoilData->HorizontalRandom, RecoilData->HorizontalRandom)) * Multiplier,
		0.f);

	const float KickScale = Multiplier * (bAiming ? RecoilData->AimCameraKickScale : 1.f);
	const FRotator CameraKick(
		RecoilData->CameraKickPitch * KickScale,
		FMath::FRandRange(-RecoilData->CameraKickYaw, RecoilData->CameraKickYaw) * KickScale,
		FMath::FRandRange(-RecoilData->CameraKickRoll, RecoilData->CameraKickRoll) * KickScale);

	const float BackScale = Multiplier * (bAiming ? RecoilData->AimWeaponKickBackScale : 1.f);
	const float RotationScale = Multiplier * (bAiming ? RecoilData->AimWeaponKickRotationScale : 1.f);
	const FVector WeaponKickOffset(-RecoilData->WeaponKickBack * BackScale, 0.f, 0.f);
	const FRotator WeaponKickRotation(
		RecoilData->WeaponKickPitch * RotationScale,
		FMath::FRandRange(-RecoilData->WeaponKickYaw, RecoilData->WeaponKickYaw) * RotationScale,
		FMath::FRandRange(-RecoilData->WeaponKickRoll, RecoilData->WeaponKickRoll) * RotationScale);

	ICustomFPSWeaponHolder::Execute_ApplyWeaponRecoil(HolderActor, RecoilData.Get(), ControlRise, CameraKick, WeaponKickOffset, WeaponKickRotation);
}

const ACustomFPSBullet* ACustomFPSWeapon::GetBulletDefaults() const
{
	return BulletClass ? BulletClass->GetDefaultObject<ACustomFPSBullet>() : nullptr;
}

float ACustomFPSWeapon::GetFleshDamage() const
{
	const ACustomFPSBullet* Bullet = GetBulletDefaults();
	return Bullet ? Bullet->GetFleshDamage() : 0.f;
}

float ACustomFPSWeapon::GetArmorDamage() const
{
	const ACustomFPSBullet* Bullet = GetBulletDefaults();
	return Bullet ? Bullet->GetArmorDamage() : 0.f;
}

float ACustomFPSWeapon::GetFleshRetainAfterArmor() const
{
	const ACustomFPSBullet* Bullet = GetBulletDefaults();
	return Bullet ? Bullet->GetFleshRetainAfterArmor() : 0.f;
}

EBulletCaliber ACustomFPSWeapon::GetBulletCaliber() const
{
	const ACustomFPSBullet* Bullet = GetBulletDefaults();
	return Bullet ? Bullet->GetCaliber() : EBulletCaliber::Caliber_556x45;
}

EBulletProjectile ACustomFPSWeapon::GetBulletProjectile() const
{
	const ACustomFPSBullet* Bullet = GetBulletDefaults();
	return Bullet ? Bullet->GetProjectile() : EBulletProjectile::FMJ;
}

bool ACustomFPSWeapon::ResolveEjectionTransform(FTransform& OutTransform)
{
	OutTransform = WeaponMesh ? WeaponMesh->GetComponentTransform() : GetActorTransform();
	if (!WeaponMesh)
	{
		return false;
	}

	if (!EjectionSocket.IsNone() && WeaponMesh->DoesSocketExist(EjectionSocket))
	{
		OutTransform = WeaponMesh->GetSocketTransform(EjectionSocket, RTS_World);
		return true;
	}

	static const FName BoltBone(TEXT("Bolt"));
	if (WeaponMesh->GetBoneIndex(BoltBone) != INDEX_NONE)
	{
		if (!bWarnedMissingEjectionSocket)
		{
			bWarnedMissingEjectionSocket = true;
			UE_LOG(LogUE_LearnFPS, Warning, TEXT("%s 没有抛壳插槽 %s，改用骨骼 Bolt。"), *GetName(), *EjectionSocket.ToString());
		}
		OutTransform = WeaponMesh->GetSocketTransform(BoltBone, RTS_World);
		return true;
	}

	if (!bWarnedMissingEjectionSocket)
	{
		bWarnedMissingEjectionSocket = true;
		UE_LOG(LogUE_LearnFPS, Warning, TEXT("%s 没有抛壳插槽 %s，改从武器原点抛壳。"), *GetName(), *EjectionSocket.ToString());
	}
	return true;
}

/* 抛出弹壳 */
void ACustomFPSWeapon::EjectSpentCasing()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !BulletClass || !WeaponMesh)
	{
		return;
	}

	FTransform SpawnTransform;
	if (!ResolveEjectionTransform(SpawnTransform))
	{
		return;
	}

	const FVector Direction = EjectionDirection.GetSafeNormal();
	const FVector WorldDirection = SpawnTransform.GetRotation().RotateVector(Direction.IsNearlyZero() ? FVector::RightVector : Direction);
	SpawnTransform.AddToTranslation(WorldDirection * 2.f);

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACustomFPSBullet* Casing = World->SpawnActor<ACustomFPSBullet>(BulletClass, SpawnTransform, Params);
	if (!Casing)
	{
		return;
	}

	const AActor* Mover = GetOwner() ? GetOwner() : this;
	const FVector InheritedVelocity = Mover->GetVelocity() * EjectionInheritVelocity;
	const FVector Spin = FMath::VRand() * EjectionSpin;
	if (!Casing->EjectCasing(InheritedVelocity + WorldDirection * EjectionSpeed, Spin))
	{
		Casing->Destroy();
	}
}

void ACustomFPSWeapon::PlayFireSound()
{
	if (!FireSound)
	{
		return;
	}

	USceneComponent* AttachParent = nullptr;
	FName Socket = NAME_None;
	if (!ResolveMuzzle(AttachParent, Socket))
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
		return;
	}

	UGameplayStatics::SpawnSoundAttached(FireSound, AttachParent, Socket);
}

void ACustomFPSWeapon::PlayFireEffect()
{
	if (!FireEffect)
	{
		return;
	}

	USceneComponent* AttachParent = nullptr;
	FName Socket = NAME_None;
	if (!ResolveMuzzle(AttachParent, Socket))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FireEffect, GetActorLocation());
		return;
	}

	// 先挂上但不激活。第一人称枪口特效如果先在世界通道里爆开，这一发就过去了，镜头里看不到。
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		FireEffect,
		AttachParent,
		Socket,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true,
		false,
		ENCPoolMethod::None,
		false);
	if (!Effect)
	{
		UE_LOG(LogUE_LearnFPS, Warning, TEXT("%s failed to spawn fire effect %s."), *GetName(), *GetNameSafe(FireEffect));
		return;
	}

	// 和武器网格同一条第一人称通道。
	Effect->SetForceLocalPlayerEffect(true);
	Effect->SetOnlyOwnerSee(true);
	Effect->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Effect->SetTranslucentSortPriority(100);
	Effect->SetBoundsScale(20.f);
	Effect->Activate(true);
}

/* 解析枪口 */
bool ACustomFPSWeapon::ResolveMuzzle(USceneComponent*& OutComponent, FName& OutSocket) const
{
	OutComponent = nullptr;
	OutSocket = NAME_None;

	UStaticMeshComponent* Part = FindWeaponPart(FireSoundPart);
	if (!Part)
	{
		return false;
	}

	OutComponent = Part;
	if (!FireSoundSocket.IsNone() && Part->DoesSocketExist(FireSoundSocket))
	{
		OutSocket = FireSoundSocket;
	}
	return true;
}

/* 查找武器部件 */
UStaticMeshComponent* ACustomFPSWeapon::FindWeaponPart(FName PartName) const
{
	if (PartName.IsNone())
	{
		return nullptr;
	}

	for (UStaticMeshComponent* Part : WeaponPartComponents)
	{
		if (IsValid(Part) && Part->ComponentHasTag(PartName))
		{
			return Part;
		}
	}

	return nullptr;
}

/* 播放开火动画 */
void ACustomFPSWeapon::PlayFireAnimations()
{
	AActor* HolderActor = GetOwner();
	// 判断持枪者是否实现了持枪者接口
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
	// 取得持枪者接口
	WeaponHolder = Cast<ICustomFPSWeaponHolder>(GetOwner());
	AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
	bEquipped = true;
	RebuildParts();
	ApplyFirstPersonView();
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
		if (!Desc.PartName.IsNone())
		{
			Part->ComponentTags.Add(Desc.PartName);
		}
		Part->SetupAttachment(WeaponMesh, Desc.AttachSocket);
		Part->RegisterComponent();
		WeaponPartComponents.Add(Part);
	}
}