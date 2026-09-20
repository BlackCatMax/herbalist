# Справочник инструментов: коммандлеты и утилиты

Назначение: одно место со списком `-run=` коммандлетов и вспомогательных
скриптов проекта — что каждый делает, когда запускать. Не дизайн-документ и
не летопись; при добавлении нового инструмента — новая строка сюда, не в
`CHANGELOG.md`.

## Коммандлеты (`UnrealEditor-Cmd.exe ... -run=<Имя>`)

| Команда | Что делает | Добавлен |
|---|---|---|
| `-run=BiomeDefaultsSync` | Приводит `DT_BiomeDefaults` к карточкам компендиума (документация — источник правды, см. `02_GDD/10_Biomes_Reference.md`) | 2026-09-07 |
| `-run=AmbientGatesPatch` | Точечно правит осевые гейты в `DT_AmbientEntities` (например, вырожденные пороги Шептунов/Плескунов) | 2026-09-07 |
| `-run=DataTableExport -out=<папка>` | Выгружает настоящее содержимое всех DataTable в json для сверки с документацией/`CSV_tabs` | 2026-09-07 |
| `-run=PlaytestMapResize [-size=N] [-dryrun]` | Перекладывает биом-полосы под новый размер сетки; без `-size` — размер сохраняется, только пересчёт | 2026-09-07 |
| `-run=ArtifactIngredientAppendCommandlet` | Добавляет ряды артефактов/перьев (§21.3, §16.4) в `DT_IngredientClass` как невыпадающие невидимо портящиеся предметы | 2026-09-02 |
| `-run=WorldStateMapSetup [-map=<путь>]` | Создаёт `RT_WorldStateMap` (размер = сетке, линейная гамма, билинейный, clamp), заводит в `MPC_WorldStateFields` параметры рамки карты (`WorldStateMapOrigin`/`WorldStateMapSize`) и назначает всё менеджеру на карте (по умолчанию `L_TestDev`). На карте World Partition менеджер — внешний актор, коммандлет его не находит и карту не трогает. Идемпотентен | 2026-09-08 |
| `-run=WorldPartitionBuilderCommandlet <карта> -Builder=WorldLayoutSyncBuilder [-ReportOnly]` | Разметка мира (`DESIGN_World_Layout.md`): собирает с карты ландшафт и сетку стриминга World Partition, пересчитывает разметку менеджера сетки (клетка, размер, начало, страница, чанк) и сохраняет его внешний актор. `-ReportOnly` — только печатает исходные величины и разметку, ничего не сохраняя. То же, что кнопка «Сверить с World Partition» на менеджере | 2026-09-12 |
| `-run=PcgGrassSeasonSetup` | Правит `/Game/PCG/PCG_Grass`, оба шага идемпотентны. (1) Вставляет `Sample Herbalist Cell` перед `Attribute Noise`: точки травы получают состояние клетки, `MeshKey`, `SeasonKey`, `SeasonMeshKey` и прореживаются по доле сезона. (2) Исключение травы на покраске слоя `Ground`: после `World Raycast` — `Projection` на данные ландшафта (вес слоя в атрибут), фильтр «`Ground` < порога» с постоянным порогом (0.8 пользователя) перед `Sample Herbalist Cell`; старая ветка точек-вычитателей (`Surface Sampler` → `Difference`) убрана. Спавнеры не трогает. Узлы ищутся по типам | 2026-09-17, 2026-09-19 |
| `-run=WorldPartitionBuilderCommandlet /Game/Maps/L_TestDev -Builder=PcgGrassRuntimeBuilder [-ReportOnly]` | Трава в рантайме: шаблон PCG-компонента в `BP_BiomeVolume` и экземпляры на карте (внешние акторы) — `GenerateAtRuntime` с разбиением на ячейки, у экземпляров вычищается запечённое; `PCG_Grass` — иерархическая генерация с сеткой 64 м (радиус по умолчанию 128 м); PCG World Actor карты — кэш ландшафта `SerializeOnlyAtCook` (при `NeverSerialize` в PIE и в сборке `Get Landscape Data` пуст, и проекция травы выбросила бы все точки). `-ReportOnly` — только замер. Из Git Bash — с `MSYS_NO_PATHCONV=1`, иначе `/Game/...` станет путём Windows | 2026-09-19 |
| `-run=PcgResourceSlotsSetup` | Слоты ресурсов (этап 5б, `DESIGN_Living_Vegetation_Research.md` §4.1), оба шага идемпотентны. (1) Собирает `/Game/PCG/PCG_ResourceSlots`, если его нет или он пуст (непустой — не трогает, дальше граф правит художник): от сплайна своего актора — вода (`Create Surface From Spline` → `Surface Sampler`, 0.3 на м²), кромка (точки по сплайну, сдвиг до 1.5 м по каждой оси, минус вода, проекция на ландшафт), суша (то же, сдвиг до 9 м по каждой оси); атрибут `SlotKind` = `Water`/`Shore`/`Land`; всё сходится в узел **Write Herbalist Resource Slots**. Шаг по сплайну — в локальных единицах сплайна (масштаб актора его растягивает). (2) Добавляет в `BP_WaterVolume` PCG-компонент с графом, `GenerateOnDemand`. Запекание слотов в ассет карты `/Game/Data/ResourceSlots/RS_<карта>` — движковым билдером, без `-nullrhi`: `MSYS_NO_PATHCONV=1 UnrealEditor-Cmd.exe <uproject> -run=WorldPartitionBuilderCommandlet /Game/Maps/L_TestDev -Builder=PCGWorldPartitionBuilder -IncludeGraphNames=PCG_ResourceSlots -GenerateComponentEditingModeNormal -AllowCommandletRendering`; в логе `[Slots] <актор>: записано N слотов`. В редакторе узел перезаписывает набор своего актора при каждой генерации компонента и помечает ассет изменённым. Набор удалённого или переименованного актора в ассете остаётся — удалить ассет и перезапечь | 2026-09-19 |
| `-run=TrampleMapSetup` | Создаёт `RT_TrampleMap` (1024×1024, RGBA8, линейная гамма, билинейный, **wrap**) и заводит в `MPC_WorldStateFields` параметры `TrampleMapFrame`/`TramplePlayerPosition`. Карты не трогает — пути лежат в Herbalist Settings. Идемпотентен | 2026-09-12 |
| `-run=TimeDisplaySetup` | Заводит в `MPC_WorldStateFields` (путь — `TimeDisplayCollection` в Herbalist Settings) параметры времени для материалов: скаляры `TimeOfDay01`, `SeasonUDW`, `LeafDrop01`, `LeafFall01`, `LeafLitter01`, `MoonFull01`, `Morok01` (воспринятое искажение в клетке игрока, сглаженное — под цветокоррекцию поста, `DECISIONS_LOG.md` №6), векторы `DayPhaseWeights` (R рассвет, G день, B закат, A ночь) и `SeasonWeights` (R весна, G лето, B осень, A зима). Значения пишет менеджер сетки каждый тик. Карты не трогает. Идемпотентен | 2026-09-16 |
| `-run=MaterialFunctionsSetup [-rebuild [-only=MF_A,MF_B]] [-verify]` | Собирает функции материалов в `/Game/Materials/Functions`: для карт мира `MF_SampleWorldState`, `MF_SampleTrample`, `MF_TrampleCompressWPO`; слой сезона и суток `MF_SeasonWeights`, `MF_SeasonColor`, `MF_LeafDrop`, `MF_GrassSquash`, `MF_FlowerOpen` (подключение — раздел «Функции материалов» ниже). Нужны ассеты коммандлетов выше (`WorldStateMapSetup`, `TrampleMapSetup`, `TimeDisplaySetup`) и коллекция Ultra Dynamic Weather. Существующую функцию не трогает; `-rebuild` перестраивает граф (правки в редакторе теряются, Id входов и выходов сохраняются — подключения в материалах не рвутся), с `-only=` — только перечисленные функции (без `-rebuild` не действует; перестроенная `MF_SampleTrample` тянет за собой зовущие её `MF_TrampleCompressWPO` и `MF_GrassSquash`). Материалы не трогает. `-verify` компилирует каждую функцию во временном материале (шейдер пикселей и вершин, обе ветки `Trampleable` у `MF_TrampleCompressWPO` и `MF_GrassSquash`) и печатает ошибки компилятора; запускать **без** `-nullrhi` и с `-AllowCommandletRendering`, иначе ресурса материала нет и проверка отказывает | 2026-09-16 |
| `-run=CompendiumAudit [-CompendiumPath=<папка>]` | Только чтение: сверка карточек компендиума с DataTable (ранги, оси, биомы; геймплейного тюнинга в карточках нет — его не сверяет) | 2026-09-03 |
| `-run=PlaytestMapCreate` | Создаёт карту `L_Playtest`: восемь `ABiomeRegionVolume` полосами, менеджер сетки, домашний якорь. `L_TestDev` не трогает | 2026-09-06 |
| `-run=BiomeGraphExport` | `DA_BiomeGraph` → `CSV_tabs/DA_BiomeGraph.json`, чтобы значения графа ревьюились в git | 2026-08-24 |
| `-run=BiomeGraphImport` | `CSV_tabs/DA_BiomeGraph.json` → живой `DA_BiomeGraph` (запись пакета) | 2026-08-24 |
| `-run=BestiaryRankMove` | Переносит Курганников, Жердяев и Курганных огней из `DT_Landmarks` в `DT_AmbientEntities` (`AddRow` + `RemoveRow`) — ранг по компендиуму | 2026-09-03 |

