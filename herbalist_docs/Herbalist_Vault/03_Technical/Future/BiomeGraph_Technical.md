---
tags: [technical, biomes, graph, future]
status: draft
---

# Biome Graph — Техническая спецификация

## Обзор

Biome Graph — подсистема, реализующая мир как граф взаимосвязанных биомов с распространением влияний, памятью и циклами коллапса/возрождения.

---

## Архитектура

```
UBiomeGraphSubsystem : public UWorldSubsystem
    ├── FBiomeGraph (данные графа)
    ├── FBiomeRuntimeState (живое состояние каждого узла)
    ├── FWavePropagator (распространение Morok/Zaryana)
    ├── FMemoryAccumulator (история биомов)
    └── FCollapseController (циклы коллапса/возрождения)
```

---

## Ключевые структуры данных

### FBiomeNode (авторские данные)

```cpp
USTRUCT(BlueprintType)
struct FBiomeNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName BiomeID;

    UPROPERTY(EditAnywhere)
    FVector4 AxisBias;           // Body, Mind, Spirit, Nature

    UPROPERTY(EditAnywhere)
    float MorokAffinity;         // [0, 1]

    UPROPERTY(EditAnywhere)
    float ZaryanaAffinity;       // [0, 1]

    UPROPERTY(EditAnywhere)
    float Stability;             // [0, 1]

    UPROPERTY(EditAnywhere)
    FGameplayTagContainer TagField;
};
```

### FBiomeEdge (связи)

```cpp
USTRUCT(BlueprintType)
struct FBiomeEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName FromBiome;

    UPROPERTY(EditAnywhere)
    FName ToBiome;

    UPROPERTY(EditAnywhere)
    float MorokLeak;             // [0, 1]

    UPROPERTY(EditAnywhere)
    float ZaryanaFlow;           // [0, 1]

    UPROPERTY(EditAnywhere)
    float AxisDrift;             // [0, 1]

    UPROPERTY(EditAnywhere)
    float Weight;                // [0, 1]
};
```

### FBiomeRuntimeState (живое состояние)

```cpp
USTRUCT(BlueprintType)
struct FBiomeRuntimeState
{
    GENERATED_BODY()

    UPROPERTY()
    FVector4 AxisCurrent;

    UPROPERTY()
    float MorokLevel;

    UPROPERTY()
    float ZaryanaLevel;

    UPROPERTY()
    float Instability;

    UPROPERTY()
    FBiomeMemory Memory;
};
```

### FBiomeMemory (история)

```cpp
USTRUCT(BlueprintType)
struct FBiomeMemory
{
    GENERATED_BODY()

    UPROPERTY()
    FVector4 AccumulatedAxisDrift;

    UPROPERTY()
    float MorokHistory;

    UPROPERTY()
    float ZaryanaHistory;

    UPROPERTY()
    float InstabilityEvents;

    UPROPERTY()
    int32 TransformationCount;

    UPROPERTY()
    float TimeSinceLastStability;
};
```

### FPlayerFootprint (след игрока)

```cpp
USTRUCT(BlueprintType)
struct FPlayerFootprint
{
    GENERATED_BODY()

    UPROPERTY()
    FVector4 AxisSignature;

    UPROPERTY()
    float MorokTrace;

    UPROPERTY()
    float ZaryanaTrace;

    UPROPERTY()
    float PersistenceTime;
};
```

---

## UBiomeGraphSubsystem

```cpp
UCLASS()
class UBiomeGraphSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // Инициализация из DataAsset
    void InitializeFromAsset(UBiomeGraphAsset* Asset);

    // Получение контекста для точки мира
    FBiomeContext ResolveContext(FVector WorldLocation);

    // Получение контекста для конкретного биома
    FBiomeContext ResolveContext(FName BiomeID);

    // Применение воздействия
    void ApplyImpact(FName BiomeID, const FBiomeImprint& Impact);

    // Tick симуляции
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    UBiomeGraphAsset* GraphAsset;

    UPROPERTY()
    TMap<FName, FBiomeRuntimeState> RuntimeStates;

    // Компоненты
    TUniquePtr<FWavePropagator> WavePropagator;
    TUniquePtr<FMemoryAccumulator> MemoryAccumulator;
    TUniquePtr<FCollapseController> CollapseController;

    // Таймеры для многоуровневого тика
    float MediumTickAccumulator = 0.f;
    float SlowTickAccumulator = 0.f;

    static constexpr float MEDIUM_TICK_INTERVAL = 0.5f;
    static constexpr float SLOW_TICK_INTERVAL = 5.0f;
};
```

---

## Tick-модель

| Слой | Интервал | Операции |
|------|----------|----------|
| Fast | Каждый кадр | Разрешение контекста для текущей позиции игрока |
| Medium | 0.5 с | Wave propagation (один шаг диффузии) |
| Slow | 5 с | Memory decay, проверка условий коллапса |

