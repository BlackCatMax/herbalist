---
tags: [technical, current, level]
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# Сборка уровня

Всё, что для игры нужно сделать руками в редакторе: поставить на карту,
назначить в Blueprint, включить в настройках проекта, прогнать
коммандлетом. Каждая строка ведёт туда, где эта вещь описана подробно, —
в технический документ главы GDD (`NN_…_Tech`); как проверить — сценарии
PIE (`docs/verification/pie/`). Рабочая карта — `L_TestDev` (World
Partition).

Условные пометки: **нужно** — без этого система молчит; **желательно** —
работает и без, но хуже; **арт** — заглушка есть, нужна модель.

## Настройки проекта

| Что | Где | Зачем | Подробно |
|---|---|---|---|
| Custom Depth-Stencil Pass = `Enabled with Stencil` | Project Settings → Rendering | **нужно** для подсветки того, на что смотришь | [[07_UX_Tech#§7.13.2 Взгляд и подсветка]] |
| `Use Ambient Spawners` | Project Settings → Herbalist → Entities\|Spawners | включает Низших на спавнерах (пока выключено, ждёт проверки) | [[16_Entity_Manifestation_Tech#§16.2 Низший]], `pie/05_Places_Entities.md` |
| `bAutoLoadOnStart` | Project Settings → Herbalist → Save | автозагрузка при старте; выключить — проверка с чистого листа | [[07_UX_Tech#§7.13.7 Сон, лагерь, автозагрузка]] |

## Карта и мир

| Что | Как | Подробно |
|---|---|---|
| Разметка сетки под World Partition | **нужно**: `-run=WorldPartitionBuilderCommandlet … -Builder=WorldLayoutSyncBuilder` или кнопка «Сверить с World Partition» на менеджере | [[13_World_Pipeline_Tech#§13.19 Масштаб мира и разметка]], `pie/01_Start.md` |
| Регионы биомов (`ABiomeRegionVolume`, Blueprint `BP_BiomeVolume`) | **нужно**: накрыть сетку — вне регионов нет ни ресурсов, ни хозяев мест | [[10_Biomes_Reference_Tech]], [[08_Content_Tech#§8.1 Типы мест]] |
| Регионы воды (`AWaterRegionVolume`) | **нужно** для воды и водных ресурсов | [[08_Content_Tech#§8.3 Вода]] |
| Трава PCG в рантайме | `-run=PcgGrassSeasonSetup`, `-Builder=PcgGrassRuntimeBuilder`, слоты ресурсов — `-run=PcgResourceSlotsSetup` | [[08_Content_Tech#§8.4 Ресурсы]], `TOOLS_REFERENCE.md` |
| Карта состояния мира | `-run=WorldStateMapSetup` | `TOOLS_REFERENCE.md` |
| Небо и погода (UDS/UDW) | акторы Ultra Dynamic Sky/Weather на карте; у UDW `Season Mode` — «Use UDS Date» | [[15_Cycles_And_Shrines_Tech#§15.7 Погода и небо]] |
| Капища (`AShrineActor`) | ставятся вручную; тип — по клетке или вручную, стартовое Restoration | [[15_Cycles_And_Shrines_Tech#§15.5 Капища]] |
| Хозяева мест, Легендарные, курганы, точки интереса, Калинов мост | сеются сами по регионам — ставить не нужно | [[16_Entity_Manifestation_Tech]], [[08_Content_Tech#§8.1 Типы мест]] |
| Маршрут пути Заряны | Лесостепь и Речная пойма с контентом, капище в Пойме — иначе фрагменты ПЕРВАЯ ВАРКА и ПОДНОШЕНИЕ недостижимы (решение 2026-09-19: карту — под маршрут) | [[23_Journey_Order_Tech#§23.7 Код и карта]] |
| Ручные спавнеры Низших (`AAmbientEntitySpawner`) | **желательно** там, где нужен конкретный вид; работают при `Use Ambient Spawners` | [[16_Entity_Manifestation_Tech#§16.2 Низший]] |

## Дом

| Что | Как | Подробно |
|---|---|---|
| Котёл (`AAlchemyTableActor`, Blueprint с мешем) | **нужно**: от него — Домовой и Роса Заряны; всегда загружен (не выгружается World Partition). Отладочному окну — `AlchemyWidgetClass` в Class Defaults | [[07_UX_Tech#§7.13.4 Применение из руки]], `pie/03_Alchemy_Stations.md` |
| Лавка (`ASleepBenchActor`) | **нужно** у дома: сон до рассвета и сохранение. Меш — Blueprint-наследник (**арт**); без меша видна подсветкой | [[07_UX_Tech#§7.13.7 Сон, лагерь, автозагрузка]], `pie/08_Saves.md` |
| Хранилища (`AStorageContainer`, `BP_StorageContainer`) | желательно у дома; отладочному окну — `TransferWidgetClass` = `WBP_InventoryTransferWidget` | [[07_UX_Tech#§7.13.3 Пестерь]], `pie/03_Alchemy_Stations.md` |
| Станции: сушилка, отстойник, выпарной куб (`ADryingRackActor`, `ASettlingStandActor`, `AEvaporationStillActor`) | Blueprint от класса (там же визуал), поставить у дома | [[05_Systems_Tech#§5 Инвентарь и эволюция состояния вне мира]], `pie/03_Alchemy_Stations.md` |
| Домашние хранилища (погреб, шкаф, кувшин) | строятся в игре; класс — `HomeStorageContainerClass` у менеджера (по умолчанию `BP_StorageContainer`) | [[17_Hero_And_Community_Tech#§17.8 Хозяйство]] |

## Община

| Что | Как | Подробно |
|---|---|---|
| Тайник для заказов (`AOrderCacheActor`) | **желательно** один-два у дома; пока тайников нет, отдавать можно где угодно. Меш — Blueprint (**арт**) | [[24_Orders_And_Repute_Tech]], [[07_UX_Tech#§7.13.4 Применение из руки]] |
| Камень-жертвенник (`AOfferingStoneActor`) | **нужно** у деревни — иначе подношения общине только командой. Меш — Blueprint (**арт**) | [[17_Hero_And_Community_Tech#§17.3 Молва]] |
| Записки заказов (`AOrderNoteActor`) | появляются сами у порога | [[24_Orders_And_Repute_Tech#§24.2 Записка у порога]] |

## Игрок и ввод

| Что | Как | Подробно |
|---|---|---|
| Input Actions в Blueprint контроллера | **нужно**: `MoveAction`, `LookAction`, `HarvestAction`, `InteractAction`, `InventoryAction`, `InfoAction`, `JournalAction` (сейчас не назначен), `ApplyAlchemyAction`, `UsePotionAction`; раскладка — `IMC_Default`, `IMC_MouseLook` | [[07_UX_Tech#Ввод]], `pie/01_Start.md` |
| Колесо мыши для строки выбора | привязано клавишей в коде; если в PIE не работает — завести Input Action | [[07_UX_Tech#§7.13.6 Строка выбора и речь хозяев]] |
| Материал подсветки (трафарет 42) | **нужно**: пост-процесс в Post Process Volume карты | [[07_UX_Tech#§7.13.2 Взгляд и подсветка]] |

## Данные

| Что | Как | Подробно |
|---|---|---|
| Ряды `DT_IngredientClass` | добавлять точечными `*Append`-коммандлетами (`AddRow`), **не** `IngredientAppend` — тот портит ряды с пробелом в имени | [[05_Systems_Tech#§5 Система ресурсов]] |
| Меш ресурса «Хворост» | `ResourceMesh` ряда «Хворост» (**арт**); пока — меш Blueprint-класса ресурса | `pie/08_Saves.md` (лагерь) |
| Модели предметов и руки | вместо заглушек `/Engine/BasicShapes` (**арт**) | [[07_UX_Tech#§7.2.3 Вид предмета без чисел]] |
