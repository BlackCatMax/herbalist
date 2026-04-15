#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HerbalistPlayerController.generated.h"

UCLASS()
class PROJECTHERBALIST_API AHerbalistPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(Exec)
    void HarvestTest(int32 X, int32 Y, int32 ResourceType);

    UFUNCTION(Exec)
    void ApplyTest(int32 X, int32 Y);

    UFUNCTION(Exec)
    void ShowInventory();
};