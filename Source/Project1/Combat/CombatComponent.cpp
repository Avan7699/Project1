#include "CombatComponent.h"
#include "BaseProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::ApplyAutoStats()
{
    // Base stats per CharType
    switch (Stats.CharType)
    {
    case ECharType::Melee:
        Stats.MaxHealth = 100.f;
        Stats.AttackDamage = 25.f;
        Stats.AttackRange = 300.f;
        Stats.AttackCooldown = 2.0f;
        Stats.MovementSpeed = 500.f;
        break;
    case ECharType::Ranged:
        Stats.MaxHealth = 80.f;
        Stats.AttackDamage = 25.f;
        Stats.AttackRange = 1500.f;
        Stats.AttackCooldown = 2.0f;
        Stats.MovementSpeed = 400.f;
        break;
    case ECharType::Flying:
        Stats.MaxHealth = 85.f;
        Stats.AttackDamage = 25.f;
        Stats.AttackRange = 800.f;
        Stats.AttackCooldown = 1.5f;
        Stats.MovementSpeed = 600.f;
        break;
    }

    // Modify based on AttackPattern
    switch (Stats.AttackPattern)
    {
    case EAttackPattern::SingleTarget:
        // Base stats are already tuned for single target
        break;
    case EAttackPattern::Team:
        Stats.MaxHealth *= 1.25f;       // More total HP (split across cells)
        Stats.AttackDamage *= 0.8f;     // Slightly less total damage
        Stats.AttackCooldown *= 0.75f;  // Faster attacks
        Stats.MovementSpeed *= 0.9f;    // Slightly slower
        Stats.CellCount = 5;
        break;
    case EAttackPattern::AoE:
        Stats.MaxHealth *= 1.0f;        // Same HP
        Stats.AttackDamage *= 0.75f;    // Lower per-hit damage (but hits everything)
        Stats.AttackCooldown *= 1.1f;   // Slightly slower attacks
        Stats.AoERadius = 300.f;
        break;
    }
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    if (Stats.bUseAutoStats)
    {
        ApplyAutoStats();
    }

    UE_LOG(LogTemp, Warning, TEXT("%s | Type: %d | Pattern: %d | HP: %.0f | Dmg: %.0f | Range: %.0f | CD: %.1f | AutoStats: %d"),
        *GetOwner()->GetName(),
        (int32)Stats.CharType, (int32)Stats.AttackPattern,
        Stats.MaxHealth, Stats.AttackDamage, Stats.AttackRange, Stats.AttackCooldown, Stats.bUseAutoStats);

    CurrentHealth = Stats.MaxHealth;

    // Apply movement speed to owner if it's a Character
    if (ACharacter* CharOwner = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = CharOwner->GetCharacterMovement())
        {
            Movement->MaxWalkSpeed = Stats.MovementSpeed;
        }
    }

    // Initialize cells for Team units
    if (Stats.AttackPattern == EAttackPattern::Team && Stats.CellCount > 1)
    {
        CellMaxHealth = Stats.MaxHealth / Stats.CellCount;
        CellDamage = Stats.AttackDamage / Stats.CellCount;
        CellsAlive = Stats.CellCount;

        CellHealths.SetNum(Stats.CellCount);
        for (int32 i = 0; i < Stats.CellCount; i++)
        {
            CellHealths[i] = CellMaxHealth;
        }

        UE_LOG(LogTemp, Warning, TEXT("%s initialized as Team unit: %d cells, %.0f HP each, %.0f damage each"),
            *GetOwner()->GetName(), Stats.CellCount, CellMaxHealth, CellDamage);
    }
}

float UCombatComponent::GetHealthPercent() const
{
    return (Stats.MaxHealth > 0.f) ? CurrentHealth / Stats.MaxHealth : 0.f;
}

// --- Effectiveness ---

float UCombatComponent::GetTypeEffectiveness(ECharType Attacker, ECharType Defender)
{
    // Melee beats Ranged, Ranged beats Flying, Flying beats Melee
    if ((Attacker == ECharType::Melee  && Defender == ECharType::Ranged) ||
        (Attacker == ECharType::Ranged && Defender == ECharType::Flying) ||
        (Attacker == ECharType::Flying && Defender == ECharType::Melee))
    {
        return 1.5f;
    }

    if ((Attacker == ECharType::Melee  && Defender == ECharType::Flying) ||
        (Attacker == ECharType::Ranged && Defender == ECharType::Melee) ||
        (Attacker == ECharType::Flying && Defender == ECharType::Ranged))
    {
        return 0.75f;
    }

    return 1.0f;
}

