# Herbalist System Contract v1.0

**Дата:** 24.04.2026
**Статус:** Принят как reference для всех фаз реализации
**Назначение:** Зафиксировать незыблемые границы, инварианты и контракты системы перед началом кодирования

---

## 1. Онтология системы

Все сущности в Herbalist принадлежат к одной из четырёх категорий. Нарушение категории при реализации = архитектурная ошибка.

| Категория | Определение | Способ существования | Примеры |
|---|---|---|---|
| **State** | Persistent snapshot | Хранится в памяти/сейвах | `FRealState::Distortion`, `FAlchemyAtom::AtomUID`, `TimeOfLastDistortionChange` |
| **Field** | Continuous function over space/time | Вычисляется при запросе, не хранится | `EffectiveDistortion(x,y)`, `Spoilage(t)`, `Perceive(RealValue, D)` |
| **Event** | Delta in time, дискретный переход | Происходит однократно, может оставлять след в State | `EAlchemyOutcome`, `CollapseTrigger`, `CleanseStart` |
| **Rule** | Transformation operator | Статическая функция без побочных эффектов | `ApplyMorok()`, `Classify()`, `ReduceModifiers()`, `ComputeCoherence()` |

---

## 2. Инварианты

Нарушение любого инварианта = баг, независимо от видимого поведения.

### I-1. Непрерывность Distortion
`FRealState::Distortion` никогда не меняется скачкообразно. Любое изменение происходит через `DistortionVelocity`, интегрируемую по времени. Коллапс, очищение, модификаторы — все влияют на `DistortionVelocity`, но не на `Distortion` напрямую.

### I-2. Чистота семантического слоя
Семантический слой (`SemanticResolver`, `SPerceived`, `FIngredientRegistry`, классификаторы) не вызывает UObject-систему. Ноль инклудов к `UHarvestService`, `UHarvestIngredientAsset`, `LoadObject`, `StaticLoadObject`.

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
- `Distortion`
- `DistortionVelocity`
- `TimeOfLastDistortionChange`
- `AtomUID`, `TimeOfCreation`, `DistortionAtCollection`
- `ActiveModifiers` (множество источников)

### Вычисляется (Field)
- `SpoilageStage(CurrentTime, Atom)`
- `EffectiveDistortion(Position)`
- `Perceive(RealValue, Distortion)`
- `Coherence(Ingredients, Distortion)`

---

## 4. Границы слоёв

| Слой | Читает | Пишет | Запрещено |
|---|---|---|---|
| **SemanticResolver** | `FRealState` (RO), `FIngredientRegistry`, `FAlchemyAtom[]` | Nothing | UObject, PhysicsPipeline, запись в FRealState |
| **PhysicsPipeline** | `FRealState` (RO), `FAlchemyAtom[]` | `OutputData` | UObject, прямая запись в FRealState |
| **WorldStateApplier** | `FRealState` (RW), `OutputData`, `FDistortionModifier` | `FRealState` (новое состояние) | UObject-манифестация, прямая модификация атомов |
| **WorldManifestor** | `FRealState` (RO), UE Assets | `AActor`, VFX, Materials, Engine UI | Запись в FRealState, изменение алхимических данных |
| **Engine UI** | `FPerceivedStats` (из SPerceived), UE Assets | Widgets, экран | Прямой доступ к сырым атомам, минуя SPerceived |

---

## 5. Unknown Handling

`EIngredientClass::Unknown` — валидный класс ингредиента.

- `FIngredientRegistry::Classify()` возвращает `Unknown` для всего, что не найдено в таблице.
- `FAlchemyAtom` с `Class == Unknown` **не отвергается**.
- Такой атом получает `ContributionVector = { Noise, Instability }`.
- `PhysicsPipeline` обрабатывает `Unknown` как источник энтропии, а не как ошибку.
- `WorldStateApplier` не имеет специальной логики для отбрасывания `Unknown`.

## 5.1. Семантика классов ингредиентов

