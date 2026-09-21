---
tags: [technical, current, ux]
gdd: "[[07_UX]]"
version: 3.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 07. UX — техническая сторона

Пара к главе [[07_UX|07. UX, интерфейс и обратная связь]]: там — что видит и
делает игрок, здесь — как это устроено в коде и что для этого нужно в
редакторе. Номера разделов совпадают с главой (§7.13 здесь — §7.13 там).
Поглотил прежний снимок интерфейса (UI_Current, 2026-09-22). Летопись решений —
`CHANGELOG.md`, решения пользователя по диегетике —
`docs/design/DESIGN_Diegetic_Interface.md`. Проверка в PIE —
`docs/verification/pie/` (ссылки в каждом разделе). Всё, что ставится на
карту и назначается в редакторе, сведено на одной странице —
[[Level_Assembly|Сборка уровня]].

## §7.2.3 Вид предмета без чисел

- До арта предмет виден заглушкой (`AHeldItemActor`): форма — по классу
  (трава и гриб — шар, камень — куб, вода и зелье — цилиндр), цвет — по
  ведущей оси воспринятого состояния (`AHeldItemActor::ColorForState`).
  Меши заглушек — `/Engine/BasicShapes`, жёсткие ссылки с CDO (в упакованной
  игре не пропадут). Модели предметов — задача арта.
- Строка ощущения при осмотре — `HerbalistSensation::Describe`
  (`Core/Types/HerbalistSensation.h`): ведущая ось словами плюс до двух
  заметок меты (Порча и Искажение первыми), без цифр. Экран —
  `USensationLineWidget`.
- Числа в подсказке остались только у отладочных окон (§7.13.8) —
  `UItemTooltipWidget`.

## §7.10 Травник

- `UJournalLogWidget` — чистый C++, строит дерево в `NativeConstruct`, без
  WBP. Открывается `JournalAction` или консолью `ToggleJournalUI`. Фильтр по
  ингредиенту; текстовые записи (память, Молва, знак, речь) в фильтр не
  попадают.
- Записи — `FJournalEntry` (`Core/Journal/JournalTypes.h`), типы
  `EJournalEntryType`: `Harvest`, `Brew`, `MemoryFragment`, `CommunityNote`,
  `Inspection` (осмотр, §7.13.1), `WorldSign`, `HostSpeech` (речь хозяев,
  §7.13.6). Состояние в записи — всегда воспринятое.
- `UJournalWidget`/`UJournalEntryRowWidget` — WBP-раскладка на будущее, в
  редакторе не назначена; рабочий путь — `UJournalLogWidget`.
- Проверка: `pie/07_Zaryana_Journey.md` (Травник).

## §7.13 Диегетический интерфейс

Всё, кроме Травника, — в мире: рука, взгляд, пестерь, пояс, сон. Данные не
менялись — это слой ввода и представления над прежней симуляцией; каждое
действие из руки идёт тем же путём, что консольная команда.

### §7.13.1 Рука и осмотр

- `UHeldItemComponent` на контроллере: рука не вынимает предмет из котомки,
  а указывает на него (снимок + подсказка ячейки; ячейку ищет
  `UHerbalistInventoryComponent::FindItemIndex` — стопка стареет и сливается).
  Предмет ушёл из котомки — рука пустеет сама.
- Вид — `AHeldItemActor` перед камерой (`HeldOffset`), при осмотре —
  ближе к глазам и медленно вращается. Осмотр пишет запись «[Осмотр]» в
  Травник и показывает строку ощущения.
- Клавиши: `InventoryAction` с предметом в руке — убрать; `InfoAction` —
  осмотреть. Отладка: `HoldItem <ячейка>`, `InspectHeld`, `PutAwayHeld`.
- Проверка: `pie/02_Harvest_Inventory.md` («Пестерь, рука и осмотр»).

### §7.13.2 Взгляд и подсветка

- `ULookHighlightComponent` (тик 0,1 с): сначала пестерь и пояс (их
  заглушки мир не видит), потом луч `ECC_Visibility`, потом
  `ECC_GameTraceChannel1`. Подсвечивается то, что `IInteractable` или
  `AHerbalistResourceActor`: на его примитивах включается Custom Depth с
  трафаретом **42** (`HighlightStencilValue`), прежние значения
  возвращаются при уходе взгляда.
