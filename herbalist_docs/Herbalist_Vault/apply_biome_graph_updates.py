#!/usr/bin/env python3
# apply_biome_graph_updates.py
# Автоматическое внедрение системы биомного графа в документацию Herbalist

import os
import re
from pathlib import Path

# ============================================================
# КОНФИГУРАЦИЯ
# ============================================================

VAULT_ROOT = "."  # Корень Herbalist_Vault

# ============================================================
# СОДЕРЖИМОЕ НОВЫХ ФАЙЛОВ
# ============================================================

GDD_14_BIOME_GRAPH = """---
tags: [gdd, biomes, graph, core]
status: draft
---

# 14. Биомный граф и экосистемная симуляция

## 14.1 Биом как оператор контекста

В архитектуре **Herbalist** биом — это не фон и не набор ресурсов. Это **постоянный оператор контекста**, который:

- влияет на входной вектор ингредиентов
- задаёт «сдвиг базиса» трансформации
- определяет допустимую степень искажения [[Morok]]
- ограничивает или усиливает стабилизацию [[Zaryana]]

Формально биом описывается структурой:

```
Biome = {
    AxisBias,        // 4D вектор смещения осей
    MorokAffinity,   // склонность к искажению
    ZaryanaAffinity, // склонность к стабилизации
    StabilityField,  // устойчивость к изменениям
    TagField         // вектор влияния тегов (Cold, Wet, Sacred...)
}
```

### 14.1.1 AxisBias

Смещение пространства трансформации:

```
Biome Bias = (B_body, B_mind, B_spirit, B_nature)
```

Этот вектор:
- усиливает соответствующие свойства ингредиентов
- «перекашивает» результат ещё до применения [[Morok]]

Примеры:
- Тундра → Nature ↑, Body ↓
- Болото → Spirit ↑, Stability ↓

### 14.1.2 MorokAffinity и ZaryanaAffinity

Определяют, насколько биом «разрешает» искажение или стабилизацию:

- `MorokAffinity ∈ [0, 1]` — множитель для силы [[Morok]]
- `ZaryanaAffinity ∈ [0, 1]` — множитель для силы [[Zaryana]]

В болотах Morok усиливается, в лабораторных зонах подавляется. «Святые» биомы усиливают очищение, «гнилые» подавляют его.

---

## 14.2 Мир как граф состояний

Состояние мира не является изолированным в каждой точке. Биомы связаны в **ориентированный взвешенный граф влияний**.

### 14.2.1 Узел графа (Biome Node)

```
struct FBiomeNode
{
    FName BiomeID;
    FVector4 AxisBias;
    float MorokAffinity;
    float ZaryanaAffinity;
    float Stability;
    FGameplayTagContainer TagField;
};
```

### 14.2.2 Ребро графа (Biome Edge)

```
struct FBiomeEdge
{
    FName FromBiome;
    FName ToBiome;
    float MorokLeak;      // утечка хаоса
    float ZaryanaFlow;    // утечка стабилизации
    float AxisDrift;      // смещение вектора
    float TransitionCost; // сложность перехода
};
```

### 14.2.3 Граф как структура

```
struct FBiomeGraph
{
    TMap<FName, FBiomeNode> Nodes;
    TArray<FBiomeEdge> Edges;
};
```

---

## 14.3 Влияние биома на пайплайн трансформации

Биом встраивается в цепочку трансформации как **контекстный слой ДО Morok, но ПОСЛЕ базового маппинга**:

```
Input
  ↓
BehaviorTags → Axis Mapping
  ↓
Biome Context Injection   ← НОВЫЙ СЛОЙ
  ↓
Morok Distortion
  ↓
Zaryana Stabilization
  ↓
Post-Validation / Constraints
  ↓
Output RealState
```

### 14.3.1 Формула контекста

Локальный контекст биома формируется не только из его собственных параметров, но и из влияния соседей:

```
B_context = B_current + Σ (Leak × B_neighbor)
```

### 14.3.2 Применение контекста

```
Axis_1 = Axis_0 + Biome.AxisBias
Axis_clamped = clamp(Axis_1, -Stability, Stability)
```

---

## 14.4 Распространение Morok и Zaryana (Wave Propagation)

Morok и Zaryana — не глобальные коэффициенты, а **волны влияния, распространяющиеся по графу**.

### 14.4.1 Morok propagation

```
M_node = M_base + Σ (MorokLeak × M_neighbor)
```

Результат:
- болота заражают леса
- руины усиливают nearby corruption
- «чистые зоны» стабилизируют соседей

### 14.4.2 Zaryana как противофаза

```
Z_node = Z_base + Σ (ZaryanaFlow × Z_neighbor)
```

Это создаёт:
- «карманы стабильности»
- зоны очищения вокруг источников
- естественные границы биомов

### 14.4.3 Эффективный Morok и Zaryana

Внутри пайплайна используются эффективные значения, учитывающие контекст биома:

```
M_effective = M_base × (1 + Biome.MorokAffinity)
Z_effective = Z_base × (1 + Biome.ZaryanaAffinity)
```

---

## 14.5 Память биома (Biome Memory)

Биом накапливает историю воздействий. Это создаёт **«невидимую геологию мира»**.

### 14.5.1 Структура памяти

```
struct FBiomeMemory
{
    FVector4 AccumulatedAxisDrift;
    float MorokHistory;
    float ZaryanaHistory;
    float InstabilityEvents;
    int32 TransformationCount;
    float TimeSinceLastStability;
};
```

### 14.5.2 Обновление памяти

Каждый тик симуляции:

```
M_t+1 = M_t + Δ_current + decay(M_old)
```

### 14.5.3 Что это даёт

- биом «помнит» хаос прошлого
- даже после стабилизации остаются следы
- зоны становятся **исторически напряжёнными**

---

## 14.6 Циклы коллапса и возрождения

Биом не стабилен навсегда. Он живёт циклами:

```
Stable → Accumulating → Critical → Collapsed → Reborn
```

### 14.6.1 Состояния жизненного цикла

| Состояние | Описание |
|-----------|----------|
| Stable | Нормальное функционирование |
| Accumulating | Накопление напряжения |
| Critical | Предколлапсное состояние |
| Collapsed | Разрушение структуры |
| Reborn | Перерождение с новыми параметрами |

### 14.6.2 Условие коллапса

```
C = M_saturation - Z_pressure + I_memory

Если C > Threshold → Collapse
```

### 14.6.3 Ребёрс (перерождение)

После коллапса:
- биом перегенерирует AxisBias
- связи графа перераспределяются
- Morok/Zaryana сбрасываются частично
- **но память остаётся** как «след руин»

---

## 14.7 След игрока (Footprint System)

Игрок не просто «использует биом». Он **впечатывает себя в граф мира**.

### 14.7.1 Структура следа

```
struct FPlayerFootprint
{
    FVector4 AxisSignature;
    float MorokTrace;
    float ZaryanaTrace;
    float SpatialInfluenceRadius;
    float PersistenceTime;
};
```

### 14.7.2 Когда создаётся след

Каждый значимый акт:
- завершение алхимического преобразования
- применение сильного зелья к миру
- массовый сбор ресурсов

### 14.7.3 Влияние следа

След:
- изменяет Morok/Zaryana affinity биома
- создаёт локальные «резонансные зоны»
- влияет на будущие преобразования в этой области

Формула распространения:

```
F(x) = F_0 × e^(-d) + Influence_player
```

---

## 14.8 Экологический балансировщик (AI)

Чтобы мир не развалился от собственной симуляции, вводится **системный стабилизатор**.

### 14.8.1 Что анализируется

- глобальный Morok saturation
- Zaryana overdominance
- энтропия графа
- «мёртвые зоны» (нулевая динамика)

### 14.8.2 Метрика устойчивости

```
E = H(Morok) - H(Zaryana) + σ(Instability)
```

### 14.8.3 Типы коррекций

- **Soft damping:** ослабление Morok волн
- **Flow redirection:** перенаправление Zaryana
- **Edge weakening:** разрыв цепочек заражения
- **Stability seeding:** создание якорей

Важно: AI не «чинит мир», а **слегка смещает веса графа**, сохраняя его живым.

---

## 14.9 Замкнутый цикл экосистемы

Все компоненты образуют единый цикл:

```
Biome Graph State
      ↓
Local Biome Context
      ↓
Player Action (Alchemy)
      ↓
ΔS (локальное изменение)
      ↓
Footprint Recording
      ↓
Wave Propagation (Morok/Zaryana по графу)
      ↓
Memory Accumulation
      ↓
Collapse/Rebirth (при достижении порога)
      ↓
AI Balancing (мягкая коррекция)
      ↓
Новое состояние графа
```

---

## 14.10 Связь с существующими системами

| Система | Роль в Biome Graph |
|---------|-------------------|
| [[GridWorldManager]] | Хранит состояния ячеек, соответствующих узлам графа |
| [[Alchemy]] | Biome Context Injection как отдельный этап пайплайна |
| [[MemoryState]] | Расширяется до полноценной памяти биома |
| [[Harvest]] | На параметры сбора влияет текущий контекст биома |
| [[S_real]] / [[S_perceived]] | Состояние биома имеет оба представления |

---

## 14.11 Статус реализации

| Компонент | Статус | Примечание |
|-----------|--------|------------|
| Biome Node/Edge структуры | ❌ не реализовано | Требует добавления |
| Biome Context Injection | ❌ не реализовано | В пайплайн алхимии |
| Wave Propagation | ❌ не реализовано | Требует подсистемы |
| Biome Memory | ⚠️ частично | Есть MemoryState, требует расширения |
| Collapse/Rebirth | ❌ не реализовано | Требует пороговой логики |
| Footprint System | ❌ не реализовано | Требует отдельной подсистемы |
| AI Balancer | ❌ не реализовано | Опционально для MVP |

---

## 14.12 Приоритеты для Vertical Slice

Для демонстрации системы в Vertical Slice (2–4 недели) достаточно:

1. **Статический граф из 3 биомов** (Stable Forest, Corrupt Swamp, Hybrid Ruins)
2. **Biome Context Injection** в пайплайн алхимии
3. **Упрощённое распространение** Morok/Zaryana (без волн, только учёт соседей)
4. **Footprint** от действий игрока
5. **Один цикл коллапса** для демонстрации

Остальное — для полной версии.

---

## 14.13 Итог

Биомный граф превращает мир **Herbalist** из набора статических зон в **живую самоизменяющуюся экосистему**. Игрок не просто «находится в биоме» — он становится частью его эволюции, оставляя следы и наблюдая, как мир реагирует на его действия.

Ключевое архитектурное достижение:

> Мир = граф состояний с памятью, где Morok и Zaryana — не параметры, а распространяющиеся волны влияния.
"""

