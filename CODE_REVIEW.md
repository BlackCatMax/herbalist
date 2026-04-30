# Полный разбор архитектуры ProjectHerbalist (UE5)

## Цель разбора
Ниже — архитектурный разбор с практическими вариантами реализации под **UE5**, с акцентом на **data-driven**, **event-driven** и **PCG** подходы.

---

## 1) Текущее состояние архитектуры (as-is)


## Ограничения и продуктовые рамки
- **Мультиплеера нет**: архитектура оптимизируется под single-player (без net replication, server authority, rollback netcode).
- Все новые механики должны **переиспользовать текущее ядро** (snapshot/commands/registry/inventory), а не дублировать его.
- Новые фичи не должны перекрывать текущие системы; они добавляются как отдельные bounded-context модули с адаптерами.

## 1.1 Слои и модули
- **Core**: доменные типы (`FInventoryItem`, состояния, биомы), world logic, harvest, storage.
- **Simulation**: пайплайн команд (`PipelineV2`), snapshot/delta/trace/replay.
- **Subsystems**: реестры ингредиентов/воды, алкемия.
- **UI/Player**: контроллер игрока, виджеты инвентаря/алхимии, drag&drop.
- **Tests**: базовые unit/functional тесты для registry/pipeline.

## 1.2 Что уже хорошо
- Есть ядро детерминированной симуляции (snapshot/delta/replay) — сильная база для серверной авторитетности и QA.
- Доменные сущности выделены в отдельные типы и структуры.
- Наличие Subsystem-уровня уже подталкивает к data-driven orchestration.

## 1.3 Ключевые архитектурные боли
1. **Runtime-загрузка ассетов строками** (`LoadObject/LoadClass` с hardcoded path).
2. **Инициализация размазана по GameMode/Subsystem/Actor BeginPlay**.
3. **PlayerController и GridWorldManager перегружены обязанностями**.
4. **Слабая событийная модель между UI/Core/Simulation** (много прямых вызовов).
5. **PCG-интеграция не выделена как отдельный слой генерации мира**.

---

## 2) Целевая архитектура (to-be)

Рекомендуемая форма — **Hexagonal / Ports & Adapters** внутри UE5-ограничений:

- **Domain (чистая логика)**
  - Pure C++: состояние мира, рецепты, алгебра эффектов, команды и редьюсеры.
  - Никаких UObject-зависимостей в критическом симуляционном ядре.

- **Application (оркестрация use-cases)**
  - Сервисы сценариев: Harvest, Craft, ApplyPotion, Transfer.
  - Работают через интерфейсы-порты (inventory port, world port, registry port).

- **Infrastructure (UE adapters)**
  - UWorldSubsystem/UGameInstanceSubsystem, Actor Components, DataAsset loaders, SaveGame.
  - Адаптеры к UI (UMG/MVVM), Input, Networking.

- **Presentation**
  - Виджеты + ViewModel (UE5 MVVM), без доменной логики внутри UUserWidget.

---

## 3) Data-driven вариант (рекомендуется как базовый)

## 3.1 Данные, которые должны стать первичными
- Ингредиенты, вода, биомы, рецепты, эффекты, правила harvest/spawn/decay.
- Настройки debug/telemetry/replay.

## 3.2 Где хранить
- **PrimaryDataAsset** для конфигов высокого уровня (rulesets).
- **DataTable** для табличных наборов (ингредиенты/вода/биомы), если важна bulk-редактура.
- **DeveloperSettings** для ссылок на корневые ассеты и environment-переключателей.

## 3.3 AssetManager паттерн
- Ввести `UHerbalistAssetCatalog : UPrimaryDataAsset`, где soft references на таблицы/графы/виджеты.
- Единая точка инициализации читает catalog через `UAssetManager`.
- Все runtime-загрузки только через soft refs + async loading (где возможно).

## 3.4 Плюсы/минусы
- **Плюсы**: устойчивость к рефакторингу контента, меньше runtime-failures, проще live tuning.
- **Минусы**: больше upfront-структуры и дисциплины контент-пайплайна.

---

## 4) Event-driven вариант (рекомендуется для UI и межмодульного взаимодействия)

## 4.1 Событийная шина
Варианты:
1. **Gameplay Message Subsystem** (UE5) — приоритетный путь.
2. UObject-based Event Bus (если нужны кастомные гарантии доставки/буферизация).

## 4.2 События первого класса
- `InventoryChanged`
- `CommandQueued`
- `CommandCommitted`
- `WorldCellChanged`
- `CraftCompleted`
- `ResourceHarvested`
- `UIWidgetStateChanged`