float UCombatComponent::GetPatternEffectiveness(EAttackPattern Attacker, EAttackPattern Defender)
{
    // Team beats SingleTarget, AoE beats Team, SingleTarget beats AoE
    if ((Attacker == EAttackPattern::Team         && Defender == EAttackPattern::SingleTarget) ||
        (Attacker == EAttackPattern::AoE          && Defender == EAttackPattern::Team) ||
        (Attacker == EAttackPattern::SingleTarget  && Defender == EAttackPattern::AoE))
    {
        return 1.5f;
    }

    if ((Attacker == EAttackPattern::Team         && Defender == EAttackPattern::AoE) ||
        (Attacker == EAttackPattern::AoE          && Defender == EAttackPattern::SingleTarget) ||
        (Attacker == EAttackPattern::SingleTarget  && Defender == EAttackPattern::Team))
    {
        return 0.75f;
    }

    return 1.0f;
}

float UCombatComponent::GetCombinedEffectiveness(ECharType AtkType, EAttackPattern AtkPattern, ECharType DefType, EAttackPattern DefPattern)
{
    return GetTypeEffectiveness(AtkType, DefType) * GetPatternEffectiveness(AtkPattern, DefPattern);
}

// --- Damage ---

void UCombatComponent::ApplyDamage(float DamageAmount, AActor* DamageInstigator)
{
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
            FString::Printf(TEXT("ApplyDamage called: %.1f damage"), DamageAmount));

    if (bIsDead) return;

    // Apply combined effectiveness
    if (DamageInstigator)
    {
        if (UCombatComponent* InstigatorCombat = DamageInstigator->FindComponentByClass<UCombatComponent>())
        {
            float Multiplier = GetCombinedEffectiveness(
                InstigatorCombat->Stats.CharType, InstigatorCombat->Stats.AttackPattern,
                Stats.CharType, Stats.AttackPattern);
            UE_LOG(LogTemp, Warning, TEXT("Damage: %s → %s | Base: %.0f | Multiplier: %.2f | Final: %.0f"),
                *DamageInstigator->GetName(), *GetOwner()->GetName(), DamageAmount, Multiplier, DamageAmount * Multiplier);
            DamageAmount *= Multiplier;
        }
    }

    // Route damage based on attacker's pattern and whether we are a Team unit
    if (IsTeamUnit() && CellsAlive > 0)
    {
        EAttackPattern AttackerPattern = EAttackPattern::SingleTarget;
        int32 AttackerCellsAlive = 1;

        if (DamageInstigator)
        {
            if (UCombatComponent* InstigatorCombat = DamageInstigator->FindComponentByClass<UCombatComponent>())
            {
                AttackerPattern = InstigatorCombat->Stats.AttackPattern;
                AttackerCellsAlive = InstigatorCombat->IsTeamUnit() ? InstigatorCombat->CellsAlive : 1;
            }
        }

        switch (AttackerPattern)
        {
        case EAttackPattern::SingleTarget:
            {
                // Single target only hits one cell — overkill is wasted
                int32 CellIdx = GetFirstLivingCellIndex();
                if (CellIdx >= 0)
                {
                    ApplyDamageToCell(CellIdx, DamageAmount);
                }
            }
            break;

        case EAttackPattern::AoE:
            {
                // AoE hits ALL living cells — full damage to first, reduced to rest
                bool bFirstCell = true;
                for (int32 i = 0; i < CellHealths.Num(); i++)
                {
                    if (CellHealths[i] > 0.f)
                    {
                        float CellDmg = bFirstCell ? DamageAmount : DamageAmount * 0.5f;
                        ApplyDamageToCell(i, CellDmg);
                        bFirstCell = false;
                    }
                }
            }
            break;

        case EAttackPattern::Team:
            {
                // Team attacker distributes cells across defender cells
                TArray<int32> LivingCells;
                for (int32 i = 0; i < CellHealths.Num(); i++)
                {
                    if (CellHealths[i] > 0.f)
                    {
                        LivingCells.Add(i);
                    }
                }

                if (LivingCells.Num() > 0)
                {
                    // Each attacker cell targets a different defender cell (round-robin)
                    for (int32 a = 0; a < AttackerCellsAlive; a++)
                    {
                        int32 DefenderIdx = LivingCells[a % LivingCells.Num()];
                        ApplyDamageToCell(DefenderIdx, DamageAmount);
                    }
                }
            }
            break;
        }

        // Recalculate total HP from cells
        float TotalHealth = 0.f;
        CellsAlive = 0;
        for (int32 i = 0; i < CellHealths.Num(); i++)
        {
            if (CellHealths[i] > 0.f)
            {
                TotalHealth += CellHealths[i];
                CellsAlive++;
            }
        }
        CurrentHealth = TotalHealth;
        OnHealthChanged.Broadcast(GetHealthPercent());

        // Check if all cells are dead
        if (CellsAlive <= 0)
        {
            bIsDead = true;
            CurrentHealth = 0.f;

            APawn* PawnOwner = Cast<APawn>(GetOwner());
            if (PawnOwner)
            {
                if (AAIController* AIC = Cast<AAIController>(PawnOwner->GetController()))
                {
                    if (AIC->GetBrainComponent())
                    {
                        AIC->GetBrainComponent()->StopLogic("Dead");
                    }
                }
            }

            OnUnitDeath.Broadcast();
        }
    }
    else
    {
        // Non-Team unit: normal single HP pool
        CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, Stats.MaxHealth);
        OnHealthChanged.Broadcast(GetHealthPercent());

        if (CurrentHealth <= 0.f)
        {
            bIsDead = true;

            APawn* PawnOwner = Cast<APawn>(GetOwner());
            if (PawnOwner)
            {
                if (AAIController* AIC = Cast<AAIController>(PawnOwner->GetController()))
                {
                    if (AIC->GetBrainComponent())
                    {
                        AIC->GetBrainComponent()->StopLogic("Dead");
                    }
                }
            }

            OnUnitDeath.Broadcast();
        }
    }
}

