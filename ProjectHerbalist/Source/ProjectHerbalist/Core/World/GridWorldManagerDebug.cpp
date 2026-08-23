// GridWorldManagerDebug.cpp
#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Types/HerbalistCoreMath.h"


void AGridWorldManager::SelectCell(int32 X, int32 Y)
{
    if (GetCell(X, Y))
    {
        SelectedX = X;
        SelectedY = Y;
        UE_LOG(LogHerbalistWorld, Log, TEXT("Selected cell (%d, %d)"), X, Y);
    }
    else
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("Invalid cell (%d, %d)"), X, Y);
    }
}

FString AGridWorldManager::GetSelectedCellInfo() const
{
    const FGridCell* Cell = GetCellConst(SelectedX, SelectedY);
    if (!Cell) return TEXT("No cell selected");
    FString ResourceStr = TEXT("None");
    if (Cell->ResourceActors.Num() > 0 && Cell->ResourceActors[0].IsValid())
    {
        ResourceStr = Cell->ResourceActors[0]->GetIngredientID().ToString();
    }
    // Distance(S_real, S0) — раньше был написан и не подключён ни к чему
    // (META_AUDIT §1.4). Первый живой потребитель: отладочная строка
    // выделенной клетки. Не нормализовано в [0,1] — см. HerbalistCoreMath.h.
    const float DistanceToS0 = HerbalistCore::Math::Distance(Cell->State, FAlatyr::S0);

    return FString::Printf(TEXT("Cell (%d,%d): Mag=%.2f, Dist=%.2f, Stress=%.3f, DistToS0=%.2f, Resource=%s"),
        SelectedX, SelectedY,
        Cell->State.Magnitude,
        Cell->State.Meta.Distortion,
        Cell->HarvestStress,
        DistanceToS0,
        *ResourceStr);
}

void AGridWorldManager::GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress)
{
    X = SelectedX;
    Y = SelectedY;
    RegrowthTimer = 0.0f;
    Distortion = 0.0f;
    HarvestStress = 0.0f;
    ResourceName = TEXT("None");
    if (X >= 0 && Y >= 0)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
        {
            Distortion = Cell->State.Meta.Distortion;
            HarvestStress = Cell->HarvestStress;
            if (Cell->bIsWater)
            {
                ResourceName = TEXT("Вода");
            }
            else if (Cell->ResourceActors.Num() > 0 && Cell->ResourceActors[0].IsValid())
            {
                ResourceName = Cell->ResourceActors[0]->GetIngredientID().ToString();
            }
        }
    }
}

void AGridWorldManager::ShowInventory()
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    UE_LOG(LogHerbalistWorld, Log, TEXT("=== INVENTORY (%d items) ==="), Inventory.Num());
    for (int32 i = 0; i < Inventory.Num(); ++i)
    {
        const FInventoryItem& Item = Inventory[i];
        const FRealState& Res = Item.State;
        FString Name;
        if (Item.IngredientID == FName(TEXT("Potion")))
        {
            Name = TEXT("Зелье");
        }
        else if (Item.IngredientID == FName(TEXT("Water")))
        {
            Name = TEXT("Вода");
        }
        else if (IngredientSubsystem)
        {
            if (const FIngredientTableRow* Row = IngredientSubsystem->GetRow(Item.IngredientID))
            {
                Name = Row->DisplayName.ToString();
            }
            else
            {
                Name = Item.IngredientID.ToString();
            }
        }
        else
        {
            Name = Item.IngredientID.ToString();
        }
        UE_LOG(LogHerbalistWorld, Log, TEXT("[%d] %s x%d: Mag=%.2f, Dist=%.2f, Pot=%.2f Res=%.2f Cor=%.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
            i, *Name, Item.Count, Res.Magnitude, Res.Meta.Distortion, Res.Meta.Potency, Res.Meta.Resonance, Res.Meta.Corruption,
            Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
    }
}