#!/usr/bin/env python3
# apply_biome_graph_updates.py
# Обновление документации Herbalist под Biome Graph v1
# Запускать из корня Herbalist_Vault

import os
import re
import shutil
from datetime import datetime

# ==================== КОНФИГУРАЦИЯ ====================
BACKUP_DIR = "00_Meta/Backups"

# Файлы, которые будут ПОЛНОСТЬЮ ЗАМЕНЕНЫ (новая версия)
FULL_REPLACE = {
    "02_GDD/14_Biome_Graph.md": "GDD_14_BIOME_GRAPH",
    "03_Technical/Future/BiomeGraph_Technical.md": "TECH_BIOMEGRAPH_FINAL",
}

# Файлы, в которые нужно ДОБАВИТЬ секции в конец (с проверкой на существование)
APPEND_SECTIONS = {
    "02_GDD/05_Systems.md": "SYSTEMS_ADDITION",
    "02_GDD/12_Biome_Change.md": "BIOME_CHANGE_ADDITION",
    "02_GDD/13_World_Pipeline.md": "WORLD_PIPELINE_ADDITION",
    "01_Glossary/_Index.md": "GLOSSARY_ADDITIONS",
}

# ==================== НОВОЕ СОДЕРЖАНИЕ ====================
GDD_14_BIOME_GRAPH = """---
tags: [gdd, biomes, graph, core]
status: final
---

# 14. Биомный граф и экосистемная симуляция

## 14.1 Биом как оператор контекста

В архитектуре **Herbalist** биом — это не фон и не набор ресурсов. Это **постоянный оператор контекста**, который:
- влияет на входной вектор ингредиентов
- задаёт «сдвиг базиса» трансформации
- определяет допустимую степень искажения [[Morok]]
- ограничивает или усиливает стабилизацию [[Zaryana]]

Формально биом описывается структурой `FBiomeGraphNode` (см. [[03_Technical/Future/BiomeGraph_Technical|техническую спецификацию]]).

## 14.2 Мир как граф состояний

Мир представлен как ориентированный взвешенный граф:
- **Узлы** — биомы с параметрами (`MorokAffinity`, `ZaryanaAffinity`, `Stability`) и runtime-полями (`MorokField`, `ZaryanaField`, `Memory`).
- **Рёбра** — направленные связи с коэффициентами утечки (`MorokLeak`, `ZaryanaFlow`).

## 14.3 Влияние биома на пайплайн трансформации

Биом встраивается в цепочку трансформации как **контекстный слой ДО Morok, но ПОСЛЕ базового маппинга**:

```
Input → Axis Mapping → Biome Context Injection → Morok → Zaryana → Output
```

Эффективный контекст формируется из полей узла (`MorokField`, `ZaryanaField`) и памяти (`Memory.AxisDrift`).

## 14.4 Распространение Morok и Zaryana (Wave Propagation)

Morok и Zaryana — не глобальные коэффициенты, а **волны влияния, распространяющиеся по графу** с фиксированным временным шагом.

Алгоритм:
1. **Grid → Graph:** поля узлов агрегируются из состояния клеток (`FGridBiomeSample`).
2. **Propagation:** на основе snapshot'а вычисляются дельты и применяются к соседним узлам.
3. **Graph → Grid:** поля узлов влияют на `TargetState` клеток (мягкая интерполяция).

## 14.5 Память биома (Biome Memory)

Каждый узел хранит историю воздействий:
- `MorokHistory`, `ZaryanaHistory` — накопленное влияние.
- `Instability` — общая нестабильность.
- `AxisDrift` — дрейф осей (Body, Mind, Spirit, Nature).

Память затухает со временем (decay), но сохраняет долгосрочные последствия действий игрока.

## 14.6 След игрока (Footprint)

Каждое применение алхимии к клетке оставляет след в биоме:
- `MorokImpact = Delta.Meta.Distortion`
- `ZaryanaImpact = 1 - Delta.Meta.Distortion`
- `AxisDelta` — вектор изменения осей.

Footprint обновляет память узла и влияет на будущие состояния.

## 14.7 Циклы коллапса и возрождения (зарезервировано)

При достижении порога `CollapseThreshold` биом может перейти в состояние коллапса с последующим возрождением. В текущей версии (v1) механизм зарезервирован, но не активирован.

## 14.8 Связь с существующими системами

| Система | Роль в Biome Graph |
|---------|-------------------|
| [[GridWorldManager]] | Источник истины (состояние клеток). Предоставляет `GetBiomeSamples()`. |
| [[Alchemy]] | Вызывает `RecordFootprint` после применения результата. |
| [[Harvest]] | Влияет на `HarvestStress` клетки, что косвенно меняет `Distortion`. |
| [[MemoryState]] (клетки) | Отдельная память клетки; не путать с памятью биома. |

## 14.9 Статус реализации (v1 freeze)

| Компонент | Статус |
|-----------|--------|
| Biome Graph Subsystem | ✅ Реализовано |
| Grid ↔ Graph Aggregation | ✅ Реализовано |
| Footprint Recording | ✅ Реализовано |
| Wave Propagation | ✅ Реализовано |
| Memory Decay | ✅ Реализовано |
| Collapse/Rebirth | ❌ Зарезервировано |
| Visualization (Debug) | ✅ Базовая |

## 14.10 Приоритеты для Vertical Slice

1. Статический граф из 3+ биомов.
2. Footprint от алхимии.
3. Визуализация полей через консольные команды.

Остальное — для полной версии.

---

> **Техническая документация:** [[03_Technical/Future/BiomeGraph_Technical|Biome Graph — Техническая спецификация]]
"""

