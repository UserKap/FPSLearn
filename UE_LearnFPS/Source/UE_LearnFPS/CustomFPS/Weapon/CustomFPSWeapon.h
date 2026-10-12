/*
* 武器基类。新的武器继承这个 Actor，网格、部件和装备都放在武器自己身上。
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CustomFPSWeapon.generated.h"

class ICustomFPSWeaponHolder;

class UAnimMontage;
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
	PrioritizeCategories=("FPS|弹药", "FPS|射击", "FPS|枪口", "FPS|开火音效", "FPS|开火特效", "FPS|开火动画", "FPS|武器"))
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

	/** 命中时造成的伤害。 */
	UFUNCTION(BlueprintPure, Category="FPS|射击", meta=(DisplayName="武器伤害", BlueprintThreadSafe))
	float GetWeaponDamage() const { return WeaponDamage; }

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

	/** 命中时造成的伤害。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|射击", meta=(DisplayName="武器伤害", ClampMin="0"))
	float WeaponDamage = 25.f;

	/** 每分钟最多射击多少次。数值越大，两发之间的间隔越短。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|射击", meta=(DisplayName="武器射速", ClampMin="1", ForceUnits="发/分"))
	float FireRate = 480.f;

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
	TWeakObjectPtr<AActor> ShotRequester;
	FTimerHandle AutoFireTimer;
	FTimerHandle BoltTimer;

	void ClearFireTimers();
	void FireRound(AActor* RequestedBy);
	void PlayFireAnimations();
	void PlayFireSound();
	void PlayFireEffect();
	UStaticMeshComponent* FindWeaponPart(FName PartName) const;
	bool ResolveMuzzle(USceneComponent*& OutComponent, FName& OutSocket) const;
	void ScheduleAutoFire();
	void HandleAutoFire();
	void FinishBolt();
};
