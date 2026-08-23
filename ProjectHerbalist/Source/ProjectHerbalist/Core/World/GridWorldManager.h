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
#include "GridWorldManager.generated.h"

class AHerbalistResourceActor;
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

    // Игровые часы, независимые от GetWorld()->GetTimeSeconds() (движковое,
    // level-relative, обнуляется при перезапуске сессии) — нужны, чтобы фаза
    // суток (и будущая погода через UltraDynamicSky, ROADMAP.md Фаза D §12)
    // переживала сохранение/загрузку, а не начинала каждую сессию с рассвета.
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

    // ---- Сохранения (Core/Save/HerbalistSaveTypes.h) ----
    TArray<FSavedCellState> CaptureSaveCells() const;
    void ApplySaveCells(const TArray<FSavedCellState>& InCells);

    const TArray<FEntityLandmark>& GetEntityLandmarks() const { return EntityLandmarks; }
    void SetEntityLandmarks(const TArray<FEntityLandmark>& InLandmarks) { EntityLandmarks = InLandmarks; }

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

    // ---- Капища ----
    TArray<FShrine> Shrines;

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