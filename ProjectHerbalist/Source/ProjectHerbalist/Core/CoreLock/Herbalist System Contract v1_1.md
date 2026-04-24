# Herbalist System Contract v1.1

**Дата:** 24.04.2026
**Статус:** Принят как reference для всех фаз реализации
**Версия:** 1.1 (добавлены Фазы 2-4, обновлены статусы)

---

## 1. Онтология системы

Все сущности в Herbalist принадлежат к одной из четырёх категорий. Нарушение категории при реализации = архитектурная ошибка.

| Категория | Определение | Способ существования | Примеры |
|---|---|---|---|
| **State** | Persistent snapshot | Хранится в памяти/сейвах | `FMemoryState::AccumulatedDistortion`, `FAlchemyAtom::AtomUID`, `TimeOfLastDistortionChange` |
| **Field** | Continuous function over space/time | Вычисляется при запросе, не хранится | `EffectiveDistortion(x,y)`, `Spoilage(t)`, `Perceive(RealValue, D)` |
| **Event** | Delta in time, дискретный переход | Происходит однократно, может оставлять след в State | `EAlchemyOutcome`, `CollapseTrigger`, `CleanseStart` |
| **Rule** | Transformation operator | Статическая функция без побочных эффектов | `ApplyMorok()`, `Classify()`, `ReduceModifiers()`, `ComputeCoherence()` |

---

## 2. Инварианты

Нарушение любого инварианта = баг, независимо от видимого поведения.

### I-1. Непрерывность Distortion
`FMemoryState::AccumulatedDistortion` никогда не меняется скачкообразно. Любое изменение происходит через `DistortionVelocity` и saturation curve. Коллапс, очищение, модификаторы — все влияют на скорость изменения, но не на значение напрямую. **Реализовано: Фаза 2.**

### I-2. Чистота семантического слоя
Семантический слой (`SemanticResolver`, `Perception`, `FIngredientRegistry`, `IntentResolver`) не вызывает UObject-систему. Ноль инклудов к `UHarvestService`, `UHarvestIngredientAsset`, `LoadObject`, `StaticLoadObject`. **Реализовано: Фаза 1.**

### I-3. Физический слой не пишет в FRealState
`PhysicsPipeline` производит `OutputData`. Только `WorldStateApplier` имеет право записывать в `FRealState`. Физический слой читает `FRealState` как read-only.

### I-4. FRealState как единый источник истины алхимического мира
Любое изменение, влияющее на алхимические процессы, выражается через изменение `FRealState`. Нет скрытых глобальных переменных, нет синглтонов с состоянием вне `FRealState`.

### I-5. Детерминизм при равных входных данных
`ApplyMorok(набор атомов, FRealState)` даёт одинаковый результат при одинаковых входных данных. Никакого внешнего random без фиксации seed в `FRealState`.

---

## 3. Derived vs Stored

### Правило
Всё, что зависит от времени — derived (вычисляется при запросе), не stored (не хранится как поле состояния).

### Хранится (State)
- `AccumulatedDistortion`
- `DistortionVelocity`
- `TimeOfLastDistortionChange`
- `AtomUID`, `TimeOfCreation`, `DistortionAtCollection`
- `CurrentGlobalDistortion` (в `HerbalistPlayerController`)

### Вычисляется (Field)
- `SpoilageStage(CurrentTime, Atom)` (Фаза 5)
- `EffectiveDistortion(Position)`
- `Perceive(RealValue, Distortion)` **Реализовано: Фаза 3.**
- `Coherence(Ingredients, Distortion)` **Реализовано: Фаза 4.**

---

## 4. Границы слоёв

| Слой | Читает | Пишет | Запрещено |
|---|---|---|---|
| **SemanticResolver** | `FRealState` (RO), `FIngredientRegistry`, `FAlchemyAtom[]` | Nothing | UObject, PhysicsPipeline, запись в FRealState |
| **IntentResolver** | `FAlchemyAtom[]`, `GlobalDistortion` | `Coherence` (float) | UObject |
| **Perception** | `RealValue`, `GlobalDistortion`, `Random` | `PerceivedValue` | UObject, запись состояния |
| **PhysicsPipeline** | `FRealState` (RO), `FAlchemyAtom[]` | `OutputData` | UObject, прямая запись в FRealState |
| **WorldStateApplier** | `FRealState` (RW), `OutputData` | `FRealState` (новое состояние) | UObject-манифестация |
| **WorldManifestor** | `FRealState` (RO), UE Assets | `AActor`, VFX, Materials, Engine UI | Запись в FRealState |
| **Engine UI** | `PerceivedStats` (из Perception), UE Assets | Widgets, экран | Прямой доступ к сырым атомам |

---

## 5. Unknown Handling

