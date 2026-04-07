#include "BTTask_MoveToTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Project1/Combat/CombatComponent.h"

UBTTask_MoveToTarget::UBTTask_MoveToTarget()
{
	NodeName = "Move To Target";
	bNotifyTick = true;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToTarget, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Failed;

	APawn* Pawn = AIC->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	UCombatComponent* Combat = Pawn->FindComponentByClass<UCombatComponent>();
	if (!Combat || Combat->bIsDead) return EBTNodeResult::Failed;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!Target) return EBTNodeResult::Failed;

	// If already in attack range, no need to move at all
	if (Combat->IsInAttackRange(Target))
	{
		AIC->StopMovement();
		return EBTNodeResult::Succeeded;
	}

	// Use the unit's attack range as the acceptable radius so it stops when in range
	float StopDistance = FMath::Max(Combat->Stats.AttackRange - 50.f, AcceptableRadius);
	AIC->MoveToActor(Target, StopDistance);

	return EBTNodeResult::InProgress;
}

void UBTTask_MoveToTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIC->GetPawn();
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UCombatComponent* Combat = Pawn->FindComponentByClass<UCombatComponent>();
	if (!Combat || Combat->bIsDead)
	{
		AIC->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

	if (!Target)
	{
		AIC->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Check if target died
	UCombatComponent* TargetCombat = Target->FindComponentByClass<UCombatComponent>();
	if (TargetCombat && TargetCombat->bIsDead)
	{
		AIC->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Succeed once we're in attack range
	if (Combat->IsInAttackRange(Target))
	{
		AIC->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Re-issue move command to follow a moving target
	float StopDistance = FMath::Max(Combat->Stats.AttackRange - 50.f, AcceptableRadius);
	AIC->MoveToActor(Target, StopDistance);
}
