// GridWorldManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Math/RandomStream.h"
#include "Core/Simulation/Public/TraceTypes.h"
#include "Core/Simulation/Public/PerceivedTypes.h"
#include "Core/Simulation/Public/PerceptionComponent.h"
#include "Core/BiomeGraph/BiomeGraphTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Shrine/ShrineTypes.h"
#include "Core/Zaryana/MemoryFragmentTypes.h"
#include "Core/Entities/ArtifactTypes.h"
#include "Core/Alchemy/RitualTypes.h"
#include "GridWorldManager.generated.h"

class AHerbalistResourceActor;
class AMemoryFragmentActor;
class AHerbalistEntityActor;
class AHerbalistPlayerController;
class ALandscape;
struct FWorldSnapshot;
struct FStateDelta;
struct FSavedCellState;

UCLASS()
class PROJECTHERBALIST_API AGridWorldManager : public AActor
{
    GENERATED_BODY()

public:
    AGridWorldManager();
	
    // ---- Трассировка ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    bool bEnableTrace = false;

    UFUNCTION(Exec)
    void DumpTrace();

    UFUNCTION(Exec)
    void ReplayLastTick();

    // ---- Snapshot / Delta ----
    FWorldSnapshot CaptureState() const;
    void ApplyStateDelta(const FStateDelta& Delta);

    // ---- Жизненный цикл ----
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ---- Инициализация ----
    void SpawnResourcesInCell(FGridCell& Cell);
    void StartRegeneration(FGridCell& Cell);

    // ---- Command Algebra ----
    void QueueCommand(const FCommandEntry& Cmd);

    // ---- Параметры мира ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    int32 GridSizeX = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    int32 GridSizeY = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float CellHeight = 10.0f;

    // Базовый сид для детерминированного пайплайна (Simulation::ExecutePipeline).
    // Не используется для процедурной генерации мира (см. WorldRNG) — по сиду
    // и номеру тика (CurrentTickID) каждый тик получает свой уникальный, но
    // воспроизводимый RNG-сид, независимый от несвязанных систем (спавн ресурсов и т.п.).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    int32 RngBaseSeed = 12345;

    // Фиксированный шаг симуляционного пайплайна (Command Intake -> ... -> World Apply),
    // в секундах. Не зависит от FPS: сколько бы кадров ни прошло за это время,
    // Pipeline выполнится ровно один раз (см. Tick Execution Model). Не путать с
    // BiomeGraphSubsystem::FixedTimeStep — у распространения биомов свой, более
    // крупный шаг, он не связан с этим.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
    float SimulationFixedTimeStep = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float ResourceRegrowthTime = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    bool bHarvestAffectsBiome = true;
    // HarvestStressIncrement переехал в UHerbalistSettings: он нужен Pipeline'у
    // (ProcessHarvestCommand), а тот до актора не достаёт. Здесь он был объявлен,
    // но не использовался никогда — сбор прибавлял захардкоженные 0.001.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Propagation")
    int32 PropagationDepth = 2;

    // ---- Отладка ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugDraw = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float BorderThickness = 2.0f;

#if WITH_EDITOR
    bool bShowBiomeGraph = false;
    bool bShowCellDistortion = false;
    bool bShowCellInfluence = false;
    void DrawGridDebug();
#endif
    void DrawBiomeGraphDebug();

    // ---- Доступ к клеткам ----
    FGridCell* GetCell(int32 X, int32 Y);
    const FGridCell* GetCellConst(int32 X, int32 Y) const;
    FVector GetCellWorldPosition(int32 X, int32 Y) const;
    FVector GetCellWorldPositionFlat(int32 X, int32 Y) const;
    float GetCellHeight(int32 X, int32 Y) const;

    // Обратное к GetCellWorldPosition — было продублировано в
    // AHerbalistPlayerController::GetCellFromHit, теперь общий метод (тем же
    // используется AAlchemyTableActor::BeginPlay для привязки капища к клетке).
    bool WorldPositionToCell(const FVector& WorldPos, int32& OutX, int32& OutY) const;

