

#include "BossCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "SampleProject1Character.h"
#include "Components/DecalComponent.h"
#include "Particles/ParticleSystem.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABossCharacter::ABossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

}

void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();

	ChaseStartTime = GetWorld()->GetTimeSeconds() + ChaseStartDelay;

	CurrentHealth = MaxHealth;
	Initiative = FMath::Clamp(InitialInitiative, -100.0f, 100.0f);

}

// Tick and input
void ABossCharacter::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);

	if (GetWorld()->GetTimeSeconds() < ChaseStartTime)
		return;

	if (AttackState != EBossAttackState::Idle && AttackState != EBossAttackState::Windup)
		return;

	//Player chase
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!IsValid(Player))
		return;

	FVector Direction = Player->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero())
		return;

	const bool bIsChasing = AttackState == EBossAttackState::Idle && Direction.SizeSquared() > FMath::Square(AttackRange);

	FVector FacingDirection = Direction;

	if (bIsChasing)
	{
		FVector MoveVelocity = GetVelocity();
		MoveVelocity.Z = 0.0f;

		if (!MoveVelocity.IsNearlyZero())
		{
			FacingDirection = MoveVelocity;
		}
	}

	//Attack phase
	const FRotator TargetRotation = FacingDirection.Rotation();
	constexpr float WindupFacingLockTime = 0.15f;

	const bool bLockWindupFacing = AttackState == EBossAttackState::Windup
		&& GetWorldTimerManager().GetTimerRemaining(AttackPhaseTimerHandle) <= WindupFacingLockTime;

	if (!bLockWindupFacing)
	{
		const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, 6.0f);
		SetActorRotation(NewRotation);
	}

	//Cone Attack
	if (AttackState == EBossAttackState::Windup && CurrentAttackType == EBossAttackType::Cone)
	{
		DrawConeAttackDebug(FColor::Yellow, 0.0f);

		if (ConeAttackDecalMaterial)
		{
			const FVector Center = GetActorLocation();
			const FVector DecalSize(200.0f, ConeAttackRange, ConeAttackRange);
			constexpr float ConeDecalYawOffset = 0.0f;
			const FRotator DecalRotation(-90.0f, GetActorRotation().Yaw + ConeDecalYawOffset, 0.0f);

			if (!IsValid(HeavyAttackDecal))
			{
				HeavyAttackDecal = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), ConeAttackDecalMaterial.Get(), DecalSize, Center, DecalRotation, 0.0f);
			}
			else
			{
				HeavyAttackDecal->SetWorldLocationAndRotation(Center, DecalRotation);
			}
		}
	}

	//Strong Attack
	if (AttackState == EBossAttackState::Windup && CurrentAttackType == EBossAttackType::Heavy && HeavyAttackDecalMaterial)
	{
		const FVector Center = GetActorLocation() + GetActorForwardVector() * 150.0f;
		const FVector DecalSize(200.0f, HeavyAttackRadius, HeavyAttackRadius);

		if (!IsValid(HeavyAttackDecal))
		{
			HeavyAttackDecal = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), HeavyAttackDecalMaterial.Get(), DecalSize, Center, FRotator(-90.0f, 0.0f, 0.0f), 0.0f);
		}
		else
		{
			HeavyAttackDecal->SetWorldLocation(Center);
		}
	}

	if (AttackState == EBossAttackState::Idle)
	{
		if (Direction.SizeSquared() <= FMath::Square(AttackRange))
		{
			GetCharacterMovement()->StopMovementImmediately();
			const EBossAttackType NextAttack = bHeavyAttackPending ? EBossAttackType::Heavy : EBossAttackType::Basic;
			bHeavyAttackPending = false;
			StartAttack(NextAttack);
		}
		else
		{
			AddMovementInput(Direction.GetSafeNormal(), 1.0f);
		}
	}
}

void ABossCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// Attack phases and hit checks
void ABossCharacter::StartAttack(EBossAttackType AttackType)
{
	if (AttackState != EBossAttackState::Idle)
		return;

	CurrentAttackType = AttackType;
	bIsSecondBasicAttack = false;
	AttackState = EBossAttackState::Windup;

	if (CurrentAttackType == EBossAttackType::Cone)
	{
		bPendingSecondBasicHit = false;

		if (ConeAttackMontage)
		{
			PlayAnimMontage(ConeAttackMontage.Get());
		}

		GetWorldTimerManager().SetTimer(AttackPhaseTimerHandle, this, &ABossCharacter::EnterActive, ConeWindupDuration, false);
		return;
	}

	const bool bHeavy = CurrentAttackType == EBossAttackType::Heavy;
	const float Duration = bHeavy ? HeavyWindupDuration : WindupDuration;

	bPendingSecondBasicHit = !bHeavy && SecondBasicAttackMontage != nullptr;

	UAnimMontage* Montage = bHeavy ? StrongAttackMontage.Get() : AttackMontage.Get();
	PlayAnimMontage(Montage);

	GetWorldTimerManager().SetTimer(AttackPhaseTimerHandle, this, &ABossCharacter::EnterActive, Duration, false);
}

void ABossCharacter::EnterActive()
{
	if (AttackState != EBossAttackState::Windup)
		return;

	AttackState = EBossAttackState::Active;
	ClearHeavyAttackDecal();

	if (CurrentAttackType == EBossAttackType::Heavy && HeavyAttackImpactEffect)
	{
		const FVector AttackCenter = GetActorLocation() + GetActorForwardVector() * 150.0f;
		const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector ImpactLocation = AttackCenter - FVector(0.0f, 0.0f, CapsuleHalfHeight - 5.0f);
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HeavyAttackImpactEffect.Get(), ImpactLocation, GetActorRotation());
	}

	else if (CurrentAttackType == EBossAttackType::Cone && ConeAttackEffect)
	{
		const float FloorOffset = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.0f;
		const FVector EffectLocation = GetActorLocation() - FVector(0.0f, 0.0f, FloorOffset);
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ConeAttackEffect.Get(), EffectLocation, GetActorRotation());
	}

	PerformAttackHitCheck();

	if (AttackState != EBossAttackState::Active)
		return;

	const float DelayBeforeRecovery = bPendingSecondBasicHit ? BasicComboTransitionDelay : ActiveDuration;
	GetWorldTimerManager().SetTimer(AttackPhaseTimerHandle, this, &ABossCharacter::EnterRecovery, DelayBeforeRecovery, false);
}

void ABossCharacter::EnterRecovery()
{
	if (AttackState != EBossAttackState::Active)
		return;

	if (bPendingSecondBasicHit)
	{
		bPendingSecondBasicHit = false;
		bIsSecondBasicAttack = true;
		AttackState = EBossAttackState::Windup;

		PlayAnimMontage(SecondBasicAttackMontage.Get());
		GetWorldTimerManager().SetTimer(AttackPhaseTimerHandle, this, &ABossCharacter::EnterActive, SecondBasicWindupDuration, false);
		return;
	}

	AttackState = EBossAttackState::Recovery;

	GetWorldTimerManager().SetTimer(AttackPhaseTimerHandle, this, &ABossCharacter::FinishAttack, RecoveryDuration, false);
}

void ABossCharacter::FinishAttack()
{
	if (AttackState != EBossAttackState::Recovery)
		return;

	AttackState = EBossAttackState::Idle;

	if (CurrentAttackType == EBossAttackType::Basic)
		bHeavyAttackPending = true;

	else if (CurrentAttackType == EBossAttackType::Heavy)
		StartAttack(EBossAttackType::Cone);

}

void ABossCharacter::PerformAttackHitCheck()
{
	ASampleProject1Character* Player = Cast<ASampleProject1Character>(UGameplayStatics::GetPlayerCharacter(this, 0));

	if (!IsValid(Player))
		return;

	//Cone Attack
	if (CurrentAttackType == EBossAttackType::Cone)
	{
		DrawConeAttackDebug(FColor::Red, 0.5f);

		const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		const FVector DirectionToPlayer = ToPlayer.GetSafeNormal2D();
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		const float ForwardDot = FVector::DotProduct(Forward, DirectionToPlayer);
		const float MinDot = FMath::Cos(FMath::DegreesToRadians(ConeAttackHalfAngle));

		const bool bInRange = ToPlayer.SizeSquared2D() <= FMath::Square(ConeAttackRange);
		const bool bInAngle = DirectionToPlayer.IsNearlyZero() || ForwardDot >= MinDot;

		if (bInRange && bInAngle && !Player->IsDodging())
		{
			UGameplayStatics::ApplyDamage(Player, ConeAttackDamage, GetController(), this, nullptr);
		}

		return;
	}

	//Heavy andd Normal Attack
	const bool bHeavy = CurrentAttackType == EBossAttackType::Heavy;

	const FVector Center = GetActorLocation() + GetActorForwardVector() * 150.0f;
	const float Radius = bHeavy ? HeavyAttackRadius : 220.0f;
	const float Damage = bHeavy ? HeavyAttackDamage : AttackDamage;

	DrawDebugSphere(GetWorld(), Center, Radius, 24, bHeavy ? FColor::Red : FColor::Yellow, false, 0.7f);
	const float DodgeRecognitionRadius = Radius + 120.0f;

	if (Player->IsDodging() && FVector::DistSquared2D(Player->GetActorLocation(), Center) <= FMath::Square(DodgeRecognitionRadius))
	{
		ChangeInitiative(bHeavy ? 0.0f : -100.0f);

		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;

	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (Overlap.GetActor() != Player)
			continue;

		const float DamageApplied = UGameplayStatics::ApplyDamage(Player, Damage, GetController(), this, nullptr);
		break;
	}
}

