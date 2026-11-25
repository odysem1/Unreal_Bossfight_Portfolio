
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BossCharacter.generated.h"

enum class EBossAttackState : uint8
{
	Idle,
	Windup,
	Active,
	Recovery,
    Staggered,
    ExecutionReady,
    Dead,
    Inactive
};

enum class EBossAttackType : uint8
{
    Basic,
    Heavy,
    Cone
};

class UAnimMontage;
class UMaterialInterface;
class UDecalComponent;
class UParticleSystem;

UCLASS()
class SAMPLEPROJECT1_API ABossCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABossCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    // Attack state queries
    bool IsWindingUp() const
    {
        return AttackState == EBossAttackState::Windup;
    }

    // Initiative
    void ChangeInitiative(float Delta);

    float GetInitiative() const
    {
        return Initiative;
    }

    bool IsInitiativeDepleted() const
    {
        return GetInitiativePercent_PB() <= 0.0f;
    }

    float GetCurrentAttackInitiativeLoss() const
    {
        switch (CurrentAttackType)
        {
        case EBossAttackType::Heavy:
            return 70.0f;
        case EBossAttackType::Cone:
            return 100.0f;
        default:
            return 20.0f;
        }
    }

    // HUD
    UFUNCTION(BlueprintPure, Category = "Boss|HUD")
    float GetHealthPercent_PB() const
    {
        return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
    }

    UFUNCTION(BlueprintPure, Category = "Boss|HUD")
    float GetInitiativePercent_PB() const
    {
        return (Initiative + 100.0f) / 200.0f;
    }

    // Parry queries
    bool IsPreparingBasicAttack() const
    {
        return AttackState == EBossAttackState::Windup && CurrentAttackType == EBossAttackType::Basic;
    }

    bool IsPreparingSecondBasicAttack() const
    {
        return IsPreparingBasicAttack() && bIsSecondBasicAttack;
    }

    bool IsCurrentAttackParryable() const
    {
        return AttackState == EBossAttackState::Active && CurrentAttackType == EBossAttackType::Basic;
    }

    void StopCombat();

    // Stagger skill and execution
    bool CanBeExecuted() const
    {
        return AttackState == EBossAttackState::ExecutionReady && CurrentHealth > 0.0f;
    }

    bool TryUseStaggerSkill(AController* InstigatorController, AActor* DamageCauser);

    bool CanUseStaggerSkill() const
    {
        return bSkillReady && AttackState == EBossAttackState::Staggered && CurrentHealth > 0.0f;
    }

    float Execute();

protected:
    // Health and battle result
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Health")
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Health")
    float CurrentHealth = 100.0f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Result")
    void OnBossDefeated();

private:
    // Attack phases
    EBossAttackState AttackState = EBossAttackState::Idle;

    bool bHeavyAttackPending = false;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float AttackRange = 350.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float WindupDuration = 0.7f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float ActiveDuration = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float RecoveryDuration = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "0.01"))
    float BasicComboTransitionDelay = 0.24f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "0.01"))
    float SecondBasicWindupDuration = 0.70f;

    bool bPendingSecondBasicHit = false;
    bool bIsSecondBasicAttack = false;

    FTimerHandle AttackPhaseTimerHandle;

    void StartAttack(EBossAttackType AttackType = EBossAttackType::Basic);
    void EnterActive();
    void EnterRecovery();
    void FinishAttack();
    void PerformAttackHitCheck();

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float AttackDamage = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "0.01"))
    float ConeWindupDuration = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "1.0"))
    float ConeAttackRange = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "1.0", ClampMax = "90.0"))
    float ConeAttackHalfAngle = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "0.0"))
    float ConeAttackDamage = 40.0f;

    void DrawConeAttackDebug(const FColor& Color, float LifeTime) const;

    // Initiative and HUD
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Initiative")
    float InitialInitiative = -50.0f;

    UPROPERTY(VisibleInstanceOnly, Category = "Boss|Initiative")
    float Initiative = -50.0f;

    UFUNCTION(BlueprintPure, Category = "Boss|HUD")
    float GetHealthPercent() const
    {
        return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
    }

    UFUNCTION(BlueprintPure, Category = "Boss|HUD")
    float GetInitiativePercent() const
    {
        return (Initiative + 100.0f) / 200.0f;
    }

    // Stagger, skill and execution
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Stagger")
    float StaggerDuration = 5.0f;

    FTimerHandle StaggerTimerHandle;

    void StartStagger();
    void FinishStagger();

    static constexpr int32 StaggersRequiredForSkill = 3;
    static constexpr float SkillDamageRatio = 0.20f;
    static constexpr float ExecutionHealthRatio = 0.25f;

    int32 StaggersTowardSkill = 0;
    bool bSkillReady = false;

    void EnterExecutionReady();

    // Attack pattern
    EBossAttackType CurrentAttackType = EBossAttackType::Basic;

    int32 AttackCount = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float HeavyWindupDuration = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float HeavyAttackDamage = 40.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    float HeavyAttackRadius = 400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack", meta = (ClampMin = "0.0"))
    float ChaseStartDelay = 10.0f;

    float ChaseStartTime = 0.0f;

    // Montages
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> SecondBasicAttackMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> StrongAttackMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> StaggerMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> SecondStaggerMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation", meta = (ClampMin = "0.01"))
    float SecondStaggerDelay = 0.8f;

    FTimerHandle SecondStaggerTimerHandle;

    void PlaySecondStaggerMontage();

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> SkillReadyStaggerMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> ExecutionReadyMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
    TObjectPtr<UAnimMontage> ConeAttackMontage = nullptr;

    void BeginDeath();

    // Combat effects
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
    TObjectPtr<UMaterialInterface> HeavyAttackDecalMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Attack")
    TObjectPtr<UMaterialInterface> ConeAttackDecalMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UDecalComponent> HeavyAttackDecal = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
    TObjectPtr<UParticleSystem> GuardImpactEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
    TObjectPtr<UParticleSystem> HeavyAttackImpactEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
    TObjectPtr<UParticleSystem> ConeAttackEffect = nullptr;

    void ClearHeavyAttackDecal();
};
