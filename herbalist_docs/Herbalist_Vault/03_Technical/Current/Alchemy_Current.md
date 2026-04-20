---
tags: [technical, current, alchemy]
---
# Алхимия — реализованный пайплайн
## Основной метод
`HerbalistCore::Pipeline::ApplyMorok` принимает:
- `TArray<FInventoryItem> Ingredients`
- `CurrentBiomeState`
- `Environment`
- `MemoryState`
- `Intent`
- `RngState`
Возвращает новое `FRealState` — результат варки.
## Этапы
### 1. Разделение воды и не-воды
- Вода: `Type == EResourceType::Water`
- Остальное — ингредиенты.
### 2. Только вода → варёная вода
- Усреднение всех `WaterStates`.
- `Magnitude *= 0.8f`
- `Purity += 0.2f`, `Stability += 0.1f`
- `Distortion -= 0.2f`, `Corruption -= 0.1f`
- Направление слегка сдвигается к центру (0.25).
### 3. Нет воды → зола
- `Magnitude = 0.1f`
- `Distortion = 0.9f`, `Corruption = 0.9f`
- `Direction = (0.25, 0.25, 0.25, 0.25)`
### 4. Смесь вода + ингредиенты
#### a) `Fold` для не-водных
- Вес первого ингредиента = 1.0, каждого следующего = предыдущий * 0.8.
- Взвешенное среднее всех параметров.
- Нормализация `Direction` по сумме.
#### b) Агрегация воды
- Простое среднее для `Magnitude`, `Direction`, `Purity`, `Stability`, `Corruption`, `Potency`.
#### c) Расчёт доли воды
`WaterRatio = WaterVolume / (WaterVolume + NonWaterVolume)`
`EffectiveWaterRatio = min(WaterRatio, 0.8f)`
`DilutionPenalty = (WaterRatio > 0.8f) ? 0.2f : 1.0f`
#### d) Смешивание
- `Magnitude` не-воды умножается на `(1 - EffectiveWaterRatio * 0.8) * DilutionPenalty`.
- `Direction`, `Purity`, `Stability`, `Corruption`, `Potency` смешиваются с весами `NonWaterWeight` и `WaterWeight`.
- `Resonance` и `Distortion` берутся **только от не-воды**.
### 5. `ComputeDelta`
Разность между агрегированным состоянием и текущим состоянием биома.
### 6. Модификаторы
- `PotencyScale = 1 + (Potency - 0.5) * 0.5` — умножает все компоненты `Delta`.
- `ResonanceFactor = 1 + Resonance * 0.5` — усиливает положительные компоненты `Direction`.
- `Corruption` добавляет к `Distortion` и снижает `Stability`/`Purity`.
### 7. Morok (нелинейное искажение)
- `Distortion` вычисляется как `1 - (1 - Toxicity*0.5) * (1 - AccumulatedDistortion)`.
- Генерация шума, `tanh`, обмен осями (Body ↔ Spirit, Mind ↔ Nature).
- `Magnitude` искажается на `±20% * Nonlinear`.
### 8. Zaryana (структурирование)
- `ZaryanaStrength = Coherence * (1 - Distortion)`.
- Усиление компонент `Direction` выше среднего, подавление ниже среднего.
- Увеличение `Stability` и `Purity` на величину, зависящую от `ZaryanaStrength`.
### 9. Применение `Delta` к состоянию биома
- Интерполяция направления с весом `Delta.Magnitude * 2`.
- Клиппинг всех значений в [0,1].
- Финальный множитель `StructureFactor = (1 - Distortion*0.7) * (1 + Stability)`.
## Бифуркация (в `GridWorldManager::ApplyAlchemyResult`)
Если `NewState.Distortion > 0.85`, с вероятностью:
- **Collapse:** `Distortion = 0.2`, `Stability += 0.1`.
- **Purification:** `Distortion = 0.4`, `Stability` и `Purity` увеличиваются.