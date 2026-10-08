/*	
* 输入组件，负责绑定输入、添加输入上下文、移除输入上下文、获取移动和视角组件等方法。
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CustomFPSInputComponent.generated.h"

class UCustomFPSLocomotionComponent;
class UCustomFPSViewComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
struct FInputActionValue;

/** 绑定 IMC_CustomFPSInput，并把按键转给移动和视角组件。 */
UCLASS(Blueprintable, ClassGroup=(CustomFPS), meta=(DisplayName="输入"))
class UE_LEARNFPS_API UCustomFPSInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomFPSInputComponent();

	void BindInput(UInputComponent* PlayerInputComponent);
	void AddMappingContext();
	void RemoveMappingContext();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	/** 优先级高于模板的 IMC_MouseLook（0），鼠标会先进入这个上下文。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	int32 MappingPriority = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> SquatAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TObjectPtr<UInputAction> SidewaysAction;

private:
	void MoveInput(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);
	void AimStarted();
	void AimCompleted();
	void RunStarted();
	void RunCompleted();
	void SquatStarted();
	void SquatCompleted();
	void SidewaysInput(const FInputActionValue& Value);
	void SidewaysCompleted();

	UCustomFPSLocomotionComponent* GetLocomotion() const;
	UCustomFPSViewComponent* GetView() const;
};
