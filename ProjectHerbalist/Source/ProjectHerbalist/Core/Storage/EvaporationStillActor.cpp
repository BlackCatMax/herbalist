// EvaporationStillActor.cpp
#include "Core/Storage/EvaporationStillActor.h"

AEvaporationStillActor::AEvaporationStillActor()
{
    if (InventoryComponent)
    {
        // См. довод у ASettlingStandActor -- та же нейтрализация дефолтного
        // ContainerType=Basket, своя независимая ось эффекта
        // (bHasEvaporated/EvaporationTimeRemainingSeconds).
        InventoryComponent->ContainerType = EStorageContainerType::None;
        InventoryComponent->StationType = EProcessingStationType::EvaporationStill;
    }
}
