# Математика мира: справочник для быстрой проверки

Назначение: одно место, где видна ВСЯ непрерывная математика симуляции мира —
формулы, темпы, порядок вызовов и **точки равновесия**. Заведён 2026-09-07
после трёх подряд неудачных правок одного и того же бага: каждая проходила
изолированные юнит-тесты и каждая проваливалась на реальном PIE, потому что
ни у кого (включая меня) не было перед глазами всей цепочки разом.

Не дизайн-документ (тот — `02_GDD/`, `DESIGN_World_State.md`) и не летопись
(`CHANGELOG.md`). Здесь только: что считается, чем, как часто, и к чему это
сходится.

**Правило:** любое изменение формулы ниже обязано пройти чек-лист §7.

---

## 1. Каденции: что как часто тикает

| Процесс | Шаг | Откуда | Что делает |
|---|---|---|---|
| `AGridWorldManager::Tick` | кадр движка | — | гонит все аккумуляторы ниже |
| `RunSimulationStep` (пайплайн команд) | `SimulationFixedTimeStep = 0.05с` | `GridWorldManager.h:127` | команды сбора/варки |
| `UBiomeGraphSubsystem::InternalStep` | `FixedTimeStep = 0.2с` (5/сек) | `BiomeGraphAsset.h:45` | весь биом-граф |
| `UpdateEntityManifestations` | `EntityManifestationIntervalSeconds = 0.1с` | `HerbalistSettings.h:54` | бестиарий, ночь/закат/Полудница/зима |
| `RegenerateCellParameters` | каждый тик, dt кадра | `GridWorldManagerCore.cpp:1950` | бистабильность, контагион, релаксация |
| Игровые сутки | `GameDayMinutes = 32` → 1920 игровых секунд | `HerbalistSettings.h:232` | фазы суток |

Фазы суток (доли суток, при 32 мин): Рассвет `[0, 0.1875]` = 0–360с,
День `(0.1875, 0.625)` = 360–1200с, Закат `[0.625, 0.8125]` = 1200–1560с,
Ночь `[0.8125, 1]` = 1560–1920с. **Окно Полудницы** (§15.2, только Степь/
Лесостепь): `|TimeOfDay01 − 0.40625| < 0.03125` → **720–840с**
(`GridWorldManagerEntities.cpp:207`). Тесты, ставящие `SetGameClockSeconds`,
обязаны учитывать это окно — оно уже один раз испортило тест.

---

## 2. Переменные состояния

| Величина | Диапазон | Кто пишет | Смысл |
|---|---|---|---|
| `Cell.State.Meta.*` | [0,1] | только релаксация (`MoveToward`) + `ApplyStateDelta` | фактическое состояние клетки |
| `Cell.TargetState.Meta.*` | [0,1] | биом-граф, контагион, бистабильность, нуджи суток | цель, к которой ползёт `State` |
| `Cell.Memory.bDegrading` | bool | только бистабильность | «испорченный полюс», липкий |
| `Node.MorokField` | [0,1] | `RecalculateFieldsFromGrid`, `PropagateWaves`, `UpdateMemories` | поле Морока биома |
| `Node.ZaryanaField` | [0,1] | те же три | поле Заряны биома |
| `Node.Memory.*` | [0,1] | `RecordFootprint`, `UpdateMemories` | память биома (History/Instability/AxisDrift) |

---

## 3. Биом-граф: порядок шагов и формулы

`InternalStep(dt=0.2)` (`BiomeGraphSubsystem.cpp:142`) выполняет РОВНО в этом
порядке:

### 3.1 `RecalculateFieldsFromGrid` (:167)
```
GridAvgMorok(B)   = среднее Cell.State.Meta.Distortion по клеткам биома B
GridAvgZaryana(B) = среднее (1 − Cell.State.Meta.Distortion)     ← см. ⚠ ниже
M ← Lerp(M, GridAvgMorok,   GridBlendFactor)      // :202
Z ← Lerp(Z, GridAvgZaryana, GridBlendFactor)      // :203
```
`GridBlendFactor = 0.3` (`BiomeGraphAsset.h:55`), **за шаг, без dt** — ⚠ см. §6.3.

