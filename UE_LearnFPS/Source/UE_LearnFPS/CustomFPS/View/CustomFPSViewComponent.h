/*	
* 视角组件，负责角色的视角、瞄准、侧身等动作。
* 包括初始化视角组件、设置相机、移动、瞄准、侧身、更新相机附件等方法。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "CustomFPSViewComponent.generated.h"

class ACharacter;
class UCameraComponent;
class USceneComponent;
class USkeletalMeshComponent;

/** 鼠标视角、开镜视野、侧身，以及相机跟随骨骼。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="视角"),
	hidecategories=(Object, ComponentTick, Tags, AssetUserData, Replication, ComponentReplication, Activation, Variable, Navigation, Cooking),
	PrioritizeCategories=("FPS|节点", "FPS|偏移", "FPS|开镜抖动", "FPS|腰射晃动", "FPS|瞄准晃动", "FPS|瞄准", "FPS|枪惯性", "FPS|自由瞄准", "FPS|侧身"))
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

	/** 开镜和收枪共用的时长，秒。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimDuration", BlueprintThreadSafe))
	float GetAimDuration() const { return AimDuration; }

	/** 0 是腰射姿势，1 是举枪完成。开镜速度走完正好到 1。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimSwayAlpha", BlueprintThreadSafe))
	float GetAimSwayAlpha() const { return AimSwayAlpha; }

	/** 收枪进度。松手时从 0 走到 1，和举枪共用开镜速度。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="AimExitAlpha", BlueprintThreadSafe))
	float GetAimExitAlpha() const { return 1.f - AimSwayAlpha; }

	/** 枪口相对镜头的偏离，度。X 向右为正，Y 向上为正。关掉自由瞄准时为 0。 */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(DisplayName="FreeAimOffset", BlueprintThreadSafe))
	FVector2D GetFreeAimOffset() const { return FreeAimOffset; }

	/** 镜头和身体的朝向，不含侧身和晃动。关掉自由瞄准时和控制器一致。 */
	UFUNCTION(BlueprintPure, Category="FPS|自由瞄准", meta=(DisplayName="镜头朝向"))
	FRotator GetFreeAimViewRotation() const { return FreeAimViewRotation; }

	/** 画面里枪实际指着的方向，带惯性落后于鼠标。以后打子弹用这个方向，不要直接用控制器旋转。 */
	UFUNCTION(BlueprintPure, Category="FPS|枪惯性", meta=(DisplayName="枪口朝向"))
	FRotator GetWeaponAimRotation() const { return WeaponAimRotation; }

	/** 身体和移动用的偏航。开自由瞄准时跟镜头走，否则跟控制器走。 */
	float GetBodyYaw() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 摄像机跟随的身体骨骼或插槽。留空，或骨骼不存在时，固定在胶囊体眼睛高度。胸前 bodycam 用朝前的插槽，不要直接用 head 的骨骼轴向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|节点", meta=(DisplayName="跟随节点"))
	FName CameraAttachBone = FName("Head");

	/** 腰射相机的位置偏移。X 前方，Y 右侧，Z 上方。只绕偏航，瞄准时收到 0。不影响手臂和枪。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|偏移", meta=(DisplayName="位置偏移", ForceUnits="cm"))
	FVector CameraAttachOffset = FVector::ZeroVector;

	/** 腰射镜头的固定旋转。瞄准时收到 0。Pitch 为负时镜头略微朝下。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|偏移", meta=(DisplayName="旋转偏移", ForceUnits="deg"))
	FRotator CameraAttachRotation = FRotator::ZeroRotator;

	/** 开镜那一下叠在镜头上的位置。曲线为 1 时到达这里，回到 0 时结束。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开镜抖动", meta=(DisplayName="位置偏移", ForceUnits="cm"))
	FVector AimKickLocation = FVector(-4.f, 0.8f, -1.5f);

	/** 开镜那一下叠在镜头上的旋转。Pitch 为负时镜头朝下顿一下。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|开镜抖动", meta=(DisplayName="旋转偏移", ForceUnits="deg"))
	FRotator AimKickRotation = FRotator(-2.5f, 0.45f, 1.f);

	/** 纵轴是抖动强度。横轴是 0 到 1：0 是按下瞄准，1 是开镜速度走完。可以大于 1，也可以为负。 */
	UPROPERTY(EditAnywhere, Category="FPS|开镜抖动", meta=(DisplayName="抖动曲线"))
	FRuntimeFloatCurve AimKickCurve;

	/** 腰射时，骨骼动画相对静止姿势的旋转放大倍数。可以大于 1。0 表示旋转不跟随动画。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|腰射晃动", meta=(DisplayName="腰射旋转倍数", ClampMin="0", ClampMax="8"))
	float BoneSwayScale = 1.f;

	/** 腰射时，骨骼晃动在每个轴上的最大角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|腰射晃动", meta=(DisplayName="腰射最大角度", ClampMin="0", ClampMax="45", ForceUnits="deg"))
	float MaxBoneSwayAngle = 12.f;

	/** 腰射时，头部相对静止姿势的位移放大倍数。1 是动画原幅度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|腰射晃动", meta=(DisplayName="腰射位置倍数", ClampMin="0", ClampMax="8"))
	float BoneLocationSwayScale = 3.f;

	/** 腰射时，只平滑骨骼晃动的位置差。角色位移每帧贴住。0 表示晃动也不平滑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|腰射晃动", meta=(DisplayName="腰射位置平滑", ClampMin="0"))
	float CameraLocationLagSpeed = 0.f;

	/** 腰射时，骨骼旋转晃动的平滑速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|腰射晃动", meta=(DisplayName="腰射旋转平滑", ClampMin="0"))
	float CameraRotationLagSpeed = 14.f;

	/** 瞄准时的旋转放大倍数。保留一点呼吸感，幅度比腰射小。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准晃动", meta=(DisplayName="瞄准旋转倍数", ClampMin="0", ClampMax="8"))
	float AimBoneSwayScale = 0.25f;

	/** 瞄准时，骨骼晃动在每个轴上的最大角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准晃动", meta=(DisplayName="瞄准最大角度", ClampMin="0", ClampMax="45", ForceUnits="deg"))
	float AimMaxBoneSwayAngle = 3.f;

	/** 瞄准时的位移放大倍数。0 表示镜头贴住枪的旋转支点，抬头不会离开准星。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准晃动", meta=(DisplayName="瞄准位置倍数", ClampMin="0", ClampMax="8"))
	float AimBoneLocationSwayScale = 0.f;

	/** 瞄准时的位置平滑。0 表示不平滑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准晃动", meta=(DisplayName="瞄准位置平滑", ClampMin="0"))
	float AimCameraLocationLagSpeed = 0.f;

	/** 瞄准时的旋转平滑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准晃动", meta=(DisplayName="瞄准旋转平滑", ClampMin="0"))
	float AimCameraRotationLagSpeed = 14.f;

	/** 0 是腰射晃动，1 是瞄准晃动。跟着开镜速度过渡。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|瞄准晃动", meta=(DisplayName="当前晃动混合"))
	float AimSwayAlpha = 0.f;

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

	/** 举枪和收枪的时长。腰射偏移归零、视野变化和动画进度都走这一段。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|瞄准", meta=(DisplayName="开镜速度", ClampMin="0.01", ForceUnits="s"))
	float AimDuration = 0.18f;

	/** 打开后枪带惯性追鼠标，不再鼠标动多少枪就立刻动多少。关掉时枪直接跟鼠标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪惯性", meta=(DisplayName="启用枪惯性"))
	bool bEnableWeaponLag = true;

	/** 腰射时枪追鼠标的弹簧频率。越小越沉、越拖。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪惯性", meta=(DisplayName="腰射跟随频率", ClampMin="0.1", ForceUnits="Hz", EditCondition="bEnableWeaponLag"))
	float HipWeaponLagFrequency = 5.f;

	/** 开镜时枪追鼠标的弹簧频率。开镜要跟手一些，一般比腰射大。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪惯性", meta=(DisplayName="开镜跟随频率", ClampMin="0.1", ForceUnits="Hz", EditCondition="bEnableWeaponLag"))
	float AimWeaponLagFrequency = 9.f;

	/** 枪追鼠标的阻尼。1 刚好不冲过头，小于 1 停手时枪会甩过一点再回来，像枪的重量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪惯性", meta=(DisplayName="跟随阻尼", ClampMin="0.05", ClampMax="2", EditCondition="bEnableWeaponLag"))
	float WeaponLagDamping = 0.65f;

	/** 枪最多落后鼠标多少度，X 左右，Y 上下。猛甩时不会拖得太远。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|枪惯性", meta=(DisplayName="最大滞后", EditCondition="bEnableWeaponLag"))
	FVector2D WeaponLagMaxAngle = FVector2D(10.f, 7.f);

	/** 当前枪口朝向。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|枪惯性", meta=(DisplayName="当前枪口朝向"))
	FRotator WeaponAimRotation = FRotator::ZeroRotator;

	/** 打开后鼠标先转枪，枪口偏出死区才带着镜头和身体转。关掉时镜头、枪和身体都直接跟鼠标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="启用自由瞄准"))
	bool bEnableFreeAim = true;

	/** 腰射时枪口可以自由偏离镜头的范围，度。X 左右，Y 上下，是个椭圆。枪口在里面转，镜头不动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="腰射死区", EditCondition="bEnableFreeAim"))
	FVector2D HipFreeAimDeadZone = FVector2D(10.f, 6.f);

	/** 开镜时的死区。镜头贴着瞄具，太大会看到瞄具歪掉。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="开镜死区", EditCondition="bEnableFreeAim"))
	FVector2D AimFreeAimDeadZone = FVector2D(1.5f, 1.f);

	/** 腰射时枪口最多偏离镜头多少度。镜头来不及跟时也不会超过这里，枪不会出画面。小于死区时按死区算。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="腰射最大偏离", EditCondition="bEnableFreeAim"))
	FVector2D HipFreeAimMaxOffset = FVector2D(24.f, 15.f);

	/** 开镜时枪口最多偏离镜头多少度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="开镜最大偏离", EditCondition="bEnableFreeAim"))
	FVector2D AimFreeAimMaxOffset = FVector2D(3.f, 2.f);

	/** 镜头追枪口的弹簧频率。越大跟得越紧，越小越拖。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="跟随频率", ClampMin="0.1", ForceUnits="Hz", EditCondition="bEnableFreeAim"))
	float FreeAimFollowFrequency = 2.5f;

	/** 镜头追枪口的阻尼。1 刚好不冲过头，小于 1 会冲过一点再回来，像身体的惯性。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="跟随阻尼", ClampMin="0.05", ClampMax="2", EditCondition="bEnableFreeAim"))
	float FreeAimFollowDamping = 0.75f;

	/** 镜头最快转动速度，度每秒。猛甩鼠标时枪先到，镜头按这个速度追。0 不限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="最大转身速度", ClampMin="0", EditCondition="bEnableFreeAim"))
	float FreeAimMaxTurnSpeed = 420.f;

	/** 鼠标停下多久后，镜头开始慢慢回中对准枪口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="回中等待", ClampMin="0", ForceUnits="s", EditCondition="bEnableFreeAim"))
	float FreeAimRecenterDelay = 0.8f;

	/** 回中速度。越大回得越快，0 不回中。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="回中速度", ClampMin="0", EditCondition="bEnableFreeAim"))
	float FreeAimRecenterSpeed = 1.5f;

	/** 枪偏离镜头时绕着转的支点，相对手臂锚点，镜头朝向的局部空间。X 前，Y 右，Z 上。0 是绕眼睛转，往后往下像绕肩膀转。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="枪摆动支点", ForceUnits="cm", EditCondition="bEnableFreeAim"))
	FVector FreeAimPivotOffset = FVector(-12.f, 0.f, -10.f);

	/** 在枪口实际指向的位置画一个红点。没有准星时用来调试。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FPS|自由瞄准", meta=(DisplayName="显示枪口指向"))
	bool bDrawFreeAimDebug = false;

	/** 当前镜头和身体的朝向。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|自由瞄准", meta=(DisplayName="当前镜头朝向"))
	FRotator FreeAimViewRotation = FRotator::ZeroRotator;

	/** 当前枪口相对镜头的偏离，度。X 向右，Y 向上。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|自由瞄准", meta=(DisplayName="当前枪口偏离"))
	FVector2D FreeAimOffset = FVector2D::ZeroVector;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|瞄准", meta=(DisplayName="AimPitch", ForceUnits="deg"))
	float AimPitch = 0.f;

	/** 控制器偏航相对角色朝向的差。向右为正。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|瞄准", meta=(DisplayName="AimYaw", ForceUnits="deg"))
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FPS|瞄准晃动", meta=(DisplayName="当前骨骼晃动", ForceUnits="deg"))
	FRotator CurrentBoneSway = FRotator::ZeroRotator;

	FVector SmoothedLocalSway = FVector::ZeroVector;
	FVector CachedLocalBoneDelta = FVector::ZeroVector;
	bool bHasSmoothedLocalSway = false;
	bool bCameraManagerPrerequisiteSet = false;
	TWeakObjectPtr<USkeletalMeshComponent> PosePrerequisiteArms;
	TWeakObjectPtr<USkeletalMeshComponent> PosePrerequisiteWeapon;

	FVector2D FreeAimViewVelocity = FVector2D::ZeroVector;
	float LastLookInputTime = 0.f;
	bool bFreeAimInitialized = false;
	FVector2D WeaponAimVelocity = FVector2D::ZeroVector;
	bool bWeaponAimInitialized = false;

	void EnsureViewIsCapturedAfterCameraUpdate();
	void WaitForMeshPose(USkeletalMeshComponent* Mesh, TWeakObjectPtr<USkeletalMeshComponent>& Registered);

	float GetAimKickAlpha() const;
	void UpdateFreeAimView(ACharacter* Character, float DeltaTime);
	void DrawFreeAimDebug(const ACharacter* Character, const FRotator& AimRotation) const;
	void UpdateDrivenCamera(float DeltaTime);
	FRotator ComputeBoneSway(const USkeletalMeshComponent* BodyMesh, float SwayScale, float MaxAngle) const;
};
