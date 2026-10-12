/*	
* 输入组件，负责绑定输入、添加输入上下文、移除输入上下文、获取移动和视角组件等方法。
*/

#include "CustomFPSInputComponent.h"

#include "CustomFPS/Character/CustomFPSCharacter.h"
#include "CustomFPS/Locomotion/CustomFPSLocomotionComponent.h"
#include "CustomFPS/Weapon/CustomFPSWeaponHolder.h"
#include "CustomFPS/View/CustomFPSViewComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UE_LearnFPS.h"
#include "UObject/ConstructorHelpers.h"

UCustomFPSInputComponent::UCustomFPSInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> ContextAsset(TEXT("/Game/CustomEditorBP/Input/IMC_CustomFPSInput.IMC_CustomFPSInput"));
	if (ContextAsset.Succeeded())
	{
		InputMappingContext = ContextAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveAsset(TEXT("/Game/CustomEditorBP/Input/IA_Move.IA_Move"));
	if (MoveAsset.Succeeded())
	{
		MoveAction = MoveAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookAsset(TEXT("/Game/CustomEditorBP/Input/IA_Look.IA_Look"));
	if (LookAsset.Succeeded())
	{
		LookAction = LookAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AimAsset(TEXT("/Game/CustomEditorBP/Input/IA_Aim.IA_Aim"));
	if (AimAsset.Succeeded())
	{
		AimAction = AimAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RunAsset(TEXT("/Game/CustomEditorBP/Input/IA_Run.IA_Run"));
	if (RunAsset.Succeeded())
	{
		RunAction = RunAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SquatAsset(TEXT("/Game/CustomEditorBP/Input/IA_Squat.IA_Squat"));
	if (SquatAsset.Succeeded())
	{
		SquatAction = SquatAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SidewaysAsset(TEXT("/Game/CustomEditorBP/Input/IA_Sideways.IA_Sideways"));
	if (SidewaysAsset.Succeeded())
	{
		SidewaysAction = SidewaysAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> FireAsset(TEXT("/Game/CustomEditorBP/Input/IA_Fire.IA_Fire"));
	if (FireAsset.Succeeded())
	{
		FireAction = FireAsset.Object;
	}
}

void UCustomFPSInputComponent::BindInput(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogUE_LearnFPS, Error, TEXT("%s requires an Enhanced Input component."), *GetNameSafe(GetOwner()));
		return;
	}

	if (!FireAction)
	{
		FireAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CustomEditorBP/Input/IA_Fire.IA_Fire"));
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UCustomFPSInputComponent::MoveInput);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &UCustomFPSInputComponent::LookInput);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Ongoing, this, &UCustomFPSInputComponent::LookInput);
	}

	if (AimAction)
	{
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &UCustomFPSInputComponent::AimStarted);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &UCustomFPSInputComponent::AimCompleted);
	}

	if (RunAction)
	{
		// 按住时 Started 或每帧 Triggered 都进入跑步；松开时 Completed 或 Canceled 都退出。
		EnhancedInput->BindAction(RunAction, ETriggerEvent::Started, this, &UCustomFPSInputComponent::RunStarted);
		EnhancedInput->BindAction(RunAction, ETriggerEvent::Triggered, this, &UCustomFPSInputComponent::RunHeld);
		EnhancedInput->BindAction(RunAction, ETriggerEvent::Completed, this, &UCustomFPSInputComponent::RunCompleted);
		EnhancedInput->BindAction(RunAction, ETriggerEvent::Canceled, this, &UCustomFPSInputComponent::RunCompleted);
	}

	if (SquatAction)
	{
		EnhancedInput->BindAction(SquatAction, ETriggerEvent::Started, this, &UCustomFPSInputComponent::SquatStarted);
		EnhancedInput->BindAction(SquatAction, ETriggerEvent::Completed, this, &UCustomFPSInputComponent::SquatCompleted);
	}

	if (SidewaysAction)
	{
		EnhancedInput->BindAction(SidewaysAction, ETriggerEvent::Triggered, this, &UCustomFPSInputComponent::SidewaysInput);
		EnhancedInput->BindAction(SidewaysAction, ETriggerEvent::Completed, this, &UCustomFPSInputComponent::SidewaysCompleted);
	}

	if (FireAction)
	{
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &UCustomFPSInputComponent::FireStarted);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &UCustomFPSInputComponent::FireCompleted);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Canceled, this, &UCustomFPSInputComponent::FireCompleted);
	}
	else
	{
		UE_LOG(LogUE_LearnFPS, Error, TEXT("FireAction is null. IA_Fire was not bound."));
	}
}