⚠ `ZaryanaValue = 1 − Distortion` (`GridWorldManagerCore.cpp:753`) — Заряна
НЕ независимая величина, а зеркало Морока. Следствие в §6.2.

### 3.2 `PropagateWaves` (:250) — консервативная диффузия
```
для каждого ребра (From→To):
    Flow = M(From) · MorokLeak · GlobalInfluenceScale
    ΔM(To) += Flow;  ΔM(From) −= Flow        // :303-306
M ← clamp(M_prev + ΔM, 0, 1)                 // :311
```
`MorokLeak` в ассете: типично 0.1, у ребра Bog↔Floodplain 0.15.
`GlobalInfluenceScale = 1.0`. **За шаг, без dt** — ⚠ §6.3.
**Инвариант:** `Σ M` по всем узлам не меняется этой функцией (тест
`Herbalist.BiomeGraph.PropagateWavesConservesTotalMorokAcrossAllNodes`).

### 3.3 `ApplyFieldsToGrid` → `AGridWorldManager::ApplyBiomeInfluences` (:778)
Пропускается целиком, если `Cell.Memory.bDegrading` (полюс управляется
бистабильностью, §4.1). «Дырявое ведро» на каждую ось:
```
D ← clamp( D + (M·Push − Decay·D)·dt , 0, 1 )                    // Distortion
S ← clamp( S + (Z·Push − Decay·S)·dt , 0, 1 )                    // Stability
P ← clamp( P + (Z·Push·0.5 − Decay·P)·dt , 0, 1 )                // Purity
```
`MorokDistortionPushRate = MorokDistortionDecayRate = 0.01`,
`ZaryanaEffectPushRate = ZaryanaEffectDecayRate = 0.01`
(`HerbalistSettings.h`). Капище «Каменное» глушит только `Push`:
`Push ×= (1 − 0.4·Restoration)`.

**Равновесие ветки (при Push/Decay = 1):** `D → M`, `S → Z`, `P → 0.5·Z`.

### 3.4 `UpdateMemories(dt)` (:334)
```
M ← M·(1 − GlobalMorokDecay·dt)         GlobalMorokDecay   = 0.01   // :355
Z ← Z·(1 − GlobalZaryanaDecay·dt)       GlobalZaryanaDecay = 0.005
MorokHistory   ← ·(1 − 0.01·dt)
ZaryanaHistory ← ·(1 − 0.005·dt)
Instability ← ·(1 − InstabilityDecay·dt)   InstabilityDecay = 0.025   // :366
AxisDrift   ← ·(1 − AxisDriftDecay·dt)     AxisDriftDecay   = 0.1     // :367
```

### 3.5 Внешний вход (единственный легитимный источник роста)
`RecordFootprint` (:375) — вызывается только из `GridWorldManagerTick.cpp:217`
по факту **Apply/варки игрока**, не по сбору. Пишет в `Node.Memory.*`, НЕ в
`MorokField` напрямую.

---

## 4. Сетка: бистабильность, контагион, релаксация

### 4.1 Бистабильный полюс (`GridWorldManagerCore.cpp:2058`)
```
bDegrading ← Hysteresis(bDegrading, State.Corruption, центр 0.75, поле 0.10)
   вход при Corruption > 0.85, выход при < 0.65
на ПЕРЕХОДЕ (не каждый тик!):
   в полюс   → TargetState = (Corr 1, Purity 0, Dist 1, Stab 0)
   из полюса → TargetState = дефолт биома (FBiomeDefaults::GetDefaultState)
```
Это **единственное** место, где дефолт биома попадает в `TargetState` после
инициализации. Постоянного якоря к дефолту биома в системе НЕТ — см. §6.1.

### 4.2 Контагион (:2100)
```
если bDegrading: для 4 прямых соседей
    Neighbor.Target.Corruption += ContagionSpreadRate·dt     (0.01/с)
    Neighbor.Target.Distortion += ContagionSpreadRate·dt
    Neighbor.Target.Purity     −= ContagionSpreadRate·dt
    Neighbor.Target.Stability  −= ContagionSpreadRate·dt
```

