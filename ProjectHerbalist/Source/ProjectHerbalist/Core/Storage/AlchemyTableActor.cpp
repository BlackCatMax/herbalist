#include "AlchemyTableActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/World/GridWorldManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AAlchemyTableActor::AAlchemyTableActor()
{
    PrimaryActorTick.bCanEverTick = false;
    StoredIngredients.SetNum(3);
}

void AAlchemyTableActor::OnInteract(AHerbalistPlayerController* PlayerController)
{
    if (!PlayerController) return;
    PlayerController->OpenAlchemyWidget(this);
}

void AAlchemyTableActor::SetSlotItem(int32 SlotIndex, const FInventoryItem& Item)
{
    if (SlotIndex == 0)
        StoredWater = Item;
    else if (SlotIndex >= 1 && SlotIndex <= 3)
    {
        if (StoredIngredients.Num() < 3) StoredIngredients.SetNum(3);
        StoredIngredients[SlotIndex - 1] = Item;
    }
}

FInventoryItem AAlchemyTableActor::GetSlotItem(int32 SlotIndex) const
{
    if (SlotIndex == 0)
        return StoredWater;
    else if (SlotIndex >= 1 && SlotIndex <= 3)
    {
        if (StoredIngredients.IsValidIndex(SlotIndex - 1))
            return StoredIngredients[SlotIndex - 1];
    }
    return FInventoryItem();
}

void AAlchemyTableActor::ClearSlot(int32 SlotIndex)
{
    SetSlotItem(SlotIndex, FInventoryItem());
}

TArray<FInventoryItem> AAlchemyTableActor::GetIngredientsForCraft() const
{
    TArray<FInventoryItem> Ingredients;
    if (StoredWater.IsValid()) Ingredients.Add(StoredWater);
    for (const FInventoryItem& Item : StoredIngredients)
        if (Item.IsValid()) Ingredients.Add(Item);
    return Ingredients;
}

void AAlchemyTableActor::UpdateGridCoords()
{
    UWorld* World = GetWorld();
    if (!World) return;
    AGridWorldManager* GridManager = nullptr;
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        GridManager = *It;
        break;
    }
    if (!GridManager) return;
    FVector Loc = GetActorLocation() - GridManager->GetActorLocation();
    int32 X = FMath::RoundToInt(Loc.X / GridManager->CellSize);
    int32 Y = FMath::RoundToInt(Loc.Y / GridManager->CellSize);
    if (X >= 0 && X < GridManager->GridSizeX && Y >= 0 && Y < GridManager->GridSizeY)
        GridCoords = FIntPoint(X, Y);
    else
        GridCoords = FIntPoint(-1, -1);
}