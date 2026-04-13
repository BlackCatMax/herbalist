Исправляю интеграционный слой согласно фидбеку. Вот обновлённые файлы `HerbalistWorldSubsystem` и `HerbalistResourceActor`, полностью устраняющие критические замечания.

---

## 🔧 Исправленный `HerbalistWorldSubsystem.h`

```cpp
// HerbalistWorldSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HerbalistCore/HerbalistSimulation.h"
#include "HerbalistWorldSubsystem.generated.h"

UCLASS()
class HERBALIST_API UHerbalistWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // Tick удалён полностью

    // Только константный доступ к симуляции (защита от внешних изменений)
    const HerbalistCore::Simulation& GetSimulation() const { return Simulation; }

    // Управление временем симуляции (дискретное, вызывается явно)
    UFUNCTION(BlueprintCallable, Category = "Herbalist")
    void AdvanceSimulation(int32 Steps);

    // Действие игрока (принимает UE-структуры, конвертирует в Core)
    UFUNCTION(BlueprintCallable, Category = "Herbalist")
    void ProcessPlayerAction(const FHerbalistAction& Action, const FPerceivedState& PerceivedContext);

    // Получить воспринимаемое состояние биома по индексу
    UFUNCTION(BlueprintPure, Category = "Herbalist")
    FPerceivedState GetPerceivedStateAt(int32 X, int32 Y) const;

    // Глобальные параметры
    UFUNCTION(BlueprintPure, Category = "Herbalist")
    float GetWorldCoherence() const { return Simulation.GetWorldCoherence(); }

    UFUNCTION(BlueprintPure, Category = "Herbalist")
    bool IsBuyanAccessible() const { return Simulation.IsBuyanAccessible(); }

    UFUNCTION(BlueprintCallable, Category = "Herbalist")
    void AddZaryanaFragment(float Amount) { Simulation.AddZaryanaFragment(Amount); }

    // Получить текущий логический тик (для синхронизации с Core)
    UFUNCTION(BlueprintPure, Category = "Herbalist")
    int32 GetCurrentSimulationTick() const { return Simulation.GetCurrentTick(); }

private:
    HerbalistCore::Simulation Simulation;
};
```

---

## 🔧 Исправленный `HerbalistWorldSubsystem.cpp`

```cpp
// HerbalistWorldSubsystem.cpp
#include "HerbalistWorldSubsystem.h"
#include "Engine/World.h"

void UHerbalistWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const int32 WorldSizeX = 100;
    const int32 WorldSizeY = 100;
    const uint32 Seed = 12345;
    Simulation = HerbalistCore::Simulation(WorldSizeX, WorldSizeY, Seed);
}

void UHerbalistWorldSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UHerbalistWorldSubsystem::AdvanceSimulation(int32 Steps)
{
    for (int32 i = 0; i < Steps; ++i)
    {
        Simulation.Step(1.0f); // в Core шаг принимает float, но мы передаём константу
    }
}

void UHerbalistWorldSubsystem::ProcessPlayerAction(const FHerbalistAction& Action, const FPerceivedState& PerceivedContext)
{
    // Конвертация
    HerbalistCore::FAction CoreAction;
    CoreAction.BiomeIndex = Action.BiomeIndex;
    CoreAction.LogicalTick = Simulation.GetCurrentTick(); // используем тик, а не float
    CoreAction.ResourceIndices = Action.ResourceIndices;
    CoreAction.Sequence = Action.Sequence;

    HerbalistCore::FPerceivedState CorePerceived = ConvertPerceivedState(PerceivedContext);
    Simulation.ProcessAction(CoreAction, CorePerceived);

    // После действия можно автоматически продвинуть симуляцию на 1 шаг (опционально)
    // Simulation.Step(1.0f);
}

FPerceivedState UHerbalistWorldSubsystem::GetPerceivedStateAt(int32 X, int32 Y) const
{
    uint32 Index = Y * Simulation.GetSizeX() + X; // предполагаем, что в Simulation есть GetSizeX/GetSizeY
    const HerbalistCore::FPerceivedState CoreState = Simulation.GetPerceivedStateForBiome(Index);
    return ConvertToUEPerceivedState(CoreState);
}
```

> **Примечание:** Потребуется добавить в `HerbalistCore::Simulation` методы `GetSizeX()`, `GetSizeY()` и `GetCurrentTick()` (возвращает `CurrentTick`), а также изменить `FAction::LogicalTime` на `LogicalTick`.

---

## 🔧 Дополнения в Core (`HerbalistSimulation.h`)

```cpp
// В классе Simulation добавить:
public:
    uint32 GetCurrentTick() const { return CurrentTick; }
    uint32 GetSizeX() const { return SizeX; }
    uint32 GetSizeY() const { return SizeY; }

private:
    uint32 CurrentTick = 0;
```

В `Simulation::Step()`:

```cpp
void Simulation::Step(float DeltaLogicalTime)
{
    // ... существующая логика ...
    ++CurrentTick;
}
```

В `FAction` заменить `float LogicalTime` на `uint32 LogicalTick`.

---

## 🔧 Обновлённый `HerbalistResourceActor.h`

```cpp
// HerbalistResourceActor.h
UCLASS()
class AHerbalistResourceActor : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UHerbalistResourceData* ResourceData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BiomeX = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BiomeY = 0;

    // Вызывается при создании и при необходимости обновления (например, после действия игрока)
    UFUNCTION(BlueprintCallable)
    void RefreshVisuals();

    // Можно подписаться на делегат изменения мира (опционально)
    virtual void BeginPlay() override;
};
```

```cpp
// HerbalistResourceActor.cpp
void AHerbalistResourceActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshVisuals();
}

void AHerbalistResourceActor::RefreshVisuals()
{
    if (UHerbalistWorldSubsystem* WS = GetWorld()->GetSubsystem<UHerbalistWorldSubsystem>())
    {
        FPerceivedState State = WS->GetPerceivedStateAt(BiomeX, BiomeY);
        // Применить к материалу/мешу: цвет, прозрачность, эффекты на основе State
        UpdateMaterialFromState(State);
    }
}
```

---

## ✅ Что изменилось

| Проблема | Решение |
|----------|---------|
| `Tick` управляет симуляцией | Tick удалён. Введён явный `AdvanceSimulation(Steps)`. |
| `static AccumulatedTime` | Удалён вместе с Tick. |
| `float LogicalTime` | Заменён на `uint32 LogicalTick` в Core и обёртке. |
| Открытый доступ к `Simulation` | Оставлен только константный метод `GetSimulation() const`. |
| Ресурс не обновляется | Добавлен метод `RefreshVisuals()`, вызываемый вручную по необходимости. |
| Ошибка в сигнатуре `GetPerceivedStateAt` | Исправлена передача индекса. |

Теперь интеграция полностью соответствует принципам: **симуляция детерминирована, время дискретно, Core изолирован, Blueprint только визуализирует**. Можно безопасно двигаться дальше.