---
tags: [technical, current, cycles, shrines, weather]
gdd: "[[15_Cycles_And_Shrines]]"
version: 1.0
based_on: ProjectHerbalist source, 2026-09-22
---
# 15. Круги времени и капища — техническая сторона

Пара к главе [[15_Cycles_And_Shrines|15. Круги времени и капища]], номера
разделов совпадают. Часы, фазы и календарь живут на `AGridWorldManager`
(источник — `GameClockSeconds`, `double`, переживает сохранение). Проверка —
`docs/verification/pie/04_World_State.md` (небо и погода),
`docs/verification/pie/05_Places_Entities.md` (капища). Числа — настройки
`UHerbalistSettings` (Project Settings → Herbalist).

## §15.2 Сутки

- Длина суток — `GameDayMinutes` (32 реальные минуты); сутки начинаются
  рассветом. Фазы: `IsDawn`, `IsDusk` (с нарастанием `GetDuskProgress01`),
  `IsNight`, окно Полудницы `IsPoludnitsaWindow` (Степь и Лесостепь).
- Эффекты фаз — разлитые по сетке сдвиги `TargetState` в
  `UpdateEntityManifestations` (рассвет — Purity и Stability, закат и ночь —
  Distortion, Полудница — Distortion). Регрессия `Herbalist.DayCycle.*`.
- Перемотка: `SkipGameDays`, `SetGameClock`; сон до рассвета —
  `SleepUntilDawn` ([[07_UX_Tech]] §7.13.7), всё через `JumpGameClock`.

## §15.3 Лунный цикл

`GetMoonPhase()` — 4 фазы по 7 суток. Эффект на сбор
(`GenerateHarvestResult`): Растущая — `MoonWaxingBoostStrength`, Полнолуние —
`MoonFullBoostStrength`; на варку — через `ResolveBrewModifiers`.
Регрессия `Herbalist.MoonPhase.*`.

## §15.4 Годовой круг

Календарь — `Core/Types/HerbalistCalendar.h` (365 суток от 1 марта, четыре
метеорологических сезона, как у Ultra Dynamic Sky): `GetSeason`,
`GetLoreSeason` (три сезона лора — осень внутри Лета), `GetDayOfYear`,
`GetCalendarMonth`, `GetCalendarDay`, `GetSeasonProgress01`, `IsAutumn`,
`IsKupalaNight`. Эффекты: сезонный множитель зарастания (`GetStressDecay`),
зимняя Purity. Регрессия `Herbalist.Season.*`.

## §15.5 Капища

- **На карте** — `AShrineActor` (`Core/Shrine/ShrineActor.h`), ставится
  вручную ([[Level_Assembly]]). Тип — по клетке (`bResolveTypeFromCell`,
  `ResolveShrineTypeForCell`: граница биомов, иначе биомная группа) или
  вручную (`ShrineType`); `InitialRestoration` — стартовое состояние
  [−1, 1]. Регистрация в `BeginPlay` → `FShrine` менеджера
  (`Core/Shrine/ShrineTypes.h`, `GridWorldManagerShrine.cpp`), строка
  `[Shrine] Registered at (x,y)`.
- **Restoration** растёт от зелья, применённого на клетку капища
  (`ShrineOfferingGain` × (Purity − Corruption), в `RunSimulationStep`),
  угасает за `ShrineNeglectDecayDays` (28 суток) в обе стороны от нуля.
  Сохраняется.
- **Эффекты** в радиусе `ShrineInfluenceRadiusMeters` (30 м), выбор
  капища-победителя — `HerbalistCore::Shrine::FindDominantShrine`:
  1. релаксация к S₀ быстрее, от S₀ медленнее — `RegenerateCellParameters`;
  2. надбавка к Coherence варки — `ShrineCoherenceBonus` × Restoration
     (`FWorldSnapshot::Shrines`, `ProcessApplyCommand`);
  3. по типу `EShrineType`: Родовое — `ShrineAncestralStabilityMultiplier`,
     Лесное — `ShrineForestHealBonus`, Водное — `ShrineWaterPurityPullRate`,
     Каменное — `ShrineStoneMorokDampening` (в `ApplyBiomeInfluences`),
     Пограничное — в `UBiomeGraphSubsystem::PropagateWaves`.
