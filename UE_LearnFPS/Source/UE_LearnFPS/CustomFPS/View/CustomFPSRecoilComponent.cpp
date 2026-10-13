/*
* 后坐力组件。准星上抬平滑加到控制器上，松手后回落一部分，玩家已经压下去的量会先扣掉。
*/

#include "CustomFPS/View/CustomFPSRecoilComponent.h"

#include "CustomFPS/Weapon/CustomFPSRecoilData.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** 一个轴这一帧要加多少：按速度走，但不越过剩余量。 */
	float StepToward(float Remaining, float Rate, float DeltaTime)
	{
		const float Step = FMath::Abs(Rate) * DeltaTime;
		return FMath::Abs(Remaining) <= Step ? Remaining : FMath::Sign(Remaining) * Step;
	}

	/** 玩家往上抬的反方向拉，就从累计上抬里扣掉。同方向拉不算。 */
	float AbsorbAxis(float Accumulated, float PlayerDelta)
	{
		if (Accumulated == 0.f || Accumulated * PlayerDelta >= 0.f)
		{
			return Accumulated;
		}
		return FMath::Sign(Accumulated) * FMath::Max(0.f, FMath::Abs(Accumulated) - FMath::Abs(PlayerDelta));
	}
}

UCustomFPSRecoilComponent::UCustomFPSRecoilComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 要排在视角组件前面，同一帧的镜头才用得上这一帧的上抬。
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCustomFPSRecoilComponent::AddRecoil(const UCustomFPSRecoilData* Data, FRotator ControlRise, FRotator CameraKick, FVector WeaponKickOffset, FRotator WeaponKickRotation)
{
	if (Data)
	{
		RiseDuration = Data->RiseDuration;
		RecoveryFraction = Data->RecoveryFraction;
		RecoveryDelay = Data->RecoveryDelay;
		RecoverySpeed = Data->RecoverySpeed;
		KickSnapSpeed = Data->CameraKickSnapSpeed;
		KickReturnSpeed = Data->CameraKickReturnSpeed;
		WeaponKickSnapSpeed = Data->WeaponKickSnapSpeed;
		WeaponKickReturnSpeed = Data->WeaponKickReturnSpeed;
	}

	WeaponKickOffsetTarget += WeaponKickOffset;
	WeaponKickRotationTarget += WeaponKickRotation;

	PendingRise += FVector2D(ControlRise.Pitch, ControlRise.Yaw);
	const float Duration = FMath::Max(RiseDuration, KINDA_SMALL_NUMBER);
	PendingRiseRate = PendingRise / Duration;
	bRecovering = false;

	const UWorld* World = GetWorld();
	LastRecoilTime = World ? World->GetTimeSeconds() : 0.f;

	CameraKickTarget += CameraKick;
}

void UCustomFPSRecoilComponent::ResetRecoil()
{
	AccumulatedRise = FVector2D::ZeroVector;
	PendingRise = FVector2D::ZeroVector;
	PendingRiseRate = FVector2D::ZeroVector;
	RecoveryTarget = FVector2D::ZeroVector;
	bRecovering = false;
	CameraKickTarget = FRotator::ZeroRotator;
	CameraKickCurrent = FRotator::ZeroRotator;
	WeaponKickOffsetTarget = FVector::ZeroVector;
	WeaponKickOffsetCurrent = FVector::ZeroVector;
	WeaponKickRotationTarget = FRotator::ZeroRotator;
	WeaponKickRotationCurrent = FRotator::ZeroRotator;
}

void UCustomFPSRecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CameraKickCurrent = FMath::RInterpTo(CameraKickCurrent, CameraKickTarget, DeltaTime, KickSnapSpeed);
	CameraKickTarget = FMath::RInterpTo(CameraKickTarget, FRotator::ZeroRotator, DeltaTime, KickReturnSpeed);

	WeaponKickOffsetCurrent = FMath::VInterpTo(WeaponKickOffsetCurrent, WeaponKickOffsetTarget, DeltaTime, WeaponKickSnapSpeed);
	WeaponKickOffsetTarget = FMath::VInterpTo(WeaponKickOffsetTarget, FVector::ZeroVector, DeltaTime, WeaponKickReturnSpeed);
	WeaponKickRotationCurrent = FMath::RInterpTo(WeaponKickRotationCurrent, WeaponKickRotationTarget, DeltaTime, WeaponKickSnapSpeed);
	WeaponKickRotationTarget = FMath::RInterpTo(WeaponKickRotationTarget, FRotator::ZeroRotator, DeltaTime, WeaponKickReturnSpeed);

	const APawn* Pawn = GetOwner<APawn>();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		bHasLastControlRotation = false;
		return;
	}

	// 上一帧结束到现在，控制器旋转多出来的部分是玩家自己拉的。
	if (bHasLastControlRotation)
	{
		const FRotator Control = Controller->GetControlRotation();
		AbsorbPlayerCompensation(FVector2D(
			FRotator::NormalizeAxis(Control.Pitch - LastControlRotation.Pitch),
			FRotator::NormalizeAxis(Control.Yaw - LastControlRotation.Yaw)));
	}

	if (!PendingRise.IsNearlyZero(KINDA_SMALL_NUMBER))
	{
		const FVector2D Step(
			StepToward(PendingRise.X, PendingRiseRate.X, DeltaTime),
			StepToward(PendingRise.Y, PendingRiseRate.Y, DeltaTime));
		PendingRise -= Step;
		AccumulatedRise += ApplyControlDelta(Controller, Step);
	}
	else
	{
		PendingRise = FVector2D::ZeroVector;
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		const bool bReadyToRecover = LastRecoilTime >= 0.f && Now - LastRecoilTime >= RecoveryDelay;
		if (bReadyToRecover && !AccumulatedRise.IsNearlyZero(0.001f))
		{
			if (!bRecovering)
			{
				bRecovering = true;
				RecoveryTarget = AccumulatedRise * (1.f - RecoveryFraction);
			}

			// 玩家已经压过了目标，这个轴就不再往回拉。
			if (AccumulatedRise.X * RecoveryTarget.X <= 0.f || FMath::Abs(AccumulatedRise.X) <= FMath::Abs(RecoveryTarget.X))
			{
				RecoveryTarget.X = AccumulatedRise.X;
			}
			if (AccumulatedRise.Y * RecoveryTarget.Y <= 0.f || FMath::Abs(AccumulatedRise.Y) <= FMath::Abs(RecoveryTarget.Y))
			{
				RecoveryTarget.Y = AccumulatedRise.Y;
			}

			FVector2D NewRise = FMath::Vector2DInterpTo(AccumulatedRise, RecoveryTarget, DeltaTime, RecoverySpeed);
			if (FVector2D::Distance(NewRise, RecoveryTarget) < 0.01f)
			{
				NewRise = RecoveryTarget;
			}
			ApplyControlDelta(Controller, NewRise - AccumulatedRise);
			AccumulatedRise = NewRise;

			if (AccumulatedRise.Equals(RecoveryTarget, 0.001f))
			{
				AccumulatedRise = FVector2D::ZeroVector;
				bRecovering = false;
			}
		}
	}

	LastControlRotation = Controller->GetControlRotation();
	bHasLastControlRotation = true;
}

FVector2D UCustomFPSRecoilComponent::ApplyControlDelta(AController* Controller, const FVector2D& Delta)
{
	if (!Controller || Delta.IsNearlyZero(KINDA_SMALL_NUMBER))
	{
		return FVector2D::ZeroVector;
	}

	FRotator Control = Controller->GetControlRotation();
	const float OldPitch = FRotator::NormalizeAxis(Control.Pitch);
	// 抬到头顶时被夹住的那部分没有真的加上去，不能记进累计上抬，否则回落会拉过头。
	const float NewPitch = FMath::Clamp(OldPitch + Delta.X, -89.f, 89.f);
	Control.Pitch = NewPitch;
	Control.Yaw = FRotator::NormalizeAxis(Control.Yaw + Delta.Y);
	Controller->SetControlRotation(Control);
	return FVector2D(NewPitch - OldPitch, Delta.Y);
}

void UCustomFPSRecoilComponent::AbsorbPlayerCompensation(const FVector2D& PlayerDelta)
{
	AccumulatedRise.X = AbsorbAxis(AccumulatedRise.X, PlayerDelta.X);
	AccumulatedRise.Y = AbsorbAxis(AccumulatedRise.Y, PlayerDelta.Y);
}
