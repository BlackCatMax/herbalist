#pragma once

#include "CoreMinimal.h"  // Для базовых типов Unreal
#include "GameFramework/GameModeBase.h"  // Для работы с GameMode
#include "ProjectHerbalistGameModeBase.generated.h"  // Это нужно для Unreal Engine (должен быть последним)

UCLASS()
class PROJECTHERBALIST_API AProjectHerbalistGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    // Конструктор
    AProjectHerbalistGameModeBase();

    // Переопределение BeginPlay
    virtual void BeginPlay() override;
};