TECH_BIOMEGRAPH_FINAL = """---
tags: [technical, biomes, graph, final]
status: implemented
---

# Biome Graph — Техническая спецификация (v1)

## Обзор

Biome Graph — подсистема, реализующая мир как граф взаимосвязанных биомов с распространением влияний, памятью и следами игрока. Внедрена в `UBiomeGraphSubsystem`.

## Архитектура

```
UBiomeGraphSubsystem : public UWorldSubsystem
├── TMap<FName, FBiomeGraphNode> Nodes   // runtime-узлы
├── TArray<FBiomeGraphEdge> Edges        // рёбра
├── TMap<FName, TArray<int32>> AdjacencyList
├── TMap<FName, FVector> CachedBiomeCenters
└── float TimeAccumulator (для фиксированного шага)
```

## Ключевые структуры (см. `BiomeGraphTypes.h`)

- `FBiomeGraphNode` – авторские параметры + runtime-поля (`MorokField`, `ZaryanaField`) + `FBiomeMemory`.
- `FBiomeGraphEdge` – `FromBiome`, `ToBiome`, `MorokLeak`, `ZaryanaFlow`.
- `FBiomeGraphNodeEntry` – запись для DataAsset (связь `BiomeID` → `FBiomeGraphNode`).
- `FGridBiomeSample` – контракт для передачи данных из Grid в Graph.

## Инициализация

1. `UBiomeGraphAsset` загружается в `GameMode::BeginPlay`.
2. `InitializeFromAsset` преобразует `TArray<FBiomeGraphNodeEntry>` в `TMap<FName, FBiomeGraphNode>`.
3. Строится `AdjacencyList`.

## Tick-модель

- **Фиксированный шаг:** `FixedTimeStep` (по умолчанию 0.2 сек).
- Внутренний цикл `InternalStep`:
  1. `RecalculateFieldsFromGrid` – агрегация `FGridBiomeSample` от `AGridWorldManager`.
  2. `PropagateWaves` – delta-based распространение.
  3. `ApplyFieldsToGrid` – вызов `AGridWorldManager::ApplyBiomeInfluences`.
  4. `UpdateMemories` – decay памяти.

## Контракт с GridWorldManager

- `GetBiomeSamples()` – возвращает массив `FGridBiomeSample` для всех клеток.
- `ApplyBiomeInfluences(MorokFields, ZaryanaFields, GlobalScale)` – применяет поля к `TargetState` клеток.
- `GetBiomeCenters()` – возвращает средние позиции биомов (для визуализации).

## Footprint

Вызывается из `AGridWorldManager::ApplyAlchemyResult`:
```cpp
Graph->RecordFootprint(BiomeID, Delta.Meta.Distortion, 1-Delta.Meta.Distortion, AxisDelta, 1.0f);
```

Обновляет Memory узла.

## Консольные команды

- `Herbalist.Graph.Print` – состояние всех узлов.
- `Herbalist.Graph.Step` – принудительный шаг симуляции.
- `Herbalist.Graph.Reset` – сброс полей и памяти.
- `Herbalist.Graph.ToggleVis` – визуализация графа (Editor).
- `Herbalist.Debug.ToggleCellDistortion` – оверлей Distortion на клетках.

## Статус

✅ Реализовано и протестировано в PIE.
❌ Collapse/Rebirth – зарезервировано.
⚠️ Визуализация требует калибровки координат.
"""

