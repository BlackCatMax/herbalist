#include "HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalistGameModeBase.h"

void AHerbalistPlayerController::HarvestTest(int32 X, int32 Y, int32 ResourceType)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        break;
    }
    if (WorldManager)
    {
        WorldManager->HarvestTest(X, Y, ResourceType);
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
    }
}

void AHerbalistPlayerController::ApplyTest(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        WorldManager = *It;
        break;
    }
    if (WorldManager)
    {
        WorldManager->ApplyTest(X, Y);
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No GridWorldManager found"));
    }
}

void AHerbalistPlayerController::ShowInventory()
{
    AProjectHerbalistGameModeBase* GM = Cast<AProjectHerbalistGameModeBase>(GetWorld()->GetAuthGameMode());
    if (!GM) return;
    TArray<FRealState*> Inventory = GM->GetInventory();
    UE_LOG(LogHerbalist, Log, TEXT("=== INVENTORY (%d items) ==="), Inventory.Num());
    for (int32 i = 0; i < Inventory.Num(); ++i)
    {
        FRealState* ResPtr = Inventory[i];
        if (ResPtr)
        {
            const FRealState& Res = *ResPtr;   // разыменовываем
            UE_LOG(LogHerbalist, Log, TEXT("[%d] Mag: %.2f, Dist: %.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
                i, Res.Magnitude, Res.Meta.Distortion,
                Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
        }
        else
        {
            UE_LOG(LogHerbalist, Warning, TEXT("[%d] NULL pointer"), i);
        }
    }
}