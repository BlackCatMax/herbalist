// GridWorldManagerHarvest.cpp
#include "GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "Core/Harvest/HerbalistHarvest.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"

// используем глобальную k_condition из HerbalistHarvest.cpp
// (она объявлена extern в HerbalistHarvest.h)

FRealState AGridWorldManager::HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return FRealState();

    if (Cell->bIsWater)
    {
        FRealState Water = Cell->State;
        Water.Magnitude += k_condition * Conditions.DeltaMagnitude;
        Water.Direction.Body += k_condition * Conditions.DeltaDirection.Body;
        Water.Direction.Mind += k_condition * Conditions.DeltaDirection.Mind;
        Water.Direction.Spirit += k_condition * Conditions.DeltaDirection.Spirit;
        Water.Direction.Nature += k_condition * Conditions.DeltaDirection.Nature;
        Water.Meta.Distortion += k_condition * Conditions.DeltaDistortion;
        Water.Meta.Stability += k_condition * Conditions.DeltaStability;
        Water.Meta.Purity += k_condition * Conditions.DeltaPurity;
        Water.Meta.Potency += k_condition * Conditions.DeltaPotency;
        Water.Meta.Resonance += k_condition * Conditions.DeltaResonance;
        Water.Meta.Corruption += k_condition * Conditions.DeltaCorruption;

        Water.Direction.NormalizeSum();
        Water.Magnitude = FMath::Clamp(Water.Magnitude, 0.0f, 1.0f);
        Water.Meta.Distortion = FMath::Clamp(Water.Meta.Distortion, 0.0f, 1.0f);
        Water.Meta.Stability = FMath::Clamp(Water.Meta.Stability, 0.0f, 1.0f);
        Water.Meta.Purity = FMath::Clamp(Water.Meta.Purity, 0.0f, 1.0f);
        Water.Meta.Potency = FMath::Clamp(Water.Meta.Potency, 0.0f, 1.0f);
        Water.Meta.Resonance = FMath::Clamp(Water.Meta.Resonance, 0.0f, 1.0f);
        Water.Meta.Corruption = FMath::Clamp(Water.Meta.Corruption, 0.0f, 1.0f);
        return Water;
    }

    if (Cell->ResourceRegrowthTimer > 0.0f)
    {
        UE_LOG(LogHerbalist, Warning, TEXT("Cell (%d,%d) resource not ready (regrowing)"), X, Y);
        return FRealState();
    }

    FRealState Resource = FHerbalistHarvest::Harvest(Cell->AvailableResource, Cell->State, Conditions);
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
            EResourceType Type = Cell->bIsWater ? EResourceType::Water : Cell->AvailableResource;
            if (Type != EResourceType::None)
            {
                FInventoryItem Item;
                Item.Type = Type;
                Item.State = Res;
                Item.Count = 1;
                PC->InventoryComponent->AddItem(Item, 1);
            }
        }
    }
    FGridCell* Cell = GetCell(X, Y);
    FString ResourceName = Cell ? (Cell->bIsWater ? TEXT("Water") : FHerbalistHarvest::GetResourceName(Cell->AvailableResource, false)) : TEXT("None");
    UE_LOG(LogHerbalist, Log, TEXT("Harvested from (%d,%d): Mag=%.2f Dist=%.2f Stress=%.3f Resource=%s"),
        X, Y, Res.Magnitude, Res.Meta.Distortion, Cell ? Cell->HarvestStress : -1.0f, *ResourceName);
}

void AGridWorldManager::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    for (int32 i = 0; i < Count; ++i) HarvestTest(X, Y);
    UE_LOG(LogHerbalist, Log, TEXT("Mass harvest %d times at (%d,%d)"), Count, X, Y);
}