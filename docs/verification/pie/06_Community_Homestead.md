# 6. Община, сад, обереги, хозяйство

**Зачем.** Всё это v1-консольное: механика работает, физических построек и
окон нет. Проверка — команда и строка лога. Материалы и пороги — черновые
числа кода (`GardenNicheUnlockTypes.h`, `HerbalistSettings`).

Списки ингредиентов в командах — через запятую, в кавычках: `"bol_01,tai_05"`.

## Молва: подношение и торговля

**Камень-жертвенник** (`AOfferingStoneActor`, 2026-09-21) — в игре подношение
общине кладут на него: предмет в руке, взгляд на камень, `IA_Interact`. Одна
штука уходит из котомки, в логе `[Community] Offered 1 item(s), ΔMolva=…` и
`[Community] Подношение '<id>' на камне: Молва …`. Артефакт или перо на камень
не кладут: `[Community] '<id>' -- артефакт, на камень не кладут`. **Подготовка
в редакторе:** поставить камень у деревни (без меша его видно только
подсветкой — сфера взгляда 60 см).

| Команда | Что ждать |
|---|---|
| `OfferToCommunity "id1,id2"` | `[Community] Offered N item(s), ΔMolva=…, Molva=…`; нет в инвентаре — `OfferToCommunity: no matching items in inventory` |
| `TradeWithCommunity <отдаю> <хочу>` | `[Community] Trade A(xN) -> B(xM), rate=…, Molva=…`; мало Молвы — `[Community] Trade A -> B refused: rate … too low for even 1 unit`; `TradeWithCommunity: '<id>' not found in inventory`; `TradeWithCommunity: not enough room for …` |

**Торговля стеком** (был баг, 2026-08-31; единственная сквозная проверка —
автотест курса без реестра невозможен): собрать один ингредиент трижды, чтобы
лёг одним стеком (`ShowInventory`: количество ≥ 3), отдать его
`TradeWithCommunity`. Стек обязан уйти **целиком**, а не на одну единицу.

## Сад

Порядок: грядка → посадочный материал → посадка → перегной.

| Шаг | Команда | Что ждать |
|---|---|---|
| Грядка с нишей | `SetGardenPlot X Y mycelium` (`cellar`, `pond`, `sunny`, `shade`, `cave`; `none` — снять) | `[Garden] Plot at (x,y) set to niche N`; снять — `[Garden] Plot at (x,y) cleared`. Отказы: `SetGardenPlot: Molva … below threshold …, refused`, `SetGardenPlot: needs N '<материал>' in a single stack, not enough`, `SetGardenPlot: this niche is already built at (x,y)` |
| Посадочный материал | `SetHarvestIntent seed`, затем собрать нужное растение (раздел 2) | в инвентаре — предмет-посадка |
| Посадка | в игре — посадочный материал в руке, взгляд на грядку, `IA_Interact`; отладка — `PlantSeed X Y <id>` | `[Garden] PlantSeedInCell: (x,y) planted with <id>`, одна штука ушла из котомки. Отказы: `PlantSeed: no planting stock of '<id>' in inventory …`, `[Garden] PlantSeedInCell: (x,y) is niche N, species <id> needs niche M, refused`, `… has no garden plot registered` |
| Перегной | в игре — перегной в руке, взгляд на клетку, `IA_Interact`; отладка — `ApplyFertilizer X Y` | `[Fertilizer] ApplyFertilizerToCell: (x,y) Fertility now F`; `ApplyFertilizer: no Peregnoy in inventory` |

Отказ грядки (не та ниша, нет пристройки) оставляет семя в руке: списывается
только посаженное.

Посаженное растение отрастает тем же видом (раздел 2, «Отрастание»). Грядка
с пристройкой без посадки со временем растит растения своей ниши, а не
туземные для биома: у `pond` — пойменные и водные.

## Обереги

В игре оберег вешают на пояс (раздел 2, «Пояс»): кристалл — включён на срок
тем же путём, что `ActivateWard`, серебряный — действует, пока висит. Команды
ниже — для отладки.

| Команда | Что ждать |
|---|---|
| `ActivateWard <кристалл>` (точное имя ряда, например `Плакун-камень`) | `[Ward] BrewBoost active until T`, `[Ward] Concealment active at (x,y) until T` или `[Ward] MorokReduction active at (x,y) until T`; тиражный — `[Ward] Tiered ward activated (Type=N, K home biomes), no expiry`. Отказы: `ActivateWard: no '<id>' in inventory`, `ActivateWard: '<id>' is not a ward` |
| `EquipSilverWard` | `EquipSilverWard: активен`; `EquipSilverWard: no '<id>' in inventory` (оберег — из кургана) |

