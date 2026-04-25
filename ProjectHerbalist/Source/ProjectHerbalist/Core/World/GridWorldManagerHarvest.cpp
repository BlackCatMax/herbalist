// GridWorldManagerHarvest.cpp
#include "Core/World/GridWorldManager.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return FRealState();

    if (Cell->bIsWater)
    {
        return HarvestService->HarvestWater(Cell->State, Conditions);
    }

    if (Cell->ResourceRegrowthTimer > 0.0f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) resource not ready (regrowing)"), X, Y);
        return FRealState();
    }

    FName IngredientID = Cell->AvailableIngredientID;
    if (IngredientID.IsNone())
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) has no available resource"), X, Y);
        return FRealState();
    }

    FRealState Resource = HarvestService->Harvest(IngredientID, Cell->State, Conditions);
    Cell->ResourceRegrowthTimer = ResourceRegrowthTime;
    MarkRegrowing(X, Y);

    if (bHarvestAffectsBiome)
    {
        Cell->HarvestStress += HarvestStressIncrement;
        Cell->HarvestStress = FMath::Clamp(Cell->HarvestStress, 0.0f, 1.0f);
        MarkStress(X, Y);
        RecalculateDistortionFromHarvestStress(*Cell);
        if (!bInterpolationActive)
        {
            bInterpolationActive = true;
            SetActorTickEnabled(true);
        }
    }

    return Resource;
}

FRealState AGridWorldManager::HarvestFromCellSimple(int32 X, int32 Y)
{
    return HarvestFromCell(X, Y, FConditionModifier());
}

void AGridWorldManager::HarvestTest(int32 X, int32 Y)
{
    int32 CellIdx = Y * GridSizeX + X;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (LastHarvestTimeMap.Contains(CellIdx))
    {
        float TimeSinceLast = CurrentTime - LastHarvestTimeMap[CellIdx];
        if (TimeSinceLast < HarvestCooldown)
        {
            UE_LOG(LogHerbalist, Warning, TEXT("HarvestTest: too fast from (%d,%d), ignored (%.3f s)"), X, Y, TimeSinceLast);
            return;
        }
    }
    LastHarvestTimeMap.Add(CellIdx, CurrentTime);

    FRealState Res = HarvestFromCellSimple(X, Y);
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && PC->InventoryComponent)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
        {
            FName IngredientID = Cell->bIsWater ? FName(TEXT("Water")) : Cell->AvailableIngredientID;
            if (!IngredientID.IsNone())
            {
                FInventoryItem Item;
                Item.IngredientID = IngredientID;
                Item.State = Res;
                Item.Count = 1;
                PC->InventoryComponent->AddItem(Item, 1);
            }
        }
    }

    FGridCell* Cell = GetCell(X, Y);
    FString ResourceName = Cell ? (Cell->bIsWater ? TEXT("Water") : Cell->AvailableIngredientID.ToString()) : TEXT("None");
    UE_LOG(LogHerbalist, Log, TEXT("Harvested from (%d,%d): Mag=%.2f Dist=%.2f Stress=%.3f Resource=%s"),
        X, Y, Res.Magnitude, Res.Meta.Distortion, Cell ? Cell->HarvestStress : -1.0f, *ResourceName);
}

void AGridWorldManager::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    for (int32 i = 0; i < Count; ++i) HarvestTest(X, Y);
    UE_LOG(LogHerbalist, Log, TEXT("Mass harvest %d times at (%d,%d)"), Count, X, Y);
}
