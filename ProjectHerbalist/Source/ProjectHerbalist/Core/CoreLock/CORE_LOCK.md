# CORE_LOCK.md

## 1. Статус

Core системы зафиксирован. Фазы 1-4 реализованы и протестированы.
Дальнейшие изменения вносятся только при обнаружении критических дефектов, подтверждённых тестами.

## 2. Область фиксации

### 2.1 Типы и базовые структуры

- `FDirection`, `FMeta`, `FRealState`
- `FEnvironment`, `FMemoryState` (включая `DistortionVelocity`, `TimeOfLastDistortionChange`)
- `FIntent`, `FRngState`
- `FAlchemyAtom`, `FContributionVector`, `EAtomOrigin`
- `EIngredientClass` (Water, Plant, Mineral, Fungus, Catalyst, Essence, Unknown)
- `FInventoryItem`

### 2.2 Данные

- `FIngredientRegistry` (data-only реестр ингредиентов)
- `FIngredientTableRow` (строка DataTable)

### 2.3 Pipeline

Фиксирован порядок операций:
1. `Fold`
2. `ComputeDelta`
3. `ApplyBiomeContext`
4. `ApplyMorok`
5. `ApplyZaryana`

Дополнительно:
- `ComputeIntentCoherence` (с учётом классов и GlobalDistortion)
- `ApplyDistortionDelta` (с saturation curve)
- `Perception::PerceiveValue`, `Perception::PerceiveClass`

### 2.4 BiomeState

- `UpdateMemory` через `ApplyDistortionDelta`
- `RecalculateDistortionFromHarvestStress` через `ApplyDistortionDelta`

### 2.5 Подсистемы

- `UAlchemySubsystem` (инициализация реестра)
- `UBiomeGraphSubsystem` (граф биомов)

---

## 3. Инварианты

### 3.1 Диапазоны

- `Magnitude ∈ [0,1]`
- Все `Meta ∈ [0,1]`
- `AccumulatedDistortion ∈ [0, 0.95]`
- `Coherence ∈ [0,1]`
- `DistortionVelocity` — без ограничений
- `Perceive(x, D) ∈ [x/K, x*K]`, `K = max(1.0, 1.0 + (D-0.3)*2.0)`

### 3.2 Direction

- Всегда нормализован (L2 или сумма)
- Не допускается нулевой вектор

### 3.3 Стабильность

- Нет NaN / Inf
- Нет деления на 0
- Все усреднения защищены от пустых массивов

---

## 4. Семантические гарантии

### 4.1 Saturation (Distortion)

- Рост `AccumulatedDistortion` замедляется при >0.8
- При >0.92 — почти плато (5% дельты)
- Непрерывность гарантирована `DistortionVelocity`

### 4.2 S_perceived

- Искажение мультипликативное, bounded
- Детерминированный seed
- Подмена класса вероятностная, не чаще 50%

### 4.3 Coherence

- Зависит от порядка, качества, классов, Distortion
- Catalyst повышает, Unknown понижает

---

## 5. Детерминизм

Система детерминирована при фиксированных:
- Seed
- Порядке ингредиентов
- GlobalDistortion

---

## 6. Реализованные фазы

| Фаза | Название | Ключевые файлы |
|---|---|---|
| 1 | Data-Only Foundation | `IngredientRegistry`, `AlchemyTypes`, `AlchemySemantics` |
| 2 | Operational FMemoryState | `AlchemyWorldStateApplier`, `GridWorldManagerCore` |
| 3 | S_perceived | `Perception`, `ItemTooltipWidget`, `HerbalistPlayerController` |
| 4 | Динамический Coherence | `IntentResolver`, `AlchemySemanticResolver`, `AlchemyTransferWidget` |