`T` — момент окончания на игровых часах мира, в секундах.

## База и домашние хранилища

| Команда | Что ждать |
|---|---|
| `FoundBase X Y` | `[Base] Founded at (x,y), biome=N`; `[Base] (x,y) is water — not registered`; `[Base] (x,y) is outside the grid — not registered`; `[Base] (x,y) already registered` |
| `BuildHomeStorage cellar` (`cabinet`, `jar`) | `BuildHomeStorage: built 'cellar' near (x,y)` и `[HomeStorage] Built container type=N near (x,y)`. Отказы: `BuildHomeStorage: Домовой Respect … below threshold …, refused`, `BuildHomeStorage: needs N Дубовая кора (broad_10) in a single stack, not enough`, `BuildHomeStorage: a cellar already exists, refusing a duplicate`, `BuildHomeStorage: no alchemy table (home anchor) found in the world` |

Построенное хранилище раскрывается пустой рукой, кладут в него из руки —
раздел 3. Класс — 
`HomeStorageContainerClass` у `AGridWorldManager`, по умолчанию
`BP_StorageContainer`; не загрузился — `[HomeStorage] HomeStorageContainerClass
не загрузился -- голый AStorageContainer, окно не откроется` (окно — только
отладочное `OpenStorageWindow`; раскладка и рука работают и у голого). Погреб, шкаф и
кувшин стоят рядом со столом на четверть клетки друг от друга. Дубль ищется
только среди построенных: сундук карты, выставленный погребом, постройку не
запрещает.

## Заказы

Люди оставляют у порога дома (клетка Заряны, у стола) записки с заказами —
`02_GDD/24_Orders_And_Repute.md`. Одна-две в игровые сутки, не больше трёх
открытых. Кто пишет, решает Молва: при добрых — селяне и ратные люди, около
нуля — все понемногу, при дурной — только лихие.

**Тайник** (с 2026-09-21 без окна): взять зелье в руку (пестерь, раздел 2),
смотреть на тайник (`AOrderCacheActor` — поставьте один-два на уровень рядом
с домом) и `IA_Interact`. Зелье уходит из котомки и исполняет открытый заказ с
ближайшим сроком: `[Orders] Заказ N исполнен, исход наутро`. Открытых нет —
`[Orders] Открытых заказов нет -- зелье осталось в котомке`; дальше 3 м от
тайника — `[Orders] До тайника не дотянуться -- подойдите ближе`; трава в
руке — тайнику ни к чему; пустой рукой — `[Orders] Тайник: зелье кладут
рукой`.

**Окно заказов** — только для отладки, `ToggleOrdersUI`. Слева записки с номером, текстом, сроком и платой, справа зелья из котомки
(трава туда не попадает). Выбрать записку и зелье → «Отдать»: строка
состояния «Зелье по записке №N оставлено. Исход — наутро». Если тайники на
уровне есть, отдать можно, только стоя у одного из них (3 м); вдали — «Не
отдано: зелье по заказу кладут в тайник». Без тайников — где угодно.

Команды ниже остались для отладки и идут тем же путём:

| Команда | Что ждать |
|---|---|
| `ListOrders` | `[Orders] N: <текст записки> -- осталось X сут.` или `-- отдано, ждём утра`; пусто — `[Orders] Заказов нет` |
| `DeliverOrder N I` | зелье из ячейки котомки `I` (с нуля) отдано по заказу `N`: `[Orders] Заказ N исполнен, исход наутро`; нет заказа — `DeliverOrder: открытого заказа N нет`; в ячейке не зелье — `DeliverOrder: в ячейке I не зелье -- по заказу отдают только сваренное` |
| `RefuseOrder N` | `[Orders] Заказ N отвергнут` |
| `SkipGameDays 1` | наутро: `[Orders] Заказ N (ID): точно/сойдёт/мимо, отклонение …` и изменение Молвы |

Проверка: при старте у порога лежит записка (заглушка-куб, сплющенный в
листок); `IA_Interact` — текст на экране, запись «[Молва] …» в Травнике,
задаток в котомке. Отдать по доброму заказу подходящее зелье, `SkipGameDays 1`
— Молва выросла, плата в котомке; отдать заведомо не то — «не помогло», Молва
упала. Дождаться лихой записки (при Молве около нуля их треть; опустить
Молву можно подношением испорченного — `OfferToCommunity`), исполнить отраву —
через 1–3 суток слух о падеже и порча у дома; отдать лихому не то — Молва
не меняется, но может случиться кража из погреба или порча на пороге.

