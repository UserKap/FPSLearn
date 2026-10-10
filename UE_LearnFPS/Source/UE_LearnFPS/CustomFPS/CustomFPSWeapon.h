/*
* 武器基类。新的武器继承这个 Actor，网格、部件和装备都放在武器自己身上。
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CustomFPSWeapon.generated.h"

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

/** 武器基类。在蓝图里继承它来做具体的枪。 */
UCLASS(Abstract, Blueprintable, meta=(DisplayName="武器"),
	hidecategories=(Actor, Input, Replication, Cooking, Rendering, Collision, HLOD, WorldPartition, DataLayers, Networking),
	PrioritizeCategories=("FPS|武器"))
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
	bool IsEquipped() const { return bEquipped; }

	/** 瞄准插槽的世界坐标。动画蓝图用这个位置去对 Head。 */
	UFUNCTION(BlueprintPure, Category="FPS|武器", meta=(DisplayName="瞄准世界坐标", BlueprintThreadSafe))
	FVector GetAimSocketLocation() const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	/** 装上之后，子类和蓝图可以在这里接开火、动画。 */
	UFUNCTION(BlueprintNativeEvent, Category="FPS|武器", meta=(DisplayName="已装备"))
	void OnEquipped();
	virtual void OnEquipped_Implementation() {}

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
};
