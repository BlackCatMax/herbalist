// Copyright Project Herbalist. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "ProjectHerbalistGameModeBase.generated.h"

class AHerbalistPlayerController;

UCLASS()
class PROJECTHERBALIST_API AProjectHerbalistGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AProjectHerbalistGameModeBase();
    virtual void BeginPlay() override;
};