// Attack telegraphs
void ABossCharacter::DrawConeAttackDebug(const FColor& Color, float LifeTime) const
{
	constexpr int32 SegmentCount = 16;

	const FVector FloorOffset(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.0f);
	const FVector Origin = GetActorLocation() - FloorOffset;
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();

	FVector PreviousPoint = Origin + Forward.RotateAngleAxis(-ConeAttackHalfAngle, FVector::UpVector) * ConeAttackRange;
	DrawDebugLine(GetWorld(), Origin, PreviousPoint, Color, false, LifeTime, 0, 2.0f);

	for (int32 Index = 1; Index <= SegmentCount; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / SegmentCount;
		const float Angle = FMath::Lerp(-ConeAttackHalfAngle, ConeAttackHalfAngle, Alpha);
		const FVector Point = Origin + Forward.RotateAngleAxis(Angle, FVector::UpVector) * ConeAttackRange;

		DrawDebugLine(GetWorld(), PreviousPoint, Point, Color, false, LifeTime, 0, 2.0f);
		PreviousPoint = Point;
	}

	DrawDebugLine(GetWorld(), Origin, PreviousPoint, Color, false, LifeTime, 0, 2.0f);
}

void ABossCharacter::ClearHeavyAttackDecal()
{
	if (IsValid(HeavyAttackDecal))
	{
		HeavyAttackDecal->DestroyComponent();
	}

	HeavyAttackDecal = nullptr;
}

// Initiative and stagger
void ABossCharacter::ChangeInitiative(float Delta)
{

	if (AttackState == EBossAttackState::Staggered || AttackState == EBossAttackState::Dead)
		return;

	const float PreviousInitiative = Initiative;

	Initiative = FMath::Clamp(Initiative + Delta, -100.0f, 100.0f);

	if (PreviousInitiative < 100.0f && Initiative >= 100.0f)
		StartStagger();
}

