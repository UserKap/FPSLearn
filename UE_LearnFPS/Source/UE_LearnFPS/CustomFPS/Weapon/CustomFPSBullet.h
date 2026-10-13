/*
* 子弹。蓝图继承这个类，填口径、弹头、两套网格和伤害。
* 枪上指定一种子弹。打出去的那一发读这里的伤害；抛出来的实体用发射后网格。
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CustomFPSBullet.generated.h"

class UPrimitiveComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
struct FHitResult;

/** 弹壳口径。决定这发子弹是哪一种弹药，不直接改伤害。 */
UENUM(BlueprintType)
enum class EBulletCaliber : uint8
{
	Caliber_9x19 UMETA(DisplayName="9x19mm"),
	Caliber_45ACP UMETA(DisplayName=".45 ACP"),
	Caliber_57x28 UMETA(DisplayName="5.7x28mm"),
	Caliber_46x30 UMETA(DisplayName="4.6x30mm"),
	Caliber_545x39 UMETA(DisplayName="5.45x39mm"),
	Caliber_556x45 UMETA(DisplayName="5.56x45mm NATO"),
	Caliber_300AAC UMETA(DisplayName=".300 AAC Blackout"),
	Caliber_762x39 UMETA(DisplayName="7.62x39mm"),
	Caliber_762x51 UMETA(DisplayName="7.62x51mm NATO"),
	Caliber_762x54R UMETA(DisplayName="7.62x54mmR"),
	Caliber_338Lapua UMETA(DisplayName=".338 Lapua Magnum"),
	Caliber_50BMG UMETA(DisplayName=".50 BMG"),
	Caliber_12Gauge UMETA(DisplayName="12号径"),
};

/**
 * 弹头结构。肉伤和穿甲伤害按这个来配：
 * 空尖、被甲空尖肉伤高、穿甲低；穿甲弹相反；全金属被甲居中。
 */
UENUM(BlueprintType)
enum class EBulletProjectile : uint8
{
	FMJ UMETA(DisplayName="全金属被甲"),
	JHP UMETA(DisplayName="被甲空尖"),
	HP UMETA(DisplayName="空尖"),
	SP UMETA(DisplayName="软尖"),
	AP UMETA(DisplayName="穿甲"),
	API UMETA(DisplayName="穿甲燃烧"),
	Tracer UMETA(DisplayName="曳光"),
	Match UMETA(DisplayName="比赛弹"),
	Subsonic UMETA(DisplayName="亚音速"),
};

/**
 * 一种子弹的数据和被抛出的弹壳。
 * 没有护甲时造成肉伤。有护甲时先用穿甲伤害扣护甲，护甲被打穿后再造成肉伤乘以穿甲后保留比例。
 * 现在还没有命中结算，这些数只供枪和以后的弹道读取。
 */
UCLASS(Blueprintable, meta=(DisplayName="子弹"),
	hidecategories=(Actor, Input, Replication, Cooking, Rendering, Collision, HLOD, WorldPartition, DataLayers, Networking),
	PrioritizeCategories=("FPS|子弹", "FPS|伤害", "FPS|弹壳"))
class UE_LEARNFPS_API ACustomFPSBullet : public AActor
{
	GENERATED_BODY()

public:
	ACustomFPSBullet();

	/** 口径。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="口径", BlueprintThreadSafe))
	EBulletCaliber GetCaliber() const { return Caliber; }

	/** 弹头结构。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="弹头", BlueprintThreadSafe))
	EBulletProjectile GetProjectile() const { return Projectile; }

	/** 打在没有护甲的身上时造成的伤害。 */
	UFUNCTION(BlueprintPure, Category="FPS|伤害", meta=(DisplayName="肉伤", BlueprintThreadSafe))
	float GetFleshDamage() const { return FleshDamage; }

	/** 打在护甲上时，用来削减护甲的伤害。 */
	UFUNCTION(BlueprintPure, Category="FPS|伤害", meta=(DisplayName="穿甲伤害", BlueprintThreadSafe))
	float GetArmorDamage() const { return ArmorDamage; }

	/** 护甲被这一发打穿之后，肉伤还保留多少。1 是打穿后肉伤不减，0 是打穿后不再伤肉。 */
	UFUNCTION(BlueprintPure, Category="FPS|伤害", meta=(DisplayName="穿甲后肉伤保留", BlueprintThreadSafe))
	float GetFleshRetainAfterArmor() const { return FleshRetainAfterArmor; }

