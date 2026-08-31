---
tags: [technical, archive, world]
---

> ⚠️ **АРХИВ (перенесено 2026-08-31).** Описывает прототип апреля 2026
> (`based_on: prototype_2026-04-21`), предшествует пайплайну команд/дельт,
> системе сохранений, BiomeGraph, капищам, бестиарию и всей работе
> последних четырёх месяцев — `FGridCell`, распространение эффектов,
> размещение воды описаны здесь так, как не устроены в реальном коде уже
> давно (полный список расхождений — `CHANGELOG.md`, "полный аудит проекта"
> 2026-08-31). Полная перезапись признана нецелесообразной — риск внести
> новые неточности выше пользы; решение принято 2026-08-31: архивировать,
> не переписывать. За текущей архитектурой — `CHANGELOG.md`/`ROADMAP.md` в
> корне репозитория и сам код (`Core/World/GridWorldManager*.cpp`).
> Сохранён как историческая точка зрения, не как актуальная документация.

# Мир на гриде — реализация
## `AGridWorldManager`
- Размер сетки: `GridSizeX` × `GridSizeY`.
- Размер клетки: `CellSize`.
- Каждая клетка — `FGridCell`.
## `FGridCell`
- `Biome` (EBiomeType)
- `State` (текущее `FRealState`)
- `TargetState` (целевое, для интерполяции)
- `Environment` (Toxicity, Fertility, Moisture)
- `Memory` (AccumulatedDistortion, StabilityMemory, HistoryPurity)
- `HarvestStress` (накапливается при сборе)
- `AvailableResource` (EResourceType)
- `ResourceRegrowthTimer`
- `bIsWater`
## Инициализация
- Биомы назначаются блоками 5×5 клеток.
- В каждом блоке одна случайная клетка становится **водной**.
- Параметры биома и воды загружаются из `DataTable` через `FBiomeDefaults`.
## Сбор ресурсов (`HarvestFromCell`)
- Для воды возвращается `Cell->State` с учётом `ConditionModifier`.
- Для ресурса вызывается `FHerbalistHarvest::Harvest`, затем:
  - `ResourceRegrowthTimer = ResourceRegrowthTime`
  - `HarvestStress += HarvestStressIncrement`
  - Пересчёт `Distortion` и `Magnitude` клетки от стресса.
## Регенерация
- `ResourceRegrowthTimer` уменьшается в `Tick`.
- При достижении 0 — `RegenerateCellResource` (новый случайный ресурс).
## Интерполяция состояний
- `DirtyCells` — множество клеток, где `State != TargetState`.
- В `Tick` для каждой грязной клетки вызывается `InterpolateCell` (линейная интерполяция всех полей с `StateInterpolationSpeed`).
- После достижения целевого состояния клетка удаляется из `DirtyCells`.
- Параллельно обновляется `Memory` клетки.
## Стресс и восстановление
- `StressCells` — клетки с `HarvestStress > 0`.
- В `Tick` стресс снижается на `HarvestStressDecayRate * DeltaTime`.
- При изменении стресса пересчитывается `Distortion` и `Magnitude`.
## Распространение эффектов (`PropagateToNeighbors`)
- BFS с ограничением глубины `PropagationDepth`.
- Передаются только `Magnitude`, `Distortion`, `Corruption` с затуханием `Falloff`.
- Направление и остальные мета-параметры не распространяются.
## Отладка
- `bEnableDebugDraw` — отрисовка цветных боксов (синий = вода, от зелёного к красному = `AccumulatedDistortion`).