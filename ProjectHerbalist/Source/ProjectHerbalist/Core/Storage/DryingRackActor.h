// DryingRackActor.h
//
// Сушилка (DESIGN_Community_And_Homestead.md §2.2, "Хранилища" пункт 3,
// 2026-09-04) -- прямой запрос пользователя: "сушка — не контейнер, а
// ПРОЦЕСС", растянутый во игровое время, не мгновенное действие (в отличие
// от BuildHomeStorage). Placeable-актор, тот же уровень аналогии, что
// AAlchemyTableActor -- станция, к которой игрок физически подходит и
// взаимодействует, а не Exec-команда без места в мире.
//
// Подкласс AStorageContainer, НЕ новый актор с нуля ("не изобретай
// архитектуру с нуля, если что-то похожее уже есть", прямая инструкция) --
// вся инфраструктура переноса предметов (OnInteract_Implementation,
// UInventoryTransferWidget, BeginPlay) уже написана и работает, единственное
// отличие сушилки от обычного сундука -- собственный UHerbalistInventoryComponent
// тикает предметы иначе (bIsDryingRack=true, см. HerbalistInventoryComponent.cpp)
// -- переключатель поведения на уже существующем компоненте, не новый класс
// компонента и не новый UI.
#pragma once

#include "CoreMinimal.h"
#include "Core/Storage/StorageContainer.h"
#include "DryingRackActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API ADryingRackActor : public AStorageContainer
{
    GENERATED_BODY()

public:
    ADryingRackActor();
};