### Функции материалов для карт мира

Собираются `-run=MaterialFunctionsSetup`. Категория `Herbalist` в палитре
материала. Позиция по умолчанию — абсолютная мировая без смещений шейдера, так
что функции работают и в World Position Offset.

| Функция | Входы | Выходы | Куда |
|---|---|---|---|
| `MF_SampleWorldState` | `WorldPosition` | `Distortion` (R), `Corruption` (G), `HarvestStress` (B), `ShrineInfluence` (A), `UV`, `InsideWindow` | цвет травы и ландшафта по клетке; за окном карты оси — крайние тексели, умножать или смешивать по `InsideWindow` |
| `MF_SampleTrample` | `Position` | `Trample` (с затуханием к краю окна), `RawTrample`, `Fade` | земля на тропе: `Lerp` к слою тропы по `Trample` |
| `MF_TrampleCompressWPO` | `WPO` (ветер) | `WPO`, `Trample` | трава на тропе: выход `WPO` — в World Position Offset вместо ветра. Переключатель `Trampleable` (выключен) включить в инстансах низкого покрова — список в `CHANGELOG.md`, «схема травы на тропе» |

Выборки карт — с явным мипом 0 (в шейдере вершин нет производных), основание
кустика — `Instance & Particle Space` (PCG-трава — Nanite). Компиляцию проверяет
`-verify`; после подключения в материал — Apply без ошибок в редакторе.

