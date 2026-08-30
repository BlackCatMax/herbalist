// Interactable.h
//
// Общий контракт "с этим актором можно провзаимодействовать" (2026-08-30,
// "заводим родительские классы для сущностей и связки"). Раньше каждый из
// четырёх интерактивных акторов (AAlchemyTableActor/AStorageContainer/
// AMemoryFragmentActor, плюс отдельно AHerbalistResourceActor::Harvest()) жил
// со своим собственным OnInteract — разными сигнатурами
// (AStorageContainer брал APlayerController*, остальные — уже
// AHerbalistPlayerController*), и AHerbalistPlayerController::Interact()
// разбирал их вручную цепочкой Cast<>. Один интерфейс — один способ
// проверить "могу ли я провзаимодействовать" (Implements<UInteractable>) и
// один способ вызвать это (Execute_OnInteract), не расширяющаяся цепочка
// if/Cast на каждый новый класс.
//
// AHerbalistResourceActor::Harvest() сознательно НЕ переведён на этот
// интерфейс — это отдельный игровой глагол (клавиша Harvest, не Interact),
// не расхождение, которое нужно "исправлять": объединять два разных действия
// игрока в один контракт значило бы стирать реальное различие ради
// единообразия ради самого единообразия.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AHerbalistPlayerController;

UINTERFACE(Blueprintable)
class PROJECTHERBALIST_API UInteractable : public UInterface
{
    GENERATED_BODY()
};

class PROJECTHERBALIST_API IInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void OnInteract(AHerbalistPlayerController* PC);
};