TECH_BIOME_GRAPH = """---
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
"""

# ============================================================
# ФРАГМЕНТЫ ДЛЯ ВСТАВКИ В СУЩЕСТВУЮЩИЕ ФАЙЛЫ
# ============================================================

CORE_LOCK_ADDITION = """
## 1.2 Мир как граф состояний

Состояние мира не является изолированным в каждой точке. Биомы связаны в **ориентированный взвешенный граф влияний**:

- Каждый биом = узел с параметрами (AxisBias, MorokAffinity, ZaryanaAffinity)
- Связи = утечки контекста (MorokLeak, ZaryanaFlow, AxisDrift)

Изменение в одном биоме распространяется на соседей через рёбра графа. Это создаёт **непрерывное поле состояний**, где локальные действия имеют глобальные последствия.

Формально мир можно представить как:

```
World = Graph(Nodes, Edges) + Memory + WaveState
```

Где:
- `Nodes` — биомы с их параметрами
- `Edges` — направленные связи с весами влияния
- `Memory` — накопленная история каждого узла
- `WaveState` — текущее состояние распространяющихся волн [[Morok]] и [[Zaryana]]

> **Подробнее:** см. [[14_Biome_Graph]].
"""

SYSTEMS_PIPELINE_REPLACEMENT = """### Общий пайплайн варки

Процесс преобразования состоит из нескольких этапов, каждый из которых зависит от параметров ингредиентов и контекста:

1. **Сбор параметров:** система извлекает из каждого ингредиента его текущий [[S_real]], учитывая изменения, произошедшие в инвентаре.

2. **Агрегация ([[Fold]]):** вклады всех ингредиентов (включая воду) объединяются через взвешенную сумму с учётом порядка. Вес первого ингредиента максимален, каждого следующего — с затуханием.

3. **Biome Context Injection:** применение смещения текущего биома. Эффективный контекст учитывает не только сам биом, но и влияние соседей через граф (см. [[14_Biome_Graph]]).

4. **Применение воды:** вода обрабатывается особым образом: её вклад в разбавление и итоговые [[Purity]]/[[Distortion]] рассчитывается отдельно, после агрегации остальных компонентов.

5. **Нормализация осей:** результирующий вектор осей ([[Body]], [[Mind]], [[Spirit]], [[Nature]]) нормализуется по сумме, чтобы значения находились в диапазоне [0,1] и отражали относительную [[Intent]].

6. **[[Morok]]:** нелинейное искажение дельты ([[Delta]]), добавляющее шум, обмен осями и непредсказуемость. Сила искажения умножается на `MorokAffinity` биома.

7. **[[Zaryana]]:** структурирование — усиление доминирующих компонент, повышение [[Stability]] и [[Purity]]. Сила стабилизации умножается на `ZaryanaAffinity` биома.

8. **[[Bifurcation]]:** при критическом [[Distortion]] возможен коллапс или очищение.

9. **Формирование зелья:** итоговые параметры фиксируются в структуре, которая затем определяет имя, внешний вид и эффекты зелья."""