- **В редакторе:** Project Settings → Rendering → Custom Depth-Stencil Pass
  = `Enabled with Stencil`; материал пост-процесса, рисующий трафарет 42
  контуром или свечением, — в Post Process Volume карты. Без этого C++
  отмечает цель, но на экране ничего не видно. Проверить без материала —
  режим показа `Buffer Visualization → Custom Stencil`.

### §7.13.3 Пестерь

- `UPesterComponent`: раскладка котомки перед камерой (`PesterOffset`, по
  горизонтали взгляда) по мешочкам `EPesterPouch` — травы, грибы, камни,
  склянки, прочее. Заглушки — `APesterItemActor`, ни один канал их не видит;
  взгляд спрашивает `FindItemUnderView` (луч прямо по форме заглушки).
- Раскладка следует за котомкой (`OnInventoryChanged`); любое открытое окно
  закрывает пестерь (`bIsAnyWidgetOpen`).
- Тем же пестерем раскладывается хранилище (`OpenContainer`, §7.13.4):
  предметы — воспринятыми (`UInventorySlotWidget::PerceiveSingleItem`), у
  станций — строка хода процесса у предмета под взглядом
  (`GetItemProcessStatus`), дальше `ContainerReachCm` (3 м) — закрывается.
- Клавиша: `InventoryAction` — раскрыть/закрыть; `InteractAction` по
  предмету — в руку (из хранилища — в котомку и в руку).

### §7.13.4 Применение из руки

С предметом в руке `Interact` сначала спрашивает цель под взглядом —
`IHeldItemTarget::ReceiveHeldItem` (`Core/Interaction/HeldItemTarget.h`);
цели он ни к чему — взаимодействие как пустой рукой. Цели:

| Цель | Класс | Из руки | Пустой рукой |
|---|---|---|---|
| Котёл | `AAlchemyTableActor` | порция в котёл (одна вода, три травы; квитанции артефактов не принимает) | помешать: шаг ритуала (`TryAdvanceRitual`), иначе варка (`QueueCauldronBrew`); ждущий результат ритуала — зачерпнуть |
| Тайник | `AOrderCacheActor` | зелье исполняет открытый заказ с ближайшим сроком (`FindMostUrgentOpenOrder`), досягаемость 3 м | ничего |
| Жертвенник | `AOfferingStoneActor` | штука в `OfferToCommunity` | ничего |
| Хранилище, станции | `AStorageContainer` и подклассы | штука в хранилище (`TransferItemTo`) | раскладка пестерем (§7.13.3) |
| Хозяин места | `ALandmarkEntityActor` | после «Сделки» на Калиновом мосту — артефакт в плату Змею | разговор (§7.13.6), пока силуэт виден |

На землю (не цель и не ресурс) — `ApplyHeldItemToGround`, по порядку:
артефакт с целью (ниже, «Артефакты») → зелье у логова Болотного царя (приманка) →
вязанка хвороста (лагерь, §7.13.7) → дар у логова Легендарной
(`FindArtifactOfLairAt`) → зелье (полив, `PourPotionOnCell`) → посадочный
материал (`PlantFromSlot`) → перегной (`FertilizeFromSlot`).

Проверка: `pie/03_Alchemy_Stations.md` (котёл, полив, хранилища),
`pie/06_Community_Homestead.md` (тайник, жертвенник, сад),
`pie/07_Zaryana_Journey.md` (дар у логова).

#### Артефакты

`UseArtifactOnCell` — с целью (Гребень, Рог, Фонарь, перья Алконоста, Сирина,
Жар-птицы) на клетку; `UseHeldOnSelf` — у лица (осмотр) и взаимодействие
(Молодильное яблоко, Шапка-невидимка, Зеркальце, Перо Гамаюна, Клубочек —
база строкой выбора §7.13.6). Пути — те же `Use*`-команды. Проверка:
`pie/07_Zaryana_Journey.md` («Артефакты в игре»).

### §7.13.5 Пояс

- `UBeltComponent`: виден при взгляде ниже `BeltPitchDegrees` (−45°).
  Места — `EBeltSlot`: инструмент (`CurrentGatheringTool`, таблица
  `GatheringToolForItem`), контейнер (`ContainerType` котомки), оберег
  (кристалл — `ActivateWardFromItem` на срок, светится до конца окна;
  серебряный — пока висит), семенной мешочек (`CurrentHarvestIntent`).
  Заглушки — `ABeltItemActor`, обновляются на месте.
