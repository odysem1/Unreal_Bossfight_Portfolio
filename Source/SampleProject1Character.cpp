// Copyright Epic Games, Inc. All Rights Reserved.

#include "SampleProject1Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "SampleProject1.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Math/RotationMatrix.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

ASampleProject1Character::ASampleProject1Character()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetAbsolute(false, true, false);
	CameraBoom->SetWorldRotation(FRotator(-45.f, -45.f, 0.f));
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;


	PrimaryActorTick.bCanEverTick = true;
}

void ASampleProject1Character::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHealth = MaxHealth;
	bIsDead = false;
}

// Input and movement
void ASampleProject1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		/*EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);*/

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASampleProject1Character::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASampleProject1Character::Look);


		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &ASampleProject1Character::Dodge);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASampleProject1Character::ClearDodgeMoveInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ASampleProject1Character::ClearDodgeMoveInput);

		EnhancedInputComponent->BindAction(CombatAction, ETriggerEvent::Started, this, &ASampleProject1Character::Combat);
		EnhancedInputComponent->BindAction(StaggerSkillAction, ETriggerEvent::Started, this, &ASampleProject1Character::UseStaggerSkill);

	}

	else
	{
		UE_LOG(LogSampleProject1, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ASampleProject1Character::Move(const FInputActionValue& Value)
{	
	if (bIsDodging && !bAllowMoveDuringDodge)
		return;

	if (bIsExecuting)
		return;

	if (CombatState != EPlayerCombatState::Idle && CombatState != EPlayerCombatState::Attacking)
	{
		return;
	}

	const FRotator CameraYaw(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f);
	const FRotationMatrix CameraRotationMatrix(CameraYaw);
	const FVector Forward = CameraRotationMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = CameraRotationMatrix.GetUnitAxis(EAxis::Y);

	const FVector2D MoveInput = Value.Get<FVector2D>();

	// Cache the dodge direction even when walking is blocked during an attack.
	DodgeMoveInput = Forward * MoveInput.Y + Right * MoveInput.X;

	if (CombatState == EPlayerCombatState::Attacking && !bAllowMoveDuringAttack)
		return;

	AddMovementInput(Forward, MoveInput.Y);
	AddMovementInput(Right, MoveInput.X);
}

void ASampleProject1Character::ClearDodgeMoveInput()
{
	DodgeMoveInput = FVector::ZeroVector;
}

void ASampleProject1Character::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ASampleProject1Character::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ASampleProject1Character::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ASampleProject1Character::DoJumpStart()
{
	Jump();
}

void ASampleProject1Character::DoJumpEnd()
{
	StopJumping();
}

// Attack and combo
void ASampleProject1Character::Combat()
{	
	//Normal Attack
	if (!bIsDead && !bIsDodging && !bIsExecuting && CombatState == EPlayerCombatState::Attacking && AttackMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			static const FName Attack1Section(TEXT("Attack1"));
			static const FName Attack2Section(TEXT("Attack2"));
			constexpr float ComboInputStart = 0.75f;
			constexpr float ComboInputEnd = 1.50f;

			const float Position = AnimInstance->Montage_GetPosition(AttackMontage.Get());
			if (AnimInstance->Montage_GetCurrentSection(AttackMontage.Get()) == Attack1Section && Position >= ComboInputStart && Position <= ComboInputEnd)
			{
				AnimInstance->Montage_SetNextSection(Attack1Section, Attack2Section, AttackMontage.Get());
			}
		}

		return;
	}

	if (bIsDead || bIsDodging || CombatState != EPlayerCombatState::Idle)
		return;

	ABossCharacter* Boss = Cast<ABossCharacter>(UGameplayStatics::GetActorOfClass(this, ABossCharacter::StaticClass()));

	// Execution Attack
	if (IsValid(Boss) && Boss->CanBeExecuted())
	{
		const FVector ToBoss = Boss->GetActorLocation() - GetActorLocation();

		if (ToBoss.SizeSquared2D() > FMath::Square(ExecutionRange))
		{
			return;
		}

		if (!ExecutionMontage || ExecutionMontage->GetPlayLength() <= ExecutionHitTime)
		{
			UE_LOG(LogTemp, Warning, TEXT("Execution montage must extend past the hit time."));
			return;
		}

		const FRotator Facing = ToBoss.GetSafeNormal2D().Rotation();
		SetActorRotation(FRotator(0.0f, Facing.Yaw, 0.0f));
		GetCharacterMovement()->StopMovementImmediately();

		const float MontageLength = PlayAnimMontage(ExecutionMontage.Get(), 1.0f);
		if (MontageLength <= 0.0f)
		{
			return;
		}

		bIsExecuting = true;
		CombatState = EPlayerCombatState::Attacking;
		ExecutionTarget = Boss;

		GetWorldTimerManager().SetTimer(AttackHitTimerHandle, this, &ASampleProject1Character::ApplyExecutionHit, ExecutionHitTime, false);
		GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ASampleProject1Character::FinishExecution, MontageLength, false);
		return;
	}

	//Parrying
	const bool bShouldParry = IsValid(Boss) && Boss->IsPreparingBasicAttack();

	if (IsValid(Boss))
	{
		constexpr float AttackFacingRange = 300.0f;

		FVector ToBoss = Boss->GetActorLocation() - GetActorLocation();
		ToBoss.Z = 0.0f;

		if (!ToBoss.IsNearlyZero() && (bShouldParry || ToBoss.SizeSquared() <= FMath::Square(AttackFacingRange)))
		{
			SetActorRotation(ToBoss.Rotation());
		}
	}

	if (bShouldParry)
	{
		StartParry(Boss->IsPreparingSecondBasicAttack());
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!AnimInstance || !AttackMontage || PlayAnimMontage(AttackMontage.Get()) <= 0.0f)
	{
		return;
	}

	CombatState = EPlayerCombatState::Attacking;

	bAllowMoveDuringAttack = false;
	GetCharacterMovement()->StopMovementImmediately();

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ASampleProject1Character::OnNormalAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage.Get());
}