```cpp
void UBiomeGraphSubsystem::Tick(float DeltaTime)
{
    // Fast — всегда
    UpdatePlayerContext();

    // Medium
    MediumTickAccumulator += DeltaTime;
    if (MediumTickAccumulator >= MEDIUM_TICK_INTERVAL)
    {
        MediumTickAccumulator = 0.f;
        WavePropagator->Step(RuntimeStates, GraphAsset->Edges);
    }

    // Slow
    SlowTickAccumulator += DeltaTime;
    if (SlowTickAccumulator >= SLOW_TICK_INTERVAL)
    {
        SlowTickAccumulator = 0.f;
        MemoryAccumulator->Decay(RuntimeStates);
        CollapseController->CheckAndProcess(RuntimeStates);
    }
}
```

---

## WavePropagator

```cpp
class FWavePropagator
{
public:
    void Step(TMap<FName, FBiomeRuntimeState>& States, const TArray<FBiomeEdge>& Edges)
    {
        // Сохраняем текущие значения
        TMap<FName, float> PrevMorok, PrevZaryana;
        for (auto& Pair : States)
        {
            PrevMorok.Add(Pair.Key, Pair.Value.MorokLevel);
            PrevZaryana.Add(Pair.Key, Pair.Value.ZaryanaLevel);
        }

        // Распространяем по рёбрам
        for (const FBiomeEdge& Edge : Edges)
        {
            float MorokSpread = PrevMorok[Edge.FromBiome] * Edge.MorokLeak * Edge.Weight;
            float ZaryanaSpread = PrevZaryana[Edge.FromBiome] * Edge.ZaryanaFlow * Edge.Weight;

            States[Edge.ToBiome].MorokLevel += MorokSpread;
            States[Edge.ToBiome].ZaryanaLevel += ZaryanaSpread;
        }

        // Нормализация и clamping
        for (auto& Pair : States)
        {
            Pair.Value.MorokLevel = FMath::Clamp(Pair.Value.MorokLevel, 0.f, 1.f);
            Pair.Value.ZaryanaLevel = FMath::Clamp(Pair.Value.ZaryanaLevel, 0.f, 1.f);
        }
    }
};
```

---

## Интеграция с HerbalistPipeline

```cpp
FRealState HerbalistPipeline::Process(const FWorldState& World, const FIngredient& Input)
{
    // 1. Base mapping
    FAxesVector Axis = MapBehaviorTags(Input.Tags);

    // 2. Biome Context Injection
    FBiomeContext BiomeCtx = World.BiomeSubsystem->ResolveContext(World.PlayerLocation);
    Axis = ApplyBiomeContext(Axis, BiomeCtx);

    // 3. Morok distortion (контекстный)
    float EffectiveMorok = World.GlobalMorok * (1.f + BiomeCtx.MorokAffinity);
    FRealState MorokResult = ApplyMorok(Axis, EffectiveMorok);

    // 4. Zaryana stabilization (контекстный)
    float EffectiveZaryana = World.GlobalZaryana * (1.f + BiomeCtx.ZaryanaAffinity);
    FRealState Final = ApplyZaryana(MorokResult.Axis, EffectiveZaryana);

    // 5. Запись следа
    World.BiomeSubsystem->ApplyImpact(
        BiomeCtx.BiomeID,
        CreateImprint(Input, Final)
    );

    return Final;
}
```

---

## Сохранение и загрузка

```cpp
USTRUCT()
struct FBiomeWorldSave
{
    GENERATED_BODY()

    UPROPERTY()
    TMap<FName, FBiomeRuntimeState> BiomeStates;

    UPROPERTY()
    float GlobalMorok;

    UPROPERTY()
    float GlobalZaryana;
};
```

**Важно:** не сохраняются:
- промежуточные значения волн
- временные Footprint (со сроком жизни < сохранения)
- AI коррекции (пересчитываются при загрузке)

---

## DataAsset для редактора

```cpp
UCLASS(BlueprintType)
class UBiomeGraphAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Nodes")
    TArray<FBiomeNode> Nodes;

    UPROPERTY(EditAnywhere, Category = "Edges")
    TArray<FBiomeEdge> Edges;

    UPROPERTY(EditAnywhere, Category = "Simulation")
    float GlobalMorokDecay = 0.01f;

    UPROPERTY(EditAnywhere, Category = "Simulation")
    float GlobalZaryanaDecay = 0.005f;

    UPROPERTY(EditAnywhere, Category = "Collapse")
    float CollapseThreshold = 0.85f;
};
```

---

## Статус реализации

| Компонент | Статус |
|-----------|--------|
| `FBiomeNode`, `FBiomeEdge` | ❌ Не реализовано |
| `UBiomeGraphAsset` | ❌ Не реализовано |
| `UBiomeGraphSubsystem` | ❌ Не реализовано |
| `FWavePropagator` | ❌ Не реализовано |
| `FMemoryAccumulator` | ❌ Не реализовано |
| `FCollapseController` | ❌ Не реализовано |
| Интеграция с `HerbalistPipeline` | ❌ Не реализовано |
| Save/Load | ❌ Не реализовано |

---

## Приоритеты для Vertical Slice

1. `FBiomeNode`, `FBiomeEdge`, `UBiomeGraphAsset` — авторские данные
2. `UBiomeGraphSubsystem` с упрощённым `ResolveContext`
3. Интеграция `BiomeContext` в пайплайн
4. Упрощённый `Footprint` без персистентности между сессиями
5. Один цикл коллапса (скриптовый)

Остальное — post-MVP.
