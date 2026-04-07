#pragma once
#include "CoreMinimal.h"
#include "FCombatTypes.generated.h"

UENUM(BlueprintType)
enum class ECharType : uint8
{
	Melee    UMETA(DisplayName = "Melee"),
	Ranged   UMETA(DisplayName = "Ranged"),
	Flying   UMETA(DisplayName = "Flying")
};

UENUM(BlueprintType)
enum class EAttackPattern : uint8
{
	SingleTarget  UMETA(DisplayName = "Single Target"),
	Team          UMETA(DisplayName = "Team"),
	AoE           UMETA(DisplayName = "AoE")
};

USTRUCT(BlueprintType)
struct FUnitStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackCooldown = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MovementSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECharType CharType = ECharType::Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttackPattern AttackPattern = EAttackPattern::SingleTarget;

	/** Radius for AoE attacks, centered on the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AoERadius = 300.f;

	/** Number of cells for Team units (ignored for non-Team units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "AttackPattern == EAttackPattern::Team"))
	int32 CellCount = 5;

	/** Minimum distance to keep from enemies — ranged/flying units retreat if closer than this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinSafeDistance = 450.f;

	/** If true, stats are auto-configured from CharType + AttackPattern on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto")
	bool bUseAutoStats = true;
};
