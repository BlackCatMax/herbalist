# 7. Заряна, Травник, артефакты, Буян

**Зачем.** Прогрессия пути: воспоминания, артефакты Легендарных, перья,
три исхода у Буяна. Большая часть — длинные условия, поэтому здесь много
отладочных ярлыков: они проверяют механику, а не честный путь к ней.

Команды на менеджере сетки — через `ke *` (`README.md`).

## Состояние

`ke * ShowZaryanaStatus`:

```text
=== ZARYANA STATUS ===
GlobalPerceptionClarity: C (Anchor: A)
ZaryanaCell: (x,y), first false Rosa signal shown: true|false
Buyan reached: true|false (path: N)
Collected fragments (N):
Active fragment in world: <ID>|none
```

## Фрагменты памяти и Роса

| Событие | Что ждать |
|---|---|
| Фрагмент появился | `[Zaryana] Fragment <ID> spawned at (x,y)`; ложный — `[Zaryana] Fragment <ID> (FALSE) spawned at (x,y)` |
| Подобрать (`IA_Interact`) | `[Zaryana] Подлинное воспоминание (<ID>): "…" (Anchor=…, Clarity=…)` или `[Zaryana] Ложное воспоминание (<ID>): "…"`; текст — попапом на экране |
| Не успел | `[MemoryFragment] <ID> at (x,y) faded uncollected` |
| Роса | `[Zaryana] Роса дрогнула сама собой -- первое совпадение (Слой 2, §19.2)` |

## Травник

| Команда | Что ждать |
|---|---|
| `ToggleJournalUI` (клавиша `JournalAction` пока не назначена) | окно Травника; повторно — закрыть |
| `ke * ShowJournal` | `=== TRAVNIK JOURNAL (N entries) ===` и записи — то, что видел игрок, не истинное состояние |

Фильтр Травника: собрать 2–3 разных ингредиента (один — дважды), открыть окно.
В выпадающем списке — только собранные ингредиенты, без повторов по имени;
выбор сужает записи до этого ингредиента, «Все» возвращает полный список;
закрыть и открыть окно — без краша.

## Артефакты в игре (этап 5б, 2026-09-21)

Решения пользователя: с целью — из руки на клетку, на себя — поднести к
лицу и взаимодействие, клубочек — выбор базы строкой.

| Артефакт | Как | Путь |
|---|---|---|
| Гребень, Рог, Фонарь, перья Алконоста, Сирина, Жар-птицы | в руке, взгляд на клетку, `IA_Interact` | `UseComb`/`UseHorn`/`UseLanternDisclosure`/`Use…Feather` на этой клетке |
| Молодильное яблоко, Шапка-невидимка, Зеркальце, Перо Гамаюна | в руке, `IA_Info` (к лицу), `IA_Interact` | `UseYouthApple`/`UseInvisibilityCap`/`UseMirror`/`EatGamayunFeather` |
| Клубочек | в руке, к лицу, `IA_Interact` | строка выбора «Куда вести клубочек?» — базы по номеру и клетке; колесо, `IA_Interact` — `UseYarnBall`; баз нет — `Клубочек: баз нет -- вести некуда (FoundBase)` |
| Зелье у логова Болотного царя | в руке, взгляд на клетку его якоря, `IA_Interact` | приманка `LureSwampTsar` этим зельем, не полив |
| Плата Змею | после ветки «Сделка» — артефакт в руке (не у лица: у лица он применится на себя), взгляд на видимый силуэт Змея на Калиновом мосту, `IA_Interact` | `PayKalinovMostToll`; без сделки и у других хозяев — обычный разговор |

## Зеркальце и Клубочек

| Команда | Что ждать |
|---|---|
| `GiveZaryanaGifts` — отладочный ярлык мимо честной добычи | `GiveZaryanaGifts: mirror and yarn ball granted` |
| `UseMirror` | текст наблюдения Заряны; без зеркальца — `UseMirror: player does not have the mirror` |
| `UseYarnBall N` (N — индекс базы, базы — `FoundBase`, раздел 6) | `UseYarnBall: travelled to base N (… units, +…s game time)` — двигает игровые часы: ночь и фазы луны наступают сразу, короткие обереги и эффекты артефактов истекают, клетки чанков, проснувшихся в точке прибытия, догоняют весь скачок; отстой, сушка и отрастание растений идут от времени мира и клубочком не ускоряются (раздел 3: `slomo`) |