WORLD_PIPELINE_ADDITION = """
**С учётом биомного графа цикл расширяется:**

```
Biome Graph State
      ↓
Local Biome Context (текущий биом + влияние соседей)
      ↓
Player Action (на основе [[S_perceived]])
      ↓
ΔS (локальное изменение [[S_real]])
      ↓
Footprint Recording (след игрока в биоме)
      ↓
Wave Propagation (распространение Morok/Zaryana по графу)
      ↓
Memory Accumulation (обновление истории биома)
      ↓
Collapse/Rebirth (при достижении порогов)
      ↓
Новое состояние графа
      ↓
Новое [[S_perceived]] для игрока
```

Этот цикл является замкнутым и непрерывным. Изменения не изолированы в точке воздействия — они распространяются, накапливаются и формируют новый контекст для последующих действий.

> **Подробнее о графе и распространении:** см. [[14_Biome_Graph]].
"""

MOC_ADDITIONS = """
| [[14_Biome_Graph]] | Биомный граф и экосистемная симуляция |"""

MOC_TECH_ADDITIONS = """
| [[03_Technical/Future/BiomeGraph_Technical|Biome Graph]] | Техническая спецификация графа биомов |"""

# ============================================================
# ФУНКЦИИ ДЛЯ РАБОТЫ С ФАЙЛАМИ
# ============================================================