    // ---- Алхимия: тонкие обёртки, собирающие FCommandEntry(Apply) и
    // отправляющие его в QueueCommand — реальный расчёт идёт в PipelineV2 ----
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent);
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent);

    // ---- Ритуальная (пошаговая) варка — Core/Alchemy/RitualTypes.h.
    // Внепайплайновая, как и Травник/подношение капищу: продвижение шага и
    // хранение прогресса не идёт через Command/Delta (это не игровая
    // причинность мира, а прогресс-бар конкретного рецепта у конкретного
    // котла), но ЗАВЕРШЕНИЕ ритуала честно варит через тот же
    // Simulation::ExecutePipeline, что и обычная варка. ----
    UPROPERTY()
    TMap<FIntPoint, FActiveRitualState> ActiveRituals;

    // NewIngredients — то, что игрок добавляет ПРЯМО СЕЙЧАС (не накопленное
    // ранее — это уже лежит в ActiveRituals[CauldronCell], если ритуал уже
    // начат). OutPotion заполняется только при ERitualStepResult::Completed.
    ERitualStepResult TryAdvanceRitual(const FIntPoint& CauldronCell, const TArray<FInventoryItem>& NewIngredients,
        FRandomStream& Rng, FInventoryItem& OutPotion);

    // ---- Сад (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31) ----
    // Клетка с зарегистрированной пристройкой (Грибница/Погреб/Водоём/
    // Открытая или Тенистая грядка) — SpawnResourcesInCell/StartRegeneration
    // берут кандидатов из EGardenNiche ингредиента вместо AllowedBiomes
    // клетки (постройка физически подделывает нишу, не переносит биом
    // целиком, см. IngredientRegistrySubsystem::GetRandomResourceForNiche).
    // v1: регистрируется Exec-командой (SetGardenPlot на PlayerController),
    // не физической постройкой-актором — тот же принцип, что и у
    // CurrentGatheringTool: сам механизм работает уже сейчас, экономика/
    // визуал пристроек — отдельный, ещё не реализованный проход.
    UPROPERTY()
    TMap<FIntPoint, EGardenNiche> GardenPlots;

    void RegisterGardenPlot(const FIntPoint& Cell, EGardenNiche Niche);

    // ---- Сбор ----
    FRealState HarvestFromCell(int32 X, int32 Y, const FConditionModifier& Conditions = FConditionModifier());
    FRealState HarvestFromCellSimple(int32 X, int32 Y);
    void ApplyPotionToCell(int32 X, int32 Y, const FRealState& PotionState);
    FRealState CollectWater(int32 X, int32 Y);
    void OnResourceCollected(AHerbalistResourceActor* Actor);

    // ---- Отладка (консольные команды) ----
    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void HarvestTest(int32 X, int32 Y);

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void MassHarvestTest(int32 X, int32 Y, int32 Count);

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ApplyTest(int32 X, int32 Y);

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ShowInventory();

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ShowJournal();

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ShowShrines();

    void SelectCell(int32 X, int32 Y);
    FString GetSelectedCellInfo() const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress);

    // ---- Биомы ----
    TArray<FGridBiomeSample> GetBiomeSamples() const;
    TMap<FName, FVector> GetBiomeCenters() const;
    void ApplyBiomeInfluences(const TMap<FName, float>& MorokFields, const TMap<FName, float>& ZaryanaFields, float GlobalScale);

    // ---- Ресурсы ----
    void SpawnResourceActor(FName IngredientID, int32 X, int32 Y, const FVector& Offset = FVector::ZeroVector);

    // ---- Восприятие ----
    const FPerceivedWorld* GetPerceivedWorld() const;
    const FPerceivedInventory* GetPerceivedInventory() const;

    // ---- Экология: восстановление клеток ----
    void RegenerateCellParameters(float DeltaTime);

    // ---- Проявление сущностей (02_GDD/16_Entity_Manifestation.md, вертикальный срез) ----
    // Внепайплайновый канал, как и RegenerateCellParameters/ApplyBiomeInfluences —
    // вызывается из Tick() каждый кадр, трогает State только через
    // Delta.TargetStateNudges -> ApplyStateDelta (Single Writer соблюдён).
    void UpdateEntityManifestations(float DeltaTime);

    // ---- Суточный цикл (02_GDD/15_Cycles_And_Shrines.md §15.2) ----
    // Минимальная реализация: только фаза суток для Морочников, без луны/сезона.
    // Длительность суток берётся из UHerbalistSettings::GameDayMinutes (уже существовала,
    // но была нигде не подключена к часам).
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    float GetTimeOfDay01() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsNight() const;

    // Рассвет/Закат/Полдень (§15.2, 2026-08-29: раньше эффект был только у
    // Ночи, таблица суток была закрыта на четверть — AUDIT_AND_REFACTORING_PLAN.md §7.2).
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsDawn() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsDusk() const;

    // 0 на входе в Закат, 1 у порога Ночи — "+Distortion (нарастающее)" §15.2.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    float GetDuskProgress01() const;

    // Полудница: короткое окно в середине Дня, только открытые биомы.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsPoludnitsaWindow() const;

    // ---- Лунный цикл (02_GDD/15_Cycles_And_Shrines.md §15.3) ----
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    EMoonPhase GetMoonPhase() const;

    // ---- Годовой круг (02_GDD/15_Cycles_And_Shrines.md §15.4) ----
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    ESeason GetSeason() const;

    // Доля пройденного текущего сезона, [0,1) — 0 в момент смены сезона,
    // ближе к 1 перед следующей сменой. Добавлено 2026-08-29 для карточек
    // §16.2, которым нужно не "какой сезон", а "какой момент внутри сезона"
    // (Листовики — поздний конец Лета как прокси "осени", календарь Купалы —
    // узкое окно внутри Лета). Тот же CycleFraction*3, что уже вычисляет
    // GetSeason(), просто читается дробная часть, а не индекс.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    float GetSeasonProgress01() const;

    // Купальская ночь (02_GDD/15_Cycles_And_Shrines.md §15.4/16_Entity §16.2,
    // "ночь на Купалу, нужно завести в календарь") — узкое окно внутри Лета,
    // не сам факт лета. Лёгкий day-of-year-эквивалент через
    // GetSeasonProgress01(), не полноценный календарь с названиями месяцев —
    // осознанный выбор 2026-08-29, тот же масштаб решения, что и трёхполье.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsKupalaNight() const;

    // Поздний конец Лета — прокси "осени" для Листовиков (§16.2), 2026-08-29
    // по прямому решению пользователя: не заводить четвёртый сезон
    // (переоткрывало бы уже принятое трёхпольное решение), а найти узкое
    // окно внутри существующих трёх. "Осень" читается как последняя,
    // увядающая часть Лета перед Зимой — то же смысловое место в году, что
    // и настоящая осень занимает между летом и зимой.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsLateSummer() const;

    // ---- Погода (02_GDD/15_Cycles_And_Shrines.md §15.7) ----
    // Собственный C++-сигнал, 2026-08-29, по прямому решению пользователя:
    // Ultra Dynamic Weather ещё не установлен в проект (см. §15.7), но три
    // карточки бестиария (Ветряные бесы/Метельники/Вихри) ждать не должны.
    // Детерминированная, без сохраняемого состояния функция от GameClockSeconds
    // (интерполяция value-noise между "погодными фронтами", тот же принцип,
    // что уже даёт CurrentTickID-хэш детерминизм пайплайну) — значения
    // 0..1, тот же формат, что и задокументированные Cached*Intensity §15.7.
    // Когда придёт реальный UDW: заменить тела этих функций на чтение
    // Blueprint-моста, сигнатуры и вызывающий код (bRequiresWeather в
    // AmbientEntityTypes.h) не меняются — ровно то swap-место, которое и
    // обещал §15.7.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    float GetWindIntensity() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    float GetSnowIntensity() const;   // 0 вне Зимы -- снегу неоткуда взяться

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    bool IsWindy() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    bool IsBlizzard() const;   // Metель = сильный ветер + снег одновременно

    // Третий, независимый канал того же шума (Channel=2) — добавлено 2026-08-29
    // для сбора трав (FIngredientTableRow::bRequiresDryWeather, §15.7): в
    // отличие от снега, дождь возможен в любой сезон, не только Зимой.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    float GetRainIntensity() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    bool IsRainy() const;

    // Игровые часы, независимые от GetWorld()->GetTimeSeconds() (движковое,
    // level-relative, обнуляется при перезапуске сессии) — нужны, чтобы фаза
    // суток (и будущая погода через UltraDynamicSky, ROADMAP.md "Реальный
    // Ultra Dynamic Weather") переживала сохранение/загрузку, а не начинала
    // каждую сессию с рассвета.
    // Копится в Tick() на DeltaTime, восстанавливается из сейва при загрузке.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    float GetGameClockSeconds() const { return GameClockSeconds; }
    void SetGameClockSeconds(float InSeconds) { GameClockSeconds = InSeconds; }

    // Воспринятое (S_Perceived) искажение для клетки: базовое Memory.AccumulatedDistortion
    // + ночная надбавка (Морочники) + надбавка от местной проявленной сущности
    // (Гнильники). Единая точка входа вместо прямого чтения Memory.AccumulatedDistortion.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Perception")
    float ComputePerceptionDistortion(int32 X, int32 Y) const;

    // ---- Капища (02_GDD/15_Cycles_And_Shrines.md §15.5, v1: эффекты 1/2/4) ----
    // Капище не ищется отдельно — оно есть там, где стоит котёл
    // (AAlchemyTableActor::BeginPlay регистрирует его на своей клетке).
    // Повторная регистрация на уже занятой клетке не создаёт дубликат.
    void RegisterShrine(const FIntPoint& Cell, EShrineType Type);

    // Домовой (DESIGN_Community_And_Homestead.md §2.1, 2026-08-31) — тот
    // же принцип, что RegisterShrine: не сеется по биому
    // (bManualRegistrationOnly в LandmarkTypes.h), регистрируется напрямую
    // AAlchemyTableActor::BeginPlay на клетке жилища, идемпотентно.
    void RegisterDomovoi(const FIntPoint& Cell);

    // Тип капища по месту, не жёстко Ancestral (находка финального аудита
    // 2026-08-30: единственная точка регистрации, AlchemyTableActor::BeginPlay,
    // передавала EShrineType::Ancestral безусловно — формулы остальных 4 типов
    // §15.5 реализованы и протестированы, но недостижимы игроком). Пограничное —
    // раньше биомной группы: капище физически на стыке биомов (тот же 4-соседский
    // критерий, что уже использует CollectBorderShrineDamping/bRequiresBiomeBorder) —
    // это более редкая и более специфичная позиция, чем просто попадание в биом.
    EShrineType ResolveShrineTypeForCell(const FIntPoint& Cell) const;

    const TArray<FShrine>& GetShrines() const { return Shrines; }
    FShrine* FindShrineAt(const FIntPoint& Cell);
    void SetShrines(const TArray<FShrine>& InShrines) { Shrines = InShrines; }

    // Спад Restoration при небрежении (§15.5) — public, тем же принципом, что
    // RegenerateCellParameters/UpdateEntityManifestations выше: вызывается из
    // Tick() каждый кадр, но и напрямую тестируемо без полной PIE-сессии.
    void UpdateShrines(float DeltaTime);

    // ---- Базы/лагеря (21_Journey_And_Artifacts.md §21.2, GridWorldManagerBases.cpp) ----
    // Проверяет не воду и не дубликат; Biome резолвится от клетки. Тот же
    // принцип идемпотентности, что RegisterShrine/RegisterDomovoi.
    void RegisterBase(const FIntPoint& Cell);

    const TArray<FHerbalistBase>& GetBases() const { return Bases; }
    void SetBases(const TArray<FHerbalistBase>& InBases) { Bases = InBases; }

    // Место варки привязано к дому/базе (§20.2 "место варки — привязано к
    // дому/базе"): true для клетки любого капища (AAlchemyTableActor всегда
    // регистрирует капище на своей клетке — см. RegisterShrine выше, это
    // одна и та же клетка по построению) ИЛИ любой зарегистрированной базы.
    // Физическая постройка-стол на каждой базе — контент/редактор, не код
    // (тот же принцип, что уже у инструментов/сада: механизм есть, визуал —
    // отдельная задача), эта функция не вызывается пока ниоткуда в коде.
    bool IsValidBrewingLocation(const FIntPoint& Cell) const;

    // ---- Общинный кластер (DESIGN_Community_And_Homestead.md §1,
    // 17_Hero_And_Community.md §17.3, реализация 2026-08-31): Молва,
    // Подношение общине, Торговля с общиной — один накопитель, три
    // интерфейса поверх него, не три отдельные системы (см. комментарий
    // у OfferToCommunity ниже). Вне детерминированного пайплайна, тем же
    // принципом, что UpdateShrines/UpdateMemoryFragments — не место/клетка,
    // абстрактная община, WorldSnap.GridState её не описывает. ----

    // [-1, 1], растёт/падает только явным подношением (OfferToCommunity),
    // без пассивного спада — тот же принцип, что уже у Landmark.Respect
    // (§16.3: "у подношения ему нет срока годности").
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Community")
    float Molva = 0.0f;

    // Подношение общине (§1.3 "то, что уже есть, назвать общим именем") —
    // тот же знаковый принцип роста, что уже даёт капищам/хозяевам места
    // (Gain × (Purity − Corruption)), просто по среднему предложенных
    // предметов, не по клетке: община — не место. Возвращает применённое
    // ΔMolva (для лога/обратной связи вызывающей стороне), сам инвентарь
    // не трогает — списание предметов остаётся на вызывающей стороне
    // (AHerbalistPlayerController), тем же разделением обязанностей, что
    // и у остальных Exec-путей этого класса.
    float OfferToCommunity(const TArray<FInventoryItem>& Items);

    // Ценность предмета для общины (§1.2) — Magnitude, взвешенный Purity и
    // обратной редкостью (1/IngredientTableRow::RarityWeight — уже
    // существующее понятие, не новая метрика). Нулевая/неизвестная
    // Ценность (не найден в реестре) — 0, не крах: тот же принцип
    // терпимости к отсутствующим данным, что у GetRow/Classify.
    float ComputeCommunityTradeValue(const FInventoryItem& Item) const;

    // Обмен (§1.2) — курс ЦенностьA/ЦенностьB, домножен на (1 +
    // TradeMolvaRateBonus×Molva). OutReceived получает WantedIngredientID
    // с его собственным BaseState (реестр) и посчитанным Count (минимум 1,
    // если курс вообще положителен — община не выдаёт пустых стопок).
    // false = ничего не найдено в реестре или Offered.Count<=0 — не значит
    // "курс невыгодный", тот случай тоже true с Count=1 (§1.2: "не магазин
    // с ценниками", округление вниз, не отказ).
    bool TryTradeWithCommunity(const FInventoryItem& Offered, FName WantedIngredientID, FInventoryItem& OutReceived) const;

    // ---- Заряна: фрагменты памяти и Буян (обсуждение в сессии 2026-08-24,
    // 06_Progression.md "Прогрессия через Заряну", 15_Cycles_And_Shrines.md
    // §15.5 "Буян как глобальное состояние") ----
    // Тем же принципом, что и остальные внепайплайновые системы —
    // UpdateEntityManifestations/UpdateShrines: тикается из Tick(), но
    // напрямую вызываемо для тестов.
    void UpdateMemoryFragments(float DeltaTime);

    // Событийный триггер (CoherentBrew) — вызывается из RunSimulationStep,
    // там же, где уже читается Coherence удавшейся варки для капищ/Травника.
    void TryTriggerCoherentBrewFragment(const FIntPoint& Cell, float Coherence, float Distortion, float Purity);

    // Вызывается из AMemoryFragmentActor::OnInteract.
    void CollectMemoryFragment(FName DefinitionID, bool bIsFalse, AHerbalistPlayerController* PC, const FIntPoint& Cell = FIntPoint(-1, -1));

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Zaryana")
    float GetGlobalPerceptionClarity() const { return GlobalPerceptionClarity; }
    void SetGlobalPerceptionClarity(float InClarity) { GlobalPerceptionClarity = InClarity; }

    // Якорь Clarity (20_Investment_And_Progression.md §20.3) — монотонная
    // база от подлинных фрагментов памяти; GlobalPerceptionClarity выше —
    // производная величина, пересчитываемая из якоря + отклика мира.
    float GetClarityAnchor() const { return ClarityAnchor; }
    void SetClarityAnchor(float InAnchor) { ClarityAnchor = InAnchor; }

    float GetClarityResponseSmoothed() const { return ClarityResponseSmoothed; }
    void SetClarityResponseSmoothed(float InResponse) { ClarityResponseSmoothed = InResponse; }

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Zaryana")
    bool IsBuyanReached() const { return bBuyanReached; }
    void SetBuyanReached(bool bInReached) { bBuyanReached = bInReached; }

    // Три исхода у Буяна (18_Ending.md §18.1-18.2, 2026-09-01) — выбранный
    // путь, None пока игрок ничего не выбрал. Persisted тем же путём, что
    // bBuyanReached выше.
    EBuyanPath GetChosenBuyanPath() const { return ChosenBuyanPath; }
    void SetChosenBuyanPath(EBuyanPath InPath) { ChosenBuyanPath = InPath; }

    // Требует bBuyanReached==true и ChosenBuyanPath==None (не переигрывается,
    // §18.1 — выбор один раз, не диалог с возможностью передумать). Путь 1
    // (страж) дополнительно требует высокие GlobalPerceptionClarity И Молву
    // (BuyanGuardianClarityThreshold/BuyanGuardianMolvaThreshold,
    // мгновенный порог — в проекте вообще нет механизма длительности,
    // находка разведки шага 1). Пути 2/3 — без порога, явно так в §18.2
    // ("искушение не должно быть наградой за прогресс"). При успехе
    // переводит ChosenBuyanPath и возвращает true — финальный текст/сцену
    // показывает вызывающая сторона (HerbalistPlayerController), не эта
    // функция: сама развязка — лорная задача 22_Lore_Roadmap.md.
    bool TryChooseBuyanPath(EBuyanPath Path);

    const TSet<FName>& GetCollectedFragmentIDs() const { return CollectedFragmentIDs; }
    void SetCollectedFragmentIDs(const TSet<FName>& InIDs) { CollectedFragmentIDs = InIDs; }

    // NAME_None, если фрагмент сейчас не заспавнен — публично только для
    // тестируемости TrySpawnStateBasedFragment/SpawnMemoryFragmentAt, тем же
    // принципом, что остальные Get*-геттеры внепайплайнового состояния выше.
    // Определение — в GridWorldManagerZaryana.cpp: AMemoryFragmentActor
    // здесь только forward-declared, полный тип нужен для вызова метода.
    FName GetActiveFragmentDefinitionID() const;

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ShowZaryanaStatus();

    // ---- Чёрная роса Заряны (19_Rosa_Signal.md §19.2) ----
    // Слои 1+3: реальное State клетки ZaryanaCell + подмешанное влияние
    // капищ/хозяев в радиусе, растущем с Clarity, + честный шум
    // PerceiveRealState (тот же приём, что уже AlchemySlotWidget.cpp —
    // Rng принадлежит вызывающему, свой фиксированный сид, не WorldRNG).
    FRealState GetZaryanaPerceivedState(FRandomStream& Rng) const;

    // Слой 2 — обнаруживает, что роса (Слой 1) поменялась без прямого
    // применения зелья на ZaryanaCell с прошлого опроса, и один раз за
    // партию помечает это как совпадение. Публично тем же принципом, что
    // CheckBuyanCondition — тикается из UpdateMemoryFragments, но и
    // напрямую тестируемо.
    void UpdateRosaSignal();

    // AAlchemyTableActor::BeginPlay вызывает это на своей клетке (дом/очаг)
    // сразу после регистрации капища/Домового — тот же принцип "дефолт
    // рядом с домом", а явная расстановка ZaryanaCell левел-дизайнером в
    // редакторе (EditAnywhere ниже) не перезаписывается. Первое (и только
    // первое, тот же idempotent-гейт) размещение сразу сеет "испорченный
    // круг" §19.4a вокруг клетки — см. SeedRosaCorruptedCircle ниже.
    void SetZaryanaCellIfUnset(const FIntPoint& Cell);

    // Первый кадр игры (19_Rosa_Signal.md §19.4a, 2026-09-02) — "место
    // вокруг них испорчено... трава полегла неестественно ровным кругом,
    // цвет земли темнее". Вертикальный срез: реальная расстановка
    // Distortion/Corruption клеток вокруг ZaryanaCell радиусом в несколько
    // клеток, спадающая к краям — не катсцена (в проекте нет системы
    // катсцен/Sequencer для нарратива, весь текстовый нарратив идёт через
    // ShowMemoryRevealText/UE_LOG, тот же канал использован здесь). Публично
    // тем же принципом, что и другие Zaryana-хелперы (SeedTestLandmarks и
    // др.) — тестируемо напрямую, не только через SetZaryanaCellIfUnset.
    void SeedRosaCorruptedCircle(const FIntPoint& Center);

    bool IsRosaFirstFalseSignalShown() const { return bRosaFirstFalseSignalShown; }
    void SetRosaFirstFalseSignalShown(bool bInShown) { bRosaFirstFalseSignalShown = bInShown; }

    // public тем же принципом, что UpdateShrines/RegenerateCellParameters —
    // тикается из UpdateMemoryFragments, но и напрямую тестируемо.
    void CheckBuyanCondition();

    // Тем же принципом — публично для прямой тестируемости State-триггеров
    // (LowLocalDistortion/ShrineRestored/HighCommunityTrust) без ожидания
    // полного MemoryFragmentStateCheckInterval через UpdateMemoryFragments.
    void TrySpawnStateBasedFragment();

    // Пересчитывает GlobalPerceptionClarity = Clamp(Max(ClarityAnchor,
    // ClarityAnchor + Response), 0, 1) из текущего ClarityAnchor и мирового
    // отклика (20_Investment_And_Progression.md §20.3). Тикается из
    // UpdateMemoryFragments на том же периодическом опросе, что и
    // TrySpawnStateBasedFragment/CheckBuyanCondition, плюс вызывается сразу
    // при сборе подлинного фрагмента (ClarityAnchor меняется событийно, не
    // только по таймеру) — публично тем же принципом, что и CheckBuyanCondition.
    void RecomputeGlobalPerceptionClarity();

    // ---- Сохранения (Core/Save/HerbalistSaveTypes.h) ----
    TArray<FSavedCellState> CaptureSaveCells() const;
    void ApplySaveCells(const TArray<FSavedCellState>& InCells);

    const TArray<FEntityLandmark>& GetEntityLandmarks() const { return EntityLandmarks; }
    void SetEntityLandmarks(const TArray<FEntityLandmark>& InLandmarks) { EntityLandmarks = InLandmarks; }

    // Подношение "хозяину" (§16.3), тот же принцип, что FindShrineAt.
    FEntityLandmark* FindLandmarkAt(const FIntPoint& Cell);

    // Легендарный ранг (§16.4, LegendaryEntityTypes.h, 2026-08-29) — в
    // отличие от Низшего (амбиентная зона, любая подходящая клетка) и
    // Основного (тоже привязан к одной клетке, но через Respect), сигнал
    // триггера здесь на уровне биом-графа (MorokField узла), общий на ВСЕ
    // клетки биома разом — без якоря "проявляется на 30+ клетках
    // одновременно" при первом же срабатывании условия, что и многословно,
    // и грязнит четверть сетки за один тик. Якорь — тот же принцип, что
    // EntityLandmarks: одна выделенная клетка на существо, назначается
    // один раз при инициализации (SeedLegendaryAnchors), не пересчитывается.
    const TMap<FName, FIntPoint>& GetLegendaryAnchors() const { return LegendaryAnchors; }

    // Общий хелпер (21_Journey_And_Artifacts.md §21.3, 2026-09-01) — не
    // существовал вовсе, весь прежний код инлайнил проверку прямо в цикл
    // тика (UpdateEntityManifestations). Только для 16 сущностей реестра
    // LegendaryEntityTypes.h — Берегиня не входит туда (см. её собственный
    // комментарий в шапке файла), для неё — IsBereginyaManifested() ниже.
    bool IsLegendaryManifested(FName EntityID) const;

    // Берегиня — отдельная ветка (не через LegendaryAnchors, у неё нет
    // фиксированного якоря: любая подходящая клетка Речной поймы может
    // проявить её, GridWorldManagerEntities.cpp). Сканирует все клетки —
    // редкий вызов (по требованию игрока при попытке добыть Гребень), не
    // тиковый путь, дороговизна не имеет значения.
    bool IsBereginyaManifested() const;

    // Артефакты Легендарных (§21.3-21.4, GridWorldManagerArtifacts.cpp) —
    // доступны только когда сущность уже проявлена; честный путь (высокий
    // РЕАЛЬНЫЙ средний Purity подношения) или обманный (высокий только
    // ВОСПРИНЯТЫЙ, через PerceiveRealState на текущей Clarity — та же
    // логика, что уже отличает S_real/S_Perceived в тултипе). Ключ по
    // ArtifactID, не LegendaryID — Гребень не имеет отдельного
    // LegendaryEntityID (пуст в реестре), см. ArtifactTypes.h. Зеркальце/
    // Клубочек (bWarmsCompanionItem) добавляют запись в AcquiredArtifacts
    // на общих основаниях (ревизия "Update docs", §21.2) — вызывающая
    // сторона (HerbalistPlayerController::OfferForArtifact) дополнительно
    // выставляет bHasMirror/bHasYarnBall по результату.
    bool TryAcquireArtifact(FName ArtifactID, const TArray<FInventoryItem>& Offered, bool& bOutViaDeception);

    // Сцена обмана Болотного царя (§21.3, подраздел "Сцена обмана Болотного
    // царя", 2026-09-02) — НЕ подношение лицом к лицу (TryAcquireArtifact
    // выше): обманное зелье-приманка, вылитое рядом с проявленным Царём,
    // физически ворует Фонарь, пока он отвлечён. Тот же принцип S_Perceived-
    // обмана (сравнение воспринятой/реальной Purity), но применён к
    // физическому присутствию в клетке, не к явному подношению — и, в
    // отличие от TryAcquireArtifact, по-настоящему вероятностный исход, не
    // детерминированный порог (см. .cpp). Возвращает true, если ПОПЫТКА
    // состоялась (приманка израсходована вызывающей стороной), независимо
    // от того, удалась ли она — bOutGranted отличает эти два случая, тот же
    // паттерн out-параметра, что уже bOutViaDeception выше.
    bool TryLureSwampTsarWithPotion(const FIntPoint& Cell, const FRealState& PotionState, bool& bOutGranted);

    const TArray<FAcquiredArtifact>& GetAcquiredArtifacts() const { return AcquiredArtifacts; }
    void SetAcquiredArtifacts(const TArray<FAcquiredArtifact>& InArtifacts) { AcquiredArtifacts = InArtifacts; }

    // Прогрев, вариант C (§21.4, GridWorldManagerTick.cpp::RunSimulationStep
    // накапливает Warmth на удачной варке нужного типа в родном регионе).
    // Фонарь — особый случай внутри: читает GlobalPerceptionClarity, не
    // Warmth (см. .cpp). Для артефактов, которых нет в AcquiredArtifacts
    // (не добыты) — всегда false.
    bool IsArtifactWarmed(FName ArtifactID) const;

    // ---- Семь эффектов артефактов (21_Journey_And_Artifacts.md §21.3,
    // 2026-09-01, ревизия "Ending and artifacts"): §21.3 "Принцип баланса"
    // — ни один не открывает эксклюзивный доступ, только ускоряет/упрощает.
    // GridWorldManagerArtifactEffects.cpp. ----

    // Рог (Индрик-зверь) — "слушает воду": честная диагностика (не через
    // PerceiveRealState — сознательно точнее тултипа, это весь смысл
    // предмета) реального состояния клетки-родника, ничего не меняет.
    // Возвращает false, если клетка не вода или артефакт не добыт.
    bool UseHornOnCell(const FIntPoint& Cell, FText& OutDiagnosis) const;

    // Гребень (Берегиня) — расходуемый побег: мгновенно снимает
    // проявленную сущность (Низший/Основной/Легендарный ранг) с указанной
    // клетки, если такая есть, и списывается из AcquiredArtifacts. Флаг+
    // немедленный эффект, не полноценная блокировка передвижения — в
    // проекте нет механики непроходимости клеток, заводить её ради одного
    // предмета не стал (согласовано с пользователем).
    bool UseCombOnCell(const FIntPoint& Cell);

    // Молодильное яблоко (Дуб-старец) — расходуемое временное окно
    // сниженного шума росы Заряны (GetZaryanaPerceivedState) вместо
    // постоянного, как у GlobalPerceptionClarity. Длительность —
    // MolodilnoeYablokoWindowSeconds (черновое число).
    bool UseYouthApple();

    // Шапка-невидимка (Баба-Яга) — временное, повторно используемое:
    // пока активно, подавляет НОВЫЕ проявления Низшего/Легендарного ранга
    // по всей сетке (упрощение "текущей зоны" из главы — полноценного
    // понятия игровой зоны в проекте нет). Не снимает уже проявленное
    // (это Гребень) — только не даёт проявиться новому.
    bool UseInvisibilityCap();
    bool IsInvisibilityCapActive() const;

    // Камень-оберег (Волот) — не Exec-команда: пассивно активен для
    // любой команды Apply, пока в AcquiredArtifacts есть неспущенный
    // заряд (см. FApplyCommand::bBifurcationCharmActive,
    // PipelineV2.cpp::ComputeApplyResult). Списывается после первой варки
    // с активным зарядом в RunSimulationStep, независимо от того, спас ли
    // он реально ("не гарантирует успех" — расходуется самим фактом
    // держания во время варки, не только удачным спасением).
    bool HasUnspentBifurcationCharm() const;

    // Фонарь, прогретая версия (21_Journey_And_Artifacts.md §21.3,
    // 2026-09-01, ревизия "Update docs" — "снято с паузы", разрешено
    // реализовать сейчас, но ТОЛЬКО для прогретого состояния). "На миг
    // показывает настоящее состояние клетки без искажения S_Perceived" —
    // читает Cell->State НАПРЯМУЮ (та же честность, что уже UseHornOnCell),
    // для ЛЮБОЙ клетки, не только воды. Требует и добытого Фонаря, и
    // IsArtifactWarmed("Фонарь") — базовая версия остаётся просто светом.
    bool UseLanternDisclosureOnCell(const FIntPoint& Cell, FText& OutDisclosure) const;

    // ---- Перья вещих птиц (16_Entity_Manifestation.md §16.4, эндгейм-
    // трофеи, 2026-09-02) — Алконост/Гамаюн/Сирин/Жар-птица. Не часть
    // ArtifactTypes.h: ни Алконост, ни Сирин, ни жар-птица не входят в
    // таблицу §21.3 (только Гамаюн — через Зеркальце), у них нет базового
    // "артефакта" в том смысле. Тот же паттерн именованных функций, что
    // GridWorldManagerArtifactEffects.cpp применяет к каждому из семи
    // эффектов, не общий registry-цикл: только четыре штуки, у каждой
    // содержательно разная логика получения/эффекта. GridWorldManager
    // ProphetFeathers.cpp. ----

    // Общий гейт получения — тот же благой полюс §16.4, что уже даёт
    // базовые артефакты Легендарным. FeatherID — точное имя: "Перо
    // Гамаюна"/"Перо Алконоста"/"Перо Сирина"/"Перо Жар-птицы". Гамаюн —
    // единственное из четырёх, требующее ещё и уже добытого и прогретого
    // Зеркальца (см. .cpp) — точная цитата §16.4: "требуют уже добытого
    // базового артефакта (Перо Гамаюна бесполезно без Зеркальца) ИЛИ
    // очень редкого мирового события" — три остальных гейтятся вторым,
    // не первым условием.
    bool TryAcquireProphetFeather(FName FeatherID);

    const TArray<FName>& GetAcquiredFeathers() const { return AcquiredFeathers; }
    void SetAcquiredFeathers(const TArray<FName>& InFeathers) { AcquiredFeathers = InFeathers; }

    // Перо Гамаюна — съедено, навсегда закрепляет вероятностный шанс
    // "усиленного (прогретого) Зеркальце — иногда пророческое" (§21.4) как
    // гарантированный. Требует уже добытого Пера (TryAcquireProphetFeather).
    bool EatGamayunFeather();
    bool IsGamayunPropheticGuaranteed() const { return bGamayunPropheticGuaranteed; }
    void SetGamayunPropheticGuaranteed(bool bIn) { bGamayunPropheticGuaranteed = bIn; }

    // Честное (без шума PerceiveRealState) чтение Заряны — Слои 1+3 §19.2,
    // без Слоя честного шума. Отдельно от GetZaryanaPerceivedState — та же
    // прямая честность, что уже применяют UseHornOnCell/
    // UseLanternDisclosureOnCell к клеткам, только для Заряны.
    FRealState GetZaryanaTrueState() const;

    // Перо Алконоста — масштабированная вверх версия Шапки-невидимки
    // (§21.3): подавляет НОВЫЕ проявления Низшего/Легендарного ранга (тот
    // же охват рангов, что уже Шапка — §21.3 явно не упоминает Основной)
    // на ВЕСЬ указанный биом сразу, не только текущую (в этом коде — всю
    // сетку, см. комментарий у UseInvisibilityCap) зону активации Шапки.
    // Тот же таймер (InvisibilityCapDurationSeconds), не отдельная
    // настройка — прямое указание задачи "тем же таймером".
    bool UseAlkonostFeatherOnBiome(EBiomeType Biome);
    bool IsAlkonostSuppressionActiveForBiome(EBiomeType Biome) const;

    // Перо Сирина — одноразовое: при активном Malign-спайке Легендарного
    // уровня в биоме клетки (X,Y) честно (без искажения S_Perceived, та же
    // прямая честность, что уже UseHornOnCell/UseLanternDisclosureOnCell)
    // показывает Cell.State, не нанося вреда самого спайка игроку.
    bool UseSirinFeatherOnCell(const FIntPoint& Cell, FText& OutDisclosure);

    // Перо Жар-птицы — единственный из четырёх с ПОСТОЯННЫМ эффектом:
    // помечает клетку как никогда не деградирующую (bEternallyPure,
    // HerbalistCoreTypes.h) — исключена из RegenerateCellParameters/
    // амбиентных, основных и легендарных проявлений навсегда.
    bool UseZharPtitsaFeatherOnCell(const FIntPoint& Cell);
    bool IsCellEternallyPure(const FIntPoint& Cell) const;

    int32 GetCurrentTickID() const { return CurrentTickID; }
    void SetCurrentTickID(int32 InTickID) { CurrentTickID = InTickID; }

    // ---- Итерация по клеткам ----
    template<typename TFunc>
    void ForEachCell(TFunc&& Func)
    {
        for (FGridCell& Cell : Cells) Func(Cell);
    }

    template<typename TFunc>
    void ForEachCell(TFunc&& Func) const
    {
        for (const FGridCell& Cell : Cells) Func(Cell);
    }

