#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryDragDropOperation.generated.h"

class UHerbalistInventoryComponent;

UCLASS()
class PROJECTHERBALIST_API UInventoryDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    int32 SourceIndex = -1;
    UPROPERTY()
    UHerbalistInventoryComponent* SourceInventory = nullptr;
};