void ABossCharacter::StartStagger()
{
	if (AttackState == EBossAttackState::Staggered || AttackState == EBossAttackState::ExecutionReady || AttackState == EBossAttackState::Dead || AttackState == EBossAttackState::Inactive)
		return;

	bPendingSecondBasicHit = false;
	bHeavyAttackPending = false;
	GetWorldTimerManager().ClearTimer(AttackPhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(StaggerTimerHandle);
	GetWorldTimerManager().ClearTimer(SecondStaggerTimerHandle);
	ClearHeavyAttackDecal();
	GetCharacterMovement()->StopMovementImmediately();


	//Execution ready
	if (CurrentHealth < MaxHealth * ExecutionHealthRatio)
	{
		EnterExecutionReady();
		return;
	}

	AttackState = EBossAttackState::Staggered;

	//Skill ready
	++StaggersTowardSkill;
	bSkillReady = StaggersTowardSkill == StaggersRequiredForSkill;

	if (bSkillReady)
	{
		StaggersTowardSkill = 0;
		UE_LOG(LogTemp, Warning, TEXT("Stagger skill ready!"));
	}

	UAnimMontage* Montage = bSkillReady ? SkillReadyStaggerMontage.Get() : nullptr;
	const bool bUseNormalStagger = Montage == nullptr;

	if (bUseNormalStagger)
	{
		Montage = StaggerMontage.Get();
	}

	if (Montage && PlayAnimMontage(Montage) > 0.0f && bUseNormalStagger && SecondStaggerMontage && SecondStaggerDelay < StaggerDuration)
	{
		GetWorldTimerManager().SetTimer(SecondStaggerTimerHandle, this, &ABossCharacter::PlaySecondStaggerMontage, SecondStaggerDelay, false);
	}

	GetWorldTimerManager().SetTimer(StaggerTimerHandle, this, &ABossCharacter::FinishStagger, StaggerDuration, false);
}

void ABossCharacter::PlaySecondStaggerMontage()
{
	if (AttackState == EBossAttackState::Staggered && SecondStaggerMontage)
	{
		PlayAnimMontage(SecondStaggerMontage.Get());
	}
}

void ABossCharacter::FinishStagger()
{
	if (AttackState != EBossAttackState::Staggered)
		return;

	GetWorldTimerManager().ClearTimer(SecondStaggerTimerHandle);

	Initiative = -50.0f;

	bSkillReady = false;

	AttackState = EBossAttackState::Idle;

	UE_LOG(LogTemp, Warning, TEXT("Boss Recovered!"));
}

// Stagger skill and execution
bool ABossCharacter::TryUseStaggerSkill(AController* InstigatorController, AActor* DamageCauser)
{
	if (!bSkillReady || AttackState != EBossAttackState::Staggered)
		return false;

	const float DamageApplied = UGameplayStatics::ApplyDamage(this, MaxHealth * SkillDamageRatio, InstigatorController, DamageCauser, nullptr);
	if (DamageApplied <= 0.0f)
		return false;

	bSkillReady = false;
	return true;
}

void ABossCharacter::EnterExecutionReady()
{
	AttackState = EBossAttackState::ExecutionReady;

	GetWorldTimerManager().ClearTimer(StaggerTimerHandle);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (ExecutionReadyMontage)
	{
		PlayAnimMontage(ExecutionReadyMontage.Get());
	}

	UE_LOG(LogTemp, Warning, TEXT("Execution ready!"));
}

float ABossCharacter::Execute()
{
	if (!CanBeExecuted())
	{
		return 0.0f;
	}

	const float DamageApplied = CurrentHealth;
	CurrentHealth = 0.0f;
	BeginDeath();
	return DamageApplied;
}

// Damage and combat shutdown
float ABossCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{

	if (CurrentHealth <= 0.0f || DamageAmount <= 0.0f || AttackState == EBossAttackState::ExecutionReady || AttackState == EBossAttackState::Inactive)
		return 0.0f;

	if (AttackState != EBossAttackState::Staggered)
	{
		if (GuardImpactEffect && IsValid(DamageCauser))
		{
			const FVector TowardAttacker = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const FVector ImpactLocation = GetActorLocation() + TowardAttacker * 45.0f + FVector(0.0f, 0.0f, 25.0f);
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), GuardImpactEffect.Get(), ImpactLocation, TowardAttacker.Rotation());
		}

		return 0.0f;
	}

	if (CurrentHealth <= 0.0f || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	const float DamageApplied = FMath::Min(DamageAmount, CurrentHealth);
	CurrentHealth -= DamageApplied;

	if (CurrentHealth <= 0.0f)
		BeginDeath();

	return DamageApplied;
}

void ABossCharacter::BeginDeath()
{
	AttackState = EBossAttackState::Dead;

	GetWorldTimerManager().ClearTimer(AttackPhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(StaggerTimerHandle);
	GetWorldTimerManager().ClearTimer(SecondStaggerTimerHandle);
	ClearHeavyAttackDecal();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);

	UE_LOG(LogTemp, Warning, TEXT("Boss has been defeated!"));

	OnBossDefeated();

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (DeathMontage && AnimInstance)
	{
		const float DeathDuration = AnimInstance->Montage_Play(DeathMontage.Get(), 1.0f, EMontagePlayReturnType::Duration);
		if (DeathDuration > 0.0f)
		{
			return;
		}
	}
}

void ABossCharacter::StopCombat()
{
	if (AttackState == EBossAttackState::Dead || AttackState == EBossAttackState::Inactive)
	{
		return;
	}

	AttackState = EBossAttackState::Inactive;

	GetWorldTimerManager().ClearTimer(AttackPhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(StaggerTimerHandle);
	GetWorldTimerManager().ClearTimer(SecondStaggerTimerHandle);
	ClearHeavyAttackDecal();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.2f);
	}
}