## 4.3 Принципы
- UI не дергает напрямую WorldManager/Simulation internals.
- UI публикует **intent** (команда), домен публикует **fact** (событие).
- Сервис-оркестратор подписывается на intent и применяет use-case.

## 4.4 Плюсы/минусы
- **Плюсы**: слабая связанность, тестируемость, расширяемость, аналитика/телеметрия.
- **Минусы**: нужна строгая схема событий и трассировка цепочек.

---

## 5) PCG вариант (UE5 PCG Framework)

## 5.1 Что отдать в PCG
- Генерацию распределения ресурсов по биомам.
- Первичный spawn-паттерн по seed + environmental masks.
- Визуальный слой (декоративные инстансы, density rules).

## 5.2 Что НЕ отдавать в PCG
- Авторитетную gameplay-симуляцию (distortion/purity/стейт-эффекты) — она должна оставаться в Domain/Simulation.

## 5.3 Рекомендуемая связка
- PCG генерирует **кандидатов** (`ResourceSpawnDescriptor`).
- Domain валидирует/нормализует и коммитит в world snapshot.
- Actor layer материализует подтвержденные спавны (или ISM/HISM).

## 5.4 Детерминизм
- Единый seed policy: world seed + chunk seed + system seed.
- Любая генерация, влияющая на gameplay, должна логироваться в trace/replay.

## 5.5 С учетом single-player
- Можно упростить контракты: не требуется replication-aware API.
- Приоритет — воспроизводимость локальной симуляции и быстрые сохранения/загрузки.
- Replay/trace используются как инструмент отладки и регрессионной проверки, а не как мультиплеерный lockstep.

---

## 6) Варианты реализации (3 дорожки)

## Вариант A — эволюционный (минимальные риски)
**Срок**: 2–4 недели.

1. Вынести asset paths в `DeveloperSettings` + `PrimaryDataAsset` каталог.
2. Ввести `UHerbalistBootstrapSubsystem` для порядка инициализации.
3. Добавить feature flags для debug draw/log verbosity.
4. Подключить Gameplay Message Subsystem для 3–5 ключевых событий.

**Когда выбирать**: нужно быстро стабилизировать текущую архитектуру без большого рефакторинга.

## Вариант B — сбалансированный (целевой)
**Срок**: 1–2 месяца.

1. Разделить Domain/Application/Infrastructure слои.
2. Тонкий `GridWorldManager` (facade + lifecycle), вынести сервисы (spawn, selection, propagation).
3. UI на MVVM + event-driven intents/facts.
4. PCG для world seeding + подтверждение через domain.

**Когда выбирать**: нужен прирост качества, но без «переписывания с нуля».

## Вариант C — радикальный (high investment)
**Срок**: 2–4 месяца.

1. Полный command/event sourcing в gameplay loop.
2. Почти весь gameplay state — через immutable snapshots + reducers.
3. Unified replay/debug tooling и deterministic lockstep-ready pipeline.
4. Полная data governance (schemas, validators, CI checks).

**Когда выбирать**: проект с долгим горизонтом и высокими требованиями к масштабированию/мультиплееру.

---

## 7) Конкретная целевая схема подсистем UE5

- `UHerbalistBootstrapSubsystem` (GameInstanceSubsystem)
  - Читает `UHerbalistAssetCatalog`
  - Инициализирует registries/services в правильном порядке.

- `UHerbalistSimulationSubsystem` (WorldSubsystem)
  - Хранит runtime snapshot, queue команд, tick apply.
  - Публикует `CommandCommitted`, `WorldCellChanged`.

- `UHerbalistInventorySubsystem` (LocalPlayerSubsystem или WorldSubsystem)
  - Операции инвентаря как use-cases.

- `UHerbalistPCGBridgeSubsystem`
  - Импорт кандидатов из PCG, валидация/commit через SimulationSubsystem.

- `UHerbalistTelemetrySubsystem`
  - Метрики, replay hooks, perf counters.

---

## 8) Антипаттерны, которых стоит избегать

- Доменные вычисления в `UUserWidget`.
- Cross-calls UI -> Actor -> Subsystem -> UI в одном sync-цикле.
- Hardcoded content paths в gameplay коде.
- Tick-логирование на Warning/Log уровне.
- Непрозрачные «магические» `FName` без реестров и валидации.

---

## 9) Технический план миграции (по спринтам)

### Sprint 1 (stability)
- Asset Catalog + DeveloperSettings.
- Bootstrap Subsystem.
- Debug/log flags.
- Smoke test загрузки критичных ассетов.

### Sprint 2 (architecture)
- Вынести use-cases в Application services.
- Ввести event contracts и Gameplay Messages.
- Облегчить PlayerController/GridWorldManager.