void ASampleProject1Character::OnNormalAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == AttackMontage.Get() && !bIsDead && !bIsExecuting && CombatState == EPlayerCombatState::Attacking)
	{
		bAllowMoveDuringAttack = false;
		FinishAttack();
	}
}

void ASampleProject1Character::FinishAttack()
{
	CombatState = EPlayerCombatState::Idle;
}

void ASampleProject1Character::ApplyNormalAttackHit()
{
	if (!bIsDead && !bIsExecuting && !bIsDodging && CombatState == EPlayerCombatState::Attacking)
	{
		PerformAttackHitCheck();
	}
}

void ASampleProject1Character::SetComboGapMovement(bool bInComboGap)
{
	if (bIsDead || bIsExecuting || CombatState != EPlayerCombatState::Attacking)
	{
		return;
	}

	bAllowMoveDuringAttack = bInComboGap;
}

void ASampleProject1Character::PerformAttackHitCheck()
{
	const FVector Center = GetActorLocation() + GetActorForwardVector() * 140.0f;
	const float Radius = 165.0f;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;

	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	DrawDebugSphere(GetWorld(), Center, Radius, 24, FColor::Yellow, false, 0.7f);

	TSet<AActor*> HitActors;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();

		if (!IsValid(Target) || HitActors.Contains(Target))
			continue;

		HitActors.Add(Target);

		const bool bHitBoss = Target->IsA<ABossCharacter>();
		const FVector TargetLocation = Target->GetActorLocation();
		const float DamageApplied = UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, nullptr);

		if (bHitBoss && DamageApplied > 0.0f && AttackHitEffect)
		{
			const FVector TowardPlayer = (GetActorLocation() - TargetLocation).GetSafeNormal2D();
			const FVector ImpactLocation = TargetLocation + TowardPlayer * 35.0f + FVector(0.0f, 0.0f, 20.0f);

			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), AttackHitEffect.Get(), ImpactLocation, TowardPlayer.Rotation());
		}
	}
}

// Parrying
void ASampleProject1Character::StartParry(bool bUseSecondMontage)
{
	CombatState = EPlayerCombatState::Parrying;
	bParryWindowActive = true;

	UAnimMontage* Montage = bUseSecondMontage && SecondParryMontage
		? SecondParryMontage.Get()
		: ParryMontage.Get();

	if (Montage)
	{
		PlayAnimMontage(Montage);
	}

	GetWorldTimerManager().SetTimer(ParryWindowTimerHandle, this, &ASampleProject1Character::CloseParryWindow, ParryWindowDuration, false);
	GetWorldTimerManager().SetTimer(ParryEndTimerHandle, this, &ASampleProject1Character::FinishParry, ParryTotalDuration, false);
}

void ASampleProject1Character::CloseParryWindow()
{	
	bParryWindowActive = false;
}

void ASampleProject1Character::FinishParry()
{
	bParryWindowActive = false;

	if (bIsDead)
		return;

	CombatState = EPlayerCombatState::Idle;
}

