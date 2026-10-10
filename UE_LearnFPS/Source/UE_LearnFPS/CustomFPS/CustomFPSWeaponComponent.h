/*
* 角色上的装备槽。只负责生成武器并交给武器自己装到手臂上。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSWeapon.h"
#include "CustomFPSWeaponComponent.generated.h"

class USkeletalMeshComponent;

/** 角色的武器装备槽。枪的网格和部件在武器 Actor 上。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="枪械"),
	hidecategories=(Object, ComponentTick, Tags, AssetUserData, Replication, ComponentReplication, Activation, Variable, Navigation, Cooking),
	PrioritizeCategories=("FPS|枪械"))
class UE_LEARNFPS_API UCustomFPSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSWeaponComponent();

	void SetArms(USkeletalMeshComponent* InArms);
	void Refresh();
	ACustomFPSWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 要生成的武器。需要是武器基类的蓝图子类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪械", meta=(DisplayName="武器类"))
	TSubclassOf<ACustomFPSWeapon> WeaponClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|枪械", meta=(DisplayName="当前武器"))
	TObjectPtr<ACustomFPSWeapon> EquippedWeapon;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	void EquipWeaponActor();
	void ClearEquippedWeapon();
};