- Порог Буяна по капищам — `BuyanShrineRestorationThreshold`.
- Регрессия `Herbalist.Shrine.*`, `Herbalist.ShrineType.*`. Консоль:
  `ke * ShowShrines`.

## §15.5.1 Расстояние с историей

`HerbalistCore::Math::DistanceWithHistory(State, AverageCoherence)` =
`Distance(State, S₀) × Clamp(2 − AverageCoherence, 1, 2)`;
`FMemoryState::AverageCoherence` — EMA Coherence применений на клетку
(`CoherenceHistoryEmaAlpha`, `ProcessApplyCommand`).

## §15.7 Погода и небо

- **Свой сигнал** (C++): `GetWindIntensity`, `GetSnowIntensity`, `IsWindy`,
  `IsBlizzard` — детерминированный шум от часов; гейтит карточки бестиария.
- **Мост к Ultra Dynamic Sky/Weather** — `UUltraDynamicSkyBridge`
  (`Core/World/Sky/UltraDynamicSkyBridge.h`): небо идёт за часами
  симуляции, своего хода у UDS нет; дата и сезон — из календаря. Строка
  `[Sky] Мост UDS/UDW: …` при старте.
- **Листопад** — `ULeafFallSubsystem` (`Core/World/Sky/`).
- **В редакторе:** акторы UDS и UDW на карте; у UDW `Season Mode` — «Use UDS
  Date» ([[Level_Assembly]]).

### Как устроена связь с UDS/UDW

Мост опрашивает акторы каждый тик по именам свойств и функций (UDS/UDW —
чистый Blueprint, C++ API у них нет; имена закреплены тестом
`Herbalist.Sky.BridgeNamesExistInUltraDynamicSky`). На диспетчеры плагина
никто в проекте не подписан.

| Направление | Что | Где |
|---|---|---|
| часы игры → UDS | время суток и дата; свой ход времени UDS выключен | `PushTime` |
| UDS → UDW | сезон из даты (`Season Mode` = Use UDS Date) | сам плагин |
| UDW → симуляция | глобальные дождь, снег, ветер, туман (0–10 → 0–1) | `PullWeather` → `SetWeatherBridgeIntensities` |
| сейв ↔ UDS/UDW | состояние неба и погоды | `CaptureState`, `QueueStateForLoad` |

Нет UDW на карте или он выгрузился — погода возвращается к своему шуму
(`ClearWeatherBridge`).

### Подписка на события времени и погоды

Источник правды — часы и погода симуляции, поэтому события — у менеджера
сетки, а не у неба (`AGridWorldManager`, `GridWorldManagerEntities.cpp`,
`UpdateCycleEvents` — каждый тик после хода часов):

| Делегат | Параметры | Когда |
|---|---|---|
| `OnGameDayStarted` | `DayIndex` (`GetGameDayIndex`) | начались новые сутки (с рассвета) |
| `OnDayPhaseChanged` | новая и прежняя `EDayPhase` (Dawn, Day, Dusk, Night; `GetDayPhase`) | сменилась фаза суток |
| `OnMoonPhaseChanged` | новая и прежняя `EMoonPhase` | сменилась фаза луны |
| `OnSeasonChanged` | новый и прежний `ESeason` | сменился сезон |
| `OnWeatherChanged` | `bRainy`, `bWindy`, `bBlizzard` | сменился хотя бы один из флагов `IsRainy`/`IsWindy`/`IsBlizzard` |

- **C++:** `Manager->OnSeasonChanged.AddDynamic(this, &ThisClass::HandleSeason)`
  — обработчик обязан быть `UFUNCTION()`; отписка — `RemoveDynamic` в
  `EndPlay`.