SYSTEMS_ADDITION = """
### Biome Context Injection

Перед применением [[Morok]] к агрегированному состоянию ингредиентов добавляется **контекст биома**:

- `MorokField` узла увеличивает эффективную силу [[Morok]].
- `ZaryanaField` усиливает стабилизацию [[Zaryana]].
- `Memory.AxisDrift` добавляет смещение к осям [[Direction]].

Контекст вычисляется в `UBiomeGraphSubsystem::ResolveContext()` (в v1 используется напрямую `MorokField`/`ZaryanaField`).
"""

BIOME_CHANGE_ADDITION = """
## 12.14 Влияние через Biome Graph

Изменение биомов теперь происходит не только через непосредственное воздействие на клетки, но и через **граф влияний** ([[14_Biome_Graph]]). 
Алхимические воздействия оставляют Footprint в узлах графа, которые затем распространяются по рёбрам и возвращаются в Grid, создавая системную обратную связь.

> **Важно:** прямое изменение `TargetState` клеток через `ApplyAlchemyResult` сохраняется, но дополняется медленной эволюцией через граф.
"""

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
Footprint Recording (след в графе)
↓
Wave Propagation (распространение Morok/Zaryana по графу)
↓
Memory Accumulation (обновление истории биома)
↓
Применение полей к Grid (ApplyBiomeInfluences)
↓
Новое состояние графа и клеток
↓
Новое [[S_perceived]] для игрока
```

Этот цикл реализован в подсистеме `UBiomeGraphSubsystem` с фиксированным временным шагом (0.2 сек).
"""

GLOSSARY_ADDITIONS = """
## Biome Graph (раздел, добавленный автоматически)

- [[Biome Graph]]
- [[Morok Field]]
- [[Zaryana Field]]
- [[Footprint]]
- [[Propagation]]
- [[Biome Memory]]
- [[GridBiomeSample]]
"""

# ==================== ФУНКЦИИ ====================
def backup_file(filepath):
    if not os.path.exists(filepath):
        return None
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_path = os.path.join(BACKUP_DIR, timestamp)
    os.makedirs(backup_path, exist_ok=True)
    dest = os.path.join(backup_path, os.path.basename(filepath))
    shutil.copy2(filepath, dest)
    print(f"  📦 Backup: {filepath} → {dest}")
    return dest

def read_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()

def write_file(filepath, content):
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"  ✅ Обновлён: {filepath}")

def full_replace(filepath, new_content):
    if not os.path.exists(filepath):
        print(f"  ❌ Файл не найден: {filepath}")
        return False
    backup_file(filepath)
    write_file(filepath, new_content)
    return True

def append_section(filepath, section_content, marker_comment="<!-- biome_graph_section -->"):
    """
    Добавляет секцию в конец файла, но только если её там ещё нет.
    Проверка происходит по наличию маркера в section_content (первая строка или маркер).
    Если маркер уже есть в файле – пропускаем.
    """
    if not os.path.exists(filepath):
        print(f"  ❌ Файл не найден: {filepath}")
        return False

    current = read_file(filepath)
    # Используем первые 50 символов секции как уникальный идентификатор
    # (можно заменить на маркер, но проще так)
    sample = section_content.strip()[:50]
    if sample in current:
        print(f"  ⏭️ Секция уже присутствует в {filepath}, пропускаем.")
        return False

    backup_file(filepath)
    # Добавляем разделитель, если в конце файла нет пустой строки
    if not current.endswith('\n\n'):
        current += '\n\n'
    new_content = current + section_content + '\n'
    write_file(filepath, new_content)
    return True

# ==================== ГЛАВНАЯ ====================
def main():
    print("=" * 60)
    print("ОБНОВЛЕНИЕ ДОКУМЕНТАЦИИ ДЛЯ BIOME GRAPH v1")
    print("=" * 60)

    # 1. Полная замена
    for filepath, var_name in FULL_REPLACE.items():
        print(f"\n📄 Обработка {filepath} (полная замена)")
        content = globals()[var_name]  # получаем строку по имени переменной
        full_replace(filepath, content)

    # 2. Добавление секций
    for filepath, var_name in APPEND_SECTIONS.items():
        print(f"\n📄 Обработка {filepath} (добавление секции)")
        content = globals()[var_name]
        append_section(filepath, content)

    print("\n✅ Готово. Резервные копии в", BACKUP_DIR)

if __name__ == "__main__":
    main()