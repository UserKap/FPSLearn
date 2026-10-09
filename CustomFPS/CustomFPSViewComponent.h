/*	
* 视角组件，负责角色的视角、瞄准、侧身等动作。
* 包括初始化视角组件、设置相机、移动、瞄准、侧身、更新相机附件等方法。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSViewComponent.generated.h"

class UCameraComponent;
class USceneComponent;
class USkeletalMeshComponent;

/** 鼠标视角、开镜视野、侧身，以及相机跟随骨骼。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="视角"),
	hidecategories=(Object, ComponentTick, Tags, AssetUserData, Replication, ComponentReplication, Activation, Variable, Navigation, Cooking),
	PrioritizeCategories=("FPS|节点", "FPS|偏移", "FPS|晃动", "FPS|瞄准", "FPS|侧身", "FPS|动画"))
class UE_LEARNFPS_API UCustomFPSViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSViewComponent();

	void SetupCamera(UCameraComponent* InCamera);
	void SetupArmsAnchor(USceneComponent* InArmsAnchor);
	void Look(float Yaw, float Pitch);
	void Lean(float Side);
	void UpdateCameraAttachment();
	void UpdateAnimationValues();

	virtual void BeginPlay() override;

	/** 控制器俯仰角。向上为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimPitch", BlueprintThreadSafe))
	float GetAimPitch() const { return AimPitch; }

	/** 控制器偏航相对角色朝向的差。向右为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimYaw", BlueprintThreadSafe))
	float GetAimYaw() const { return AimYaw; }

	/** 当前侧身滚转角。向右为正。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="LeanAngle", BlueprintThreadSafe))
	float GetLeanAngle() const { return CurrentLeanRoll; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 摄像机跟随的身体骨骼或插槽。留空，或骨骼不存在时，固定在胶囊体眼睛高度。胸前 bodycam 用朝前的插槽，不要直接用 head 的骨骼轴向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|节点", meta=(DisplayName="跟随节点"))
	FName CameraAttachBone = FName("Head");

	/** 只加在相机上的位置偏移。X 前方，Y 右侧，Z 上方。不影响手臂和枪。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|偏移", meta=(DisplayName="位置偏移", ForceUnits="cm"))
	FVector CameraAttachOffset = FVector::ZeroVector;

	/** 加在瞄准方向上的固定旋转。Pitch 为负时镜头略微朝下。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|偏移", meta=(DisplayName="旋转偏移", ForceUnits="deg"))
	FRotator CameraAttachRotation = FRotator::ZeroRotator;

	/** 骨骼动画相对静止姿势的旋转放大倍数。可以大于 1。0 表示旋转不跟随动画。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|晃动", meta=(DisplayName="旋转晃动倍数", ClampMin="0", ClampMax="8"))
	float BoneSwayScale = 1.f;

	/** 骨骼晃动在每个轴上的最大角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|晃动", meta=(DisplayName="最大晃动角度", ClampMin="0", ClampMax="45", ForceUnits="deg"))
	float MaxBoneSwayAngle = 12.f;

	/** 头部相对静止姿势的位移放大倍数。1 是动画原幅度，走路不明显时往上加。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|晃动", meta=(DisplayName="位置晃动倍数", ClampMin="0", ClampMax="8"))
	float BoneLocationSwayScale = 3.f;

	/** 只平滑骨骼晃动的位置差。角色位移每帧贴住，避免跑步时镜头落在枪后面。0 表示晃动也不平滑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|晃动", meta=(DisplayName="位置平滑", ClampMin="0"))
	float CameraLocationLagSpeed = 0.f;

	/** 骨骼晃动的平滑速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|晃动", meta=(DisplayName="旋转平滑", ClampMin="0"))
	float CameraRotationLagSpeed = 14.f;

	/** 非瞄准时镜头偏航灵敏度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准", meta=(DisplayName="水平灵敏度", ClampMin="0"))
	float LookSensitivityYaw = 0.5f;

	/** 非瞄准时镜头俯仰灵敏度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准", meta=(DisplayName="垂直灵敏度", ClampMin="0"))
	float LookSensitivityPitch = 0.5f;

	/** 非瞄准时镜头缩放值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准", meta=(DisplayName="腰射视野", ClampMin="1", ClampMax="170", ForceUnits="deg"))
	float HipFieldOfView = 105.f;

	/** 瞄准时镜头缩放值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准", meta=(DisplayName="瞄准视野", ClampMin="1", ClampMax="170", ForceUnits="deg"))
	float AimFieldOfView = 60.f;

	/** 瞄准时镜头缩放插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准", meta=(DisplayName="开镜速度", ClampMin="0"))
	float AimInterpSpeed = 12.f;

	/** 侧身最大滚转角。负值为向左，正值为向右。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|侧身", meta=(DisplayName="最大侧身角", ClampMin="0", ClampMax="89", ForceUnits="deg"))
	float MaxLeanAngle = 20.f;

	/** 侧身滚动插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|侧身", meta=(DisplayName="侧身速度", ClampMin="0"))
	float LeanInterpSpeed = 2.f;

	/** 目标侧身滚转角度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|侧身", meta=(DisplayName="目标侧身角"))
	float TargetLeanRoll = 0.f;

	/** 当前侧身滚转角度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|侧身", meta=(DisplayName="当前侧身角"))
	float CurrentLeanRoll = 0.f;

	/** 控制器俯仰角。向上为正。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="AimPitch", ForceUnits="deg"))
	float AimPitch = 0.f;

	/** 控制器偏航相对角色朝向的差。向右为正。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|动画", meta=(DisplayName="AimYaw", ForceUnits="deg"))
	float AimYaw = 0.f;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<USceneComponent> ArmsAnchor;

	/** 蓝图资源上的默认值一直是 false。放进关卡或开始游戏后，身体网格上找到跟随节点才会变成 true。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|节点", meta=(DisplayName="是否正在跟随骨骼"))
	bool bDrivingCameraFromBone = false;

	/** 实际用到的骨骼或插槽。为空表示没找到。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|节点", meta=(DisplayName="当前节点"))
	FName ActiveCameraBone = NAME_None;

	/** 当前混进镜头的骨骼旋转。播放时看这里是否在变。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|晃动", meta=(DisplayName="当前骨骼晃动", ForceUnits="deg"))
	FRotator CurrentBoneSway = FRotator::ZeroRotator;

	FVector SmoothedLocalSway = FVector::ZeroVector;
	bool bHasSmoothedLocalSway = false;
	bool bCameraManagerPrerequisiteSet = false;

	void EnsureViewIsCapturedAfterCameraUpdate();

	void UpdateDrivenCamera(float DeltaTime);
	FRotator ComputeBoneSway(const USkeletalMeshComponent* BodyMesh) const;
};
