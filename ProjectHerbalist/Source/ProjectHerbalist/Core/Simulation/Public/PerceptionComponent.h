// Core/Simulation/Public/PerceptionComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Simulation/Public/PerceivedTypes.h"
#include "PerceptionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTHERBALIST_API UPerceptionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPerceptionComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Инвентарь — маленький (десятки предметов), пересчитывается в тике
    // каждые 0.5с безусловно, дёшево даже так.
    const FPerceivedInventory& GetPerceivedInventory() const { return CachedPerceivedInventory; }

    // Мир — ВЕСЬ грид (250 000 клеток на масштабе 5x5 км), поэтому НЕ
    // считается в тике (2026-09-03, разбор жалобы пользователя на
    // периодические просадки FPS). Раньше TickComponent безусловно тратил
    // тут ~500 000 TMap-вставок (Grid->CaptureState() + ComputePerceivedWorld)
    // и 250 000 RNG-вызовов КАЖДЫЕ 0.5 секунды, всегда — а
    // AGridWorldManager::GetPerceivedWorld() не имел в проекте НИ ОДНОГО
    // читателя (в отличие от GetPerceivedInventory() выше — тот реально
    // используется InventorySlotWidget/ItemTooltipWidget). Теперь считается
    // по требованию, в момент вызова: когда появится реальный читатель
    // (Zaryana UI и т.п.), он получит свежий снапшот на момент своего
    // вызова, а не устаревший на до 0.5с тиковый кэш. Не inline — реальный
    // расчёт, не просто чтение поля; см. .cpp.
    const FPerceivedWorld& GetPerceivedWorld() const;

protected:
    // GetPerceivedWorld() выше const, но лениво пересчитывает и кэширует
    // это поле при каждом вызове (не в тике) — тем же приёмом (const_cast
    // в .cpp), что уже есть у UIngredientRegistrySubsystem::EnsureLoaded.
    // Не mutable: UPROPERTY-поля так не помечают в этом проекте.
    UPROPERTY()
    FPerceivedWorld CachedPerceivedWorld;

    // FPerceivedInventory — обычная C++ структура (не USTRUCT), UPROPERTY не нужен;
    // внутри только FName/FRealState/int32/float/bool, GC-отслеживаемых указателей нет.
    FPerceivedInventory CachedPerceivedInventory;
};