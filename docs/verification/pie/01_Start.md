# 1. Запуск и паспорт мира

**Зачем.** За минуту понять, что мир собрался, прежде чем проверять
что-либо ещё. Без этого любая следующая проверка отлаживается вслепую.

## Перед запуском

- Карта — `L_TestDev` (`GameDefaultMap` в `Config/DefaultEngine.ini`).
  `L_Playtest` — восемь полос биомов, но без PCG и травы.
- Правки мира **сохранены**. PIE берёт мир из редактора вместе с
  несохранёнными правками, коммандлеты и сейвы — с диска.
- Прошлый лог скопирован, если он нужен: каждый запуск, включая прогон
  тестов, перезаписывает `ProjectHerbalist/Saved/Logs/ProjectHerbalist.log`.
- Консоль в PIE — клавиша `` ` `` (тильда). Подробный вывод своего канала:
  `log LogHerbalistWorld Verbose`.

## Паспорт: строки лога по порядку

Образец — PIE на `L_TestDev`, 2026-09-14.

| Строка | Что значит | Тревога |
|---|---|---|
| `[Layout] клетка 9.00 м (авто), страница 14 кл. = 126 м (авто), чанк 7 кл. = 63 м (авто), действующий радиус симуляции 63 м, сетка 224x224 от (-112, -112), …` | разметка выведена из World Partition | `[Layout] Разметка мира не выведена -- сетка ручная` — запечь разметку (`WorldLayoutSyncBuilder`, `TOOLS_REFERENCE.md`) |
| `InitializeCells: регионов воды N, водных клеток M` | объёмы воды нашлись и накрыли клетки | `M = 0` при поставленном объёме — объём не над сеткой |
| `InitializeCells: X/Y клеток (P%) вне всех ABiomeRegionVolume -- …` | Warning: вне регионов нет ни ресурсов, ни хозяев мест | на `L_TestDev` сейчас 50062 из 50176 вне — один маленький регион, контентная задача |
| `[Entities] Seeded landmark <Имя> at (x,y)` | хозяин места посеян | ни одной строки — регионы не накрывают сетку |
| `[Entities] Seeded legendary anchor <Имя> at (x,y)` | якорь легендарной сущности | то же |
| `[Kurgan] Seeded '<предмет>' at (x,y)` | два кургана | нет строк — нет подходящих клеток |
| `[KalinovMost] ЗмейГорыныч registered at (x,y)` | Змей на Калиновом мосту | |
| `[POI] Seeded: Totem(x,y) Svetloyar(x,y) GoryuchKamen(x,y) Solovey(x,y) KalinovMost(x,y)` | точки интереса — координаты для раздела 5 | |
| `[Shrine] Registered at (x,y), type=N, Restoration=R` | капище, поставленное на карте | |
| `AlchemyTableActor stands on cell (x,y)` | домашний якорь найден | `AlchemyTableActor at … is outside the grid — Домовой/Роса не зарегистрированы` — вся ветка Домового и Росы мертва |
| `[Domovoi] Registered at (x,y)` | Домовой у стола | |
| `IngredientRegistrySubsystem loaded 103 ingredients`, `WaterTypeRegistrySubsystem initialized with 8 water types` | таблицы данных загрузились | 0 — путь к DataTable |
| `Initialized with 8 nodes, 14 edges`, `Biome table initialized successfully.` | граф биомов и таблица биомов | |
| `Ввод: <Действие> -> IA_<…>` и `Ввод: привязано N действий, не назначено M` | какие действия привязаны | `Ввод: <Действие> не назначен в Blueprint'е контроллера` — клавиша молчит, консольная команда работает (сейчас `JournalAction`, `UsePotionAction`) |
| `Grid corruption (auto): 50176 cells, D degrading (P%), Distortion avg=… min=0.250 max=…` | снимок мира раз в 30 с | разбор — раздел 4 |

Строки `UIngredientRegistrySubsystem ещё недоступен …` и
`UWaterTypeRegistrySubsystem ещё недоступен …` — порядок старта подсистем, не
ошибка: реестры загружаются сами при первом чтении.

## Клавиши

Раскладка — ассеты `Content/Input/IMC_Default` и `IMC_MouseLook`; сами
клавиши здесь не перечислены намеренно — они меняются в ассете, а не в коде.
Действия: `IA_Move`, `IA_Look`, `IA_Jump`, `IA_Harvest` (сбор), `IA_Info`
(сведения о клетке под прицелом), `IA_Inventory`, `IA_ApplyAlchemy`,
`IA_Interact`. Что из них привязано в этом запуске — строки `Ввод:` выше.