bool UCombatComponent::ApplyDamageToCell(int32 CellIndex, float DamageAmount)
{
    if (!CellHealths.IsValidIndex(CellIndex) || CellHealths[CellIndex] <= 0.f)
        return false;

    CellHealths[CellIndex] = FMath::Max(CellHealths[CellIndex] - DamageAmount, 0.f);

    float CellPercent = (CellMaxHealth > 0.f) ? CellHealths[CellIndex] / CellMaxHealth : 0.f;

    // Count living cells for the event
    int32 LivingCount = 0;
    for (float HP : CellHealths)
    {
        if (HP > 0.f) LivingCount++;
    }

    OnCellHealthChanged.Broadcast(CellIndex, CellPercent, LivingCount);

    if (CellHealths[CellIndex] <= 0.f)
    {
        OnCellDeath.Broadcast(CellIndex, LivingCount);
        return true;
    }

    return false;
}

int32 UCombatComponent::GetFirstLivingCellIndex() const
{
    for (int32 i = 0; i < CellHealths.Num(); i++)
    {
        if (CellHealths[i] > 0.f) return i;
    }
    return -1;
}

// --- Range ---

bool UCombatComponent::IsInAttackRange(AActor* Target) const
{
    if (!Target || !GetOwner()) return false;
    float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
    return Distance <= Stats.AttackRange;
}

// --- Attack ---

bool UCombatComponent::Attack()
{
    if (!bCanAttack || bIsDead || !CurrentTarget) return false;
    if (!IsInAttackRange(CurrentTarget)) return false;

    AActor* Owner = GetOwner();
    if (!Owner) return false;

    // For Team units, damage output scales with living cells
    float EffectiveDamage = Stats.AttackDamage;
    if (IsTeamUnit() && Stats.CellCount > 0)
    {
        EffectiveDamage = CellDamage * CellsAlive;
    }

    // Ranged and Flying units deal damage via projectile — skip instant damage
    bool bUsesProjectile = (Stats.CharType != ECharType::Melee && ProjectileClass);

    if (!bUsesProjectile)
    {
        // Melee: apply damage instantly
        switch (Stats.AttackPattern)
        {
        case EAttackPattern::SingleTarget:
            DealDamageToTarget(CurrentTarget);
            break;

        case EAttackPattern::Team:
            DealDamageToTarget(CurrentTarget);
            break;

        case EAttackPattern::AoE:
            {
                TArray<AActor*> Enemies = GetEnemiesInRadius(CurrentTarget->GetActorLocation(), Stats.AoERadius);
                for (AActor* Enemy : Enemies)
                {
                    DealDamageToTarget(Enemy);
                }
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange,
                        FString::Printf(TEXT("%s [AoE] hit %d enemies for %.0f each"),
                            *Owner->GetName(), Enemies.Num(), EffectiveDamage));
                }
            }
            break;
        }
        StartCooldown();
    }
    else
    {
        // Ranged/Flying: fire projectile — damage applied on projectile overlap
        FireProjectile();
    }

    return true;
}

void UCombatComponent::MeleeAttack()
{
    Attack();
}

