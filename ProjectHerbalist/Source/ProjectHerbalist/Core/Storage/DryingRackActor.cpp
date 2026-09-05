// DryingRackActor.cpp
#include "Core/Storage/DryingRackActor.h"

ADryingRackActor::ADryingRackActor()
{
    if (InventoryComponent)
    {
        // AStorageContainer::AStorageContainer() (уже выполнился к этому
        // моменту -- базовый конструктор тела выполняется раньше тела
        // производного) ставит ContainerType=Basket как дефолт для
        // найденного в мире контейнера -- сушилке это не подходит, у неё
        // своя, независимая ось decay (bIsDried/DriedItemDecayMultiplier,
        // см. довод в HerbalistInventoryComponent.cpp), None -- нейтральная
        // база без модификации тарой поверх сушки, тот же принцип, что уже
        // "на себе, без контейнера".
        InventoryComponent->ContainerType = EStorageContainerType::None;

        // Единственное функциональное отличие сушилки от обычного сундука
        // (AStorageContainer) -- см. довод в HerbalistInventoryComponent.h
        // у bIsDryingRack.
        InventoryComponent->bIsDryingRack = true;
    }
}