`EIngredientClass::Unknown` — валидный класс ингредиента.

- `FIngredientRegistry::Classify()` возвращает `Unknown` для всего, что не найдено в таблице.
- `FAlchemyAtom` с `Class == Unknown` **не отвергается**.
- Такой атом получает `ContributionVector = { Noise, Instability }`.
- `PhysicsPipeline` обрабатывает `Unknown` как источник энтропии.
- В `IntentResolver` Unknown снижает Coherence на 0.15 за каждый.

## 5.1. Семантика классов ингредиентов

| Класс | Источник | Алхимическая роль | Влияние на Coherence |
|---|---|---|---|
| Water | Водоёмы, родники | Растворитель, носитель | Бонус через Purity |
| Plant | Флора | Базовые эффекты | Нейтрально |
| Mineral | Порода, руды | Структура, длительность | Нейтрально |
| Fungus | Грибы, плесень | Энтропия, трансформация | Нейтрально |
| Catalyst | Редкие минералы/органика | Усиление реакций | +0.1 за каждый |
| Essence | Духи существ, ихор | Уникальные векторы | +0.05 за каждый |
| Unknown | Неизвестное/искажённое | Чистая энтропия | -0.15 за каждый |

---

## 6. FAlchemyAtom Identity Model

| Поле | Тип | Назначение |
|---|---|---|
| `AtomUID` | `FGuid` | Уникальный идентификатор |
| `SourceID` | `FName` | Имя ингредиента (ключ реестра) |
| `bIsWater` | `bool` | Является ли водой |
| `State` | `FRealState` | Алхимическое состояние |
| `Class` | `EIngredientClass` | Класс из реестра |
| `OriginContext` | `EAtomOrigin` | Harvest, Decomposition, Spawn, Unknown |
| `DistortionAtCollection` | `float` | Снимок Distortion в момент создания |
| `TimeOfCreation` | `float` | Игровое время создания |
| `ContributionVector` | `FContributionVector` | Вычисляется PhysicsPipeline |

---

## 7. FMemoryState Scope (Фиксация 1)

`FMemoryState` моделирует **память искажения клетки**.

### Входит в FMemoryState
- `AccumulatedDistortion` (float)
- `StabilityMemory` (float)
- `HistoryPurity` (float)
- `DistortionVelocity` (float) **— Фаза 2**
- `TimeOfLastDistortionChange` (float) **— Фаза 2**

### Не входит в FMemoryState
- Параметры инвентаря
- Координаты капищ, погода, сезоны
- Параметры сущностей (Entity)
- Флаги квестов

---

## 8. Modifier Reduction Contract (Фиксация 2) — отложено

`ReduceModifiers` будет реализован при добавлении капищ, погоды, сезонов (Фаза 6+).

---

## 9. S_perceived Bounds (Фиксация 3) — реализовано

### Гарантии для числовых параметров
1. `Perceive(x, 0.3) == x` — нет искажения на базовом Distortion.
2. `Perceive(x, D) ∈ [x / K, x * K]`, где `K = max(1.0, 1.0 + (D - 0.3) * M)`, `M = 2.0`.
3. Распределение шума — EaseInOut к центру.
4. Детерминированный seed: `Hash(IngredientID) ^ FloatToIntBits(D * 1000)`.

### Гарантии для качественных параметров (PerceivedClass)
5. `P(PerceivedClass != RealClass) = clamp((D - 0.5) * 1.5, 0.0, 0.5)`.
6. При `D = 0.3` вероятность подмены = 0.

---

## 10. Dynamic Coherence — реализовано

`ComputeIntentCoherence(Atoms, GlobalDistortion) -> float` учитывает:
- Порядок ингредиентов (веса с затуханием)
- Доминантные оси (AxisAgreement)
- Качество ингредиентов (Purity + Stability)
- Классы (Catalyst +0.1, Unknown -0.15, Essence +0.05)
- Глобальный Distortion (DistPen = 1 - D * 0.5)
- Бонус воды (Purity * 0.2)

---

## 11. Статус фаз

| Фаза | Название | Статус |
|---|---|---|
| 0 | System Contract | ✅ v1.1 |
| 1 | Data-Only Foundation | ✅ |
| 2 | Operational FMemoryState | ✅ |
| 3 | S_perceived | ✅ |
| 4 | Динамический Coherence | ✅ |
| 5 | Эволюция предметов | ⏳ |
| 6+ | Капища, погода, Entity | ⏳ |

---

## 12. Процедура принятия изменений

Любое изменение, противоречащее данному контракту, требует:
1. Явного обсуждения с обоснованием.
2. Фиксации изменения в новой версии контракта.
3. Проверки всех фаз на соответствие обновлённому контракту.

Контракт первичен. Код вторичен.