---
tags: [technical, current, systems]
gdd: "[[05_Systems]]"
version: 3.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 05. Игровые системы — техническая сторона

Пара к главе [[05_Systems|05. Игровые системы]]. У главы разделы без
номеров — здесь они названы так же («§5 Система сбора» — раздел «Система
сбора» главы). Поглотил прежние снимки ядра и инвентаря (Core_Current,
Inventory_Current, 2026-09-22); детерминированный пайплайн тика — в
[[13_World_Pipeline_Tech]], граф биомов — в [[14_Biome_Graph_Tech]], вся
непрерывная математика — `docs/reference/MATH_REFERENCE.md`. Летопись —
`CHANGELOG.md`.

## §5 Параметрическая модель

Все вещи мира — одно пространство параметров (`Core/Types/HerbalistCoreTypes.h`):

    struct FRealState  { float Magnitude; FDirection Direction; FMeta Meta; };
    struct FDirection  { float Body, Mind, Spirit, Nature; };   // L1: сумма = 1
    struct FMeta       { float Distortion, Stability, Purity, Potency, Resonance, Corruption; };
    struct FEnvironment{ float Toxicity, Fertility, Moisture; };
    struct FMemoryState{ float AccumulatedDistortion, StabilityMemory, HistoryPurity,
                         DistortionVelocity, TimeOfLastDistortionChange, AverageCoherence;
                         bool bDegrading; };

- **Нормализация** — `FDirection::NormalizeSum()`: отрицательные в ноль,
  деление на сумму, вырожденный ноль → по 0,25. L2-вариант (`FL2Direction`)
  — частный случай, не путь по умолчанию.
- **`bDegrading`** — гистерезис бистабильной релаксации клетки: порча
  прошла порог входа, цель релаксации сдвинута к испорченному полюсу,
  пассивное восстановление не работает, пока клетку не вычистят
  (`RegenerateCellParameters`, `GridWorldManagerCore.cpp`; глава
  [[12_Biome_Change]] §12.10).
- **Алатырь S₀** — `FAlatyr::S0`: Magnitude 1, оси по 0,25, Stability,
  Purity, Potency, Resonance — 1, Distortion и Corruption — 0. Не база сбора
  (сбор берёт `BaseState` карточки и биом клетки), а недостижимый ориентир:
  `HerbalistCore::Math::Distance(State, FAlatyr::S0)` и путь-зависимая
  `DistanceWithHistory` (× `Clamp(2 − AverageCoherence, 1, 2)`) — метрика
  мира Заряны и Буяна (глава [[15_Cycles_And_Shrines]] §15.5.1).
- **Случайность** — всегда `FRandomStream`: общий `AGridWorldManager::WorldRNG`
  (сид `RngBaseSeed`) идёт в пайплайн явным параметром; презентационный шум
  (восприятие, джиттер) — на отдельных локальных потоках, общий сид не
  двигают. Старый ЛКГ `FRngState` остался только у `FL2Direction::NormalizeL2`.

## §5 Система сбора

- Путь: взгляд и `HarvestAction` → `AHerbalistPlayerController::Harvest`
  (луч `ECC_Visibility`, затем `ECC_GameTraceChannel1` — канал ресурсов) →
  команда `Harvest` в очередь менеджера → `ProcessHarvestCommand` /
  `GenerateHarvestResult` (`Core/Simulation/Private/PipelineV2.cpp`):
  `BaseState` карточки, сдвинутый состоянием клетки и её `Resilience`,
  инструментом (`EGatheringTool`, железо слабит «железобоязненные»
  `bIronAverse`) и намерением (`EHarvestIntent::Seed` — посадочный
  материал `bIsPlantingStock`).
- Ресурс в мире — `AHerbalistResourceActor` (меш — `ResourceMesh` ряда
  карточки или Blueprint-класса). Плотность задаётся на площадь, не на
  клетку (разметка мира, [[13_World_Pipeline_Tech]] §13.19).
- Инструмент и намерение надеваются на пояс ([[07_UX_Tech]] §7.13.5);
  отладка — `SetGatheringTool`, `SetHarvestIntent`, `HarvestHere`.
- Проверка: `docs/verification/pie/02_Harvest_Inventory.md`.

## §5 Система ресурсов

- Карточки — `herbalist_docs/Herbalist_Vault/04_Compendium/`, их отражение —
  `CSV_tabs/ingredients.json` (`tools/data_extraction/extract_ingredients.py`),
  живая таблица — `DT_IngredientClass` (`FIngredientTableRow`,
  `Core/Data/IngredientTableRow.h`), реестр в игре —
  `UIngredientRegistrySubsystem`. Порядок правды: карточка → json → ассет.
- Новые ряды — точечными `*Append`-коммандлетами (`AddRow`); **не**
  `-run=IngredientAppend` — тот проходит всю таблицу через JSON и портит ряды
  с пробелом в имени (`docs/reference/TOOLS_REFERENCE.md`).
- `RowName` трав — короткий код (`riv_06`), найденных и сделанных вещей —
  само имя по-русски («Костяной нож», «Хворост»).

## §5 Инвентарь и эволюция состояния вне мира

### Компонент