### Листопад у игрока (`ULeafFallSubsystem`)

Этап 4 плана. Подсистема держит систему Niagara из Herbalist Settings
(`LeafFallSystem`, пусто — листопада нет) на пешке игрока в лиственных биомах
(`DeciduousBiomes`: смешанный и широколиственный лес, лесостепь, пойма) и
выключает её вне листопада, над водой и за сеткой. Уже падающие листья
долетают. Систему частиц собирает художник, пользовательские параметры:

| Параметр | Тип | Значение |
|---|---|---|
| `LeafFallRate` | float 0..1 | темп: `LeafFall01 × ((1 − LeafFallWindShare) + LeafFallWindShare × ветер)`, `LeafFallWindShare` = 0.6 — множитель частоты спавна |
| `WindIntensity` | float 0..1 | ветер симуляции (от UDW через мост) — снос и скорость листьев |

Цвет листьев материал частиц берёт из `MF_SeasonWeights` / `MF_SeasonColor`.
Короткий нулевой темп (клетка с водой) — только `LeafFallRate` = 0, система
деактивируется после 10 секунд нулевого темпа подряд. `DeciduousBiomes` в
ini можно дополнить или заменить, но не очистить до пустого — пустой список
возвращает значение по умолчанию из кода.

### Функции материалов: сезон и сутки

