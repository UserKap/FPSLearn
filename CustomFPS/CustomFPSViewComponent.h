/*	
* 视角组件，负责角色的视角、瞄准、侧身等动作。
* 包括初始化视角组件、设置相机、移动、瞄准、侧身、更新相机附件等方法。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSViewComponent.generated.h"

class UCameraComponent;

/** 鼠标视角、开镜视野、侧身，以及相机跟随骨骼。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="视角"))
class UE_LEARNFPS_API UCustomFPSViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSViewComponent();

	void SetupCamera(UCameraComponent* InCamera);
	void Look(float Yaw, float Pitch);
	void Lean(float Side);
	void UpdateCameraAttachment();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 摄像机跟随的身体骨骼或插槽。留空，或骨骼不存在时，固定在胶囊体眼睛高度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FName CameraAttachBone = FName("head");

	/** 非瞄准时镜头偏航灵敏度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Look", meta=(ClampMin="0"))
	float LookSensitivityYaw = 0.5f;

	/** 非瞄准时镜头俯仰灵敏度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Look", meta=(ClampMin="0"))
	float LookSensitivityPitch = 0.5f;

	/** 非瞄准时镜头缩放值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim", meta=(ClampMin="1", ClampMax="170", ForceUnits="deg"))
	float HipFieldOfView = 105.f;

	/** 瞄准时镜头缩放值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim", meta=(ClampMin="1", ClampMax="170", ForceUnits="deg"))
	float AimFieldOfView = 60.f;

	/** 瞄准时镜头缩放插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aim", meta=(ClampMin="0"))
	float AimInterpSpeed = 12.f;

	/** 侧身最大滚转角。负值为向左，正值为向右。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sideways", meta=(ClampMin="0", ClampMax="89", ForceUnits="deg"))
	float MaxLeanAngle = 20.f;

	/** 侧身滚动插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sideways", meta=(ClampMin="0"))
	float LeanInterpSpeed = 2.f;

	/** 目标侧身滚转角度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sideways")
	float TargetLeanRoll = 0.f;

	/** 当前侧身滚转角度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sideways")
	float CurrentLeanRoll = 0.f;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
};
