#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToTarget.generated.h"

struct FMoveToTargetMemory
{
	FVector LastIssuedTargetLocation = FVector::ZeroVector;
};

UCLASS()
class PROJECT1_API UBTTask_MoveToTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MoveToTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FMoveToTargetMemory); }

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** How close the unit needs to get before the task succeeds */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float AcceptableRadius = 100.f;

	/** Only re-issue MoveToActor if the target has moved at least this far since the last command */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float ReMoveThreshold = 75.f;
};