def ensure_dir(path):
    """Создать директорию, если не существует"""
    Path(path).mkdir(parents=True, exist_ok=True)


def write_file(path, content):
    """Записать файл"""
    ensure_dir(os.path.dirname(path))
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"  ✓ Создан: {path}")


def insert_after_marker(filepath, marker, insertion):
    """Вставить текст после маркера"""
    if not os.path.exists(filepath):
        print(f"  ⚠ Файл не найден: {filepath}")
        return False
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    if marker not in content:
        print(f"  ⚠ Маркер не найден в {filepath}: {marker[:50]}...")
        return False
    
    # Вставка после маркера
    new_content = content.replace(marker, marker + insertion)
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f"  ✓ Обновлён: {filepath}")
    return True


def replace_section(filepath, start_marker, end_marker, new_content):
    """Заменить секцию между маркерами"""
    if not os.path.exists(filepath):
        print(f"  ⚠ Файл не найден: {filepath}")
        return False
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    pattern = f"{start_marker}.*?{end_marker}"
    replacement = f"{start_marker}\n{new_content}\n{end_marker}"
    
    new_content_full = re.sub(pattern, replacement, content, flags=re.DOTALL)
    
    if new_content_full == content:
        print(f"  ⚠ Секция не найдена в {filepath}")
        return False
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content_full)
    
    print(f"  ✓ Обновлён: {filepath}")
    return True


def append_to_file(filepath, text):
    """Добавить текст в конец файла"""
    if not os.path.exists(filepath):
        print(f"  ⚠ Файл не найден: {filepath}")
        return False
    
    with open(filepath, 'a', encoding='utf-8') as f:
        f.write(text)
    
    print(f"  ✓ Обновлён: {filepath}")
    return True


def insert_in_moc_section(filepath, section_marker, insertion):
    """Вставить строку в таблицу MOC после заголовка секции"""
    if not os.path.exists(filepath):
        print(f"  ⚠ Файл не найден: {filepath}")
        return False
    
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    new_lines = []
    inserted = False
    
    for i, line in enumerate(lines):
        new_lines.append(line)
        if section_marker in line and not inserted:
            new_lines.append(insertion + "\n")
            inserted = True
    
    if not inserted:
        print(f"  ⚠ Секция не найдена в MOC: {section_marker}")
        return False
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)
    
    print(f"  ✓ Обновлён MOC: {filepath}")
    return True


# ============================================================
# ГЛАВНАЯ ФУНКЦИЯ
# ============================================================