	/** 乘在枪固有精度上的倍率。 */
	UFUNCTION(BlueprintPure, Category="FPS|子弹", meta=(DisplayName="精度倍率", BlueprintThreadSafe))
	float GetSpreadMultiplier() const { return SpreadMultiplier; }

	/** 已经变成弹壳、正在被抛出时为真。 */
	UFUNCTION(BlueprintPure, Category="FPS|弹壳", meta=(DisplayName="已抛出"))
	bool IsEjected() const { return bEjected; }

	/**
	 * 换成发射后网格，打开物理，并按给定速度抛出。
	 * 线速度是厘米/秒，角速度是度/秒。发射后网格为空时返回假。
	 */
	bool EjectCasing(FVector LinearVelocity, FVector AngularVelocityDegrees);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	/** 这发子弹的口径。MCX 用 5.56x45mm NATO。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|子弹", meta=(DisplayName="口径"))
	EBulletCaliber Caliber = EBulletCaliber::Caliber_556x45;

	/** 弹头结构。全金属被甲是普通步枪弹，空尖偏肉伤，穿甲偏护甲。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|子弹", meta=(DisplayName="弹头"))
	EBulletProjectile Projectile = EBulletProjectile::FMJ;

	/** 乘在枪固有精度上的倍率。比赛弹小于 1，劣质弹大于 1。只影响枪本身的精度，不影响移动、腰射这些额外散布。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|子弹", meta=(DisplayName="精度倍率", ClampMin="0"))
	float SpreadMultiplier = 1.f;

	/** 没有护甲时的伤害。空尖、被甲空尖应高于全金属被甲，穿甲弹应更低。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|伤害", meta=(DisplayName="肉伤", ClampMin="0"))
	float FleshDamage = 34.f;

	/** 对护甲造成的伤害。穿甲弹、穿甲燃烧应高于全金属被甲，空尖应更低。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|伤害", meta=(DisplayName="穿甲伤害", ClampMin="0"))
	float ArmorDamage = 20.f;

	/** 护甲被打穿后保留的肉伤比例。穿甲弹接近 1，空尖接近 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|伤害", meta=(DisplayName="穿甲后肉伤保留", ClampMin="0", ClampMax="1"))
	float FleshRetainAfterArmor = 0.6f;

	/** 还在弹夹或枪膛里时的整颗子弹网格。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|子弹", meta=(DisplayName="发射前网格"))
	TObjectPtr<UStaticMesh> CartridgeMesh;

	/** 击发之后抛出的弹壳网格。需要简单碰撞，否则会穿过地面。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|子弹", meta=(DisplayName="发射后网格"))
	TObjectPtr<UStaticMesh> CasingMesh;

	/** 弹壳质量，千克。5.56 弹壳大约 0.006。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="弹壳质量", ClampMin="0.001", ForceUnits="kg"))
	float CasingMassKg = 0.008f;

	/** 弹壳留在场景里的时间。到时销毁。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="弹壳存活", ClampMin="0.1", ForceUnits="s"))
	float CasingLifetime = 4.f;

	/** 弹壳撞到地面或墙时播放。空则不播。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="弹壳落地音效"))
	TObjectPtr<USoundBase> CasingHitSound;

	/** 撞击速度低于这个值不出声，避免弹壳在地上滚动时一直响。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="出声最低速度", ClampMin="0", ForceUnits="cm/s"))
	float CasingHitMinSpeed = 40.f;

	/** 撞击速度达到这个值时音量最大。介于最低速度和它之间时按比例变小。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="满音量速度", ClampMin="0", ForceUnits="cm/s"))
	float CasingHitFullSpeed = 250.f;

	/** 两次落地声之间至少间隔多久。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="落地音效间隔", ClampMin="0", ForceUnits="s"))
	float CasingHitInterval = 0.08f;

	/** 一颗弹壳最多响几次。弹跳几下之后就安静。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|弹壳", meta=(DisplayName="最多落地次数", ClampMin="1"))
	int32 CasingHitMaxCount = 3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|子弹", meta=(DisplayName="子弹网格"))
	TObjectPtr<UStaticMeshComponent> BulletMesh;

	bool bEjected = false;

private:
	UFUNCTION()
	void HandleCasingHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	float LastCasingHitTime = -1.f;
	int32 CasingHitCount = 0;
};
