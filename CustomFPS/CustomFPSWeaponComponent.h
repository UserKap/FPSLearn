/*	
* 枪械组件，负责枪械的部件、枪械的附件、枪械的动画等动作。
* 包括初始化枪械组件、设置枪械部件、设置枪械附件、刷新枪械等方法。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSWeaponComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 枪身上的一个部件，挂到枪械骨骼的插槽上。 */
USTRUCT(BlueprintType)
struct UE_LEARNFPS_API FWeaponPartDesc
{
	GENERATED_BODY()

	/** 部件名字，只用于区分，例如 Barrel、Stock、Muzzle。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FName PartName = NAME_None;

	/** 挂到枪械网格上的骨骼或插槽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FName AttachSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
};

/** 枪械插槽，以及挂在枪上的静态网格部件。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="枪械"))
class UE_LEARNFPS_API UCustomFPSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSWeaponComponent();

	void SetMeshes(USkeletalMeshComponent* InArms, USkeletalMeshComponent* InWeapon);
	void Refresh();
	FName GetWeaponAttachSocket() const { return WeaponAttachSocket; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 枪械挂到手臂上的插槽。手臂网格没有该插槽时，枪会留在手臂组件原点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FName WeaponAttachSocket = FName("HandGrip_R");

	/** 枪械部件。每一项都是枪模型的子网格，并挂到对应骨骼上。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(TitleProperty="PartName"))
	TArray<FWeaponPartDesc> WeaponParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WeaponPartComponents;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	void AttachToArms();
	void RebuildParts();
};
