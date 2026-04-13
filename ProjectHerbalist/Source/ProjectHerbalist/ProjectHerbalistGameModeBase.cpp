#include "ProjectHerbalistGameModeBase.h"

// Конструктор
AProjectHerbalistGameModeBase::AProjectHerbalistGameModeBase()
{
}

// BeginPlay
void AProjectHerbalistGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("Game started"));
}