### Sprint 3 (pcg + quality)
- PCG bridge + deterministic seed policy.
- Replay assertions для PCG-driven спавнов.
- Property-based тесты инвентарных инвариантов.

---

## 10) Риски и как их снижать

- **Риск регрессий UX/UI**: контрактные тесты ViewModel + golden recordings.
- **Риск потери производительности**: профилирование до/после, budget на события/аллоцирование.
- **Риск усложнения пайплайна контента**: schema validators + editor tooling.

---

## 11) Итог
Для этого проекта оптимален **Вариант B (сбалансированный)**:
- data-driven как источник правды,
- event-driven как механизм связности,
- PCG как генератор кандидатов, а не носитель gameplay-истины,
- симуляция остается детерминированной и проверяемой через replay.

Это даст лучший баланс между скоростью разработки, качеством кода и масштабируемостью под UE5.

---

## 12) Дополнительные механики без конфликта с существующим ядром

Принцип: любая новая механика должна выражаться через уже существующие примитивы:
- **команды** (intent),
- **изменения состояния** (delta/fact),
- **реестры данных** (ingredients/water/biomes + новые таблицы),
- **инвентарь/контейнеры**.

### 12.1 Капища (ритуальные точки)
- Новый bounded context: `ShrineSystem`.
- Не хранит отдельный инвентарь-движок; использует текущий `UHerbalistInventoryComponent` и command pipeline.
- Команды: `OfferToShrine`, `InvokeShrineEffect`.
- Эффекты — data-driven (таблица/asset), применяются к тем же world meta-полям (distortion/purity/stability) через существующие редьюсеры.

### 12.2 Сады игрока (выращивание)
- Новый bounded context: `GardenSystem`.
- Использует существующие `GridCell` и harvest-логику, добавляя стадию роста как метаданные клетки/актора.
- Команды: `PlantSeed`, `WaterPlant`, `HarvestPlant`.
- Важно: не дублировать pipeline урожая; расширять его через policy/strategy слой.

### 12.3 Торговля
- Новый bounded context: `TradeSystem`.
- Торговец = специализированный контейнер + прайсинг policy.
- Команды: `QuoteTrade`, `ExecuteTrade`.
- Расчеты цен data-driven (редкость, чистота, искажение), но списание/добавление предметов — через существующие inventory операции.

### 12.4 Зачарования оберегов
- Новый bounded context: `CharmEnchantSystem`.
- Оберег — предмет с расширенным state/tag набором, а не новая сущность инвентаря.
- Команды: `ImbueCharm`, `ActivateCharm`.
- Эффекты зачарований подключаются как модификаторы к текущим алгоритмам (harvest/alchemy/perception), без копирования логики.

### 12.5 Правило «не перекрываться» (governance)
Перед добавлением механики проверять:
1. Есть ли уже аналогичный use-case в ядре?
2. Можно ли реализовать через существующую команду + новый policy?
3. Не создается ли второй источник истины для инвентаря/состояния мира?
4. Все ли параметры вынесены в data assets/tables?

Если хотя бы один ответ «нет» — фича возвращается на пересмотр дизайна.

---

## 13) Практический шаблон внедрения новой механики
1. Описать механику в data-driven схеме (таблицы, теги, коэффициенты).
2. Добавить 1–3 новые команды в существующий command pipeline.
3. Реализовать обработчик как policy/strategy, не меняя инварианты ядра.
4. Пробросить события в UI через event bus.
5. Добавить replay-тест и инвариантные проверки инвентаря/состояния.


---

## 14) Детализация развития Варианта B (сбалансированный)

Целевой вектор:
- **data-driven** как источник правды,
- **event-driven** как механизм связности,
- **PCG** как генератор кандидатов,
- **детерминированная симуляция + replay** как контроль качества.

## 14.1 Архитектурный контракт Варианта B

### Источник правды (data-driven)
1. `UHerbalistAssetCatalog` хранит ссылки на:
   - таблицы ингредиентов/воды/биомов,
   - таблицы механик (капища/сады/торговля/обереги),
   - профили PCG-правил.
2. Runtime-код не содержит hardcoded asset paths.
3. Любой баланс/формулы — только из data assets/tables (с версионированием).

### Механизм связности (event-driven)
1. UI публикует `Intent*` события (например, `IntentPlantSeed`, `IntentExecuteTrade`).
2. Application сервисы конвертируют intent -> команды симуляции.
3. Симуляция публикует `Fact*` события (`FactInventoryChanged`, `FactCellUpdated`, `FactCraftCompleted`).
4. UI подписывается только на facts, без прямого доступа к mutating API ядра.