def main():
    print("=" * 60)
    print("ВНЕДРЕНИЕ СИСТЕМЫ БИОМНОГО ГРАФА В HERBALIST")
    print("=" * 60)
    
    # --------------------------------------------------------
    # 1. Создание новых файлов
    # --------------------------------------------------------
    print("\n[1/6] Создание новых файлов...")
    
    write_file(
        os.path.join(VAULT_ROOT, "02_GDD/14_Biome_Graph.md"),
        GDD_14_BIOME_GRAPH
    )
    
    write_file(
        os.path.join(VAULT_ROOT, "03_Technical/Future/BiomeGraph_Technical.md"),
        TECH_BIOME_GRAPH
    )
    
    # --------------------------------------------------------
    # 2. Обновление 00_Core_Lock.md
    # --------------------------------------------------------
    print("\n[2/6] Обновление 00_Core_Lock.md...")
    
    core_lock_path = os.path.join(VAULT_ROOT, "02_GDD/00_Core_Lock.md")
    insert_after_marker(
        core_lock_path,
        "## 1. Единое состояние мира",
        CORE_LOCK_ADDITION
    )
    
    # --------------------------------------------------------
    # 3. Обновление 05_Systems.md (пайплайн алхимии)
    # --------------------------------------------------------
    print("\n[3/6] Обновление 05_Systems.md...")
    
    systems_path = os.path.join(VAULT_ROOT, "02_GDD/05_Systems.md")
    replace_section(
        systems_path,
        "### Общий пайплайн варки",
        "Подробное техническое описание пайплайна см. в",
        SYSTEMS_PIPELINE_REPLACEMENT
    )
    
    # --------------------------------------------------------
    # 4. Обновление 13_World_Pipeline.md
    # --------------------------------------------------------
    print("\n[4/6] Обновление 13_World_Pipeline.md...")
    
    pipeline_path = os.path.join(VAULT_ROOT, "02_GDD/13_World_Pipeline.md")
    insert_after_marker(
        pipeline_path,
        "## 13.1 Общая структура цикла",
        WORLD_PIPELINE_ADDITION
    )
    
    # --------------------------------------------------------
    # 5. Обновление _MOC.md
    # --------------------------------------------------------
    print("\n[5/6] Обновление _MOC.md...")
    
    moc_path = os.path.join(VAULT_ROOT, "_MOC.md")
    
    # Вставка в раздел GDD
    insert_in_moc_section(
        moc_path,
        "| [[13_World_Pipeline]]",
        "| [[14_Biome_Graph]] | Биомный граф и экосистемная симуляция |"
    )
    
    # Вставка в раздел Technical
    insert_in_moc_section(
        moc_path,
        "| [[03_Technical/Current/Core_Current|Core_Current]]",
        "| [[03_Technical/Future/BiomeGraph_Technical|Biome Graph]] | Техническая спецификация графа биомов |"
    )
    
    # --------------------------------------------------------
    # 6. Создание резюме изменений
    # --------------------------------------------------------
    print("\n[6/6] Создание отчёта об изменениях...")
    
    report = f"""# Отчёт о внедрении Biome Graph

**Дата:** {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

## Созданные файлы

- ✅ `02_GDD/14_Biome_Graph.md` — полное описание системы биомного графа
- ✅ `03_Technical/Future/BiomeGraph_Technical.md` — техническая спецификация

## Обновлённые файлы

- ✅ `02_GDD/00_Core_Lock.md` — добавлен раздел 1.2 «Мир как граф состояний»
- ✅ `02_GDD/05_Systems.md` — обновлён пайплайн алхимии (Biome Context Injection)
- ✅ `02_GDD/13_World_Pipeline.md` — расширен цикл с учётом графа
- ✅ `_MOC.md` — добавлены ссылки на новые страницы

## Что изменилось в архитектуре

1. **Биом** теперь не статическая зона, а **оператор контекста** с параметрами AxisBias, MorokAffinity, ZaryanaAffinity
2. **Мир** представлен как **ориентированный взвешенный граф** биомов
3. **Morok и Zaryana** распространяются по графу как **волны влияния**
4. **Биомы накапливают память** о действиях игрока
5. **Циклы коллапса и возрождения** делают мир живым
6. **Footprint System** связывает действия игрока с эволюцией мира

## Следующие шаги

1. Ознакомиться с `14_Biome_Graph.md`
2. Определить scope для Vertical Slice (см. раздел 14.12)
3. Реализовать упрощённую версию для демонстрации

---
*Отчёт сгенерирован автоматически скриптом apply_biome_graph_updates.py*
"""
    
    write_file(os.path.join(VAULT_ROOT, "00_Meta/biome_graph_update_report.md"), report)
    
    # --------------------------------------------------------
    # Итог
    # --------------------------------------------------------
    print("\n" + "=" * 60)
    print("ВНЕДРЕНИЕ ЗАВЕРШЕНО")
    print("=" * 60)
    print("\nСоздано файлов: 2")
    print("Обновлено файлов: 4")
    print("\nОтчёт сохранён в: 00_Meta/biome_graph_update_report.md")
    print("\nРекомендуется:")
    print("  1. Проверить изменения в обновлённых файлах")
    print("  2. Ознакомиться с 14_Biome_Graph.md")
    print("  3. Открыть _MOC.md для навигации")


if __name__ == "__main__":
    main()