| Класс | Источник | Алхимическая роль | ContributionVector |
|---|---|---|---|
| Water | Водоёмы, родники | Растворитель, носитель | Stability↑, Potency↓ |
| Plant | Флора | Базовые эффекты | Potency↑, Stability→ |
| Mineral | Порода, руды | Структура, длительность | Stability↑↑, Potency↓ |
| Fungus | Грибы, плесень | Энтропия, трансформация | Instability↑, Noise↑ |
| Catalyst | Редкие минералы/органика | Усиление, ускорение реакций | Potency↑↑, Instability→ |
| Essence | Духи существ, ихор | Уникальные векторы | Instability↑↑, Signature зависит от существа |
| Unknown | Неизвестное/искажённое | Чистая энтропия | Noise↑↑, Instability↑↑ |

---

## 6. FAlchemyAtom Identity Model

Каждый атом обладает следующими неотъемлемыми свойствами:

| Поле | Тип | Назначение |
|---|---|---|
| `AtomUID` | `FGuid` | Уникальный идентификатор, присваивается при создании |
| `IngredientName` | `FName` | Имя ингредиента |
| `Class` | `EIngredientClass` | Класс из реестра (может быть Unknown) |
| `OriginContext` | `EAtomOrigin` | Harvest, Decomposition, Spawn, Unknown |
| `DistortionAtCollection` | `float` | Снимок `FRealState::Distortion` в момент создания |
| `TimeOfCreation` | `float` | Игровое время создания (для derived-вычислений) |
| `ContributionVector` | `FContributionVector` | Вычисляется PhysicsPipeline на основе Class и DistortionAtCollection |

---

## 7. FRealState Scope (Фиксация 1)

`FRealState` моделирует **исключительно алхимическое состояние**. Он не является God Struct.

### Входит в FRealState
- `Distortion` (float, 0.3 по умолчанию)
- `DistortionVelocity` (float, 0.0 по умолчанию)
- `TimeOfLastDistortionChange` (float)
- `ActiveModifiers` (`TArray<FDistortionModifier>`)

### Не входит в FRealState
- Список ингредиентов в инвентаре
- Координаты капищ, параметры погоды, сезоны
- Параметры сущностей (Entity)
- Таймеры порчи отдельных предметов
- Флаги квестов, сюжетные переменные

### Правило взаимодействия
Погода, капище, сезон — это системы, которые **генерируют** `FDistortionModifier` и передают его в `WorldStateApplier`. Сами они живут вне `FRealState`.

---

## 8. Modifier Reduction Contract (Фиксация 2)

`FRealState::ActiveModifiers` — неупорядоченное множество источников искажения.

`ReduceModifiers(TArray<FDistortionModifier>) -> float` — единственная функция, вычисляющая итоговый модификатор.

### Гарантии ReduceModifiers
1. **Bounded output**: результат всегда в диапазоне `[MinModifierBound, MaxModifierBound]`.
2. **Идемпотентность по дубликатам**: добавление идентичного модификатора не удваивает эффект.
3. **Нейтральность пустого множества**: `ReduceModifiers([]) == 1.0`.
4. **Неаддитивность**: `ReduceModifiers` не является суммой значений. Конкретный алгоритм фиксируется в Фазе 2.

### Структура FDistortionModifier
- `Source` (enum: Biome, Weather, Shrine, Item, Entity)
- `Priority` (int)
- `Value` (float)
- `ExpiryTime` (float, 0 = бессрочный)

---

## 9. S_perceived Bounds (Фиксация 3)

`SPerceived::Perceive(RealValue, Distortion) -> PerceivedValue` — чистая функция искажения восприятия.

### Гарантии для числовых параметров
1. **Нет искажения на базовом Distortion**: `Perceive(x, 0.3) == x`.
2. **Мультипликативные границы**: `Perceive(x, D) ∈ [x / K, x * K]`, где `K = 1 + (D - 0.3) * M`, `M ≤ 3.0`.
3. **Монотонность**: при равных входных данных искажение монотонно растёт с Distortion.

### Гарантии для качественных параметров (PerceivedClass)
4. **Вероятностная подмена класса**: `P(PerceivedClass != RealClass) ≤ D²`, но не более 0.5.
5. **Возможность прорыва к истине**: при `D = 0.3` вероятность подмены = 0. Игрок всегда может снизить Distortion до базового, чтобы видеть истину.

---

## 10. Процедура принятия изменений

Любое изменение, противоречащее данному контракту, требует:
1. Явного обсуждения с обоснованием.
2. Фиксации изменения в новой версии контракта.
3. Проверки всех фаз на соответствие обновлённому контракту.

Контракт первичен. Код вторичен.

---