### 4.3 Релаксация State→TargetState (:2225)
```
DeltaRegen = RegenerationRate·dt,  RegenerationRate = 0.0005/с   // :1954
MoveToward(S, T, DeltaRegen)  — ЛИНЕЙНЫЙ шаг, не экспонента     // :2012
```
⚠ Комментарий на :2026 называет сходимость «экспоненциальной» — это неверно,
форма линейная. На корректность `CatchUpActivatedChunks` не влияет (для
линейного приближения догон одним шагом `Step·N` как раз точен), но
формулировку стоит поправить.

**Практическое следствие:** любой сдвиг `TargetState` на 0.3 отыгрывается
`State`-ом за 0.3/0.0005 = **600 секунд**. Всё, что видно в логах как
медленный дрейф `Distortion`, идёт именно с этой скоростью.

### 4.4 Прочие непрерывные нуджи `TargetState` (`GridWorldManagerEntities.cpp`)
| Источник | Формула | Условие |
|---|---|---|
| Ночь | `Dist += NightHorrorDistortionRate·dt` (:1019) | `IsNight()` |
| Закат | `Dist += DuskDistortionRate·DuskProgress01·dt` (:1069) | `IsDusk()` |
| Полудница | `Dist += PoludnitsaDistortionRate·dt` (:1084) | Степь/Лесостепь, окно 720–840с |
| Сущности | `Dist += Def.DistortionRate·dt` и т.п. (:916) | по карточке существа |
| HarvestStress | `Stress −= StressDecay(Biome)·dt` (:2247) | всегда |

---

## 5. Сводная таблица констант

| Константа | Значение | Файл |
|---|---|---|
| `FixedTimeStep` (граф) | 0.2 с | `BiomeGraphAsset.h:45` |
| `GridBlendFactor` | 0.3 /шаг | `BiomeGraphAsset.h:55` |
| `GlobalInfluenceScale` | 1.0 | `BiomeGraphAsset.h:48` |
| `GlobalMorokDecay` | 0.01 /с | `BiomeGraphAsset.h:23` |
| `GlobalZaryanaDecay` | 0.005 /с | `BiomeGraphAsset.h:26` |
| `InstabilityDecay` | 0.025 /с | `BiomeGraphAsset.h` |
| `AxisDriftDecay` | 0.1 /с | `BiomeGraphAsset.h` |
| `MorokLeak` (ребро) | 0.1–0.15 /шаг | `DA_BiomeGraph` |
| `MorokDistortionPushRate/DecayRate` | 0.01 / 0.01 | `HerbalistSettings.h` |
| `ZaryanaEffectPushRate/DecayRate` | 0.01 / 0.01 | `HerbalistSettings.h` |
| `ContagionSpreadRate` | 0.01 /с | `HerbalistSettings.h:905` |
| `BiomeDegradeCenterCorruption` / `Margin` | 0.75 / 0.10 | `HerbalistSettings.h:885` |
| `RegenerationRate` | 0.0005 /с | `GridWorldManagerCore.cpp:1954` |
| `SimulationFixedTimeStep` | 0.05 с | `GridWorldManager.h:127` |

Дефолты `Distortion` по биомам (`DT_BiomeDefaults`): Тайга 0.25, MixedForest
0.28, Тундра 0.30, BroadleafForest 0.30, Лесостепь 0.35, Степь 0.38, Пойма
0.50, Болото 0.70.

---

## 6. Анализ равновесия замкнутого контура

Контур: `Cell.State.Distortion` → (3.1) → `MorokField` → (3.3) →
`Cell.TargetState.Distortion` → (4.3) → `Cell.State.Distortion`.

Обозначим за шаг графа (dt=0.2): `D` — TargetState.Distortion, `S` —
State.Distortion, `M` — MorokField, `b`=0.3, `p`=`k`=0.01, `g`=0.01.

