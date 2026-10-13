/*
* 武器基类。新的武器继承这个 Actor，网格、部件和装备都放在武器自己身上。
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CustomFPSBullet.h"
#include "CustomFPSWeapon.generated.h"

class ICustomFPSWeaponHolder;

class UAnimMontage;
class UCustomFPSRecoilData;
class UMetaSoundSource;
class UNiagaraSystem;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 枪身上的一个部件，挂到武器骨骼的插槽上。 */
USTRUCT(BlueprintType)
struct UE_LEARNFPS_API FWeaponPartDesc
{
	GENERATED_BODY()

	/** 部件名字，只用于区分，例如 Barrel、Stock、Muzzle。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|武器", meta=(DisplayName="部件名"))
	FName PartName = NAME_None;

	/** 挂到武器网格上的骨骼或插槽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|武器", meta=(DisplayName="插槽"))
	FName AttachSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|武器", meta=(DisplayName="网格"))
	TObjectPtr<UStaticMesh> Mesh = nullptr;
};

/** 武器的射击方式。 */
UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	/** 按住开火键，按射速连续射击。 */
	FullAuto UMETA(DisplayName="全自动"),

	/** 每按一次开火键射出一发。 */
	SemiAuto UMETA(DisplayName="半自动"),

	/** 每按一次射出一发，然后必须拉栓才能再射。换弹每次只装一发。 */
	Manual UMETA(DisplayName="手动"),
};

/** 武器基类。在蓝图里继承它来做具体的枪。 */
UCLASS(Abstract, Blueprintable, meta=(DisplayName="武器"),
	hidecategories=(Actor, Input, Replication, Cooking, Rendering, Collision, HLOD, WorldPartition, DataLayers, Networking),
	PrioritizeCategories=("FPS|弹药", "FPS|子弹", "FPS|弹壳", "FPS|射击", "FPS|弹道", "FPS|瞄具", "FPS|散布", "FPS|后坐力", "FPS|枪口", "FPS|开火音效", "FPS|开火特效", "FPS|开火动画", "FPS|武器"))
class UE_LEARNFPS_API ACustomFPSWeapon : public AActor
{
	GENERATED_BODY()

public:
	ACustomFPSWeapon();

	/** 装到手臂网格的装备插槽上，并切成第一人称显示。 */
	void Equip(USceneComponent* AttachParent);

	/** 从手上卸下。 */
	void Unequip();

	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	/** 是否装备 */
	bool IsEquipped() const { return bEquipped; }

	/** 瞄准插槽的世界坐标。动画蓝图用这个位置去对 Head。 */
	UFUNCTION(BlueprintPure, Category="FPS|武器", meta=(DisplayName="瞄准世界坐标", BlueprintThreadSafe))
	FVector GetAimSocketLocation() const;

	/** 枪口位置和这一发不加散布时的子弹方向。子弹沿枪管直线飞出。没有武器网格时返回假。 */
	UFUNCTION(BlueprintPure, Category="FPS|弹道", meta=(DisplayName="枪口瞄准"))
	bool GetShotAim(UPARAM(DisplayName="枪口位置") FVector& OutMuzzle, UPARAM(DisplayName="子弹方向") FVector& OutDirection) const;

	/**
	 * 红点指向的瞄准线，从瞄准插槽出发。
	 * 先对准子弹在归零距离上的位置，再按高低、风偏修正转一个小角度。镜片上的红点就画在这个方向上。
	 * 没有武器网格时返回假。
	 */
	UFUNCTION(BlueprintPure, Category="FPS|瞄具", meta=(DisplayName="瞄准线"))
	bool GetSightLine(UPARAM(DisplayName="眼睛位置") FVector& OutEye, UPARAM(DisplayName="瞄准方向") FVector& OutDirection) const;

	/** 把瞄准线方向写到瞄具部件上给镜片材质读。要在枪这一帧的姿势算完之后调用，红点才不会慢一拍。 */
	void UpdateReticle();

