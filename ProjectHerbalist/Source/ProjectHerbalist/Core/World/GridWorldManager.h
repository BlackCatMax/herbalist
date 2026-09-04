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
class ABiomeRegionVolume;
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
    // Один ресурс, не вся клетка (2026-09-04) -- вынесено из
    // SpawnResourcesInCell как общий шаг между первичным заселением (цикл
    // по NumResources) и поресурсным отрастанием (StartRegeneration, один
    // вызов на один собранный слот). Context/PlotNiche/ClaimingRegion --
    // общие для всех ресурсов одного вызова, считаются один раз вызывающей
    // стороной, не на каждый ресурс. Публичный ради того же принципа, что
    // и у IsCrowdedBySameEntity -- прямая юнит-проверка без обхода через
    // приватный API.
    bool SpawnOneResourceInCell(FGridCell& Cell, const struct FHarvestContext& Context,
        const EGardenNiche* PlotNiche, ABiomeRegionVolume* ClaimingRegion,
        class UIngredientRegistrySubsystem* IngredientSubsystem);
    // Общее окно условий (сезон/время суток/луна/погода/высота) для всех
    // ресурсов одного момента -- было продублировано между
    // SpawnResourcesInCell и StartRegeneration, вынесено сюда (2026-09-04).
    struct FHarvestContext BuildHarvestContextForCell(const FGridCell& Cell) const;
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

    // 420 с = 7 минут -- середина запрошенного диапазона "минут 5-10"
    // (2026-09-04). Старый дефолт 10 СЕКУНД был откровенно отладочным
    // значением, из-за которого отрастание в PIE выглядело почти
    // мгновенным. Игровое число не для меня выдумывать -- если 7 минут не
    // то, правится один параметр здесь (и одноимённый
    // ABiomeRegionVolume::ResourceRegrowthTimeSeconds для региона).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest")
    float ResourceRegrowthTime = 420.0f;

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

    // Радиус джиттера ресурсов вокруг центра клетки -- единственный
    // источник истины для трёх мест, которые раньше дублировали одну и ту
    // же формулу (`CellSize * 0.3f`): SpawnResourcesInCell,
    // SpawnResourceActor, PreviewResourceSpawnPoints. Половина CellSize --
    // классический "jittered grid" (Cook 1986): offset — независимая
    // равномерная выборка по X и по Y в [-Half, +Half], значит квадрат
    // джиттера ровно совпадает по размеру с самой клеткой и покрывает её
    // целиком, без пустого кольца по краям. 0.3f (найдено 2026-09-03,
    // прямая жалоба пользователя "отвратительный тайлинг") покрывал только
    // 36% площади клетки вокруг центра -- на масштабе всего мира это
    // читалось как решётка кустов с пустыми швами по границам клеток.
    float GetResourceJitterRadius() const { return CellSize * 0.5f; }

    // Тот же принцип, что у GetResourceJitterRadius() выше, но для
    // проявленных сущностей (капище-заглушки, бестиарий) -- доля
    // (UHerbalistSettings::EntityManifestationJitterFraction) от CellSize,
    // не абсолютное число сантиметров (была тем же тайлингом, что и у
    // ресурсов -- см. довод у поля настройки). Меньше, чем у ресурсов:
    // сущность семантически "якорь", не должна плавать по всей клетке.
    // Не inline -- нужен UHerbalistSettings, чей заголовок сюда не тянем.
    float GetEntityManifestationJitterRadius() const;

    // Есть ли уже проявление ЭТОГО ЖЕ вида в радиусе Def.MinSpacingMeters
    // (2026-09-03, жалоба "слишком много существ"). Подавляет только НОВЫЕ
    // проявления -- уже стоящий экземпляр себя не вытесняет, иначе мигал бы
    // каждый такт (тот же приём, что уже у Шапки-невидимки/Пера Алконоста
    // в том же условии). Полное обоснование механизма и почему дистанция в
    // метрах -- у FAmbientEntityDefinition::MinSpacingMeters.
    //
    // Публична ради прямого теста: определения приходят из боевой
    // DT_AmbientEntities через function-local static кэш
    // (GetAmbientEntityDefinitions), подменить их в тесте нечем, а вот
    // передать сюда свой FAmbientEntityDefinition с нужной дистанцией --
    // можно.
    bool IsCrowdedBySameEntity(const FGridCell& Cell, const struct FAmbientEntityDefinition& Def) const;

    // Обратное к GetCellWorldPosition — было продублировано в
    // AHerbalistPlayerController::GetCellFromHit, теперь общий метод (тем же
    // используется AAlchemyTableActor::BeginPlay для привязки капища к клетке).
    bool WorldPositionToCell(const FVector& WorldPos, int32& OutX, int32& OutY) const;

    // PCG-сплайны, спавн внутри формы (2026-09-02) — джиттерит позицию
    // вокруг центра клетки (тот же приём, что уже был у ресурсов), затем,
    // если клетка реально покрыта хотя бы одним ABiomeRegionVolume
    // (CachedBiomeRegions заполнен), до 5 раз передобирает точку, пока та
    // не пройдёт Region->IsPointInside — иначе позиция могла бы визуально
    // уехать за границу формы региона у самого её края. Без регионов
    // (блочный фолбэк, тестовое окружение без волюмов на уровне) — старое
    // поведение, джиттер без проверки, ничего не меняется.
    FVector GetSpawnPositionWithinBiome(int32 X, int32 Y, float JitterRadius, FRandomStream& Rng) const;

    // Клетка "заявлена" реальным сплайном биома, не только блочным
    // фолбэком? (2026-09-02, прямое требование пользователя: "сетка просто
    // хранит переменные, сплайн биома, попадающий на сетку, влияет на
    // спавн того, что присуще биому"). true, если на уровне вообще нет ни
    // одного ABiomeRegionVolume (старое поведение без PCG не меняется) ИЛИ
    // клетка реально покрыта хотя бы одним регионом (Cell.BiomeWeights
    // непуст). false — клетка вне всех регионов при существующей PCG-
    // авторской расстановке: у неё есть какой-то Biome (для математики
    // релаксации/восстановления), но это не значит, что там должен
    // появляться контент этого биома. Используется у спавна ресурсов
    // (SpawnResourcesInCell) и биом-специфичного проявления сущностей
    // (Низший/Берегиня) — НЕ у "сквозной" ночной нечисти §16.5 (та без
    // привязки к биому по дизайну) и не у атмосферных нуджей суток/сезона
    // (те не "контент биома", а свойство места/времени в целом).
    bool IsCellClaimedByBiomeRegion(const FGridCell& Cell) const;

    // Какой именно регион реально заявил эту клетку (2026-09-02, для
    // пер-региональных настроек плотности -- MinResourcesPerCell/
    // MaxResourcesPerCell/ResourceRegrowthTimeSeconds/WaterDensity на самом
    // ABiomeRegionVolume). nullptr — клетка вне всех регионов ИЛИ регионов
    // на уровне вообще нет (в обоих случаях вызывающая сторона откатывается
    // на прежние глобальные дефолты). Если клетку перекрывают несколько
    // регионов одного биома (редкий, не запрещённый на уровне случай) --
    // берётся первый найденный в CachedBiomeRegions, тот же порядок, что
    // уже определяет доминирующий биом в InitializeCells.
    ABiomeRegionVolume* GetClaimingRegion(const FGridCell& Cell) const;

    // ---- Активное множество клеток (2026-09-03, стриминг сетки) ----
    // Данные ВСЕХ клеток всегда в памяти; активность решает лишь, считаются
    // ли на клетке дорогие проходы (релаксация, проявление сущностей,
    // влияние биом-графа). Мировые сканы намеренно НЕ спрашивают активность
    // — им нужен весь мир, и он у них есть.
    //
    // Центры активности берутся у самого World Partition
    // (UWorldPartitionSubsystem::GetStreamingSources) — сетка следует ровно
    // тем же источникам, что и стриминг уровня, включая те, что появятся
    // позже (второй игрок, камера, транспорт). Без партишена — позиция
    // пешки игрока; без неё (headless-тест) центров нет вовсе.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Streaming")
    bool IsCellActive(const FGridCell& Cell) const;

    // Координата чанка, которой принадлежит клетка.
    FIntPoint GetChunkCoordForCell(int32 CellX, int32 CellY) const;

    // Радиус активности в чанках, посчитанный из метров
    // (UHerbalistSettings::ActiveSimulationRadiusMeters) и текущих CellSize/
    // ChunkSizeInCells. -1 = механизм выключен, активно всё.
    int32 GetActiveRadiusInChunks() const;

    // Итерирует только клетки активных чанков — реализация обещания
    // "стримим стоимость, а не данные" (2026-09-03, найдено при разборе
    // жалобы на низкую производительность на масштабе 500x500). До этой
    // правки RegenerateCellParameters/ApplyBiomeInfluences/
    // UpdateEntityManifestations честно ПРОВЕРЯЛИ IsCellActive перед
    // дорогой работой, но сам паттерн `for (FGridCell& Cell : Cells) { if
    // (!IsCellActive(Cell)) continue; ... }` всё равно проходит ВЕСЬ
    // массив — 250 000 клеток на 500x500 — чтобы решить, какие из них
    // пропустить. При активном радиусе в несколько чанков (тысячи клеток)
    // это стократная переплата: сотни тысяч холостых итераций каждый тик
    // ради работы над долями процента массива. ForEachActiveCell вместо
    // "пройти всё и отфильтровать" сразу идёт по ActiveChunks (уже посчитан
    // в CatchUpActivatedChunks на этот кадр) и внутри каждого — по
    // диапазону клеток чанка напрямую, без обращения к остальным.
    //
    // Поведение при выключенном стриминге ИЛИ отсутствии источников
    // активности совпадает с прежним 1:1 (полный проход) -- та же
    // трёхветочная логика, что уже была внутри IsCellActive, просто
    // решается один раз для всего вызова, а не 250 000 раз внутри цикла.
    // ВАЖНО: не переиспользует TSet ActiveChunks (кэш CatchUpActivatedChunks)
    // -- тот валиден только после Tick() этого кадра, а RegenerateCellParameters/
    // ApplyBiomeInfluences/UpdateEntityManifestations вызываются и напрямую,
    // без прогона Tick (тесты вроде GridStreamingTest.cpp, которые задают
    // ActiveChunkCenters через SetActiveChunkCentersForTests и сразу зовут
    // RegenerateCellParameters). Первая версия этой правки полагалась на
    // ActiveChunks и молча обрабатывала ноль клеток в такой сценарий --
    // поймано тестом Herbalist.GridStreaming.RadiusGatesCellsByChunkDistance
    // ("Active cell relaxes towards its target" ожидал Purity > 0, получил
    // нетронутое 0.0). Геометрия теперь считается заново из
    // ActiveChunkCenters/Radius, тем же кодом, что и CatchUpActivatedChunks
    // (общий приватный ComputeChunksWithinRadius ниже) — независимо от того,
    // прогонялся ли в этом кадре Tick.
    void ForEachActiveCell(TFunctionRef<void(FGridCell&)> Func);

    // Клетки одного конкретного чанка напрямую, без обхода остальных --
    // используется и внутри ForEachActiveCell (по одному вызову на каждый
    // активный чанк), и догоном (RegenerateCellParameters с OnlyChunk),
    // который раньше делал тот же полный skip-scan ради одного чанка из
    // тысяч клеток.
    void ForEachCellInChunk(const FIntPoint& Chunk, TFunctionRef<void(FGridCell&)> Func);

    // ---- Размещение ресурсов в мире (2026-09-03) ----
    // Ищет свободную точку в клетке: джиттер внутри формы биома (как
    // раньше) + посадка на поверхность трейсом + проверка, что там ещё
    // никто не стоит. Возвращает false, если за MaxSpawnPlacementAttempts
    // попыток свободного места не нашлось — вызывающая сторона тогда просто
    // не спавнит, и это правильный исход: лучше пустая клетка, чем трава
    // внутри валуна.
    bool FindFreeSpawnPositionInCell(int32 X, int32 Y, float JitterRadius, FRandomStream& Rng, FVector& OutPosition) const;

    // Занята ли точка статической геометрией (ландшафт не в счёт).
    bool IsSpawnPointBlocked(const FVector& Point) const;

    // ---- Превью размещения прямо в редакторе, без запуска игры ----
    // Кнопка в панели Details у размещённого менеджера. Считает те же
    // точки, что посчитал бы спавн, и рисует их: зелёная сфера — место
    // свободно, красная — забраковано занятостью. Так видно ДО рантайма,
    // что трава не полезет в камень.
    UFUNCTION(CallInEditor, Category = "Herbalist|Debug")
    void PreviewResourceSpawnPoints();

    UFUNCTION(CallInEditor, Category = "Herbalist|Debug")
    void ClearResourceSpawnPreview();

    // Сколько клеток максимум обсчитывать в превью — полный проход по
    // 100x100 с трейсами подвесил бы редактор без предупреждения.
    UPROPERTY(EditAnywhere, Category = "Herbalist|Debug", meta = (ClampMin = "1"))
    int32 PreviewMaxCells = 2000;

    // Пересчитать центры активности. Вызывается из Tick; публична, чтобы
    // тест мог задать состояние детерминированно, не гоняя настоящий Tick.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Streaming")
    void UpdateActiveChunkCenters();

    // Явно задать центры (тесты и отладка) — обходит поиск источников.
    void SetActiveChunkCentersForTests(const TArray<FIntPoint>& InCenters) { ActiveChunkCenters = InCenters; }

    const TArray<FIntPoint>& GetActiveChunkCenters() const { return ActiveChunkCenters; }

    // Догон только что активированных чанков: релаксация за всё время, что
    // чанк простоял неактивным, одним шагом. Точно, а не приближённо —
    // релаксация идёт через MoveToward (линейный шаг с остановкой у цели),
    // поэтому один шаг на N*dt даёт ровно то же, что N шагов по dt.
    // Публична ради теста, который сравнивает догон с эталоном непрерывного
    // прогона. Вызывается из Tick после пересчёта активного множества.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Streaming")
    void CatchUpActivatedChunks();

    // Материализовать/усыпить ресурсы чанка (2026-09-03, стриминг). При
    // активации: первичное заселение, если клетка его ещё не проходила,
    // иначе — восстановление ровно того, что стояло (DormantResourceIDs).
    // При деактивации: акторы СЕТКИ уничтожаются, их IngredientID уезжают
    // в DormantResourceIDs; акторы PCG-графа не трогаются.
    void SetChunkResourcesActive(const FIntPoint& Chunk, bool bActive);

    // Уничтожить акторов проявленных сущностей (капище-заглушки-деревья и
    // т.п.) в деактивированном чанке -- найдено пользователем 2026-09-03
    // ("отлично режется чанками растительность, но не деревья-заглушки для
    // entities"). У ресурсов есть симметричная пара
    // SetChunkResourcesActive(true/false); у сущностей активная сторона не
    // нужна отдельной функцией -- UpdateEntityManifestations (через
    // ForEachActiveCell) сама переспавнит актора на следующем проходе по
    // только что активированной клетке, увидев Cell.ManifestedEntityID без
    // актора. Только деактивация требует явного шага: SyncManifestedEntityActor
    // вызывается ИЗНУТРИ UpdateEntityManifestations, а чанк, выпавший из
    // активного множества, эту функцию для своих клеток больше не проходит
    // вовсе -- без этого метода актор оставался бы висеть в мире вечно,
    // сколько бы игрок ни удалялся. Cell.ManifestedEntityID НЕ трогается --
    // это данные "что должно проявиться", переживают деактивацию.
    void DespawnChunkEntities(const FIntPoint& Chunk);

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

    // ---- Сад (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31; шестая
    // пристройка Cave — 2026-09-04) ----
    // Клетка с зарегистрированной пристройкой (Грибница/Погреб/Водоём/
    // Открытая или Тенистая грядка/Пещера) — SpawnResourcesInCell/StartRegeneration
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

    // Посадка (PlantSeed, DESIGN_Community_And_Homestead.md §2.4, 2026-09-04)
    // -- в отличие от RegisterGardenPlot выше (какую нишу подделывает
    // пристройка), это про то, какой КОНКРЕТНО вид посажен в уже
    // существующую пристройку: FGridCell::PlantedSpeciesID персистентно
    // переопределяет вероятностный GetRandomResourceForNiche в
    // SpawnOneResourceInCell/StartRegeneration. SpeciesNiche (IngredientTableRow::
    // GardenNiche растения) резолвится вызывающей стороной (AHerbalistPlayerController::
    // PlantSeed через IngredientRegistrySubsystem) -- та же граница "инвентарный
    // поиск + резолв ряда в контроллере, мировое состояние здесь", что уже
    // держат ActivateWard/OfferToCommunity; эта функция не трогает GameInstance,
    // поэтому напрямую вызываема из автотестов. false + лог -- нет такой
    // клетки/нет пристройки/ниша не совпала с видом (тот же класс валидации,
    // что RegisterGardenPlot/SetGardenPlot).
    bool PlantSeedInCell(const FIntPoint& CellCoord, FName SpeciesID, EGardenNiche SpeciesNiche);

    // ---- Сбор ----
    // HarvestFromCell/HarvestFromCellSimple удалены 2026-09-02 (чистка мёртвого
    // кода): обе с давних пор были заглушками-пустышками ("deprecated, use
    // command-based harvest", возвращали пустой FRealState), никем не
    // вызывались. Настоящий сбор идёт командой через пайплайн, см.
    // OnResourceCollected/PipelineV2.
    void ApplyPotionToCell(int32 X, int32 Y, const FRealState& PotionState);
    FRealState CollectWater(int32 X, int32 Y);
    void OnResourceCollected(AHerbalistResourceActor* Actor);

    // ---- Отладка (консольные команды) ----
    // HarvestTest/MassHarvestTest удалены 2026-09-02 вместе с телом файла
    // GridWorldManagerHarvest.cpp -- обе только логировали "deprecated" и не
    // делали ничего, оставаясь при этом видимыми Exec-командами в консоли.
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
    // OnlyChunk != nullptr — считать только клетки этого чанка (догон при
    // активации, CatchUpActivatedChunks). Обычный вызов из Tick оставляет
    // nullptr и идёт по всем активным клеткам, как раньше.
    void RegenerateCellParameters(float DeltaTime, const FIntPoint* OnlyChunk = nullptr);

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
    //
    // 2026-09-04: UDW физически в проекте (Content/UltraDynamicSky) --
    // swap-точка сработала ровно так, как и была обещана §15.7. Если
    // Blueprint-мост хоть раз позвал SetWeatherBridgeIntensities() ниже
    // (bWeatherBridgeActive==true), эти функции читают Cached*Intensity, а
    // не шум. Без моста в сцене (все автотесты, старые уровни без UDW) --
    // ровно прежнее поведение, шум от GameClockSeconds+RngBaseSeed, ноль
    // регрессии. Сигнатуры и вызывающий код (bRequiresWeather в
    // AmbientEntityTypes.h) не поменялись вовсе, как и было обещано.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    float GetWindIntensity() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    float GetSnowIntensity() const;   // 0 вне Зимы -- снегу неоткуда взяться (только пока мост не активен)

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

    // ---- Мост в C++ от Ultra Dynamic Weather (02_GDD/15_Cycles_And_Shrines.md
    // §15.7, "Мост в C++: тот же паттерн, что уже применён к GameClockSeconds") ----
    // Ровно план из GDD, п.1+3: обычные ячейки-кэш + единственная точка
    // входа для Blueprint-моста (п.2 -- "четыре присваивания, не игровая
    // логика"). Cached*Intensity -- ВХОД в симуляцию, не то, что симуляция
    // меняет: игровой код никогда не пишет сюда и не должен звать
    // ChangeWeather() плагина в обход FStateDelta (тот же принцип, что уже
    // утверждён для Morok/Zaryana-полей).
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Weather")
    float CachedRainIntensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Weather")
    float CachedSnowIntensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Weather")
    float CachedWindIntensity = 0.0f;

    // Пока не подключён ни к одному гейту (нет карточки, которой нужен
    // именно туман) -- ровно "просто ячейка" из плана GDD, заводить порог
    // для несуществующего потребителя значило бы выдумывать геймдизайн.
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Weather")
    float CachedFogIntensity = 0.0f;

    // Различает "моста в сцене нет" (Cached*Intensity==0 по умолчанию, но
    // это НЕ значит "штиль/ясно" -- значит "некому было написать") от "мост
    // есть, и сейчас действительно 0". Без этого флага Get*Intensity не
    // смогли бы решить, читать кэш или всё ещё считать по шуму.
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Weather")
    bool bWeatherBridgeActive = false;

    // Единственный Blueprint-код во всей интеграции (GDD §15.7, п.2) --
    // маленький Blueprint-мост (подкласс AGridWorldManager или отдельный
    // актор в сцене с UDW) на Tick или по событию UDW "State Change - *"
    // читает у найденного в сцене Ultra Dynamic Weather его Wind
    // Intensity/Rain Intensity/... и зовёт это. Клампится на входе -- сами
    // Get*Intensity ниже везде подряд предполагают строго [0,1].
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Weather")
    void SetWeatherBridgeIntensities(float RainIntensity01, float SnowIntensity01, float WindIntensity01, float FogIntensity01 = 0.0f);

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
    // Капище — отдельное МЕСТО, расставляемое левел-дизайнером
    // (AShrineActor::BeginPlay регистрирует его на своей клетке). До
    // 2026-09-02 капище существовало только на клетке котла
    // (AAlchemyTableActor) — развязано по прямому запросу пользователя.
    // Повторная регистрация на уже занятой клетке не создаёт дубликат:
    // обновляет тип, но НЕ трогает накопленное Restoration (и потому
    // InitialRestoration применяется только при создании нового капища).
    void RegisterShrine(const FIntPoint& Cell, EShrineType Type, float InitialRestoration = 0.0f);

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
    // дому/базе"): true для клетки любого капища (AShrineActor, расставляется
    // отдельно от котла с 2026-09-02) ИЛИ любой зарегистрированной базы.
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
    // тика (UpdateEntityManifestations). Покрывает ВСЕ 17 сущностей реестра
    // LegendaryEntityTypes.h, оба механизма триггера (2026-09-02, унификация
    // Берегини): для 16 якорных — быстрый поиск по LegendaryAnchors; если
    // якорь не найден (per-клеточные карточки, bUsesCellHistoryPurity=true,
    // у них никогда нет фиксированного якоря) — fallback, сканирует все
    // клетки (был отдельным методом IsBereginyaManifested(), поглощён сюда).
    // Редкий вызов (по требованию игрока при попытке добыть артефакт), не
    // тиковый путь, дороговизна fallback-сканирования не имеет значения.
    bool IsLegendaryManifested(FName EntityID) const;

    // Артефакты Легендарных (§21.3-21.4, GridWorldManagerArtifacts.cpp) —
    // доступны только когда сущность уже проявлена; честный путь (высокий
    // РЕАЛЬНЫЙ средний Purity подношения) или обманный (высокий только
    // ВОСПРИНЯТЫЙ, через PerceiveRealState на текущей Clarity — та же
    // логика, что уже отличает S_real/S_Perceived в тултипе). Ключ по
    // ArtifactID. Зеркальце/Клубочек (bWarmsCompanionItem) добавляют запись
    // в AcquiredArtifacts на общих основаниях (ревизия "Update docs", §21.2) — вызывающая
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
    // в НАСТОЯЩЕЙ зоне (2026-09-02, "чиним до настоящей зоны" — Chebyshev-
    // радиус InvisibilityCapRadius вокруг клетки игрока в момент
    // применения, IsInvisibilityCapActive(Cell) ниже), не по всей сетке,
    // как раньше. Не снимает уже проявленное (это Гребень) — только не
    // даёт проявиться новому.
    bool UseInvisibilityCap(const FIntPoint& Cell);

    // Общее "активна ли Шапка вообще прямо сейчас" — только таймер, без
    // геометрии. Для проверки конкретной клетки (проявление сущностей) —
    // перегрузка ниже, учитывающая ещё и радиус зоны.
    bool IsInvisibilityCapActive() const;
    bool IsInvisibilityCapActive(const FIntPoint& Cell) const;

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

    // ---- Обереги (кристаллы Пещеры, DESIGN_Community_And_Homestead.md
    // §2.4, 2026-09-04) — GridWorldManagerWards.cpp. Владение (есть ли
    // кристалл в инвентаре игрока) проверяет ВЫЗЫВАЮЩАЯ сторона
    // (AHerbalistPlayerController::ActivateWard, тот же приём, что уже
    // OfferToCommunity/RegisterGardenPlot: инвентарные поиски — дело
    // контроллера, GridWorldManager только хранит мировое состояние
    // эффекта) — эти функции ничего не проверяют, только активируют/читают
    // таймер, тем же GameClockSeconds-паттерном, что уже InvisibilityCap
    // выше. ----

    // BrewBoost (Громовая стрела) — не расходуется, реактивируется свободно
    // (тот же принцип, что Шапка-невидимка): пока в инвентаре есть хотя бы
    // один кристалл, активировать можно снова после истечения окна.
    bool ActivateWardBrewBoost();
    bool IsWardBrewBoostActive() const;

    // EntityConceal (Плакун-камень) — та же настоящая зона, что и у Шапки
    // (Center фиксируется в момент активации), но заметно меньше радиусом
    // (WardConcealmentRadius) — общая проверка без геометрии + проверка
    // конкретной клетки, тот же парный API, что уже IsInvisibilityCapActive().
    bool ActivateWardConcealment(const FIntPoint& Center);
    bool IsWardConcealmentActive() const;
    bool IsWardConcealmentActive(const FIntPoint& Cell) const;

    // MorokReduction (Куриный бог, второй заход 2026-09-04) — тот же
    // Center+Radius приём, что и EntityConceal выше, но читается из
    // ComputePerceptionDistortion, не из гейта проявления сущностей, и
    // только ночью (см. довод у EWardEffectType, HerbalistCoreTypes.h).
    bool ActivateWardMorokReduction(const FIntPoint& Center);
    bool IsWardMorokReductionActive() const;
    bool IsWardMorokReductionActive(const FIntPoint& Cell) const;

    int32 GetCurrentTickID() const { return CurrentTickID; }
    void SetCurrentTickID(int32 InTickID) { CurrentTickID = InTickID; }

    // Тот же сид, что CaptureState() кладёт в FWorldSnapshot::WorldSeed --
    // вынесено отдельно (2026-09-03), чтобы получить его можно было БЕЗ
    // дорогого полного захвата клеток (см. UPerceptionComponent, которому
    // для сида инвентарного восприятия нужен только сид, не сама сетка).
    int32 GetCurrentWorldSeed() const { return static_cast<int32>(HashCombine(static_cast<uint32>(RngBaseSeed), static_cast<uint32>(CurrentTickID))); }

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

    // PCG-биомы (2026-08-31) / спавн внутри формы (2026-09-02) — тот же
    // список, что InitializeCells уже строит через TActorIterator для
    // раскраски клеток, сохранённый для повторного использования во время
    // игры (GetSpawnPositionWithinBiome), не только на старте. Слабые
    // указатели — уровневые акторы теоретически могут быть удалены в
    // редакторе между сборкой этого кэша и следующим спавном ресурса.
    UPROPERTY()
    TArray<TWeakObjectPtr<ABiomeRegionVolume>> CachedBiomeRegions;

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

    // Центр настоящей зоны Шапки (2026-09-02) — клетка игрока в момент
    // применения. (-1,-1) = никогда не применялась (тот же сентинел, что
    // уже ZaryanaCell использует для "не размещена").
    FIntPoint InvisibilityCapCenter = FIntPoint(-1, -1);

    // ---- Обереги (кристаллы Пещеры, §2.4, 2026-09-04) — тот же
    // GameClockSeconds-сентинел, что и все поля выше. Не персистятся
    // (Save/Load) — тот же сознательно принятый класс "короткого окна", что
    // уже не сохраняют InvisibilityCap/YouthApple/Alkonost выше: активация
    // — часть текущей игровой сессии, не долгоживущий прогресс. ----
    float WardBrewBoostExpiryGameSeconds = 0.0f;
    float WardConcealmentExpiryGameSeconds = 0.0f;
    FIntPoint WardConcealmentCenter = FIntPoint(-1, -1);
    float WardMorokReductionExpiryGameSeconds = 0.0f;
    FIntPoint WardMorokReductionCenter = FIntPoint(-1, -1);

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

    // "Выдержано N секунд" (2026-09-02, HerbalistCore::Math::TickSustainedCondition) —
    // опросное состояние, тот же класс поля, что FragmentStateCheckAccumulator
    // выше: не игровой прогресс, намеренно не персистится (перезагрузка просто
    // начинает отсчёт заново, тот же принцип, что уже у Слоя 2 росы). KHLEB_SOL —
    // один скаляр (Molva не привязана к клетке); TISHINA_LESA/OJIDANIE_BURI —
    // per-клеточные аккумуляторы (какая именно клетка выдержала условие, важно
    // для места спавна фрагмента), сбрасываются целиком при первом же провале
    // условия на конкретной клетке.
    float KhlebSolSustainedMolvaSeconds = 0.0f;
    TMap<FIntPoint, float> TishinaLesaHoldSeconds;
    TMap<FIntPoint, float> OjidanieBuriHoldSeconds;

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

    // Накопитель такта проявлений (2026-09-03, см.
    // UHerbalistSettings::EntityManifestationIntervalSeconds). В
    // UpdateEntityManifestations передаётся именно накопленное время, не
    // время кадра — ставки эффектов (rate/сек) остаются точными.
    float EntityManifestationAccumulator = 0.0f;

    // Чанки-центры активного множества (2026-09-03, стриминг сетки) —
    // координаты чанков, в которых сейчас находятся источники стриминга
    // World Partition (или игрок, если партишена нет). Пересчитывается в
    // Tick, читается IsCellActive.
    TArray<FIntPoint> ActiveChunkCenters;

    // Общая геометрия для CatchUpActivatedChunks и ForEachActiveCell: какие
    // координаты чанков попадают в Radius вокруг заданных центров. Вынесено
    // 2026-09-03, чтобы у обоих был один источник истины, а не два похожих
    // тройных цикла, которые легко рассинхронизировать правкой одного и
    // забытым вторым.
    TSet<FIntPoint> ComputeChunksWithinRadius(const TArray<FIntPoint>& Centers, int32 Radius) const;

    // Чанки, активные в этом кадре, и в предыдущем — разница между ними даёт
    // «только что активированные», которым нужен догон.
    TSet<FIntPoint> ActiveChunks;
    TSet<FIntPoint> PreviousActiveChunks;

    // Игровое время последнего прогона чанка. У неактивного чанка тут
    // остаётся момент, когда он перестал считаться, — разница с текущим
    // временем и есть пропущенный интервал.
    TMap<FIntPoint, float> ChunkLastSimulatedGameTime;

    // Игровое время инициализации сетки. Чанк, который игрок не посещал ни
    // разу, «простаивал» именно с этого момента — иначе клетка, испорченная
    // до ухода игрока, никогда бы не восстановилась: при первой встрече
    // догонять было бы «нечего», и дальний мир стоял бы замороженным.
    float GridInitGameClock = 0.0f;
    void RunSimulationStep();

    // ---- Очередь команд нового пайплайна ----
    TArray<FCommandEntry> PendingCommands;

    // ---- Выделение клетки (отладка) ----
    int32 SelectedX = -1, SelectedY = -1;
};