```
(3.1)  M ← M + b·(S − M)
(3.2)  M ← M + ΔM_диффузии            (Σ по узлам = 0, при симметрии ≈ 0)
(3.3)  D ← D + (M·p − k·D)·dt
(3.4)  M ← M·(1 − g·dt)
(4.3)  S → D линейно, шаг 0.0005/с
```

**Стационарность требует одновременно:**
- из (3.3): `M·p = k·D` ⟹ при `p=k`: **`D = M`**
- из (4.3): `S = D`
- из (3.1)+(3.4): `M` пополняется на `b·(S − M)` и теряет `g·dt·M = 0.002·M`.
  Баланс требует `0.3·(S − M) = 0.002·M` ⟹ **`S = 1.00667·M`**

`D = M` и `S = 1.00667·M` при `S = D` совместимы **только при `M = 0`**.

> ### ✅ 6.1 Distortion устойчив — тревога снята замером (была моя ошибка)
> **Первоначальный вывод был неверен.** Я потребовал строгого `D = M` из (3.3)
> и, не найдя совместимости с `S = 1.00667·M` из (3.1)+(3.4), заключил, что
> единственное равновесие — ноль. Ошибка: «дырявое ведро» приходит к своей
> цели асимптотически, и в связанной системе `D` устойчиво стоит **чуть выше**
> `M` — ровно настолько, чтобы блендинг компенсировал декей. Это и есть
> настоящая ненулевая квазиточка равновесия.
>
> **Замер (зонд 2026-09-07, 300 симулированных секунд, самосогласованный
> старт D=0.3, M=0.3):**
> ```
> t=0с    D=0.3000  M=0.3000
> t=60с   D=0.3001  M=0.2982
> t=200с  D=0.3005  M=0.2985
> t=300с  D=0.3007  M=0.2988      D/M = 1.0064  ≈ выведенные 1.00667 ✓
> ```
> Дрейф `M` — 0.4% за 300с (τ ≈ 20 часов игрового времени), практически
> стабильно. Регрессия закреплена тестом
> `Herbalist.BiomeGraph.SelfConsistentRestingStateStaysPut`.
>
> **И PIE-лог 2026-09-06 22:12–22:21 я тоже прочитал неверно:** падал только
> `max` (0.685→0.365) — это одиночный выброс релаксировал к общему уровню,
> при этом `avg` был стабилен (0.111→0.089), а `min` **рос** (0.007→0.024).
> Схождение крайних значений к середине — признак здоровой системы, а не
> осушения. Урок: смотреть на avg/min/max вместе, не на один max.

> ### ⚠ 6.2 Purity/Stability тянутся к функции от Distortion, а не к своим дефолтам
> `Z = 1 − D` (§3.1), а ветка Заряны (§3.3) тянет `Stability → Z`,
> `Purity → 0.5·Z`. Значит равновесие Purity любой клетки — `0.5·(1 − D)`,
> к её собственному дефолту отношения не имеющее. Для Тайги (дефолт Purity
> 0.8, Distortion 0.25) это `0.375` — вдвое ниже её природы, **без единой
> внешней причины**.
>
> **Подтверждено замером** (тот же зонд, старт P=0.7, S=0.6, Z=0.7):
> ```
> t=0с    P=0.7000  S=0.6000
> t=60с   P=0.6701  S=0.6299
> t=200с  P=0.6002  S=0.6818
> t=300с  P=0.5502  S=0.6819    P падает на 0.0005/с (= RegenerationRate),
>                               цель ≈ 0.5·Z = 0.35; S сошёлся к Z = 0.70
> ```
> Purity теряет 0.15 за 300 секунд и продолжает падать — это прямое нарушение
> принципа «биом не портится без внешней причины» (Purity ↓ = порча).
> Отдельно сомнительно само определение «поле Заряны = единица минус Морок»
> (`GridWorldManagerCore.cpp:753`) вместо самостоятельной величины — стоит
> сверить с каноном (`01_Glossary/Zaryana.md`, `DESIGN_World_State.md`).
> **Не чинилось: нужна модель того, чем Заряна должна быть, а это дизайн.**