	/** 现在开枪的散布半角，度。子弹落在以枪口瞄准方向为轴、这个半角的锥里，越靠中心越密。 */
	UFUNCTION(BlueprintPure, Category="FPS|散布", meta=(DisplayName="当前散布"))
	float GetCurrentSpread() const;

	/** 持枪者要求开枪。请求者是发出这次开火的蓝图 Actor。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="开始开火"))
	void StartFire(UPARAM(meta=(DisplayName="请求者")) AActor* RequestedBy);

	/** 持枪者松开开火。请求者是发出这次停火的蓝图 Actor。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="停止开火"))
	void StopFire(UPARAM(meta=(DisplayName="请求者")) AActor* RequestedBy);

	/** 弹夹最多能装多少发。 */
	UFUNCTION(BlueprintPure, Category="FPS|弹药", meta=(DisplayName="弹夹容量", BlueprintThreadSafe))
	int32 GetMagazineSize() const { return MagazineSize; }

	/** 弹夹里还剩多少发。 */
	UFUNCTION(BlueprintPure, Category="FPS|弹药", meta=(DisplayName="弹夹剩余", BlueprintThreadSafe))
	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }

	/** 弹夹外还剩多少发。 */
	UFUNCTION(BlueprintPure, Category="FPS|弹药", meta=(DisplayName="备用弹药", BlueprintThreadSafe))
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	/** 弹夹里还有子弹，并且距离上一发已经过了射速间隔时为真。 */
	UFUNCTION(BlueprintPure, Category="FPS|弹药", meta=(DisplayName="能否开火"))
	bool CanFire() const;

	/** 当前子弹打在没有护甲的身上时造成的伤害。没指定子弹时为 0。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="肉伤"))
	float GetFleshDamage() const;

	/** 当前子弹打在护甲上时，用来削减护甲的伤害。没指定子弹时为 0。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="穿甲伤害"))
	float GetArmorDamage() const;

	/** 护甲被打穿后，肉伤还保留的比例。没指定子弹时为 0。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="穿甲后肉伤保留"))
	float GetFleshRetainAfterArmor() const;

	/** 当前子弹的口径。没指定子弹时是 5.56x45mm NATO。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="口径"))
	EBulletCaliber GetBulletCaliber() const;

	/** 当前子弹的弹头。没指定子弹时是全金属被甲。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="弹头"))
	EBulletProjectile GetBulletProjectile() const;

	/** 每分钟最多射击多少次。 */
	UFUNCTION(BlueprintPure, Category="FPS|射击", meta=(DisplayName="武器射速", BlueprintThreadSafe))
	float GetFireRate() const { return FireRate; }

	/** 当前射击方式。 */
	UFUNCTION(BlueprintPure, Category="FPS|射击", meta=(DisplayName="射击方式", BlueprintThreadSafe))
	EWeaponFireMode GetFireMode() const { return FireMode; }

	/** 手动武器打空枪膛后，拉栓才能再次开火。全自动和半自动调用时不会发生。 */
	UFUNCTION(BlueprintCallable, Category="FPS|射击", meta=(DisplayName="拉栓"))
	bool CycleBolt();

	/** 手动武器枪膛里有一发、可以射击时为真。其他射击方式始终为真。 */
	UFUNCTION(BlueprintPure, Category="FPS|射击", meta=(DisplayName="枪膛就绪"))
	bool IsChamberReady() const { return FireMode != EWeaponFireMode::Manual || bChamberReady; }

	/** 用备用弹药把弹夹补满。没有空位或没有备用弹药时返回假。 */
	UFUNCTION(BlueprintCallable, Category="FPS|弹药", meta=(DisplayName="换弹"))
	bool Reload();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 当前拿着这把枪的人。玩家、其他玩家或 AI。未实现持枪者接口时为空。 */
	ICustomFPSWeaponHolder* GetWeaponHolder() const { return WeaponHolder; }

	/** 装上之后，子类和蓝图可以在这里接开火、动画。 */
	UFUNCTION(BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="已装备"))
	void OnEquipped();
	virtual void OnEquipped_Implementation() {}

	virtual void StartFire_Implementation(AActor* RequestedBy);
	virtual void StopFire_Implementation(AActor* RequestedBy);

	/** 发射后效果 成功扣掉一发之后。弹道和枪口效果接在这里。 */
	UFUNCTION(BlueprintImplementableEvent, Category="FPS|武器", meta=(DisplayName="已发射"))
	void OnRoundFired(AActor* RequestedBy);

	/** 这一发的射线打到了东西。还不扣血，蓝图可以先在这里放弹孔、火花。 */
	UFUNCTION(BlueprintImplementableEvent, Category="FPS|弹道", meta=(DisplayName="子弹命中"))
	void OnShotHit(const FHitResult& Hit);

	/** 卸下之后。 */
	UFUNCTION(BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="已卸下"))
	void OnUnequipped();
	virtual void OnUnequipped_Implementation() {}

	/** 装到手臂网格上的骨骼或插槽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|武器", meta=(DisplayName="装备插槽"))
	FName AttachSocket = FName("HandGrip_R");

	/** 武器网格上的瞄准骨骼或插槽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|武器", meta=(DisplayName="瞄准插槽"))
	FName AimSocket = FName("AimSocket");

	/** 弹夹最多能装多少发。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹药", meta=(DisplayName="弹夹容量", ClampMin="1"))
	int32 MagazineSize = 30;

	/** 当前弹夹里的子弹。开局时按弹夹容量填满。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|弹药", meta=(DisplayName="弹夹剩余", ClampMin="0"))
	int32 AmmoInMagazine = 0;

	/** 弹夹外的子弹。换弹时从这里扣。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹药", meta=(DisplayName="备用弹药", ClampMin="0"))
	int32 ReserveAmmo = 90;

	/** 这把枪使用的子弹。伤害、口径和弹壳网格都在子弹蓝图上。空则不抛壳，伤害为 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|子弹", meta=(DisplayName="子弹类型"))
	TSubclassOf<ACustomFPSBullet> BulletClass;

	/** 机匣骨骼网格上的抛壳插槽。从武器网格取，不从枪管部件取。没有这个插槽时改用 Bolt 骨骼。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="抛壳插槽"))
	FName EjectionSocket = FName("Ejection");

	/** 抛壳方向，插槽局部空间。X 向前，Y 向右，Z 向上。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="抛壳方向"))
	FVector EjectionDirection = FVector(0.15f, 1.f, 0.45f);

	/** 弹壳抛出的速度，厘米/秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="抛壳速度", ClampMin="0", ForceUnits="cm/s"))
	float EjectionSpeed = 120.f;

	/** 弹壳旋转速度，度/秒。方向每次随机。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="抛壳旋转", ClampMin="0", ForceUnits="deg/s"))
	float EjectionSpin = 720.f;

	/** 弹壳继承持枪者移动速度的比例。1 表示和人一起移动，侧移开枪时弹壳仍从抛壳口那一侧飞出；0 表示不继承。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="继承移动速度", ClampMin="0", ClampMax="1"))
	float EjectionInheritVelocity = 1.f;

	/** 每分钟最多射击多少次。数值越大，两发之间的间隔越短。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|射击", meta=(DisplayName="武器射速", ClampMin="1", ForceUnits="发/分"))
	float FireRate = 480.f;

	/** 红点出厂对准的距离。修正为 0 时，这个距离上红点就是弹着点；更近时子弹比红点低，低多少约等于瞄具高出枪管的距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹道", meta=(DisplayName="归零距离", ClampMin="100", ForceUnits="cm"))
	float ZeroDistance = 5000.f;

	/** 射线最远打多远。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹道", meta=(DisplayName="最大射程", ClampMin="100", ForceUnits="cm"))
	float MaxRange = 30000.f;

	/** 子弹射线用的碰撞通道。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹道", meta=(DisplayName="射击碰撞通道"))
	TEnumAsByte<ECollisionChannel> ShotTraceChannel = ECC_Visibility;

	/** 画出每一发的弹道和命中点，并在屏幕上打出命中信息。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹道", meta=(DisplayName="显示弹道"))
	bool bDrawShotDebug = true;

	/** 弹道线留在画面上的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹道", meta=(DisplayName="弹道显示时间", ClampMin="0", ForceUnits="s", EditCondition="bDrawShotDebug"))
	float ShotDebugDuration = 2.f;

	/**
	 * 红点的高低旋钮，角分（MOA）。正数让弹着点相对红点往上走，负数往下。1 角分约等于 100 米处 2.9 厘米。
	 * 真红点镜一格一般是 0.5 角分：打一组看弹着点偏下多少，换算成角分填进来。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄具", meta=(DisplayName="高低修正"))
	float SightElevationMOA = 0.f;

	/** 红点的风偏旋钮，角分（MOA）。正数让弹着点相对红点往右走，负数往左。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄具", meta=(DisplayName="风偏修正"))
	float SightWindageMOA = 0.f;

	/**
	 * 镜片所在的武器部件。名字对应武器部件里的部件名。
	 * 瞄准线方向写在这个部件的自定义图元数据 0~3，枪的上方写在 4~7，镜片材质 M_RedDotLens 从这里读。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄具", meta=(DisplayName="瞄具部件"))
	FName ReticlePart = FName("Scope");

	/**
	 * 枪本身的精度，角分（MOA），表示弹着散布的直径。1 角分约等于 100 米处 2.9 厘米。步枪一般 1 到 3。
	 * 会乘子弹的精度倍率。真实枪械很准，打不准主要来自枪身晃动和后坐，下面几项是给游戏手感的额外散布。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="固有精度", ClampMin="0"))
	float WeaponMOA = 2.f;

	/** 腰射时额外的散布半角。枪没抵肩，握得没那么稳。开镜时没有。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="腰射散布", ClampMin="0", ForceUnits="deg"))
	float HipSpreadAngle = 0.25f;

	/** 移动到满速时额外的散布半角。按速度线性增加。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="移动散布", ClampMin="0", ForceUnits="deg"))
	float MoveSpreadAngle = 0.6f;

	/** 移动散布达到满值的水平速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="移动散布满值速度", ClampMin="1", ForceUnits="cm/s"))
	float MoveSpreadFullSpeed = 500.f;

	/** 跳起或下落时额外的散布半角。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="空中散布", ClampMin="0", ForceUnits="deg"))
	float AirSpreadAngle = 2.f;

	/** 连射时每发增加的散布半角。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="连射每发散布", ClampMin="0", ForceUnits="deg"))
	float BloomPerShot = 0.04f;

	/** 连射散布最多加到多少。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="连射散布上限", ClampMin="0", ForceUnits="deg"))
	float BloomMax = 0.4f;

	/** 停火后连射散布每秒恢复多少度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|散布", meta=(DisplayName="连射散布恢复", ClampMin="0", ForceUnits="deg/s"))
	float BloomRecoveryRate = 1.5f;

	/** 这把枪的后坐力。空则开枪不抬准星、不抖镜头。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|后坐力", meta=(DisplayName="后坐力"))
	TObjectPtr<UCustomFPSRecoilData> RecoilData;

	/** 全自动按住连射，半自动点射，手动每发之后要拉栓。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|射击", meta=(DisplayName="射击方式"))
	EWeaponFireMode FireMode = EWeaponFireMode::SemiAuto;

	/** 手动武器拉栓需要的时间。0 表示调用拉栓后立刻完成。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|射击", meta=(DisplayName="拉栓时间", ClampMin="0", ForceUnits="s"))
	float BoltDuration = 0.8f;

	/** 待机时角色手臂的开火蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火动画", meta=(DisplayName="待机角色开火"))
	TObjectPtr<UAnimMontage> IdleCharacterFire;

	/** 走路时角色手臂的开火蒙太奇。没填时使用待机角色开火。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火动画", meta=(DisplayName="走路角色开火"))
	TObjectPtr<UAnimMontage> MoveCharacterFire;

	/** 瞄准时角色手臂的开火蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火动画", meta=(DisplayName="瞄准角色开火"))
	TObjectPtr<UAnimMontage> AimCharacterFire;

	/** 待机时武器自己的开火蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火动画", meta=(DisplayName="待机武器开火"))
	TObjectPtr<UAnimMontage> IdleWeaponFire;

	/** 走路时武器自己的开火蒙太奇。没填时使用待机武器开火。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火动画", meta=(DisplayName="走路武器开火"))
	TObjectPtr<UAnimMontage> MoveWeaponFire;

	/** 瞄准时武器自己的开火蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火动画", meta=(DisplayName="瞄准武器开火"))
	TObjectPtr<UAnimMontage> AimWeaponFire;

	/** 每发实际射出时播放。空则不播。一次性 MetaSound 会在开始播放时触发自己的 On Play。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火音效", meta=(DisplayName="开火音效"))
	TObjectPtr<UMetaSoundSource> FireSound;

	/** 每发实际射出时在枪口播放。空则不播。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开火特效", meta=(DisplayName="开火特效"))
	TObjectPtr<UNiagaraSystem> FireEffect;

	/** 音效和特效挂在这个部件的静态网格上。名字对应武器部件里的部件名。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪口", meta=(DisplayName="枪口部件"))
	FName FireSoundPart = FName("Barrel");

	/** 枪口部件网格上的插槽。音效和特效都挂在这里，不在武器骨骼上。找不到时挂在部件原点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪口", meta=(DisplayName="枪口插槽"))
	FName FireSoundSocket = FName("Muzzle");

	/** 武器部件。每一项都是枪模型的子网格。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|武器", meta=(DisplayName="武器部件", TitleProperty="PartName"))
	TArray<FWeaponPartDesc> WeaponParts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|武器", meta=(DisplayName="武器网格"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	void RebuildParts();
	void ApplyFirstPersonView();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WeaponPartComponents;

	bool bEquipped = false;

	/** 装备时从 Owner 上取得。武器回调持枪者时用它，不引用具体角色类。 */
	ICustomFPSWeaponHolder* WeaponHolder = nullptr;

	/** 上一发实际射出的时间。没有开过枪时为 -1。 */
	float LastShotTime = -1.f;

	/** 手动武器枪膛里已经有一发。开局为真。 */
	bool bChamberReady = true;

	bool bCyclingBolt = false;
	bool bTriggerHeld = false;
	bool bWarnedMissingEjectionSocket = false;

	/** 这一轮连射到了第几发。两发间隔超过弹道重置时间就从 1 重新数。 */
	int32 RecoilShotNumber = 0;
	float RecoilLastShotTime = -1.f;

	/** 上一发之后累积的连射散布，以及它对应的时间。读取时按恢复速度扣掉经过的时间。 */
	float SpreadBloom = 0.f;
	float SpreadBloomTime = -1.f;
	TWeakObjectPtr<AActor> ShotRequester;
	FTimerHandle AutoFireTimer;
	FTimerHandle BoltTimer;

	void ClearFireTimers();
	void FireRound(AActor* RequestedBy);
	void FireShot();
	bool ResolveBore(FVector& OutLocation, FVector& OutDirection) const;
	bool ResolveSightLine(FVector& OutEye, FVector& OutDirection, FVector& OutUp) const;
	float GetDecayedBloom() const;
	FVector ApplySpread(const FVector& Direction, float HalfAngleDegrees) const;
	void EjectSpentCasing();
	void ApplyRecoil();
	bool ResolveEjectionTransform(FTransform& OutTransform);
	const ACustomFPSBullet* GetBulletDefaults() const;
	void PlayFireAnimations();
	void PlayFireSound();
	void PlayFireEffect();
	UStaticMeshComponent* FindWeaponPart(FName PartName) const;
	bool ResolveMuzzle(USceneComponent*& OutComponent, FName& OutSocket) const;
	void ScheduleAutoFire();
	void HandleAutoFire();
	void FinishBolt();
};