- **Blueprint:** ссылка на менеджер (`Get Actor Of Class` →
  `GridWorldManager`), затем `Bind Event to On Season Changed` и т. п.
- **Правила.** Первая проверка после старта только запоминает состояние —
  события «с нуля» не приходят; нужное начальное значение читать геттером
  (`GetSeason`, `GetDayPhase`, `IsRainy`). Скачок часов (сон, перемотка,
  загрузка) даёт одно событие с итоговым значением, пропущенные фазы и сутки
  не проигрываются. Порядок в одном тике: сутки → сезон → луна → фаза суток →
  погода. Состояние запоминается до рассылки, так что обработчик может сам
  двигать часы.
- **Погода без UDW** тоже шлёт события — от своего шума.

Почему не диспетчеры UDS/UDW (Sunrise, Sunset, Hourly, Started Raining):
часы UDS неравномерны (6 минут ночи игры растянуты на часы UDS — Hourly
приходит неровно, Sunrise — на середине рассвета игры); прыжок часов при
сне перескакивает время UDS разом; погодные события UDW, вероятно, про
погоду у камеры с учётом локальных зон, а симуляция читает глобальную.
Для чистого визуала (звук, частицы при дожде) диспетчер плагина допустим:
Blueprint-наследник UDS/UDW (класс — в Herbalist Settings, мост найдёт
наследника), Bind Event в его Event Graph. Игровую логику — только на
делегаты менеджера.

Регрессия: `Herbalist.CycleEvents.*`, `Herbalist.Sky.*`.

История и разбор (перенесено из главы 2026-09-22):