> ### ⚠ 6.3 Два темпа не масштабированы на dt
> `GridBlendFactor` (§3.1) и `MorokLeak` (§3.2) применяются «за шаг», а
> `GlobalMorokDecay`/`Push`/`Decay` — «в секунду». При `FixedTimeStep = 0.2с`
> это даёт эффективную утечку ~0.75/с против декея 0.01/с — разница на два
> порядка, и она молча поменяется, если когда-нибудь тронуть `FixedTimeStep`.
> Тот же класс, что уже чинился у `GnilnikiNudgeRate`, `Memory.HistoryPurity`,
> `Instability`/`AxisDrift` (см. `AUDIT_AND_REFACTORING_PLAN.md §1.1, §8.1`).

**Про §6.2 — возможные направления (решение за пользователем, не за мной):**
- (а) тянуть `Purity`/`Stability` к дефолтам их биома, а поле Заряны считать
  отклонением от них (симметрично тому, как `Distortion` работает сейчас);
- (б) сделать Заряну самостоятельной величиной, а не `1 − Distortion`
  (тогда нужен источник: подношения, капища, фрагменты Заряны);
- (в) признать, что ambient-ветка Заряны вообще не должна трогать
  `Purity`/`Stability`, и оставить их только бистабильности и явным событиям.

Что бы ни выбрали — §7 п.3 (выписать равновесие) и п.7 (тест на контур)
обязательны, иначе повторится история 2026-09-06/07.

---

## 7. Чек-лист: правка любой формулы выше

1. **Единицы.** Темп «в секунду» умножен на `dt`? Темп «за шаг» — осознанно ли
   он за шаг, и что будет, если шаг изменится?
2. **Двойной счёт.** Величина, которую я добавляю, не приходит ли уже в эту же
   переменную по другому пути того же конвейера? (Сгубило правку 2026-09-07.)
3. **Равновесие.** Выписать стационарные условия ВСЕХ шагов контура и решить
   систему. Если единственное решение — 0 или 1, это баг, а не баланс.
4. **Якорь.** К чему возвращается величина в покое? Это осознанное значение
   (дефолт биома) или случайный побочный ноль?
5. **Полюс.** Учтён ли `bDegrading` (не подрывает ли правка «выход только
   действием игрока»)? И `bEternallyPure`?
6. **Кто ещё пишет.** `grep` по имени поля: сколько систем пишут в него, не
   спорят ли они, соблюдён ли `MarkCellDirty` (иначе потеря при сохранении).
7. **Тест на контур, не только юнит.** Если величину читает обратно другая
   подсистема того же тика — обязателен прогон реального `Tick()` на сотни
   симулированных секунд с проверкой **замедления** прироста
   (`Herbalist.BiomeGraph.AmbientDistortionStabilizesOverLongRealTimeWithoutExternalInput`
   — шаблон). Изолированный юнит-тест этот класс багов НЕ ловит: трижды
   пропустил его 2026-09-06/07.
8. **Окна суток.** Тест не задевает Рассвет/Закат/Ночь/Полудницу (§1)?
9. **Реальный PIE.** Для правок мировой симуляции — прогон
   `ReportGridCorruption` в живой сессии. Дважды за один день зелёные тесты
   не совпали с реальностью.

---

## 8. Открытые вопросы (на 2026-09-07)

| # | Что | Статус |
|---|---|---|
| 6.1 | ~~Контур сходится в ноль~~ | ❌ **опровергнуто замером** — Distortion устойчив, была моя ошибка в анализе; закреплено тестом |
| 6.2 | `Purity`/`Stability` тянутся к `f(Distortion)`, а не к своим дефолтам; `Zaryana = 1 − Morok` | ⚠ **подтверждено замером** (P: 0.70→0.55 за 300с), не чинилось — нужно дизайн-решение |
| 6.3 | `GridBlendFactor`, `MorokLeak` не масштабированы на dt | ⚠ найдено, не чинилось — дремлет, пока `FixedTimeStep` постоянен |
| 4.3 | Комментарий про «экспоненциальную сходимость» противоречит линейному коду | косметика |
