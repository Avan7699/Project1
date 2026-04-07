#include "MyGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/Character.h"

void AMyGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), AllUnits);

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("Found %d units"), AllUnits.Num()));

    for (AActor* Unit : AllUnits)
    {
        UCombatComponent* Combat = Unit->FindComponentByClass<UCombatComponent>();
        if (Combat)
        {
            Combat->OnUnitDeath.AddDynamic(this, &AMyGameModeBase::OnUnitDead);
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                    FString::Printf(TEXT("Bound death event for: %s"), *Unit->GetName()));
        }
    }
}

void AMyGameModeBase::OnUnitDead()
{
    
    if (!bBattleOver)
    {
        IsBattleOver();
    }
}

void AMyGameModeBase::IsBattleOver()
{
    TArray<AActor*> AllUnits;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), AllUnits);

    bool bTeam0Alive = false;
    bool bTeam1Alive = false;

    for (AActor* Unit : AllUnits)
    {
        UCombatComponent* Combat = Unit->FindComponentByClass<UCombatComponent>();
        if (Combat && !Combat->bIsDead)
        {
            if (Combat->TeamID == 0) bTeam0Alive = true;
            if (Combat->TeamID == 1) bTeam1Alive = true;
        }
    }
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange,
            FString::Printf(TEXT("Team0 alive: %d | Team1 alive: %d | WidgetClass: %d"),
                bTeam0Alive, bTeam1Alive, ResultWidgetClass != nullptr));


    if (!bTeam0Alive || !bTeam1Alive)
    {
        bBattleOver = true;

        if (ResultWidgetClass)
        {
            UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), ResultWidgetClass);
            if (Widget)
            {
                // Set the result text
                UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("ResultText")));
                if (Text)
                {
                    if (!bTeam0Alive)
                        Text->SetText(FText::FromString("GAME OVER"));
                    else
                        Text->SetText(FText::FromString("LEVEL CLEARED"));
                }

                Widget->AddToViewport();
            }
        }

        // Pause the game
        UGameplayStatics::SetGamePaused(GetWorld(), true);
    }
}