// Dodging
void ASampleProject1Character::Dodge()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (bIsDead || !bCanDodge || bIsDodging || bIsExecuting || !Movement->IsMovingOnGround())
	{
		return;
	}

	//Dodge Direction
	const FVector LocalInput = GetActorQuat().UnrotateVector(DodgeMoveInput);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(LocalInput.Y, LocalInput.X));
	const int32 DirectionIndex = LocalInput.IsNearlyZero() ? 0 : (FMath::RoundToInt(Angle / 45.0f) + 8) % 8;

	UAnimMontage* Montage = DodgeMontage.Get();
	if (DirectionalDodgeMontages.IsValidIndex(DirectionIndex) && DirectionalDodgeMontages[DirectionIndex])
	{
		Montage = DirectionalDodgeMontages[DirectionIndex].Get();
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!Montage || !AnimInstance || PlayAnimMontage(Montage) <= 0.0f)
	{
		return;
	}

	ActiveDodgeMontage = Montage;
	Movement->StopMovementImmediately();
	ConsumeMovementInputVector();

	bIsDodging = true;
	bCanDodge = false;
	bAllowMoveDuringDodge = false;

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ASampleProject1Character::OnDodgeMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, ActiveDodgeMontage.Get());

	FTimerHandle DodgeCooldownTimerHandle;
	GetWorldTimerManager().SetTimer(DodgeCooldownTimerHandle, this, &ASampleProject1Character::ResetDodgeCooldown, 0.5f, false);
}

void ASampleProject1Character::UnlockDodgeMovement()
{
	if (bIsDead || !bIsDodging || bAllowMoveDuringDodge)
		return;

	bAllowMoveDuringDodge = true;
	StopAnimMontage(ActiveDodgeMontage.Get());
}

void ASampleProject1Character::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bIsDead && bIsDodging && Montage == ActiveDodgeMontage.Get())
	{
		FinishDodge();
	}
}

void ASampleProject1Character::FinishDodge()
{
	const bool bMovementWasUnlocked = bAllowMoveDuringDodge;

	bIsDodging = false;
	bAllowMoveDuringDodge = false;
	ActiveDodgeMontage = nullptr;

	if (!bMovementWasUnlocked)
	{
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void ASampleProject1Character::ResetDodgeCooldown()
{
	bCanDodge = true;
}

// Stagger skill
void ASampleProject1Character::UseStaggerSkill()
{
	if (bIsDead || bIsDodging || bIsExecuting || CombatState != EPlayerCombatState::Idle || !StaggerSkillMontage)
		return;

	ABossCharacter* Boss = Cast<ABossCharacter>(UGameplayStatics::GetActorOfClass(this, ABossCharacter::StaticClass()));
	if (!IsValid(Boss) || !Boss->CanUseStaggerSkill())
		return;

	const FVector ToBoss = Boss->GetActorLocation() - GetActorLocation();
	if (ToBoss.SizeSquared2D() > FMath::Square(ExecutionRange))
		return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || PlayAnimMontage(StaggerSkillMontage.Get()) <= 0.0f)
		return;

	const FVector FacingDirection = ToBoss.GetSafeNormal2D();
	if (!FacingDirection.IsNearlyZero())
		SetActorRotation(FacingDirection.Rotation());

	GetCharacterMovement()->StopMovementImmediately();
	ConsumeMovementInputVector();

	CombatState = EPlayerCombatState::UsingSkill;
	StaggerSkillTarget = Boss;

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ASampleProject1Character::OnStaggerSkillMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, StaggerSkillMontage.Get());
}

void ASampleProject1Character::ApplyStaggerSkillHit()
{
	ABossCharacter* Boss = StaggerSkillTarget.Get();
	if (bIsDead || bIsDodging || CombatState != EPlayerCombatState::UsingSkill || !IsValid(Boss))
	{
		return;
	}

	if (FVector::DistSquared2D(GetActorLocation(), Boss->GetActorLocation()) > FMath::Square(SkillRange))
	{
		return;
	}

	const FVector ImpactLocation = Boss->GetActorLocation();
	if (Boss->TryUseStaggerSkill(GetController(), this))
	{
		if (StaggerSkillHitEffect)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), StaggerSkillHitEffect.Get(), ImpactLocation, GetActorRotation());
		}
	}
}

void ASampleProject1Character::OnStaggerSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == StaggerSkillMontage.Get() && CombatState == EPlayerCombatState::UsingSkill)
	{
		StaggerSkillTarget.Reset();
		CombatState = EPlayerCombatState::Idle;
	}
}

// Execution
void ASampleProject1Character::ApplyExecutionHit()
{
	ABossCharacter* Boss = ExecutionTarget.Get();
	if (bIsDead || !bIsExecuting || !IsValid(Boss) || !Boss->CanBeExecuted())
	{
		return;
	}

	const FVector TargetLocation = Boss->GetActorLocation();
	const float DamageApplied = Boss->Execute();

	if (DamageApplied > 0.0f && AttackHitEffect)
	{
		const FVector TowardPlayer = (GetActorLocation() - TargetLocation).GetSafeNormal2D();
		const FVector ImpactLocation = TargetLocation + TowardPlayer * 35.0f + FVector(0.0f, 0.0f, 20.0f);

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), AttackHitEffect.Get(), ImpactLocation, TowardPlayer.Rotation());
	}
}