### PCG-грань ответственности
1. PCG выдает кандидатов `FSpawnCandidate`.
2. Validation policy в домене фильтрует кандидатов.
3. Только валидированные кандидаты коммитятся как команды в simulation pipeline.
4. Фактический gameplay-state всегда формируется доменными редьюсерами.

### Детерминизм и replay
1. Команды применяются строго упорядоченно (tick + sequence).
2. RNG разделяется на stream-ы: `World`, `Harvest`, `Alchemy`, `PCGBridge`.
3. Каждый commit пишет trace frame: inputs, seed snapshot, delta summary.
4. Replay-проверка: `same input + same seed => same delta`.

## 14.2 Схема потока данных (end-to-end)

`Input/UI -> Intent Event -> Application Service -> Simulation Command -> Reducer -> Delta -> Fact Event -> UI`

Гарантии:
- нет второго источника истины,
- нет bypass симуляции,
- любые изменения мира и инвентаря воспроизводимы.

## 14.3 Минимальный набор событий и команд для Варианта B

### Intent события
- `IntentHarvestResource`
- `IntentCollectWater`
- `IntentCraftPotion`
- `IntentApplyPotion`
- `IntentTransferItem`
- `IntentPlantSeed`
- `IntentExecuteTrade`

### Команды симуляции
- `CmdHarvest`
- `CmdCollectWater`
- `CmdCraft`
- `CmdApply`
- `CmdTransfer`
- `CmdGardenPlant`
- `CmdTradeExecute`

### Fact события
- `FactInventoryChanged`
- `FactCellChanged`
- `FactItemCrafted`
- `FactTradeCompleted`
- `FactGardenStateChanged`

## 14.4 Расклад по компонентам UE5

- `UHerbalistBootstrapSubsystem`
  - Загружает `AssetCatalog`, проверяет версионность датасетов, поднимает сервисы.

- `UHerbalistSimulationSubsystem`
  - Очередь команд, tick apply, trace/replay hooks.
  - Публичный API: только enqueue/read-only snapshot.

- `UHerbalistMessageRouter`
  - Адаптер между Gameplay Message Subsystem и application services.

- `UHerbalistPCGBridgeSubsystem`
  - Pull кандидатов из PCG графа, отправка `CmdSpawnCandidate` после validation.

- `UHerbalistViewModel*`
  - Подписка на facts и проекция состояния в UI.

## 14.5 План внедрения Варианта B (по неделям)

### Week 1–2: Data foundation
- Ввести `UHerbalistAssetCatalog` + `DeveloperSettings`.
- Удалить все hardcoded content path из runtime.
- Добавить data validators (Editor utility / CI check).

### Week 3–4: Event backbone
- Поднять intent/fact контракты.
- Внедрить `UHerbalistMessageRouter`.
- Перевести 2 ключевых флоу на events: harvest и transfer.

### Week 5–6: Simulation boundary hardening
- Закрыть прямые мутации мира/инвентаря вне `SimulationSubsystem`.
- Ввести единый enqueue path.
- Добавить trace frame checksum.

### Week 7–8: PCG bridge
- Подключить кандидатный спавн через bridge.
- Валидация кандидатов по доменным правилам.
- Replay тесты для PCG-driven инициализации.

### Week 9–10: Extensions on core
- Подключить одну доп.механику (например, Garden) через команды/факты.
- Проверить отсутствие overlap по governance-чеклисту.
- Закрыть регрессионные тесты.

## 14.6 Definition of Done для Варианта B

Считаем внедрение успешным, если:
1. 100% runtime asset references идут через catalog/settings.
2. Все UI-mutating сценарии проходят через intents -> commands.
3. Нет прямых записей в world/inventory state из UI/Actor слоев.
4. Replay тесты стабильно проходят на фиксированном seed.
5. PCG не пишет в gameplay-state напрямую — только через validated commands.

## 14.7 Риски Варианта B и контрмеры

- **Событийная сложность**: ввести naming convention (`Intent*`, `Fact*`) и event catalog.
- **Просадка производительности от событий**: батчить факты по tick и ограничить payload.
- **Data drift между таблицами**: schema version + validators + fail-fast bootstrap.
- **PCG nondeterminism**: фиксированные seed policy и запрет random вне выделенного RNG stream.

---

## 15) Качество и точность симуляционного ядра/математики по материалам Herbalist_Vault

Источник анализа: `herbalist_docs/Herbalist_Vault` (GDD + Glossary) с сопоставлением с текущими принципами ядра в этом документе.

## 15.1 Что подтверждается как сильная сторона

