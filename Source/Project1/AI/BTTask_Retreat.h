#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Retreat.generated.h"

UCLASS()
class PROJECT1_API UBTTask_Retreat : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_Retreat();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    /** Retreat triggers when an enemy gets within this distance */
    UPROPERTY(EditAnywhere, Category = "Retreat")
    float MinSafeDistance = 300.f;
};
