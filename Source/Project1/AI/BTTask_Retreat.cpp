#include "BTTask_Retreat.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Project1/Combat/CombatComponent.h"

UBTTask_Retreat::UBTTask_Retreat()
{
    NodeName = "Retreat";
    bNotifyTick = true;
}

// --- Helpers ---

static AActor* FindNearestEnemy(APawn* Pawn, UCombatComponent* Combat, float& OutDistance)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(Pawn->GetWorld(), ACharacter::StaticClass(), AllActors);

    AActor* NearestEnemy = nullptr;
    float NearestDist = FLT_MAX;
    FVector MyLocation = Pawn->GetActorLocation();

    for (AActor* Actor : AllActors)
    {
        if (Actor == Pawn) continue;
        UCombatComponent* OtherCombat = Actor->FindComponentByClass<UCombatComponent>();
        if (!OtherCombat || OtherCombat->bIsDead || OtherCombat->TeamID == Combat->TeamID) continue;

        float Dist = FVector::Dist(MyLocation, Actor->GetActorLocation());
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestEnemy = Actor;
        }
    }

    OutDistance = NearestDist;
    return NearestEnemy;
}

// --- ExecuteTask ---

EBTNodeResult::Type UBTTask_Retreat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    APawn* Pawn = AIC->GetPawn();
    if (!Pawn) return EBTNodeResult::Failed;

    UCombatComponent* Combat = Pawn->FindComponentByClass<UCombatComponent>();
    if (!Combat || Combat->bIsDead) return EBTNodeResult::Failed;

    // Only ranged and flying units retreat — melee units stand their ground
    if (Combat->Stats.CharType == ECharType::Melee) return EBTNodeResult::Failed;

    float NearestDist = 0.f;
    AActor* NearestEnemy = FindNearestEnemy(Pawn, Combat, NearestDist);

    // Read safe distance from the unit's own stats (set per blueprint)
    float SafeDist = Combat->Stats.MinSafeDistance;

    // No enemy nearby or already at safe distance — skip retreat
    if (!NearestEnemy || NearestDist >= SafeDist) return EBTNodeResult::Failed;

    // Only retreat from Melee units — ranged vs ranged/flying should stand and shoot
    UCombatComponent* EnemyCombat = NearestEnemy->FindComponentByClass<UCombatComponent>();
    if (!EnemyCombat || EnemyCombat->Stats.CharType != ECharType::Melee) return EBTNodeResult::Failed;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow,
        FString::Printf(TEXT("%s RETREATING — enemy %.0f units away (min: %.0f)"),
            *Pawn->GetName(), NearestDist, SafeDist));

    // Target a point well behind safe distance relative to the enemy
    // so the unit always has somewhere to run toward
    FVector AwayDir = (Pawn->GetActorLocation() - NearestEnemy->GetActorLocation()).GetSafeNormal();
    FVector IdealRetreat = NearestEnemy->GetActorLocation() + AwayDir * SafeDist * 2.f;

    // Validate the retreat point is on the NavMesh — if not, find nearest valid point
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
    FNavLocation NavLocation;
    FVector RetreatTarget = IdealRetreat;
    if (NavSys && NavSys->ProjectPointToNavigation(IdealRetreat, NavLocation, FVector(200.f, 200.f, 200.f)))
    {
        RetreatTarget = NavLocation.Location;
    }

    AIC->MoveToLocation(RetreatTarget, 50.f);
    return EBTNodeResult::InProgress;
}

// --- TickTask ---

void UBTTask_Retreat::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

    APawn* Pawn = AIC->GetPawn();
    if (!Pawn) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

    UCombatComponent* Combat = Pawn->FindComponentByClass<UCombatComponent>();
    if (!Combat || Combat->bIsDead) { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }

    float NearestDist = 0.f;
    AActor* NearestEnemy = FindNearestEnemy(Pawn, Combat, NearestDist);

    float SafeDist = Combat->Stats.MinSafeDistance;

    // If nearest enemy is not Melee, no need to retreat — stop
    UCombatComponent* EnemyCombat = NearestEnemy ? NearestEnemy->FindComponentByClass<UCombatComponent>() : nullptr;
    if (!EnemyCombat || EnemyCombat->Stats.CharType != ECharType::Melee)
    {
        AIC->StopMovement();
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    // Safe — stop retreating
    if (!NearestEnemy || NearestDist >= SafeDist)
    {
        AIC->StopMovement();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green,
            FString::Printf(TEXT("%s RETREAT COMPLETE — enemy now %.0f units away"),
                *Pawn->GetName(), NearestDist));
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    // Still too close — keep moving away
    FVector AwayDir = (Pawn->GetActorLocation() - NearestEnemy->GetActorLocation()).GetSafeNormal();
    FVector IdealRetreat = NearestEnemy->GetActorLocation() + AwayDir * SafeDist * 2.f;

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
    FNavLocation NavLocation;
    FVector RetreatTarget = IdealRetreat;
    bool bFoundRetreat = NavSys && NavSys->ProjectPointToNavigation(IdealRetreat, NavLocation, FVector(200.f, 200.f, 200.f));

    if (bFoundRetreat)
    {
        RetreatTarget = NavLocation.Location;
    }

    // Cornered — straight back is blocked by map edge, try sliding laterally
    bool bCornered = !bFoundRetreat ||
        FVector::Dist(RetreatTarget, Pawn->GetActorLocation()) < 150.f;

    if (bCornered)
    {
        // Try 90 degrees left, then right
        FVector PerpDir = FVector::CrossProduct(AwayDir, FVector::UpVector).GetSafeNormal();
        FVector LeftTarget  = Pawn->GetActorLocation() + PerpDir  * SafeDist;
        FVector RightTarget = Pawn->GetActorLocation() - PerpDir  * SafeDist;

        FNavLocation LeftNav, RightNav;
        bool bLeft  = NavSys && NavSys->ProjectPointToNavigation(LeftTarget,  LeftNav,  FVector(200.f, 200.f, 200.f));
        bool bRight = NavSys && NavSys->ProjectPointToNavigation(RightTarget, RightNav, FVector(200.f, 200.f, 200.f));

        if (bLeft)       RetreatTarget = LeftNav.Location;
        else if (bRight) RetreatTarget = RightNav.Location;
        // If neither lateral direction works, just keep shooting in place
    }

    AIC->MoveToLocation(RetreatTarget, 50.f);

    // Kite — shoot at the enemy while retreating if cooldown is ready
    if (Combat->bCanAttack && Combat->CurrentTarget && Combat->IsInAttackRange(Combat->CurrentTarget))
    {
        Combat->Attack();
    }
}
