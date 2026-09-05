// EvaporationStillActor.h
//
// Выпарной куб (многоступенчатые зелья, 2026-09-05, прямой запрос
// пользователя: "5. Выпаривание"). Тот же приём, что уже ADryingRackActor/
// ASettlingStandActor -- placeable-подкласс AStorageContainer, не новый
// актор с нуля.
#pragma once

#include "CoreMinimal.h"
#include "Core/Storage/StorageContainer.h"
#include "EvaporationStillActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API AEvaporationStillActor : public AStorageContainer
{
    GENERATED_BODY()

public:
    AEvaporationStillActor();
};
