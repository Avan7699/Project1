// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameModeBase.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FEnemySpawnInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<ACharacter> EnemyClass;

	UPROPERTY(EditAnywhere)
	FVector SpawnLocation;
};

USTRUCT(BlueprintType)
struct FLevelData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FEnemySpawnInfo> Enemies;
};

UCLASS()
class PROJECT1_API AMyGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnUnitDead();
	
	void IsBattleOver();
	
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> ResultWidgetClass;
	
	UPROPERTY(EditAnywhere, Category = "Levels")
	TArray<FLevelData> Levels;
	
private:
	bool bBattleOver = false;
	
};
