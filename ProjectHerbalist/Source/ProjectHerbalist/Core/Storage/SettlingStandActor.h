// SettlingStandActor.h
//
// Отстойник (многоступенчатые зелья, 2026-09-05, прямой запрос
// пользователя: "2. Отстой — процесс усиления доминирующей оси"). Тот же
// приём, что уже ADryingRackActor -- placeable-подкласс AStorageContainer,
// НЕ новый актор с нуля: вся инфраструктура переноса предметов
// (OnInteract_Implementation, UInventoryTransferWidget, BeginPlay) уже
// написана и работает, единственное отличие от обычного сундука --
// собственный UHerbalistInventoryComponent тикает предметы иначе
// (StationType=SettlingStand, см. HerbalistInventoryComponent.cpp).
#pragma once

#include "CoreMinimal.h"
#include "Core/Storage/StorageContainer.h"
#include "SettlingStandActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API ASettlingStandActor : public AStorageContainer
{
    GENERATED_BODY()

public:
    ASettlingStandActor();
};