`UHerbalistInventoryComponent` (`Core/Inventory/`): `TArray<FInventoryItem>`,
`MaxSlots` 20, `MAX_STACK_SIZE` 9. `ContainerType` (`EStorageContainerType`)
даёт только множитель порчи: `Sack` 1,4 > `Basket` 1,3 > `None` 1,0 >
`Tues` 0,85 > `Cabinet` 0,7 > `Cellar` 0,4 > `Jar` 0,25 (числа —
`UHerbalistSettings`). Переносные (корзина, мешок, туёс) надеваются на пояс,
стационарные строятся у дома (`BuildHomeStorage`, глава [[17_Hero_And_Community]]).
`StationType` (`EProcessingStationType`) включает процессы станций.

### Предмет

`FInventoryItem`: `IngredientID`, `State` (уже воспринятое при сборе),
`Count`, `CreationTime`, `bSubjectToDecay`, `bIsWater`, `BrewOutcome`,
`bIsPlantingStock`, `SourceBiome`, три независимые пары процессов станций —
сушка (`bIsDried`/`DryingTimeRemainingSeconds`), отстой
(`bHasSettled`/…), выпаривание (`bHasEvaporated`/…). Таймер процесса идёт,
только пока предмет лежит в своей станции.

### Стопки и поиск

- `AreItemsStackable`: один `IngredientID`, одинаковые `bIsPlantingStock`,
  сушка, отстой, выпаривание, никто не в процессе, и
  `HerbalistCore::Math::AreStatesSimilar`. Слияние — взвешенное среднее
  (`BlendRealStatesForStack`), `CreationTime` тоже смешивается.
- `FindItemIndex(Snapshot, Hint)` — где сейчас предмет, который кто-то
  запомнил (рука, пестерь, тайник): подсказанная ячейка → имя и время
  создания → имя и близкое состояние.
- Операции: `AddItem`, `RemoveItem`, `TransferItemTo` (одна штука),
  `SplitStack`, `GetAvailableCapacityFor` (место до списания).

### Порча и перегной

Порча — множитель `InventoryDecayRate` × `DecayRate` карточки × контейнер;
утварь и артефакты (`DecayRate` 0, `bSubjectToDecay` false) не портятся.
Сгнившее до конца становится перегноем (`PeregnoyIngredientID`).

### Представление

Числа игроку не показываются: предмет виден формой и цветом
([[07_UX_Tech]] §7.2.3), осматривается строкой ощущения. Под сильным Мороком
(`PerceiveClassDistortionThreshold` 0,5, `PerceiveClassMaxChance` 0,5) вид
подменяется похожим видом того же биома (`PerceiveClass`,
`docs/DECISIONS_LOG.md` решение №2); утварь двойником не бывает.

## §5 Система преобразования (Алхимия)

- **Вход в игре** — котёл из руки ([[07_UX_Tech]] §7.13.4): закладка по
  порции, «помешать» → `AGridWorldManager::QueueCauldronBrew` →
  `BuildCauldronBrewCommand` (команда `Apply` с `bIsCrafting`,
  `bIngredientsAlreadyWithdrawn`) и `ResolveBrewModifiers` (Камень-оберег,
  фаза луны, BrewBoost, межбиомность, тиражный оберег). Результат приходит в
  котомку следующим тиком, рассылка — `OnBrewCompleted` (с клеткой котла).
- **Расчёт** — `ComputeApplyResult` (`PipelineV2.cpp`): свёртка по порядку
  (вес первого максимален), Coherence — `ComputeIntentCoherence` из самих
  ингредиентов (то, что кладёт вызывающий, перезаписывается), контекст биома
  (Biome Context Injection ниже), вода отдельно, нормализация, Морок и
  Заряна по аффинити биома, бифуркация. Вырожденные исходы (`EAlchemyOutcome`):
  зола без воды, кипячёная вода без трав, катастрофа, очищенное.
- **Ритуалы** — `TryAdvanceRitual` (`GridWorldManagerRitual.cpp`,
  `Core/Alchemy/RitualTypes.h`): шаг — нужные травы, вода и час; прогресс на
  клетке котла (`ActiveRituals`). В игре — то же «помешать».
- **Многоступенчатая варка** — станции отстоя и выпаривания (§5 Инвентарь),
  фильтрация — `TryFilterPotion` (команда `FilterPotion`).
- **Передозировка** — выше `PotionOverdoseThreshold` (0,75) зелье,
  применённое к клетке, тянет Stability и Purity вниз, Distortion и
  Corruption вверх; при варке не действует.
- Проверка: `docs/verification/pie/03_Alchemy_Stations.md`.

## §5 Применение зелий к объектам мира

Зелье из руки на землю — `PourPotionOnCell` → `ApplyPotionToCell` →
`ApplyAlchemyResult` (команда `Apply` на клетку); на клетку хозяина — это и
есть подношение ему (Respect). Отладка — `UsePotion`, `TestNewApply`.

## §5 Biome Context Injection

Контекст биома подмешивается в `ComputeApplyResult` из полей узла графа
(`MorokField`, `ZaryanaField`, `Memory.AxisDrift`), снимок — `FBiomeSnapshot`;
отдельной функции разрешения контекста нет. Граф — [[14_Biome_Graph_Tech]].