- Надетое ничего не хранит отдельно и не уходит из котомки; ушло из котомки —
  слот пустеет (`ReleaseMissing`).
- Проверка: `pie/02_Harvest_Inventory.md` («Пояс»).

### §7.13.6 Строка выбора и речь хозяев

- Строка выбора — на контроллере (`BeginChoice`/`MoveChoice`/
  `ConfirmChoice`/`CancelChoice`), экран — `UChoiceLineWidget`: реплика
  субтитром, ответы строками. Колесо мыши — `BindKey(MouseScrollUp/Down)`
  (не Input Action), выбор — `InteractAction`, уйти — `InventoryAction`.
- Разговор: `ALandmarkEntityActor` ловит `ECC_Visibility`; пока актор не
  скрыт и меш видим, `IA_Interact` зовёт `TalkTo` своей клетки. Реплика —
  запись `HostSpeech` в Травник.
- **Проверить в PIE:** что колесо работает при Enhanced Input; не работает —
  нужен Input Action для колеса.
- Проверка: `pie/05_Places_Entities.md` («Хозяева мест: разговор»).

### §7.13.7 Сон, лагерь, автозагрузка

- `SleepUntilDawn` (контроллер): до ближайшего рассвета (`JumpGameClock`),
  заказы решаются до записи, затем `SaveGame`. Лавка —
  `ASleepBenchActor`; лагерь — вязанка «Хворост» из руки на землю.
- Автозагрузка: `AHerbalistPlayerController::BeginPlay`, следующим кадром,
  только в игровом мире и не в автотестах, флаг `bAutoLoadOnStart`
  (Herbalist Settings → Save).
- Проверка: `pie/08_Saves.md`.

### §7.13.8 Отладочные окна и команды (только техническая сторона)

Окна ушли из игры, но живы за командами (для проверки чисел):
`OpenCauldronWindow` (окно варки у котла под взглядом),
`ToggleOrdersUI` (окно заказов), `OpenStorageWindow` (окно переноса у
хранилища под взглядом). Их виджеты:

- `UAlchemyTransferWidget` — вода, три слота трав, витрина результата;
  «Смешать» → `QueueCauldronBrew`; нужен `AlchemyWidgetClass` в Class Defaults
  стола.
- `UInventoryTransferWidget` — два `UInventoryWidget`; нужен
  `TransferWidgetClass` = `WBP_InventoryTransferWidget` у хранилища.
- `UOrdersWindowWidget` — записки и зелья, строится в C++.
- `UInventorySlotWidget` — слот: перетаскивание (`UInventoryDragDropController`),
  реальный индекс по `CreationTime` (`FindRealIndex`), воспринятая копия
  предмета (`ResolvePerceivedItem`), строка процессов станций.
- `UItemTooltipWidget` — воспринятые значения и строка процессов.
- Окна растут под текст: `HerbalistUI::LetSizeBoxesGrowWithContent`
  (`UI/HerbalistWidgetSizing.h`).

`UMemoryRevealWidget` — текст памяти Заряны и знаков на экране (решение
пользователя: пока остаётся, плюс запись в Травник).

## Ввод

`AHerbalistPlayerController::SetupInputComponent` пишет в лог каждую
привязку и `Warning` на незаданный `UInputAction` (задаются в Blueprint
контроллера; раскладка — `Content/Input/IMC_Default`, `IMC_MouseLook`):

| Действие | Что делает |
|---|---|
| `MoveAction`, `LookAction` | движение, камера |
| `HarvestAction` | сбор ресурса под взглядом |
| `InteractAction` | по порядку: подтвердить выбор (§7.13.6) → у лица — на себя (§7.13.4, «Артефакты») → закрыть окно котла → предмет пестеря → пояс → цель из руки (§7.13.4) → земля → `IInteractable` |
| `InventoryAction` | уйти из выбора → убрать предмет из руки → пестерь |
| `InfoAction` | с предметом — осмотр, иначе сведения о клетке |
| `JournalAction` | Травник |
| `ApplyAlchemyAction`, `UsePotionAction` | первое зелье котомки в клетку (прежний путь) |
| колесо мыши | строка выбора (клавиша, не Input Action) |

Проверка: `pie/01_Start.md` (строки `Ввод:`).
