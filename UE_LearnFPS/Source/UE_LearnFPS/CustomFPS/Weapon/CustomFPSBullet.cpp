/*
* 子弹实体。平时显示发射前网格，抛壳时换成发射后网格并交给物理。
*/

#include "CustomFPSBullet.h"

#include "UE_LearnFPS.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ACustomFPSBullet::ACustomFPSBullet()
{
	PrimaryActorTick.bCanEverTick = false;

	BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh"));
	SetRootComponent(BulletMesh);
	BulletMesh->SetMobility(EComponentMobility::Movable);
	BulletMesh->SetCollisionProfileName(FName("NoCollision"));
	BulletMesh->SetSimulatePhysics(false);
	BulletMesh->SetGenerateOverlapEvents(false);
	BulletMesh->CastShadow = false;
	BulletMesh->bCastDynamicShadow = false;

	static ConstructorHelpers::FObjectFinder<USoundBase> CasingHitSoundAsset(TEXT("/Game/ShotSystem/Shot_SFX/SC_IG_WEP_Casing_Random.SC_IG_WEP_Casing_Random"));
	if (CasingHitSoundAsset.Succeeded())
	{
		CasingHitSound = CasingHitSoundAsset.Object;
	}
}

/**
 * 在蓝图编辑器中构造时调用。
 */
void ACustomFPSBullet::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bEjected || !BulletMesh)
	{
		return;
	}

	BulletMesh->SetStaticMesh(CartridgeMesh);
}

/**
 * 换成发射后网格，打开物理，并按给定速度抛出。
 * 线速度是厘米/秒，角速度是度/秒。发射后网格为空时返回假。
 */
bool ACustomFPSBullet::EjectCasing(FVector LinearVelocity, FVector AngularVelocityDegrees)
{
	if (bEjected || !BulletMesh || !CasingMesh)
	{
		return false;
	}

	bEjected = true;
	BulletMesh->SetStaticMesh(CasingMesh);
	BulletMesh->SetOnlyOwnerSee(true);
	BulletMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	BulletMesh->SetCastShadow(false);
	BulletMesh->bCastDynamicShadow = false;
	BulletMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BulletMesh->SetCollisionObjectType(ECC_WorldDynamic);
	BulletMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BulletMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BulletMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BulletMesh->SetUseCCD(true);
	BulletMesh->SetNotifyRigidBodyCollision(true);
	BulletMesh->OnComponentHit.AddUniqueDynamic(this, &ACustomFPSBullet::HandleCasingHit);
	BulletMesh->SetSimulatePhysics(true);
	BulletMesh->SetEnableGravity(true);
	BulletMesh->SetMassOverrideInKg(NAME_None, FMath::Max(CasingMassKg, 0.001f), true);
	BulletMesh->AddImpulse(LinearVelocity, NAME_None, true);
	BulletMesh->AddAngularImpulseInDegrees(AngularVelocityDegrees, NAME_None, true);
	SetLifeSpan(CasingLifetime);

	const UBodySetup* Body = CasingMesh->GetBodySetup();
	if (!Body || Body->AggGeom.GetElementCount() == 0)
	{
		UE_LOG(LogUE_LearnFPS, Warning, TEXT("%s 的发射后网格 %s 没有简单碰撞，弹壳会穿过地面。"), *GetName(), *CasingMesh->GetName());
	}

	return true;
}

/**
 * 弹壳撞到东西时出声。按撞击速度调音量，太慢、太密或响过几次之后不再出声。
 */
void ACustomFPSBullet::HandleCasingHit(UPrimitiveComponent* HitComponent, AActor* /*OtherActor*/, UPrimitiveComponent* /*OtherComp*/, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bEjected || !CasingHitSound || !HitComponent || CasingHitCount >= CasingHitMaxCount)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (LastCasingHitTime >= 0.f && Now - LastCasingHitTime < CasingHitInterval)
	{
		return;
	}

	// 回调时刚体已经反弹，读到的速度偏小。用碰撞冲量除以质量估算撞击速度。
	const float Mass = FMath::Max(HitComponent->GetMass(), KINDA_SMALL_NUMBER);
	const float ImpactSpeed = NormalImpulse.IsNearlyZero()
		? HitComponent->GetPhysicsLinearVelocity().Size()
		: NormalImpulse.Size() / Mass;
	if (ImpactSpeed < CasingHitMinSpeed)
	{
		return;
	}

	LastCasingHitTime = Now;
	++CasingHitCount;

	const float FullSpeed = FMath::Max(CasingHitFullSpeed, CasingHitMinSpeed + 1.f);
	const float Volume = FMath::GetMappedRangeValueClamped(FVector2D(CasingHitMinSpeed, FullSpeed), FVector2D(0.3f, 1.f), ImpactSpeed);
	UGameplayStatics::PlaySoundAtLocation(this, CasingHitSound, Hit.ImpactPoint, Volume, FMath::FRandRange(0.95f, 1.05f));
}
