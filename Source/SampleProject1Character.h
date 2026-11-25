// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "BossCharacter.h"
#include "SampleProject1Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

enum class EPlayerCombatState : uint8
{
	Idle,
	Attacking,
	Parrying,
	UsingSkill,
	KnockedDown
};

class UAnimMontage;
class UParticleSystem;

UCLASS(abstract)
class ASampleProject1Character : public ACharacter
{
	GENERATED_BODY()

	// Camera components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

protected:

	// Input bindings
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

	void Dodge();

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CombatAction;
	void Combat();

	// Battle result
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Result")
	void OnPlayerDefeated();

public:

	ASampleProject1Character();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyNormalAttackHit();

	UFUNCTION(BlueprintCallable, Category = "Combat|Dodge")
	void UnlockDodgeMovement();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyStaggerSkillHit();

protected:

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;

protected:

	void Move(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);

public:

	// Input helpers
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	// Dodge state
	bool bCanDodge = true;
	void ResetDodgeCooldown();
	bool bIsDodging = false;
	bool bAllowMoveDuringDodge = false;

	void FinishDodge();
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void ClearDodgeMoveInput();

private:
	// Combat state and montages
	EPlayerCombatState CombatState = EPlayerCombatState::Idle;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> ParryMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> SecondParryMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> DodgeMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation|Dodge", meta = (ToolTip = "Forward, ForwardRight, Right, BackRight, Back, BackLeft, Left, ForwardLeft"))
	TArray<TObjectPtr<UAnimMontage>> DirectionalDodgeMontages;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDodgeMontage = nullptr;

	FVector DodgeMoveInput = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> KnockdownMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> PlayerDeathMontage = nullptr;

	// Combat effects
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UParticleSystem> ParrySuccessEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UParticleSystem> AttackHitEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UParticleSystem> StaggerSkillHitEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UParticleSystem> PlayerHitEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> ExecutionMontage = nullptr;

	// Execution
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Execution")
	float ExecutionRange = 330.0f;
	float SkillRange = 500.0f;

	static constexpr float ExecutionHitTime = 1.9f;

	TWeakObjectPtr<ABossCharacter> ExecutionTarget;
	bool bIsExecuting = false;

	void ApplyExecutionHit();
	void FinishExecution();

	// Stagger skill
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> StaggerSkillAction = nullptr;

	void UseStaggerSkill();

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> StaggerSkillMontage = nullptr;

	TWeakObjectPtr<ABossCharacter> StaggerSkillTarget;

	void OnStaggerSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// Knockdown
	void StartKnockdown(const AActor* Attacker);
	void OnKnockdownMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// Attack and combo
	bool bAllowMoveDuringAttack = false;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetComboGapMovement(bool bInComboGap);

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackDamage = 7.0f;

	FTimerHandle AttackTimerHandle;
	FTimerHandle AttackHitTimerHandle;

	void FinishAttack();
	void PerformAttackHitCheck();
	void OnNormalAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// Health and death
	UPROPERTY(EditDefaultsOnly, Category = "Player|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Player|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Player|Health")
	bool bIsDead = false;

	void Die();

	// Parrying
	UPROPERTY(EditDefaultsOnly, Category = "Player|Parry")
	float ParryWindowDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Parry")
	float ParryTotalDuration = 0.45f;

	bool bParryWindowActive = false;

	FTimerHandle ParryWindowTimerHandle;
	FTimerHandle ParryEndTimerHandle;

	void StartParry(bool bUseSecondMontage);
	void CloseParryWindow();
	void FinishParry();

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// HUD
	UFUNCTION(BlueprintPure, Category = "Player|HUD")
	float GetHealthPercent() const
	{
		return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Player|HUD")
	float GetHealthPercent_PB() const
	{
		return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDodging() const
	{
		return bIsDodging;
	}
};