Этап 3 `docs/research/DESIGN_Living_Vegetation_Research.md`. Значения времени
приходят готовыми из `MPC_WorldStateFields` (`TimeDisplaySetup`), снег — `Snowy`
коллекции Ultra Dynamic Weather; материал с `MF_GrassSquash` читает две
коллекции — предел движка. Прежде чем добавлять в материал функции UDW (ветер
`Foliage_Wind_Movement` и т.п.), проверить, что они читают ту же коллекцию
погоды UDW, а не третью ❓ — иначе материал не скомпилируется.

| Функция | Входы (по умолчанию) | Выходы | Куда |
|---|---|---|---|
| `MF_SeasonWeights` | — | `SeasonWeights` (R весна, G лето, B осень, A зима), `Spring`…`Winter`, `SeasonUDW`, `LeafDrop01`, `LeafFall01` (темп листопада), `LeafLitter01` (подстилка на земле) | свои смеси по сезону; слой подстилки ландшафта — `Lerp` к опавшей листве по `LeafLitter01` |
| `MF_SeasonColor` | `Color`, `SpringTint` (0.95, 1.08, 0.9), `SummerTint` (1), `AutumnTint` (1.25, 0.95, 0.45), `WinterTint` (0.85, 0.8, 0.7), `Strength` (1) | `Color`, `Tint` | между текущим цветом и Base Color; оттенки подбирать в инстансах |
| `MF_LeafDrop` | `OpacityMask` (1), `ClumpSize` (30 см), `Position` | `OpacityMask`, `Kept`, `LeafDrop01` | маскированная листва деревьев: между текущей маской и Opacity Mask |
| `MF_GrassSquash` | `WPO` (ветер), `WinterStrength` (0.8), `SnowStrength` (1), `Snow` (`Snowy` UDW) | `WPO`, `Squash`, `Trample` | трава: `WPO` — в World Position Offset вместо ветра; ложится от максимума тропы (`Trampleable`), зимы и снега; `Squash` — для пожухлого цвета |
| `MF_FlowerOpen` | `OpenPhase` (Vector4, R рассвет, G день, B закат, A ночь; день), `WPO`, `PetalMask` (красный канал цвета вершин), `CloseAmount` (0.7) | `WPO`, `Open` | цветы: `WPO` цепочкой **до** `MF_GrassSquash` (ветер → `MF_FlowerOpen` → `MF_GrassSquash`): закрытие цветка ослабляет входящий WPO и после сжатия отменило бы его — цветок вставал бы из-под снега; `OpenPhase` — параметр вектора в мастере, значение в инстансе вида |

**Подключение в мастер-материалы — вручную в редакторе.** Коммандлет в них не
пишет: в `M_Foliage_Master` и `M_landscape` уже стоит ручная сборка троп.

- `M_Foliage_Master` (листва и трава Stylized PBR Nature): текущий цвет перед
  Base Color → `MF_SeasonColor.Color` → Base Color. Текущий WPO (ветер со
  сборкой троп) → `MF_GrassSquash.WPO` → World Position Offset, `Trampleable`
  выключен, чтобы тропа не считалась дважды. Листве деревьев — текущая маска →
  `MF_LeafDrop.OpacityMask` → Opacity Mask под статическим переключателем
  «листва», траве не нужен. «Nanite Foliage выключается» в плане — это
  `r.Nanite.Foliage`; маскированная листва на Nanite-меше — программируемый
  растеризатор, дорого: для деревьев с листопадом Nanite у меша лучше выключить.
- `M_plants` (Stylized Forest): цвет → `MF_SeasonColor`; `SimpleGrassWind` →
  `MF_GrassSquash.WPO` → World Position Offset; у цветов между ними
  `MF_FlowerOpen`: `SimpleGrassWind` → `MF_FlowerOpen.WPO` →
  `MF_GrassSquash.WPO`, `OpenPhase` — параметр вектора.