void UCustomFPSInputComponent::AddMappingContext()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !InputMappingContext)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (!PlayerController)
	{
		UE_LOG(LogUE_LearnFPS, Error, TEXT("PlayerController is null."));
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->AddMappingContext(InputMappingContext, MappingPriority);
	}
}

void UCustomFPSInputComponent::RemoveMappingContext()
{
	if (!InputMappingContext)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->RemoveMappingContext(InputMappingContext);
	}
}

void UCustomFPSInputComponent::MoveInput(const FInputActionValue& Value)
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		const FVector2D Movement = Value.Get<FVector2D>();
		Locomotion->Move(Movement.X, Movement.Y);
	}
}

void UCustomFPSInputComponent::LookInput(const FInputActionValue& Value)
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSViewComponent* View = GetView())
	{
		const FVector2D Look = Value.Get<FVector2D>();
		View->Look(Look.X, Look.Y);
	}
}

void UCustomFPSInputComponent::AimStarted()
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StartAim();
	}
}

void UCustomFPSInputComponent::AimCompleted()
{
	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StopAim();
	}
}

void UCustomFPSInputComponent::RunStarted()
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StartSprint(true);
	}
}

void UCustomFPSInputComponent::RunHeld()
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StartSprint(false);
	}
}

void UCustomFPSInputComponent::RunCompleted()
{
	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StopSprint();
	}
}

void UCustomFPSInputComponent::SquatStarted()
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StartCrouch();
	}
}

void UCustomFPSInputComponent::SquatCompleted()
{
	if (UCustomFPSLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->StopCrouch();
	}
}

void UCustomFPSInputComponent::SidewaysInput(const FInputActionValue& Value)
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSViewComponent* View = GetView())
	{
		View->Lean(Value.Get<float>());
	}
}

void UCustomFPSInputComponent::SidewaysCompleted()
{
	if (IsOwnerDead())
	{
		return;
	}

	if (UCustomFPSViewComponent* View = GetView())
	{
		View->Lean(0.f);
	}
}

void UCustomFPSInputComponent::FireStarted()
{
	if (AActor* Owner = GetOwner(); Owner && Owner->GetClass()->ImplementsInterface(UCustomFPSWeaponHolder::StaticClass()))
	{
		ICustomFPSWeaponHolder::Execute_StartWeaponFire(Owner, Owner);
	}
}

void UCustomFPSInputComponent::FireCompleted()
{
	if (AActor* Owner = GetOwner(); Owner && Owner->GetClass()->ImplementsInterface(UCustomFPSWeaponHolder::StaticClass()))
	{
		ICustomFPSWeaponHolder::Execute_StopWeaponFire(Owner, Owner);
	}
}

bool UCustomFPSInputComponent::IsOwnerDead() const
{
	const ACustomFPSCharacter* Character = GetOwner<ACustomFPSCharacter>();
	return Character && Character->IsDead();
}

UCustomFPSLocomotionComponent* UCustomFPSInputComponent::GetLocomotion() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UCustomFPSLocomotionComponent>() : nullptr;
}

UCustomFPSViewComponent* UCustomFPSInputComponent::GetView() const
{
	AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UCustomFPSViewComponent>() : nullptr;
}
