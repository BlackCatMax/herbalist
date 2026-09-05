// SettlingStandActor.cpp
#include "Core/Storage/SettlingStandActor.h"

ASettlingStandActor::ASettlingStandActor()
{
    if (InventoryComponent)
    {
        // AStorageContainer::AStorageContainer() (уже выполнился к этому
        // моменту) ставит ContainerType=Basket как дефолт для найденного в
        // мире контейнера -- отстойнику это не подходит, у него своя,
        // независимая ось эффекта (bHasSettled/SettlingTimeRemainingSeconds),
        // None -- нейтральная база без модификации тарой поверх отстоя, тот
        // же принцип, что уже у ADryingRackActor.
        InventoryComponent->ContainerType = EStorageContainerType::None;

        // Единственное функциональное отличие отстойника от обычного
        // сундука -- см. довод в HerbalistInventoryComponent.h у
        // EProcessingStationType.
        InventoryComponent->StationType = EProcessingStationType::SettlingStand;
    }
}