#### Реализация (2026-08-29) — `AGridWorldManager::GetWindIntensity/
GetSnowIntensity/IsWindy/IsBlizzard` (`GridWorldManagerEntities.cpp`)

Детерминированная value-noise, БЕЗ сохраняемого состояния — чистая функция
`GameClockSeconds` + `RngBaseSeed`, тот же принцип, что уже даёт
детерминизм пайплайну через `HashCombine(сид, номер тика)`
(`ExecuteTick`): время делится на "погодные фронты"
(`WeatherFrontDurationSeconds`, 480с по умолчанию — четверть игровых
суток), каждому фронту хэш даёт число в [0,1) (`DeterministicNoise01`),
между соседними фронтами — сглаженная интерполяция (`SmoothStep`), чтобы
погода менялась плавно, не мигала каждый тик. Ветер и снег — разные
хэш-каналы (иначе были бы идеально скоррелированы). Снег возможен только
Зимой (`GetSeason()==Winter`, иначе 0 без обращения к шуму вовсе).
`IsWindy()`/`IsBlizzard()` — пороги из `HerbalistSettings.h`
(`WindyThreshold`/`BlizzardWindThreshold`/`BlizzardSnowThreshold`), Метель —
сильный ветер И снег одновременно (значит и Зима неявно, раз снег нулевой
вне её).

**Когда придёт настоящий UDW: swap-точка ровно такая, как обещал этот
раздел с самого начала** — заменить тела этих четырёх функций на чтение
Blueprint-моста (см. "Мост в C++" ниже, без изменений), сигнатуры и
вызывающий код (`bRequiresWeather` в `AmbientEntityTypes.h`,
`UpdateEntityManifestations`) трогать не нужно.

Заодно, тем же днём, два смежных дизайн-решения той же природы
("реализовать без ожидания недостающей системы"):

- ~~**`GetSeasonProgress01()` + `IsLateSummer()`** — Листовики ("осень")
  читают конец Лета как прокси осени, не заводя `ESeason::Autumn`~~ —
  заменено календарём 2026-09-16 (§15.4): `ESeason::Autumn` в механике,
  Листовики — `IsAutumn()`, по лору осень всё так же часть Лета.
- **`IsKupalaNight()`** — ~~лёгкое день-года-окно внутри Лета
  (`KupalaWindowStart/End`)~~ — с 2026-09-16 ночь на 24 июня по календарю.

Подробности всех трёх решений (пороги, коллизии в реестре бестиария,
обоснование чисел) — `16_Entity_Manifestation.md`, "Погода/календарь
ВЫПОЛНЕНЫ". 75/75 тестов зелёные (5 новых —
`Herbalist.Weather.*`/`Herbalist.AmbientEntity.*Weather/Listoviki/Kupalskye*`).

#### Мост реализован (2026-09-04) — плагин физически в проекте

> **2026-09-16:** мост целиком в C++ — `UUltraDynamicSkyBridge`
> (`Core/World/Sky/`), через отражение по именам Blueprint'ов UDS/UDW, без
> Blueprint-кода: время суток и дата → UDS, сезон UDW от даты UDS
> (метеорологический), погода `Global Weather State` UDW →
> `SetWeatherBridgeIntensities`, состояние неба и погоды — в сейв. Инструкция
> ниже про Blueprint-мост в редакторе — история. Подробности —
> `docs/research/DESIGN_Living_Vegetation_Research.md` §5.

Пользователь залил `Content/UltraDynamicSky` (783 файла, оба актора —
`Ultra_Dynamic_Sky.uasset` и `Ultra_Dynamic_Weather.uasset`, полный
Content-only пакет, не C++-плагин). Swap-точка, обещанная разделом с
2026-08-29, сработала ровно по плану ниже ("Мост в C++"), без
переписывания сигнатур:

- `AGridWorldManager` получил `CachedRainIntensity`/`CachedSnowIntensity`/
  `CachedWindIntensity`/`CachedFogIntensity` (п.1 плана) и
  `SetWeatherBridgeIntensities(Rain, Snow, Wind, Fog=0)` (п.2, точка входа
  для будущего Blueprint-моста) — `GridWorldManagerEntities.cpp`.
- `GetWindIntensity/GetSnowIntensity/GetRainIntensity` (п.3) читают кэш,
  если мост хоть раз позвал `SetWeatherBridgeIntensities` (`bWeatherBridgeActive`) —
  иначе прежний шум `SampleWeatherNoise`, БЕЗ ИЗМЕНЕНИЙ. Уровни без моста в
  сцене (все автотесты) ведут себя ровно как раньше — регрессии ноль,
  311/311, два чистых прогона (`Herbalist.Weather.BridgeActiveOverridesNoisePlaceholder`).
- При активном мосте `GetSnowIntensity` больше НЕ форсирует 0 вне
  `ESeason::Winter` — это было правило-заплатка для шумового плейсхолдера,
  у реального моста Cached-поля — чистый passthrough, C++ ему не перечит
  (собственный календарь UDW может не совпадать с трёхпольным сезоном
  проекта один-в-один, и не должен; с 2026-09-16 UDS получает дату
  календаря — этап 2 `docs/research/DESIGN_Living_Vegetation_Research.md`).

**Единственное, что физически не может сделать C++-агент — сам Blueprint-мост
(п.2 плана) и визуальный push времени суток/сезона/луны в UDS.** Требуется
редактор:

1. Поставить `BP_Ultra_Dynamic_Sky` и `BP_Ultra_Dynamic_Weather` (или как
   называются акторы пакета в контент-браузере — `Content/UltraDynamicSky/
   Blueprints/Ultra_Dynamic_Sky.uasset`/`Ultra_Dynamic_Weather.uasset`) на
   `L_TestDev` (сейчас в уровне нет ни одного — проверено).
2. **Push времени суток/луны/сезона (C++ → UDS, визуал, направление уже
   утверждено ниже в "Что такое UDS и UDW") — Blueprint-код НЕ нужен на
   стороне C++ вовсе**, `GetTimeOfDay01()`/`GetMoonPhase()`/`GetSeason()`
   уже `BlueprintCallable`. На Event Tick любого Blueprint-актора в сцене
   (годится сам UDS-актор через Event Graph, или отдельный маленький
   мост): найти `AGridWorldManager` (`Get All Actors Of Class` или
   каст `Get Player Pawn`-владельца), взять `GetTimeOfDay01() * 24`
   (UDS ждёт часы 0..24, не долю 0..1 — уточнить по факту нод `Time of
   Day`/`Set New Time`, найденных строковым поиском в
   `Ultra_Dynamic_Sky.uasset`) и вызвать сеттер времени UDS.
3. **Pull погоды (UDW → C++)** — на Event Tick того же моста читать у
   найденного в сцене UDW его `Wind Intensity`/`Rain Intensity` (оба имени
   подтверждены строковым поиском по `Ultra_Dynamic_Weather.uasset` —
   реальные свойства актора, не выдумка) и звать
   `WorldManager->SetWeatherBridgeIntensities(Rain, Snow, Wind)`. **Снег —
   под вопросом**: явного `Snow Intensity`/`Snow Amount` строковый поиск
   не нашёл (найдены только `Snow Melt Speed Above/Below Freezing` и
   похожие — похоже на то, что UDW сам решает "дождь или снег" по
   температуре, без отдельного паблик-значения снега). Проверить в
   редакторе через список нод `Get...` на найденном в сцене UDW-акторе —
   если отдельного значения нет, третий параметр можно временно не
   передавать (дефолт функции — не трогать, если 0 не тот случай) или
   получать через `Get Weather Info`/`Get Local Weather State` (снапшот
   найден тем же поиском).
4. Точные имена нод — **перепроверить по факту установленной версии**,
   как и предупреждал раздел с самого начала: строковый поиск по бинарнику
   `.uasset` подтверждает, что свойства/функции существуют и называются
   примерно так, но не форму пинов/типов — этого без открытия графа в
   редакторе не увидеть никаким инструментом C++-агента.

Дальше — снова чистый C++, без изменений: `IsWindy/IsBlizzard/IsRainy`
вызывают уже подключенные `Get*Intensity`, `bRequiresWeather`-гейт
бестиария (`EWeatherCondition::Wind/Blizzard`) не тронут вовсе.

#### Что такое UDS и UDW на самом деле — важное уточнение

Это два разных актора, не один:

- **Ultra Dynamic Sky (UDS)** — небо, солнце, луна, облака, время суток.
  Проект **уже** реализует время суток/фазу луны/сезон собственным C++
  (`GameClockSeconds` → `GetTimeOfDay01/IsNight/IsDawn/IsDusk/GetMoonPhase/
  GetSeason`, §15.2-15.4 выше) — **не** через UDS. Если UDS появится в
  сцене, его роль — чисто визуальная (небо должно выглядеть так же, как
  говорит симуляция), а не источник истины: `GameClockSeconds` **пишет** в
  UDS'ов вход времени суток (Blueprint-вызов `Set Sky Sphere`/`Time of Day`
  UDS'а из тика, тем же принципом, что уже применён к материалам мира через
  `MPC_WorldStateFields`), UDS **не** читается симуляцией обратно. Смешать
  роли — завести вторую, несинхронную запись времени суток, ровно то, чего
  избегает Single Writer/Causal Execution Spec, на котором стоит весь пайплайн.
- **Ultra Dynamic Weather (UDW)** — отдельный актор, "находит" UDS в сцене
  сам, добавляет дождь/снег/туман/ветер/пыль/грозу. **Это и есть погода**,
  которую называет `ROADMAP.md` ("Реальный Ultra Dynamic Weather") — не UDS.

#### Blueprint-API UDW (проверено по официальной и открытой документации, 2026-08-29)

Через `WebSearch`/`WebFetch` — `ultradynamicsky.com` (официальная
документация, часть текста недоступна для прямого fetch, но подтверждена
через поисковую выдачу) и открытый обзор Blueprint-API
(`github.com/kevinpbuckley/unreal-engine-skills`, независимый, но детальный
разбор нод плагина). Точные имена нод — **надо перепроверить по факту
установленной версии плагина**, документация меняется между релизами
(6.x/7.x/8.x) — но общая форма API стабильна годами, а значит и форма
интеграции ниже:

- **Значения состояния погоды** (float, примерно 0..1 каждое):
  `Rain`, `Snow`, `Dust`, `Fog`, `Wind Intensity`, `Cloud Coverage`,
  `Thunder/Lightning`. Плюс `Wind Direction` (градусы).
- **Функции-запросы**: `Get Cloud Coverage()`, `Get Wind Intensity()`,
  `Get Local Weather State()` (весь снапшот), `Get Display Name for Current
  Weather()` (человекочитаемое имя пресета — "Rain", "Blizzard",
  "Thunderstorm" и т.п.), `TestActorForWeatherExposure(actor)` (сколько
  дождя/ветра/снега/пыли попадает конкретно на актора — уже готовая замена
  собственному raycast'у, если когда-нибудь понадобится "укрытие от дождя").
- **Управление**: `ChangeWeather(Preset, TransitionSeconds)`,
  `ChangeToRandomWeatherVariation()`.
- **События** (Blueprint-диспетчеры): `Started Raining`/`Finished Raining`,
  `Started Snowing`/`Finished Snowing`, `Getting Cloudy`/`Clouds Clearing`,
  `Getting Foggy`/`Fog Clearing`, `Weather Display Name Changed`, плюс
  отдельный диспетчер на каждое состояние ("State Change - Rain" и т.п.).

#### Мост в C++: тот же паттерн, что уже применён к `GameClockSeconds`

Погода — глобальный (не поклеточный) параметр, ровно как время
суток/сезон — значит НЕ подходит под `EAmbientTriggerAxis`
(`AmbientEntityTypes.h`), который читает `FGridCell` поосно. Нужен тот же
приём, что уже даёт `bRequiresNight`/`bRequiresSeason` у существ §16.2:
третий необязательный гейт, `bRequiresWeather` + `EWeatherCondition`.

Минимальная площадь Blueprint-кода — единственное место, которое
действительно ДОЛЖНО быть на Blueprint (мост к самому плагину), всё
остальное остаётся чистым C++:

1. **Новые поля на `AGridWorldManager`** (C++, как `GameClockSeconds`):
   `float CachedRainIntensity`, `CachedSnowIntensity`, `CachedFogIntensity`,
   `CachedWindIntensity` — обычные `UPROPERTY(BlueprintReadWrite)` float'ы,
   без всякой логики, просто ячейки.

   **Уточнение по факту реализации 2026-08-29:** временный C++-сигнал
   (см. вводный блок раздела выше) обходится БЕЗ кэш-полей вовсе —
   `GetWindIntensity()`/`GetSnowIntensity()` вычисляют значение на лету,
   чистой функцией `GameClockSeconds`, каждый вызов. Кэш не нужен, пока
   нечего кэшировать — писать не в тике не из внешнего Blueprint-моста.
   Когда придёт настоящий UDW, кэш-поля из этого пункта станут актуальны
   ровно потому, что тогда появится источник, которому есть что писать
   раз в тик, а не пересчитывать при каждом обращении.
2. **Один Blueprint-мост** (Blueprint-подкласс `AGridWorldManager`, или
   отдельный маленький Blueprint-актор, слушающий события UDW) — в тике или
   по событию `State Change - *` читает `Get Wind Intensity()` и т.п. у
   найденного в сцене UDW и пишет в поля из пункта 1. Это ЕДИНСТВЕННЫЙ
   Blueprint-код во всей интеграции — четыре присваивания, не игровая логика.
3. **Дальше — снова чистый C++**, тем же способом, что `IsNight()`:
   ```cpp
   bool AGridWorldManager::IsWindy() const
   {
       const UHerbalistSettings* Settings = GetHerbalistSettings();
       return CachedWindIntensity >= (Settings ? Settings->WindyThreshold : 0.5f);
   }
   ```
   и в `FAmbientEntityDefinition` — новое необязательное поле
   `bRequiresWeather` + `EWeatherCondition RequiredWeather` (`Rain`/`Snow`/
   `Fog`/`Wind`/`Blizzard`, где `Blizzard` = `Snow && Wind` оба выше порога,
   не отдельное поле UDW), проверяемое в `UpdateEntityManifestations` тем же
   местом, что уже проверяет `bRequiresNight`/`bRequiresSeason`.

**Важно, тот же принцип, что уже утверждён для Morok/Zaryana-полей и
`GameClockSeconds`:** `Cached*Intensity` — вход в симуляцию, не то, что
симуляция МЕНЯЕТ. Игровой код не должен вызывать `ChangeWeather()` как
побочный канал в обход `FStateDelta` — если когда-нибудь понадобится
"вызвать дождь заклинанием", это тоже идёт через `Delta`/эффект, который
УЖЕ читается мостом из п.2, а не через прямой вызов Blueprint-ноды из C++.

#### Куда это дальше подключается (готовые получатели, не новые системы)

- **Существа §16.2, ждущие погоды** (`16_Entity_Manifestation.md`): Ветряные
  бесы (Лесостепь, "открытая клетка, ветер" → `bRequiresWeather=Wind`,
  `CachedWindIntensity` выше порога, эффект — нестабильность Direction, тот
  же язык, что уже есть у Морока в §16.5); Метельники (Тундра, "активная
  метель/буря" → `bRequiresWeather=Blizzard`, эффект — усиленная
  дезориентация восприятия, надстройка над `PerceiveRealState`, не новая
  ось).
- **✅ Сбор ингредиентов ВЫПОЛНЕНО 2026-08-29.** `FIngredientTableRow`
  (`Core/Data/IngredientTableRow.h`) получил `bRequiresDryWeather` (плюс
  `AllowedSeasons`/`bAutumnOnly`/`HarvestTimeWindow`/`bRequiresMoonPhase`+
  `RequiredMoonPhase` — фактическая связка оказалась шире одной погоды, см.
  `DESIGN_World_State.md §16` статус-блок). Реализация разошлась с планом
  выше в двух местах, оба осознанно: (1) читается не в `GenerateHarvestResult`
  (тот вычисляет уже СОБРАННОГО ингредиента стат-блок, не решает, что вообще
  доступно к сбору), а в `GetRandomResourceForBiome`
  (`IngredientRegistrySubsystem.cpp`) — том самом месте, что уже фильтрует
  пригодность по биому/дистанции состояния (§15 звено 3); (2) не отдельный
  `TArray<EWeatherCondition> AllowedWeather`, а один `bool bRequiresDryWeather`
  — сухая погода оказалась почти единственным погодным условием во всём
  компендиуме (76 карточек), ветер как отдельное условие сбора трав не
  встретился ни разу (в отличие от бестиария §16.2, где он реален). Погода
  расширена третьим каналом value-noise, `GetRainIntensity()`/`IsRainy()` —
  ни Ветер, ни Метель (только Зимой) не покрывали "сухой день" для трав,
  собираемых в основном Весной/Летом.
- **"Хозяева" §16.3 — погода НЕ подключена как условие штрафа Respect**,
  сознательно: 2026-08-29 отдельным решением Respect landmark'а стал меняться
  ТОЛЬКО через явное подношение (Apply-на-клетку-обиталище,
  `GridWorldManagerTick.cpp`), не пассивно от условий сбора вообще —
  расходится с идеей ниже по духу (штраф за "неправильный" сбор), не только
  по погоде. См. `DESIGN_World_State.md §16` статус-блок и `ROADMAP.md`.

#### Что НЕ делать

Не тюнинговать визуальные пресеты/VFX плагина (это визуал, не гейм-дизайн
этого документа) и не заводить точные пороги/числа сейчас — они зависят от
фактической версии установленного плагина и от того, какие значения она
реально отдаёт в разных пресетах; заводить константы до того, как плагин
физически в проекте, значит гадать. Этот раздел фиксирует ФОРМУ моста и
точки подключения, не финальные цифры.