## Артефакты Легендарных

В игре дар кладут у логова (решение пользователя 2026-09-21): предмет в руке,
взгляд на землю клетки-якоря Легендарной, `IA_Interact`. Каждый жест — один
предмет; решает его чистота, артефакт — той сущности, чьё это логово. Отказ
предмет не списывает, и зелье у логова не выливается. Артефакт в дар не идёт:
`OfferForArtifact: '<id>' -- артефакт, в дар не идёт`. Добытый артефакт логово
больше не ждёт — клетка снова обычная земля (полить, посадить). Гребень
Берегини — по команде: у Берегини нет одного логова; Фонарь — только
приманкой (`LureSwampTsar`), даром у логова Болотного царя его не получить.

| Команда | Что ждать |
|---|---|
| `OfferForArtifact <артефакт> "id1,id2"` (сущность проявлена рядом) | `OfferForArtifact: <артефакт> acquired …` и `[Artifact] <артефакт> acquired (…), RealPurity=…, PerceivedPurity=…`; отказ — `OfferForArtifact: <артефакт> not acquired (entity not manifested, already held, or offering too weak)`; полная сумка — `OfferForArtifact: сумка полна -- <артефакт> некуда положить, подношение не тронуто` (подношение из одной штуки освобождает свою строку и в счёт идёт) |
| (прогрев в родном регионе) | `[Artifact] <артефакт> Warmth += … (now …)` |
| `UseHorn X Y` (водная клетка) | диагностика родника попапом; `UseHorn: no Рог, or (x,y) is not water` |
| `UseComb X Y` | `[Artifact] Гребень spent at (x,y), entity cleared: <сущность>` |
| `UseYouthApple` | `[Artifact] Молодильное яблоко spent, Rosa clarity window until T` |
| `UseInvisibilityCap` | `[Artifact] Шапка-невидимка active at (x,y) until T` |
| `UseLanternDisclosure X Y` (прогретый Фонарь) | честное состояние клетки попапом; `UseLanternDisclosure: no Фонарь, not warmed yet, or (x,y) is outside the grid` |
| (Камень-оберег в ритуальной варке) | `[Artifact] Камень-оберег charge spent` |
| `LureSwampTsar X Y <зелье>` (рядом проявлен Болотный царь) | `LureSwampTsar: attempt at (x,y), Фонарь stolen` или `failed`; причина — `[Artifact] Lure potion at (x,y) unconvincing, PerceivedPurity=…` / `… convincing (PerceivedPurity=…) but the roll failed`; полная сумка — `LureSwampTsar: сумка полна -- Фонарь некуда положить, приманка не тронута` |

Имена артефактов: `Рог`, `Гребень`, `Молодильное яблоко`, `Шапка-невидимка`,
`Камень-оберег`, `Фонарь`, `Зеркальце`, `Клубочек`.

## Перья вещих птиц

| Команда | Что ждать |
|---|---|
| `AcquireFeather "Перо Гамаюна"` (`Перо Алконоста`, `Перо Сирина`, `Перо Жар-птицы`) | `[Feather] <перо> acquired`; `AcquireFeather: <перо> not acquired (trigger not met, already held, or unknown feather)`; полная сумка — `AcquireFeather: сумка полна -- <перо> некуда положить` |
| `EatGamayunFeather` | `[Feather] Перо Гамаюна eaten -- Зеркальце's prophetic reading is now guaranteed` |
| `UseAlkonostFeather X Y` | `[Feather] Перо Алконоста spent -- biome N suppressed until T` |
| `UseSirinFeather X Y` (при Malign-спайке в биоме) | `[Feather] Перо Сирина spent at (x,y) during an active Malign spike` |
| `UseZharPtitsaFeather X Y` | `[Feather] Перо Жар-птицы spent -- cell (x,y) marked eternally pure` |

## Буян

| Событие | Что ждать |
|---|---|
| Условие выполнено | `[Zaryana] === БУЯН ДОСТИГНУТ === (AvgDistance=…)` |
| `BecomeBuyanGuardian` / `TradePlacesWithZaryana` / `AcceptBuyanReality` | `[Zaryana] === ПУТЬ У БУЯНА ВЫБРАН: N ===` и финальный фрагмент; раньше срока — `<команда>: not available yet (…)` |

Путь выбирается один раз — для повторной проверки нужен новый запуск PIE или
сейв до выбора (раздел 8).