- `M_landscape`: цвет слоя травы → `MF_SeasonColor`; подстилка — `Lerp` цвета
  земли к текстуре опавшей листвы по `MF_SeasonWeights.LeafLitter01` (текстура
  подстилки — выбор художника). Снега в нём сейчас нет
  (UDW-функций `M_landscape` не читает, проверено по ассету); снег на земле —
  `DLWE_SnowCoverage` UDW ❓, подключать отдельно.

### Создание таблиц с нуля (`*Create`: ассет уже есть — ничего не делает)

| Команда | Что делает | Добавлен |
|---|---|---|
| `-run=AmbientEntitiesCreate` | `DT_AmbientEntities` — Низший ранг бестиария | 2026-09-02 |
| `-run=LandmarksCreate` | `DT_Landmarks` — хозяева мест | 2026-09-02 |
| `-run=LegendaryEntitiesCreate` | `DT_LegendaryEntities` — 17 Легендарных, включая Берегиню | 2026-09-02 |
| `-run=ArtifactsCreate` | `DT_Artifacts` — 8 артефактов | 2026-09-02 |
| `-run=DialogueCreate` | `DT_Dialogue` — дерево Домового | 2026-09-02 |
| `-run=OrdersCreate [-sync]` | `DT_Orders` — каталог заказов (`02_GDD/24_Orders_And_Repute.md` §24.4): 11 заказов трёх кругов, записка, область в осях, срок, задаток, плата, последствие и слух. Ассет есть — без `-sync` ничего не делает; с `-sync` переписывает ряды по каталогу | 2026-09-19 |
| `-run=MemoryFragmentsCreate [-sync]` | `DT_MemoryFragments` — 13 фрагментов памяти. Ассет есть — без `-sync` ничего не делает; с `-sync` выравнивает вес в якоре (`ClarityGain`) и дописывает недостающие ряды, тексты и класс актора не трогает | 2026-09-02, `-sync` 2026-09-19 |

### Добавление рядов (`*Append`: ряд уже есть — пропускает)

| Команда | Что делает | Добавлен |
|---|---|---|
| `-run=IngredientAppend -Names=<id,…>` | Ряды из `CSV_tabs/ingredients.json` в `DT_IngredientClass`. Работает полным JSON-проходом по таблице — по заголовкам Patch-коммандлетов такой проход молча терял ряды с пробелом в имени («Молодильное яблоко», «Перо …»): после прогона сверить таблицу (`-run=DataTableExport`) | 2026-08-24 |
| `-run=ContainerAppend` | Корзина, Мешок, Туёс | 2026-09-04 |
| `-run=GatheringToolAppend` | Железный и Медный серп, Костяной нож, Серебряный оберег | 2026-09-06 |
| `-run=PeregnoyAppend` | Перегной — продукт гниения, в мире не растёт | 2026-09-04 |
| `-run=WardCrystalAppend` | Кристаллы-обереги Пещеры | 2026-09-04 |
| `-run=TieredWardCrystalAppend` | Три тиражных кристалла-оберега (награда ритуалов перехода ярусов) | 2026-09-04 |
| `-run=BestiaryStubsAppend` | Заготовки рядов для карточек бестиария без ряда; ставки эффекта нулевые, пока их не проставят | 2026-09-03 |
| `-run=KalinovMostDialogueAppend` | Ряд «ЗмейГорыныч» в `DT_Dialogue`: ветки «Бой» и «Сделка» | 2026-09-06 |

### Точечные правки рядов (`*Patch`: `FindRow`, остальные ряды не трогает)

