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
    const TArray<FShrine>& GetShrines() const { return Shrines; }
    FShrine* FindShrineAt(const FIntPoint& Cell);
    void SetShrines(const TArray<FShrine>& InShrines) { Shrines = InShrines; }

    // Спад Restoration при небрежении (§15.5) — public, тем же принципом, что
    // RegenerateCellParameters/UpdateEntityManifestations выше: вызывается из
    // Tick() каждый кадр, но и напрямую тестируемо без полной PIE-сессии.
    void UpdateShrines(float DeltaTime);

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

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Zaryana")
    bool IsBuyanReached() const { return bBuyanReached; }
    void SetBuyanReached(bool bInReached) { bBuyanReached = bInReached; }

    const TSet<FName>& GetCollectedFragmentIDs() const { return CollectedFragmentIDs; }
    void SetCollectedFragmentIDs(const TSet<FName>& InIDs) { CollectedFragmentIDs = InIDs; }

    UFUNCTION(Exec, BlueprintCallable, Category = "Test")
    void ShowZaryanaStatus();

    // public тем же принципом, что UpdateShrines/RegenerateCellParameters —
    // тикается из UpdateMemoryFragments, но и напрямую тестируемо.
    void CheckBuyanCondition();

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

    // ---- Заряна: фрагменты памяти и Буян ----
    float GlobalPerceptionClarity = 0.0f;
    bool bBuyanReached = false;
    TSet<FName> CollectedFragmentIDs;   // подлинно собранные — больше не спавнятся

    TWeakObjectPtr<AMemoryFragmentActor> ActiveFragment;   // v1: не больше одного за раз
    float FragmentSpawnCooldownRemaining = 0.0f;
    float FragmentStateCheckAccumulator = 0.0f;

    void TrySpawnStateBasedFragment();
    void SpawnMemoryFragmentAt(FName DefinitionID, const FIntPoint& Cell, bool bIsFalse);

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