void UCombatComponent::DealDamageToTarget(AActor* Target)
{
    if (!Target) return;

    UCombatComponent* TargetCombat = Target->FindComponentByClass<UCombatComponent>();
    if (TargetCombat && !TargetCombat->bIsDead)
    {
        // For Team attackers, total damage = CellDamage * CellsAlive
        float DamageOutput = Stats.AttackDamage;
        if (IsTeamUnit() && Stats.CellCount > 0)
        {
            DamageOutput = CellDamage * CellsAlive;
        }

        TargetCombat->ApplyDamage(DamageOutput, GetOwner());
    }
}

// --- Projectile ---

void UCombatComponent::FireProjectile()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("[FireProjectile] Called"));

    if (!bCanAttack || bIsDead || !CurrentTarget || !ProjectileClass)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
            FString::Printf(TEXT("[FireProjectile] EARLY EXIT — CanAttack:%d Dead:%d HasTarget:%d HasClass:%d"),
                bCanAttack, bIsDead, CurrentTarget != nullptr, ProjectileClass != nullptr));
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner) return;

    FVector SpawnLocation = Owner->GetActorLocation() + FVector(0.f, 0.f, 50.f);
    FVector TargetLocation = CurrentTarget->GetActorLocation() + FVector(0.f, 0.f, 50.f);
    FRotator SpawnRotation = (TargetLocation - SpawnLocation).Rotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
        ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);

    if (SpawnedActor)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("[FireProjectile] Projectile SPAWNED OK"));
        ABaseProjectile* Projectile = Cast<ABaseProjectile>(SpawnedActor);
        if (Projectile)
        {
            float DamageOutput = Stats.AttackDamage;
            if (IsTeamUnit() && Stats.CellCount > 0)
            {
                DamageOutput = CellDamage * CellsAlive;
            }
            Projectile->SetOwnerUnit(Owner, DamageOutput);
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[FireProjectile] Cast to ABaseProjectile FAILED — wrong parent class?"));
        }
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[FireProjectile] SpawnActor FAILED — projectile not spawned"));
    }

    StartCooldown();
}

// --- Cooldown ---

void UCombatComponent::StartCooldown()
{
    bCanAttack = false;
    GetWorld()->GetTimerManager().SetTimer(
        CooldownTimerHandle, this,
        &UCombatComponent::ResetAttackCooldown,
        Stats.AttackCooldown, false);
}

void UCombatComponent::ResetAttackCooldown()
{
    bCanAttack = true;
}

// --- Targeting ---

TArray<AActor*> UCombatComponent::GetAllEnemies() const
{
    TArray<AActor*> AllCharacters;
    TArray<AActor*> Enemies;

    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), AllCharacters);

    for (AActor* Unit : AllCharacters)
    {
        if (Unit == GetOwner()) continue;

        UCombatComponent* OtherCombat = Unit->FindComponentByClass<UCombatComponent>();
        if (OtherCombat && OtherCombat->TeamID != TeamID && !OtherCombat->bIsDead)
        {
            Enemies.Add(Unit);
        }
    }

    return Enemies;
}

TArray<AActor*> UCombatComponent::GetEnemiesInRadius(FVector Center, float Radius) const
{
    TArray<AActor*> AllEnemies = GetAllEnemies();
    TArray<AActor*> InRadius;

    for (AActor* Enemy : AllEnemies)
    {
        if (FVector::Dist(Enemy->GetActorLocation(), Center) <= Radius)
        {
            InRadius.Add(Enemy);
        }
    }

    return InRadius;
}

AActor* UCombatComponent::FindBestTarget() const
{
    TArray<AActor*> AllCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), AllCharacters);

    AActor* BestTarget = nullptr;
    float BestScore = -FLT_MAX;
    FVector MyLocation = GetOwner()->GetActorLocation();

    for (AActor* Unit : AllCharacters)
    {
        if (Unit == GetOwner()) continue;

        UCombatComponent* OtherCombat = Unit->FindComponentByClass<UCombatComponent>();
        if (!OtherCombat || OtherCombat->TeamID == TeamID || OtherCombat->bIsDead)
            continue;

        float Score = 0.f;

        // Combined type + pattern effectiveness bonus
        float Effectiveness = GetCombinedEffectiveness(
            Stats.CharType, Stats.AttackPattern,
            OtherCombat->Stats.CharType, OtherCombat->Stats.AttackPattern);
        Score += (Effectiveness - 0.5625f) * 25.f;

        // Distance penalty (closer = better)
        float Distance = FVector::Dist(MyLocation, Unit->GetActorLocation());
        float MaxRange = 5000.f;
        Score += (1.f - FMath::Clamp(Distance / MaxRange, 0.f, 1.f)) * 20.f;

        // Low health bonus (finishing off weak targets)
        float HealthPct = OtherCombat->GetHealthPercent();
        Score += (1.f - HealthPct) * 15.f;

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Unit;
        }
    }

    return BestTarget;
}