protected:
    // ---- Данные мира ----
    TArray<FGridCell> Cells;
    FRandomStream WorldRNG;

    // ---- Ландшафт и кеш высот ----
    UPROPERTY()
    TObjectPtr<ALandscape> CachedLandscape;

    UPROPERTY()
    TArray<float> CachedCellHeights;

    void FindAndCacheLandscape();
    void CacheCellHeights();

    // ---- Вспомогательные данные ----
    TSet<int32> RegrowingCells;
    TMap<int32, float> LastHarvestTimeMap;
    const float HarvestCooldown = 0.2f;

    float GameClockSeconds = 0.0f;

    // Клетки, отклонившиеся от детерминированной генерации (DESIGN_World_State.md
    // §3 Вариант A + разбор открытых миров — Valheim/Skyrim и т.п. сохраняют
    // только тронутое, не весь мир). Липкая: раз клетка тронута, остаётся в
    // сейве даже после релаксации обратно к базе — дешевле и надёжнее, чем
    // сверять на выходе "а не совпало ли снова с базой ровно".
    TSet<int32> DirtyCellIndices;

    UPROPERTY()
    TObjectPtr<UPerceptionComponent> PerceptionComponent;

    // ---- Проявление сущностей (16_Entity_Manifestation §16.3, вертикальный срез) ----
    // Клетки-"обиталища" с аккумулятором Respect (Полевик и т.п.).
    // Заполняются автоматически в InitializeCells для тестируемых биомов среза;
    // в продакшене должны стать ручно расставленны дизайнером.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Herbalist|Entities")
    TArray<FEntityLandmark> EntityLandmarks;

    void SeedTestLandmarks();

    // Одна клетка-якорь на Легендарное существо (см. GetLegendaryAnchors
    // выше) — не UPROPERTY/EditAnywhere, как EntityLandmarks: это чисто
    // вычислительный кэш, не авторский контент, TMap ключом FName тоже не
    // рефлексируется движком без доп. работы, а нужды в этом нет.
    TMap<FName, FIntPoint> LegendaryAnchors;
    void SeedLegendaryAnchors();

    // Спавнит/деспавнит AHerbalistEntityActor на клетке, чтобы
    // Cell.ManifestedEntityActor совпадал с Cell.ManifestedEntityID
    // (2026-08-30, "заводим родительские классы для сущностей и связки") —
    // общий на все три ранга бестиария (Низший/Основной/Легендарный), т.к.
    // сам жизненный цикл проявления одинаков у всех троих (см. три прохода
    // в UpdateEntityManifestations, каждый пишет ManifestedEntityID/None тем
    // же способом). RequestedClass — ActorClass конкретного определения
    // (может быть пуст); DefaultClass — класс ранга (AAmbientEntityActor/
    // ALandmarkEntityActor/ALegendaryEntityActor), на который откатываемся,
    // если контент ещё не назначил свой Blueprint-класс.
    void SyncManifestedEntityActor(FGridCell& Cell, TSubclassOf<AHerbalistEntityActor> RequestedClass, TSubclassOf<AHerbalistEntityActor> DefaultClass);

    // ---- Капища ----
    TArray<FShrine> Shrines;

    // ---- Базы/лагеря (21_Journey_And_Artifacts.md §21.2) ----
    TArray<FHerbalistBase> Bases;

    // ---- Артефакты Легендарных (21_Journey_And_Artifacts.md §21.3-21.4) ----
    TArray<FAcquiredArtifact> AcquiredArtifacts;

    // ---- Перья вещих птиц (16_Entity_Manifestation.md §16.4) ----
    TArray<FName> AcquiredFeathers;

    // Перо Гамаюна съедено -- перманентный флаг, переживает потерю самого
    // Пера из AcquiredFeathers (оно расходуется на поедание).
    bool bGamayunPropheticGuaranteed = false;

    // Перо Алконоста — один активный слот подавления (как у Шапки), биом +
    // до какого GameClockSeconds. Следующее применение просто перезаписывает
    // оба поля, тот же принцип, что InvisibilityCapExpiryGameSeconds.
    EBiomeType AlkonostSuppressedBiome = EBiomeType::ForestSteppe;
    float AlkonostSuppressionExpiryGameSeconds = 0.0f;

    // Общая часть GetZaryanaPerceivedState/GetZaryanaTrueState — Слои 1+3
    // §19.2 (реальное State клетки + подмешанное влияние капищ/хозяев в
    // радиусе), без честного шума PerceiveRealState. Вынесена отдельно
    // 2026-09-02 для Пера Гамаюна (GetZaryanaTrueState — то же самое, но без
    // шума вовсе).
    FRealState ComputeZaryanaBlendedState() const;

    // Молодильное яблоко — GameClockSeconds, до которого действует окно
    // сниженного шума росы. 0 = не активно (GameClockSeconds никогда не
    // отрицателен, безопасный сентинел).
    float YouthAppleClarityBoostExpiryGameSeconds = 0.0f;

    // Шапка-невидимка — GameClockSeconds, до которого подавлены новые
    // проявления. Тот же сентинел, что и выше.
    float InvisibilityCapExpiryGameSeconds = 0.0f;

    // ---- Заряна: фрагменты памяти и Буян ----
    float GlobalPerceptionClarity = 0.0f;

    // Якорь (20_Investment_And_Progression.md §20.3, 2026-09-01) — растёт
    // только от подлинных фрагментов, никогда не уменьшается.
    // GlobalPerceptionClarity выше — производная, пересчитывается из этого
    // поля + отклика мира в RecomputeGlobalPerceptionClarity().
    float ClarityAnchor = 0.0f;

    // Сглаженный отклик мира (§20.3, 2026-09-02) — экспоненциально лерпится
    // к сырому Response при каждом RecomputeGlobalPerceptionClarity, не
    // применяется мгновенно. Персистится (HerbalistSaveTypes.h) — иначе
    // перезагрузка сбрасывала бы уже накопленную сходимость к нулю,
    // мгновенно меняя видимую Clarity на месте.
    float ClarityResponseSmoothed = 0.0f;

    bool bBuyanReached = false;

    // Три исхода у Буяна (18_Ending.md §18.1) — None, пока не выбран.
    EBuyanPath ChosenBuyanPath = EBuyanPath::None;
    TSet<FName> CollectedFragmentIDs;   // подлинно собранные — больше не спавнятся

    TWeakObjectPtr<AMemoryFragmentActor> ActiveFragment;   // v1: не больше одного за раз
    float FragmentSpawnCooldownRemaining = 0.0f;
    float FragmentStateCheckAccumulator = 0.0f;

    void SpawnMemoryFragmentAt(FName DefinitionID, const FIntPoint& Cell, bool bIsFalse);

    // ---- Роса Заряны (19_Rosa_Signal.md §19.2) ----
    // (-1,-1) = не размещена — SetZaryanaCellIfUnset (AAlchemyTableActor::
    // BeginPlay) или ручная расстановка в редакторе задают реальное значение.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Zaryana")
    FIntPoint ZaryanaCell = FIntPoint(-1, -1);

    // Слой 2 — состояние опроса, не игровой прогресс: намеренно не
    // персистится (тот же класс полей, что FragmentStateCheckAccumulator
    // выше), кроме итогового bRosaFirstFalseSignalShown ниже.
    float LastRosaRealMagnitude = 0.0f;
    bool bRosaBaselineCaptured = false;
    bool bZaryanaCellTouchedSinceLastPoll = false;

    // Разовая метка на партию — персистится (HerbalistSaveTypes.h), чтобы
    // "первое совпадение" не срабатывало заново после каждой перезагрузки.
    bool bRosaFirstFalseSignalShown = false;

    // ---- Инициализация ----
    UFUNCTION(BlueprintCallable, Category = "World|Init")
    void InitializeCells();

    // ---- Маркеры состояния ----
    void MarkRegrowing(int32 X, int32 Y) { RegrowingCells.Add(Y * GridSizeX + X); }
    void UnmarkRegrowing(int32 X, int32 Y) { RegrowingCells.Remove(Y * GridSizeX + X); }
    void MarkCellDirty(int32 X, int32 Y) { DirtyCellIndices.Add(Y * GridSizeX + X); }

    inline int32 GetCellIndex(int32 X, int32 Y) const { return Y * GridSizeX + X; }

private:
    FTraceRingBuffer TraceBuffer;
    int32 CurrentTickID = 0;

    // ---- Фиксированный шаг симуляции ----
    float SimulationTimeAccumulator = 0.0f;
    void RunSimulationStep();

    // ---- Очередь команд нового пайплайна ----
    TArray<FCommandEntry> PendingCommands;

    // ---- Выделение клетки (отладка) ----
    int32 SelectedX = -1, SelectedY = -1;
};