1. **Корректная системная рамка `S_real -> S_perceived -> Action -> ΔS -> S_real'`**  
   Документация фиксирует замкнутый контур и разделение объективного/воспринимаемого состояния, что методологически верно для детерминированного single-player ядра.

2. **Явная роль биомного графа как «медленного слоя эволюции»**  
   В GDD биомный граф выступает как волновой контур (Grid→Graph→Grid), а не как мгновенная подмена локальной симуляции. Это соответствует выбранному Варианту B.

3. **Нормализация направлений (Direction) и композиция Meta-полей**  
   Для осей задана нормализация по сумме, что снижает риск неустойчивых состояний и упрощает инварианты.

## 15.2 Точность: где есть разрыв «документация ↔ воспроизводимая математика»

1. **Высокоуровневые формулы без полного вычислительного контракта**  
   Формулировки типа `ΔS = F(Action, Intent_system, S_real, Events, Time)` и `Morok = f(...)` описывают концепт, но недостаточно для строгой верификации.
   - Нужны точные уравнения/псевдокод: порядок операций, clamp-правила, коэффициенты, единицы измерения.

2. **Смешение «геймдизайнерского» и «численного» уровня в одном блоке**  
   Часть разделов задает нарратив, часть — математику, но без жесткой границы спецификации вычислений.
   - Риск: при реализации два разработчика могут интерпретировать одинаковый раздел по-разному.

3. **Недостаточная формализация ошибок и погрешностей**  
   Для perception/искажений не зафиксированы статистические требования: диапазоны, bias, допустимая variance для стабильного UX.

4. **Недостаточная спецификация replay-сверки**  
   В docs есть идея детерминизма, но не задан канонический формат assert'ов: какие поля сравниваются, с какой точностью, на каком горизонте тиков.

## 15.3 Качество математического слоя (оценка)

### Оценка зрелости
- **Концептуальная целостность:** высокая.
- **Инженерная проверяемость:** средняя.
- **Формальная воспроизводимость:** средне-низкая (до введения machine-checkable спецификации).

### Почему так
- Сильная доменная модель и причинно-следственная структура уже есть.
- Не хватает «исполняемой математики» (execution spec), чтобы исключить неоднозначность при рефакторинге и расширении.

## 15.4 Что добавить для точности (конкретно)

## 15.4.1 Numerical Spec (обязательный артефакт)
Для каждого вычислительного этапа зафиксировать:
1. Входы/выходы и типы.
2. Формулу в нормализованном виде.
3. Порядок применения модификаторов (strict order).
4. Границы (`clamp`) и saturation-правила.
5. Политику RNG (seed + stream + consumption order).

Мини-шаблон:
- `ComputeDelta(cell, ingredients, intent, biomeCtx, rng) -> delta`
- `ApplyDelta(cell, delta, params) -> newCell`
- `Perceive(real, distortion, rng) -> perceived`

## 15.4.2 Tolerance policy для float-математики
- Ввести допуски сравнения:
  - `epsilon_meta = 1e-4`
  - `epsilon_dir = 1e-5`
- Правило replay assert: exact для дискретных полей, epsilon для float.

## 15.4.3 Trace schema v2
В каждый trace-frame писать:
- tick, sequence,
- command payload hash,
- RNG snapshots по stream,
- delta checksum,
- post-state checksum (cell/window/global).

Это сделает replay-диагностику практически мгновенной.

## 15.4.4 Property-based тесты для математики
Минимальный набор инвариантов:
1. `Direction` после любой операции нормирована (сумма ≈ 1).
2. `Meta` всегда в допустимых диапазонах.
3. При одинаковых seed и командах результат идентичен.
4. Добавление «нулевого» воздействия не меняет состояние.
5. Перцепция не меняет `S_real` (чистая проекция).

## 15.5 Проверка согласованности с Вариантом B

С точки зрения выбранной стратегии документ Vault в целом совместим с Вариантом B:
- data-driven: совместимо, но нужны жесткие schema/validators;
- event-driven: совместимо, но нужно формализовать контракты intent/fact;
- PCG-кандидаты: совместимо при обязательной доменной валидации;
- deterministic replay: заявлено концептуально, требуется формализация trace/assert контрактов.

## 15.6 Приоритетный action list (короткий)

1. Зафиксировать **Numerical Spec v1** (machine-checkable markdown + тестовые кейсы).
2. Ввести **Trace schema v2** и checksum-проверки.
3. Добавить **property-based suite** для Delta/Perception/Direction.
4. Встроить **CI replay regression pack** (фиксированные seed-наборы).
5. Обновлять GDD и numerical spec синхронно (single source of truth для математики).
