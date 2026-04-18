// GridWorldManagerDebug.cpp
#include "GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Player/HerbalistPlayerController.h"

void AGridWorldManager::SelectCell(int32 X, int32 Y)
{
    if (GetCell(X, Y))
    {
        SelectedX = X;
        SelectedY = Y;
        UE_LOG(LogHerbalist, Log, TEXT("Selected cell (%d, %d)"), X, Y);
    }
    else
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Invalid cell (%d, %d)"), X, Y);
    }
}

FString AGridWorldManager::GetSelectedCellInfo() const
{
    const FGridCell* Cell = GetCellConst(SelectedX, SelectedY);
    if (!Cell) return TEXT("No cell selected");
    FString ResourceStr = Cell->bIsWater ? TEXT("Water") : FString::Printf(TEXT("%d"), (int32)Cell->AvailableResource);
    return FString::Printf(TEXT("Cell (%d,%d): Mag=%.2f, Dist=%.2f, Stress=%.3f, Resource=%s, Regrowth=%.1f"),
        SelectedX, SelectedY,
        Cell->State.Magnitude,
        Cell->State.Meta.Distortion,
        Cell->HarvestStress,
        *ResourceStr,
        Cell->ResourceRegrowthTimer);
}

void AGridWorldManager::GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress)
{
    X = SelectedX;
    Y = SelectedY;
    if (X >= 0 && Y >= 0)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
        {
            if (Cell->bIsWater)
                ResourceName = TEXT("Вода");
            else
                ResourceName = FHerbalistHarvest::GetResourceName(Cell->AvailableResource, false);
            RegrowthTimer = Cell->ResourceRegrowthTimer;
            Distortion = Cell->State.Meta.Distortion;
            HarvestStress = Cell->HarvestStress;
            return;
        }
    }
    ResourceName = TEXT("None");
    RegrowthTimer = 0.0f;
    Distortion = 0.0f;
    HarvestStress = 0.0f;
}

void AGridWorldManager::ShowInventory()
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    UE_LOG(LogHerbalist, Log, TEXT("=== INVENTORY (%d items) ==="), Inventory.Num());
    for (int32 i = 0; i < Inventory.Num(); ++i)
    {
        const FInventoryItem& Item = Inventory[i];
        const FRealState& Res = Item.State;
        FString Name = FHerbalistHarvest::GetResourceName(Item.Type, false);
        UE_LOG(LogHerbalist, Log, TEXT("[%d] %s x%d: Mag=%.2f, Dist=%.2f, Pot=%.2f Res=%.2f Cor=%.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
            i, *Name, Item.Count, Res.Magnitude, Res.Meta.Distortion, Res.Meta.Potency, Res.Meta.Resonance, Res.Meta.Corruption,
            Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
    }
}