void ASampleProject1Character::FinishExecution()
{
	bIsExecuting = false;
	ExecutionTarget.Reset();
	FinishAttack();
}

// Damage and death
float ASampleProject1Character::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	//Already dead
	if (bIsDead)
		return 0.0f;

	ABossCharacter* Boss = Cast<ABossCharacter>(DamageCauser);

	//Dodge invincible
	if (bIsDodging)
	{
		return 0.0f;
	}

	//Parrying - No damage
	if (CombatState == EPlayerCombatState::Parrying && bParryWindowActive && IsValid(Boss) && Boss->IsCurrentAttackParryable())
	{
		bParryWindowActive = false;

		Boss->ChangeInitiative(100.0f);

		static const FName ParryImpactSocket(TEXT("FX_weapon_tip"));

		if (ParrySuccessEffect && GetMesh()->DoesSocketExist(ParryImpactSocket))
		{
			const FVector ImpactLocation = GetMesh()->GetSocketLocation(ParryImpactSocket);
			const FVector DirectionToBoss = (Boss->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ParrySuccessEffect.Get(), ImpactLocation, DirectionToBoss.Rotation());
		}
		return 0.0f;
	}

	
	if (DamageAmount <= 0.0f || CurrentHealth <= 0.0f)
		return 0.0f;

	bool bShouldKnockDown = false;

	//Initiative -100 -> knockdown, more damage taken
	if (IsValid(Boss))
	{
		Boss->ChangeInitiative(-Boss->GetCurrentAttackInitiativeLoss());
		bShouldKnockDown = Boss->IsInitiativeDepleted();
	}

	constexpr float KnockdownBonusDamage = 15.0f;
	const float TotalDamage = DamageAmount + (bShouldKnockDown ? KnockdownBonusDamage : 0.0f);
	const float ActualDamage = FMath::Min(TotalDamage, CurrentHealth);

	Super::TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
	CurrentHealth -= ActualDamage;

	if (PlayerHitEffect)
	{
		const FVector TowardAttacker = IsValid(DamageCauser) ? (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
		const FVector ImpactLocation = GetActorLocation() + TowardAttacker * 30.0f + FVector(0.0f, 0.0f, 20.0f);
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PlayerHitEffect.Get(), ImpactLocation, TowardAttacker.Rotation());
	}

	if (CurrentHealth <= 0.0f)
		Die();

	else if (bShouldKnockDown)
		StartKnockdown(Boss);

	else if (HitReactMontage && CombatState == EPlayerCombatState::Idle)
		PlayAnimMontage(HitReactMontage.Get());

	return ActualDamage;
}

void ASampleProject1Character::StartKnockdown(const AActor* Attacker)
{
	if (CombatState == EPlayerCombatState::KnockedDown || !KnockdownMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || PlayAnimMontage(KnockdownMontage.Get()) <= 0.0f)
	{
		return;
	}

	const FVector TowardAttacker = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!TowardAttacker.IsNearlyZero())
	{
		SetActorRotation(TowardAttacker.Rotation());
	}

	CombatState = EPlayerCombatState::KnockedDown;
	bAllowMoveDuringAttack = false;
	bParryWindowActive = false;

	GetWorldTimerManager().ClearTimer(ParryWindowTimerHandle);
	GetWorldTimerManager().ClearTimer(ParryEndTimerHandle);

	GetCharacterMovement()->StopMovementImmediately();
	ConsumeMovementInputVector();

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ASampleProject1Character::OnKnockdownMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, KnockdownMontage.Get());
}

void ASampleProject1Character::OnKnockdownMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bIsDead && Montage == KnockdownMontage.Get() && CombatState == EPlayerCombatState::KnockedDown)
	{
		CombatState = EPlayerCombatState::Idle;
	}
}

void ASampleProject1Character::Die()
{
	if (bIsDead)
		return;

	bIsDead = true;
	UE_LOG(LogTemp, Error, TEXT("Player Died!"));
	bIsDodging = false;

	GetWorldTimerManager().ClearTimer(AttackHitTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	ABossCharacter* Boss = Cast<ABossCharacter>(UGameplayStatics::GetActorOfClass(this, ABossCharacter::StaticClass()));
	if (IsValid(Boss))
	{
		Boss->StopCombat();
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	if (PlayerDeathMontage)
	{
		PlayAnimMontage(PlayerDeathMontage);
	}

	OnPlayerDefeated();
}
