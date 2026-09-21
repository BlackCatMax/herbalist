// HeldItemComponent.h
//
// Рука (DESIGN_Diegetic_Interface.md, этап 1, 2026-09-21). Три глагола
// диегетического интерфейса -- взять, осмотреть, применить -- держатся на
// одном: что сейчас в руке. Этот компонент и есть рука.
//
// Предмет «в руке» -- не вынутый из котомки: он остаётся в
// UHerbalistInventoryComponent, рука только указывает на него. Так ничего
// не теряется, если игрок бросит смотреть, и вместимость не меняется (20
// мест на всё, решение 2026-09-21). Рука помнит сам предмет, а не номер
// ячейки: номер сдвигается, когда из котомки уходит что-то выше (тот же урок,
// что у окна заказов).
//
// Видимая часть -- заглушка перед камерой (AHeldItemActor): рук и моделей
// предметов пока нет, это задача арта. Цвет заглушки -- по оси, которая в
// воспринятом состоянии преобладает.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HeldItemComponent.generated.h"

class AHeldItemActor;
class USensationLineWidget;

UCLASS(ClassGroup = (Herbalist), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UHeldItemComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHeldItemComponent();

    // Взять предмет из ячейки котомки. false -- ячейки нет.
    bool TakeFromInventory(int32 InventoryIndex);

    // Убрать обратно (в котомку он и не уходил -- рука просто пустеет).
    void PutAway();

    // Поднести к глазам / опустить. При подносе -- строка ощущения на экран
    // и запись в Травник.
    void ToggleInspect();

    bool IsHolding() const { return bHolding; }
    bool IsInspecting() const { return bHolding && bInspecting; }
    const FInventoryItem& GetHeldItem() const { return HeldItem; }

    // Где предмет сейчас лежит в котомке; INDEX_NONE -- его там больше нет
    // (потратили, отдали, сгнил) -- тогда рука пустеет сама.
    int32 ResolveHeldIndex() const;

    // Строка ощущения последнего осмотра -- для тестов и для виджета.
    const FString& GetSensationLine() const { return SensationLine; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void SyncVisual();
    void ShowSensation(bool bShow);

    bool bHolding = false;
    bool bInspecting = false;
    FInventoryItem HeldItem;
    // Где предмет лежал в последний раз -- первая догадка при поиске.
    int32 HeldIndexHint = INDEX_NONE;
    FString SensationLine;

    UPROPERTY()
    TObjectPtr<AHeldItemActor> HeldActor = nullptr;

    UPROPERTY()
    TObjectPtr<USensationLineWidget> SensationWidget = nullptr;
};