| Команда | Что делает | Добавлен |
|---|---|---|
| `-run=AmbientEntitySpacingPatch` | `MinSpacingMeters` из `CSV_tabs/ambient_entity_spacing.json` | 2026-09-04 |
| `-run=IngredientBiomeRangePatch` | `AllowedBiomes` из `ingredient_biome_range_patch.json` | 2026-09-04 |
| `-run=IngredientHarvestWindowPatch` | Окна сбора (сезоны, время суток, луна, сухая погода) из `ingredient_harvest_windows.json` | 2026-08-29 |
| `-run=IngredientGatheringAndGardenPatch` | `bIronAverse`, `bDelicate`, `GardenNiche` из `ingredient_gathering_and_garden_flags.json` | 2026-08-31 |
| `-run=DryingStatePatch` | `DriedStateDelta` из `ingredient_drying_state_patch.json` | 2026-09-05 |
| `-run=IngredientDryingDurationPatch` | `DryingDurationSeconds` всем растениям и грибам из `ingredient_drying_duration_patch.json` | 2026-09-05 |
| `-run=AmbientPackSizePatch` | Размер стаи Низшего (`PackSize`) из `ambient_pack_size.json` | 2026-09-20 |
| `-run=IngredientHostPatch` | Хозяин травы (`HostEntityID`, Основной биома) из `ingredient_hosts.json` | 2026-09-20 |
| `-run=DomovoiMilkOfferingPatch` | Символическое подношение в ветке Домового «блюдце молока» | 2026-09-06 |
| `-run=KalinovMostDealPatch` | Флаг сделки в ветке Змея «Откупиться подношением» | 2026-09-06 |

## Python-скрипты (`tools/data_extraction/`, запуск из корня репозитория)

| Скрипт | Что делает |
|---|---|
| `extract_biomes.py` | Выводит числовые параметры биомов (включая `StressRecoveryMultiplier`) из Fertility/Distortion/характера воды карточек компендиума |
| `extract_ingredients.py` | Извлекает параметры ингредиентов из карточек `04_Compendium/Растительность` |
| `extract_flower_open_phase.py [путь.json]` | Начальные окна раскрытия цветов для `MF_FlowerOpen` из карточек `04_Compendium/Растительность` и `CSV_tabs/ingredient_harvest_windows.json`: цветущие виды (трава и кустарники с цветением в тексте), `OpenPhase` [рассвет, день, закат, ночь] из окна сбора, пометки `Review` там, где текст о сборе или цветении называет другую фазу суток, сбор нераскрытым или раскрытие и закрытие. Без пути пишет `CSV_tabs/ingredient_flower_open_phase.json`, список на разметку — в консоль |
| `extract_water.py [путь.json]` | Выводит `water_types.json` (типы воды по биомам) из карточек `04_Compendium/Биомы`: Potency/Stability из фронтматтера, Purity/Distortion/Corruption из раздела «Вода». Без пути пишет `CSV_tabs/water_types.json` |

## Хранилище документации (`herbalist_docs/Herbalist_Vault/`)

| Скрипт | Что делает |
|---|---|
| `build_docs.py` | Склеивает главы GDD, глоссарий, растения, бестиарий и биомы в `Herbalist_Vault/build/*.md` (в git не попадают) |

## Проверка документации (`tools/docs_audit/`)

| Скрипт | Что делает |
|---|---|
| `audit_docs.py [--only C06,C09] [--strict] [--list]` | Только читает: фронтматтер и поля карточек, ссылки, глоссарий, нумерация GDD, упоминания кода/тестов/команд, легаси, индексы `docs/README.md` и этого справочника, эталоны гайдов проверки, сверка карточек с `CSV_tabs`. Отчёт — `build/docs_audit/report.md`, код выхода 1 при ошибках. Как читать — `docs/maintenance/DOCS_AUDIT_CHECKLIST.md` |
| `test_audit_docs.py` | Тесты проверок на искусственном мини-репозитории: `py -m unittest tools/docs_audit/test_audit_docs.py` |

## Правило добавления

Новый коммандлет/скрипт — новая строка в подходящую таблицу выше, с датой.
Не описывать здесь ЧТО он чинит в конкретный день (это `CHANGELOG.md`) —
только что инструмент делает как таковой, актуально всегда.

## Проверка системы в движке

Пошаговый гайд «как проверить всю игровую систему» —
`docs/verification/ENGINE_VERIFICATION_GUIDE.md`: пять уровней от сборки до долгого прогона, эталонные значения
здорового мира, разбор логов и список ловушек.
