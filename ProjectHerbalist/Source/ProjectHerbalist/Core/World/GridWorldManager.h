// GridWorldManager.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCellCoord.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Engine/TextureRenderTarget2D.h"
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
#include "Core/World/POITypes.h"
#include "Core/World/ChunkSummaryTypes.h"
#include "Core/World/CellPageTypes.h"
// Полное определение, не форвард-декларация (аудит 2026-09-05; снимки клеток
// с этапа 8 -- в страницах): TArray<FSavedCellState> — данные-член по значению, его конструктору/
// деструктору (в т.ч. авто-сгенерированному UHT в GridWorldManager.gen.cpp)
// нужен полный тип в КАЖДОЙ единице трансляции, включающей этот заголовок,
// не только в GridWorldManagerSave.cpp/GridWorldManagerCore.cpp. Циклической
// зависимости нет — HerbalistSaveTypes.h ничего не включает из этого файла.
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/World/WorldLayout.h"
#include "GridWorldManager.generated.h"

class AHerbalistResourceActor;
class AMemoryFragmentActor;
class AHerbalistEntityActor;
class AHerbalistPlayerController;
class ALandscape;
class ABiomeRegionVolume;
class AWaterRegionVolume;
class AStorageContainer;
class UMaterialParameterCollection;
struct FWorldSnapshot;
struct FStateDelta;
struct FInventoryOperation;

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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    // ---- Инициализация ----
    // IngredientSubsystemOverride -- для тестов: у editor-мира автотеста нет
    // GameInstance. nullptr -- подсистема из GameInstance.
    void SpawnResourcesInCell(FGridCell& Cell, class UIngredientRegistrySubsystem* IngredientSubsystemOverride = nullptr);

    // Тип воды клетки из её собственного потока (этап 4 разметки мира): у
    // клетки -- по доминирующему биому, RollWaterTypeForBiome -- по любому.
    FName RollWaterTypeForCell(const FGridCell& Cell, const class UWaterTypeRegistrySubsystem* WaterSubsystem) const;
    FName RollWaterTypeForBiome(int32 X, int32 Y, EBiomeType Biome, const class UWaterTypeRegistrySubsystem* WaterSubsystem) const;

    // Состояние воды клетки: смесь состояний типов воды её биомов по долям
    // BiomeWeights (решение пользователя 2026-09-13); без реестра воды -- смесь
    // умолчаний воды биомов. Клетка без долей -- вода одного Cell.Biome.
    FRealState RollWaterStateForCell(const FGridCell& Cell, const class UWaterTypeRegistrySubsystem* WaterSubsystem) const;

    // Умолчание клетки, к которому её тянут Морок, Заряна и выход из
    // испорченного полюса: у суши -- биома, у воды -- смесь умолчаний воды её
    // биомов по долям.
    static FRealState GetCellDefaultState(const FGridCell& Cell);

    // Сколько ресурсов положить в клетку: плотность на 100 м² (случайная между
    // Min и Max) × площадь клетки × DensityScale (затухание к краю региона);
    // дробная часть разыгрывается, чтобы среднее на площадь не зависело от
    // размера клетки (решение пользователя 7). Не больше MaxResourcesPerCell.
    // Статическая -- проверяется тестом без мира.
    static constexpr int32 MaxResourcesPerCell = 100000;
    static int32 RollResourceCount(float MinPer100SquareMeters, float MaxPer100SquareMeters,
        double CellSizeCm, FRandomStream& Rng, float DensityScale = 1.0f);
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

    // То же с явным потоком вида и слотом места. Место -- из потока клетки с
    // солью слота (этап 4 разметки мира): слоты не зависят друг от друга, и
    // ресурс, поставленный заново, встаёт туда же. Первичное заселение
    // передаёт поток вида клетки и слот i; отрастание (перегрузка выше) --
    // общий WorldRNG для вида и следующий свободный слот: отросшее -- живое
    // состояние, его вид хранится в сейве и из сида не пересчитывается.
    bool SpawnOneResourceInCell(FGridCell& Cell, const struct FHarvestContext& Context,
        const EGardenNiche* PlotNiche, ABiomeRegionVolume* ClaimingRegion,
        class UIngredientRegistrySubsystem* IngredientSubsystem, FRandomStream& SpeciesRng, int32 PlacementSlot);

    // Следующий свободный слот места в клетке: на единицу больше самого
    // большого у стоящих и спящих ресурсов.
    static int32 AllocatePlacementSlot(const FGridCell& Cell);

    // Запомнить спящий ресурс со слотом, выровняв массив слотов по списку ID.
    static void AddDormantResource(FGridCell& Cell, FName IngredientID, int32 PlacementSlot);
    // Общее окно условий (сезон/время суток/луна/погода/высота) для всех
    // ресурсов одного момента -- было продублировано между
    // SpawnResourcesInCell и StartRegeneration, вынесено сюда (2026-09-04).
    struct FHarvestContext BuildHarvestContextForCell(const FGridCell& Cell) const;
    void StartRegeneration(FGridCell& Cell);
    // Таймер одного отрастания по координате клетки -- из StartRegeneration и
    // из загрузки сейва, которая перезапускает отрастания в процессе
    // (2026-09-14). Таймер прошлого поколения (до загрузки) срабатывает вхолостую.
    void ScheduleRegrowthTimer(const FIntPoint& Coord, float RegrowthTime);
    // Тело таймера отрастания: таймер другого поколения -- вхолостую, клетка
    // выгруженной страницы -- в отложенные до её загрузки. Вынесено ради
    // проверки поколения автотестом.
    void OnRegrowthTimer(const FIntPoint& Coord, float RegrowthTime, int32 Generation);
    int32 GetRegrowthTimerGenerationForTests() const { return RegrowthTimerGeneration; }
    int32 GetRegrowthTimersScheduledForTests() const { return RegrowthTimersScheduled; }
    // Очередь команд до шага симуляции: автотест не может прогнать тик
    // (GetSimulationWorld не видит editor-мир) и сверяет собранную команду.
    const TArray<FCommandEntry>& GetPendingCommandsForTests() const { return PendingCommands; }
    // Стресс клетки на сейчас. В спящем чанке стресс не спадает до догона при
    // активации (CatchUpActivatedChunks); спад линейный, поэтому прогноз по
    // пропущенному времени точен. Без него истощённое место не возвращалось,
    // пока игрок далеко (ревью 2026-09-14).
    float GetCurrentHarvestStress(const FGridCell& Cell) const;
    // Через сколько секунд отрастёт ресурс, собранный с этой клетки сейчас:
    // базовое время региона плюс надбавка за истощение (2026-09-12). Вынесено
    // из StartRegeneration ради прямой проверки -- сам таймер на автотесте не
    // дождаться, а его длительность иначе нигде не наблюдаема.
    float GetRegrowthDelaySeconds(const FGridCell& Cell) const;

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

    // ---- Разметка мира (2026-09-12, DESIGN_World_Layout.md) ----
    // Сетка увязана с ландшафтом и World Partition. Исходные величины
    // запекаются в редакторе кнопкой «Сверить с World Partition» (в собранной
    // игре границ мира нет), итог пересчитывается при сверке и при старте
    // игры -- чанк зависит от радиуса симуляции в настройках. Без ландшафта в
    // исходных величинах разметка не выводится, и GridSizeX/GridSizeY/CellSize
    // с положением актора остаются ручными, как до разметки.
    UPROPERTY(VisibleAnywhere, Category = "World|Layout")
    FHerbalistWorldLayoutSource BakedLayoutSource;

    UPROPERTY(EditAnywhere, Category = "World|Layout")
    FHerbalistWorldLayoutOverrides LayoutOverrides;

    // Итог не сохраняется: пересчитывается в PostInitializeComponents, при
    // старте и при сверке -- сохранённый итог устаревал бы вместе с настройками.
    UPROPERTY(VisibleAnywhere, Transient, Category = "World|Layout")
    FHerbalistWorldLayout ResolvedLayout;

    // Пересчитать ResolvedLayout из BakedLayoutSource, ручных значений и
    // действующего радиуса симуляции. Сетку не трогает.
    bool ResolveWorldLayout(TArray<FString>& OutWarnings);

    // Записать итог в GridSizeX/GridSizeY/CellSize. Актор не двигается:
    // начало сетки при выведенной разметке -- GetGridOrigin().
    void ApplyResolvedLayout();

    // Мировая XY угла сетки по разметке: начало отсчёта + MinCell клеток.
    FVector2D GetLayoutGridOrigin() const;

    // Угол клетки (0, 0) в мире. С разметкой -- из неё (у C++-класса менеджера
    // нет корневого компонента, положение актора ничего не значит); без
    // разметки -- положение актора, как было всегда.
    FVector GetGridOrigin() const;

    // Совпадают ли поля сетки (клетка и размер) с разметкой. Незапечённый
    // менеджер -- всегда да.
    bool IsGridMatchingResolvedLayout() const;

    virtual void PostInitializeComponents() override;

    // Чанк активности в клетках: из разметки, если она выведена, иначе из
    // настроек (ChunkSizeInCells).
    int32 GetChunkSizeInCells() const;

    // Дальность самого дальнобойного локального механизма, в метрах.
    static float GetLongestLocalMechanicMeters();

    // Радиус в клетках для величины в метрах на клетке этой сетки (радиусы
    // капищ, оберегов, Шапки, Соловья, Росы -- в метрах с 2026-09-12).
    int32 GetCellRadius(float Meters) const;

    // Первая клетка сетки в глобальных координатах (разметка мира, этап 6):
    // начало разметки или (0,0) без неё. Cell.X/Cell.Y и все функции с (X, Y)
    // принимают глобальные координаты -- от начала сетки World Partition
    // (решение пользователя 13); массивы клеток -- локальный индекс.
    FIntPoint GetGridMinCell() const { return ResolvedLayout.bValid ? ResolvedLayout.MinCell : FIntPoint::ZeroValue; }

    bool IsCellInGrid(int32 X, int32 Y) const
    {
        const FIntPoint Min = GetGridMinCell();
        return X >= Min.X && X < Min.X + GridSizeX && Y >= Min.Y && Y < Min.Y + GridSizeY;
    }

    // Диапазон чанков сетки в глобальных координатах чанков, включительно. У
    // пустой сетки максимум меньше минимума.
    void GetGridChunkRange(FIntPoint& OutMinChunk, FIntPoint& OutMaxChunk) const;

    // Поток случайных чисел клетки (этап 4 разметки мира): тот же результат
    // при любом порядке обхода. Основа клетки -- тип воды, число, виды и места
    // ресурсов -- берётся отсюда; мировые выборы (хозяева мест, курганы,
    // места силы) пока остаются на общем WorldRNG.
    FRandomStream MakeCellRandomStream(int32 X, int32 Y, FWorldLayoutSolver::ECellRandomPurpose Purpose, int32 Salt = 0) const;

    // Скорость, с которой State клетки идёт к TargetState, в долях в секунду
    // (0.05%, линейный шаг в RegenerateCellParameters). Она же -- предел скорости
    // фронта порчи: действующая ставка заражения быстрее неё фронт не ускоряет
    // (клетки мельче 9 м) -- см. ContagionSpreadRate в HerbalistSettings.h.
    static constexpr float StateRelaxationPerSecond = 0.0005f;

    // Шаг, с которым Tick зовёт RegenerateCellParameters (2026-09-14, ревью).
    // Каждый кадр прибавлял бы к State/TargetState крошечный шаг: на клетке 30 м
    // при 240 fps толчок заражения ~6e-7, релаксация ~2e-6, а соседние значения
    // float около 0.2-0.9 отстоят на 1.5e-8-6e-8. Округление каждой прибавки в
    // одну сторону уводило скорость на проценты. Шаг 0.1 с -- толчок ~1.5e-5,
    // ошибка меньше 0.2%; тот же такт, что у проявлений сущностей
    // (EntityManifestationIntervalSeconds).
    static constexpr float CellRegenerationStepSeconds = 0.1f;

    // Пересчитывать разметку при каждом сохранении менеджера в редакторе.
    UPROPERTY(EditAnywhere, Category = "World|Layout")
    bool bSyncLayoutOnSave = true;

    // Кнопка: собрать исходные величины с ландшафта и World Partition,
    // пересчитать разметку и применить к сетке. То же делает коммандлет
    // WorldLayoutSyncBuilder для карты целиком.
    UFUNCTION(CallInEditor, Category = "World|Layout")
    void SyncWithWorldPartition();

    virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

#if WITH_EDITOR
    // Флаг пространственной загрузки в редакторе не меняется (ревью этапа 5
    // разметки мира): менеджер, у которого галку включили обратно вручную,
    // выгружался бы вместе со своей ячейкой -- со всей симуляцией мира.
    virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }

    // Исходные величины из мира редактора: первый ландшафт (границы через
    // GetCompleteBounds), разбиение World Partition, в которое он попадает.
    static FHerbalistWorldLayoutSource GatherWorldLayoutSource(UWorld* World, TArray<FString>& OutWarnings);

    // Собрать, пересчитать и применить. true -- что-то изменилось.
    bool SyncWorldLayoutFromWorld(UWorld* World, TArray<FString>& OutWarnings, bool bMarkModified);
#endif

    // Автоматический периодический снимок GetGridCorruptionReport() в лог
    // (2026-09-06, прямой запрос пользователя: "мне останется только
    // инициировать пару сборов и ждать", вместо ручного ReportGridCorruption
    // каждый раз). <=0 выключает его совсем -- на случай, если понадобится
    // реже/чаще или совсем не понадобится в обычной игре.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float GridCorruptionReportIntervalSeconds = 30.0f;

    // Тестовая видимость без обхода приватного API (тот же принцип, что и у
    // SpawnOneResourceInCell выше) -- подтверждает, что таймер реально
    // запланирован/остановлен, не просто что код скомпилировался.
    bool IsGridCorruptionAutoReportScheduled() const;

    // ---- Карта состояния мира в текстуру (2026-09-07, "план A") ----
    // Полный довод -- в шапке GridWorldManagerWorldStateMap.cpp. Коротко:
    // материалы травы и ландшафта читают состояние симуляции по мировой
    // позиции, один тексель на клетку.
    //
    // Цель назначается вручную (мягкая ссылка, как VisualizationMPC у
    // UBiomeGraphSubsystem). Не назначена -- механизм выключен, без ошибок:
    // в автотестах и headless-прогонах рисовать всё равно некуда.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
    TSoftObjectPtr<UTextureRenderTarget2D> WorldStateMap;

    // Куда класть рамку карты (начало и размер сетки в мире), чтобы
    // материал построил UV из абсолютной мировой позиции сам:
    // UV = (WorldPos.XY - Origin.XY) / Size.XY. Отдельного канала под два
    // вектора не заводим -- кладём в тот же MPC, где уже живёт GlobalMorok.
    // Мягкая ссылка по тому же образцу, что WorldStateMap выше: не
    // назначена -- просто не пишем, это не ошибка.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
    TSoftObjectPtr<UMaterialParameterCollection> WorldStateFrameCollection;

    // Период выгрузки карты. 1 секунда -- не круглое число "на глаз", а
    // граница, посчитанная от самой симуляции: пассивный дрейф Distortion
    // ограничен сверху 0.01/сек (MorokDistortionPushRate и
    // MorokDistortionDecayRate оба 0.01, а оба множителя лежат в [0,1] --
    // см. UHerbalistSettings). За секунду значение уходит максимум на
    // 0.01, то есть ~2.5 шага 8-битного квантования (1/255) -- ступенька
    // порядка 1% диапазона, глазом не ловится. Чаще ~0.4 с смысла не
    // имеет вовсе: тогда изменение между кадрами мельче одного шага
    // квантования, и лишние выгрузки ничего не добавляют к картинке.
    //
    // Оговорка, которую важно знать: 0.01/сек -- предел ПАССИВНОГО дрейфа.
    // Дискретные события (сбор, зелье на клетку) меняют значение мгновенно,
    // и для них этот период -- задержка отклика до секунды, а не ступенька.
    // Если понадобится мгновенная реакция на действие игрока, правильным
    // решением будет внеочередная выгрузка на самом событии, а не общее
    // учащение таймера.
    //
    // <=0 выключает выгрузку совсем -- тот же приём, что у
    // GridCorruptionReportIntervalSeconds выше.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization", meta = (ClampMin = "0.0"))
    float WorldStateMapUpdateIntervalSeconds = 1.0f;

    // Скорость, с какой КАРТИНКА догоняет состояние мира, единиц оси в
    // секунду. Нужна потому, что мир меняется двумя разными способами, а
    // выглядеть они должны одинаково:
    //   * пассивный дрейф идёт медленно и сам по себе гладок;
    //   * дискретное событие (зелье на клетку, сбор) меняет значение
    //     МГНОВЕННО -- и без сглаживания трава перекрашивалась бы скачком.
    //     Именно это и было замечено в редакторе 2026-09-08: "цвет травы
    //     лерпается мгновенно после применения зелья, надо постепенно".
    //
    // 0.01 -- не подобранное число. Это ровно предел пассивного дрейфа
    // (MorokDistortionPushRate и MorokDistortionDecayRate оба 0.01 при
    // множителях в [0,1]), то есть правило звучит так: КАРТИНКА НЕ МЕНЯЕТСЯ
    // БЫСТРЕЕ, ЧЕМ МИР СПОСОБЕН ИЗМЕНИТЬСЯ САМ. Трава догоняет разлитое
    // зелье с той же скоростью, с какой это изменение произошло бы
    // естественно. Полный размах оси занимает 100 секунд -- при
    // GameDayMinutes=32 это около полутора игровых часов.
    //
    // Догоняние линейное (ограничение скорости), а не экспоненциальное:
    // экспонента быстро стартует и долго доползает, то есть даёт как раз
    // тот рывок в начале, от которого уходим. Линейное идёт ровно и
    // приходит за конечное время.
    //
    // Побочно это делает точным обоснование периода выгрузки выше: раньше
    // "не быстрее 0.01 за секунду" было свойством типичного случая, теперь
    // -- гарантией по построению.
    //
    // <=0 отключает сглаживание совсем (картинка следует за миром мгновенно).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization", meta = (ClampMin = "0.0"))
    float WorldStateMapVisualRatePerSecond = 0.01f;

    // Двигает показываемое состояние к настоящему с ограничением скорости.
    // Публичный ради тестов: сглаживание -- как раз то, что проверяемо
    // headless, в отличие от самой выгрузки.
    // Истинное состояние клетки по четырём осям карты, одним местом --
    // чтобы сглаживание и сборка пикселей не разъехались в том, ЧТО именно
    // они считают состоянием.
    FVector4f GetCellWorldStateAxes(const FGridCell& Cell) const;

    void AdvanceWorldStateMapDisplay(float DeltaSeconds);

    // Ставит показываемое состояние равным настоящему без сглаживания.
    // Нужен при загрузке уровня: иначе мир каждый раз выцветал бы из нулей
    // на глазах у игрока, изображая изменение, которого не было.
    void SnapWorldStateMapDisplayToWorld();

    // Собирает буфер карты: по пикселю на клетку окна (GetWorldStateWindow),
    // индекс -- от угла окна построчно. Без разметки окно -- вся сетка, и
    // индекс совпадает с GetCellIndex.
    // Публичный ради тестов -- проверяемая часть механизма именно эта
    // (раскладка и значения), выгрузка на GPU headless не проверяется.
    TArray<FColor> BuildWorldStateMapPixels() const;

    // Мировая позиция -> UV карты. Возвращает false, если точка вне окна.
    // Существует, чтобы соответствие "материал <-> симуляция" можно было
    // проверить тестом, а не сверять формулу глазами с материалом.
    bool GetWorldStateMapUV(const FVector& WorldPosition, FVector2D& OutUV) const;

    // Начало и размер окна в мире -- то, что материалу нужно знать, чтобы
    // самому построить UV из абсолютной мировой позиции. С разметкой окно
    // переезжает за зрителем: рамку нельзя запомнить один раз.
    UFUNCTION(BlueprintCallable, Category = "Visualization")
    void GetWorldStateMapFrame(FVector& OutOrigin, FVector2D& OutWorldSize) const;

    UFUNCTION(BlueprintCallable, Category = "Visualization")
    void UpdateWorldStateMap();

    // Та же тестовая видимость, что у IsGridCorruptionAutoReportScheduled.
    bool IsWorldStateMapUpdateScheduled() const;

    // Окно карты состояния (разметка мира, этап 7, DESIGN_World_Layout.md
    // §9): прямоугольник клеток, который попадает в текстуру. Без разметки
    // или когда окно не меньше сетки -- вся сетка, как до этапа. С разметкой
    // -- WorldStateWindowCells клеток вокруг зрителя (GetWorldStateViewerCell).
    void GetWorldStateWindow(FIntPoint& OutMinCell, FIntPoint& OutSize) const;

    // Поставить окно к зрителю: первый раз -- сразу, дальше -- когда зритель
    // ушёл от центра окна дальше тайла. true -- окно сдвинулось.
    bool UpdateWorldStateWindow();

    // Ушёл ли зритель от центра поставленного окна дальше тайла -- тогда карту
    // выгружают вне расписания (Tick), не дожидаясь таймера.
    bool IsWorldStateWindowStale() const;

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
    // Не ниже 0.1 с -- как у региона: таймер с нулевым временем не ставится,
    // и место ждало бы вечно.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest", meta = (ClampMin = "0.1"))
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

    // Клетка по линейному индексу прямоугольника сетки -- построчно от первой
    // клетки (этап 8). nullptr -- индекс вне сетки или страница не загружена.
    FGridCell* GetCellByGridIndex(int32 GridIndex);
    const FGridCell* GetCellByGridIndex(int32 GridIndex) const;
    int32 GetGridCellCount() const { return FMath::Max(GridSizeX, 0) * FMath::Max(GridSizeY, 0); }

    // Клетки загруженных страниц (этап 8).
    int32 GetLoadedCellCount() const { return LoadedCellCount; }

    // Сетка создана -- страницы есть, даже если все выгружены (этап 8). Для
    // вопроса «есть ли сетка», а не «сколько клеток в памяти».
    bool HasCellPages() const { return CellPages.Num() > 0; }

    // Расширить сетку до целых страниц с местами за её краем (решение
    // пользователя 2026-09-13: места за убранными плитками ландшафта живут).
    // Только с разметкой. Страницы мест загружаются из основы и закрепляются,
    // линейные индексы клеток пересчитываются. Возвращает число страниц мест.
    int32 EnsureGridCoversSites(const TArray<FIntPoint>& Sites);

    // Клетка страницы-заполнителя: в расширенной сетке, вне сетки ландшафта и
    // не на странице места (ревью 2026-09-13). Такие клетки не часть мира.
    bool IsCellInExtensionFiller(int32 X, int32 Y) const;

    // Обходу строками нужны страницы напрямую.
    template<typename ManagerType, typename CellType> friend class TGridCellRange;

    // Обход клеток сетки построчно для range-for (этап 8) -- порядок единого
    // массива до страниц: от него зависит расход WorldRNG при посеве мест.
    TGridCellRange<AGridWorldManager, FGridCell> GetCellsInGridOrder() { return TGridCellRange<AGridWorldManager, FGridCell>(this); }
    TGridCellRange<const AGridWorldManager, const FGridCell> GetCellsInGridOrder() const { return TGridCellRange<const AGridWorldManager, const FGridCell>(this); }

    // Для тестов адресации страниц.
    int32 GetCellPageCountForTests() const { return CellPages.Num(); }
    bool GetCellPageBoundsForTests(int32 X, int32 Y, FIntPoint& OutMinCell, FIntPoint& OutSize) const;
    bool GetCellBaselineCellForTests(int32 GridIndex, FIntPoint& OutCell) const;
    int32 GetUnloadedCellDeltaCountForTests() const { return UnloadedCellDeltas.Num(); }
    void SetLegendaryAnchorsForTests(const TMap<FName, FIntPoint>& InAnchors) { LegendaryAnchors = InAnchors; }
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
    // пер-региональных настроек плотности -- MinResourcesPer100SquareMeters/
    // MaxResourcesPer100SquareMeters/ResourceRegrowthTimeSeconds на самом
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
    void MarkCellDirtyForTests(int32 X, int32 Y) { MarkCellDirty(X, Y); }
    void InvalidateAllChunkSummariesForTests() { InvalidateAllChunkSummaries(); }

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

    const TSet<FIntPoint>& GetActiveChunks() const { return ActiveChunks; }

    // ---- Материализация акторов (2026-09-12) ----
    // Активность решает, считается ли симуляция клетки; материализация --
    // существуют ли в мире АКТОРЫ её ресурсов. Акторы сетки созданы в
    // рантайме и в ячейки World Partition не входят -- сам партишен их не
    // выгружает. Правило: ресурс стоит, пока под ним загружена земля, --
    // независимо от радиуса симуляции (второй заход того же дня, решение
    // пользователя: "ресурсы пропадают раньше" -- радиус ~100 м, ландшафт
    // L_TestDev грузится на 252 м).
    //
    // Земля -- зарегистрированные компоненты прокси ландшафта; на карте без
    // ландшафта -- активные ячейки World Partition (не HLOD, не всегда
    // загруженные). Чанк материализован, если его углы и центр на земле.
    // Вне игрового мира и без стриминга партишена земля неизвестна --
    // материализованы активные чанки, как до этого правила. Сущности уходят
    // на границе симуляции, а не земли (CatchUpActivatedChunks): их поведение
    // считается только в активных чанках. Всё, что спавнится в
    // нематериализованную клетку (отрастание, загрузка сейва), уходит в
    // DormantResourceIDs, а не в мир. Вызывается из CatchUpActivatedChunks.
    void UpdateMaterializedChunks();
    bool IsChunkMaterialized(const FIntPoint& Chunk) const;
    bool IsCellMaterialized(const FGridCell& Cell) const;
    bool IsChunkGroundLoaded(const FIntPoint& Chunk) const;
    const TSet<FIntPoint>& GetMaterializedChunks() const { return MaterializedChunks; }

    // В editor-мире теста партишен не стримит -- подать загруженную землю
    // прямоугольниками в мировых координатах, как их собрал бы
    // RefreshGroundCoverage. Пустой массив -- земля известна, но не загружена
    // нигде; Clear -- снова спрашивать мир.
    void SetGroundCoverageForTests(const TArray<FBox2D>& InCoverage) { GroundCoverageOverride = InCoverage; }
    void ClearGroundCoverageForTests() { GroundCoverageOverride.Reset(); }

    // Чанк мировой точки в координатах сетки -- в том числе за её пределами:
    // источник стриминга за краем сетки всё равно даёт центр активности.
    FIntPoint WorldPositionToChunk(const FVector& WorldPos) const;

    // Тело таймера StartRegeneration -- публично ради прямой проверки: сам
    // таймер на автотесте не дождаться.
    void CompleteRegrowth(FGridCell& Cell, float RegrowthTime);

    // ---- Алхимия: тонкие обёртки, собирающие FCommandEntry(Apply) и
    // отправляющие его в QueueCommand — реальный расчёт идёт в PipelineV2 ----
    // bIngredientsAlreadyWithdrawn -- вызывающая сторона уже сняла предметы из
    // сумки сама (UsePotion), Pipeline их не списывает (FApplyCommand).
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent,
        bool bIngredientsAlreadyWithdrawn = false);
    void ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent);

    // Варка у котла (UAlchemyTransferWidget, 2026-09-14). Ингредиенты уже
    // изъяты из сумки переносом в слоты, результат ляжет в сумку. Раньше
    // виджет собирал команду сам и выставлял только заряд оберега -- фаза
    // луны, BrewBoost, межбиомность и тиражный оберег на варку у стола не
    // действовали.
    FCommandEntry BuildCauldronBrewCommand(const FIntPoint& TableCell, const TArray<FInventoryItem>& Ingredients) const;
    void QueueCauldronBrew(const FIntPoint& TableCell, const TArray<FInventoryItem>& Ingredients);

    // Модификаторы варки, которые резолвятся вне Pipeline: заряд
    // Камня-оберега, фаза луны, оберег BrewBoost, межбиомность, тиражный
    // оберег. Читает Apply.Ingredients. Общий для котла и ApplyAlchemyResult.
    void ResolveBrewModifiers(FApplyCommand& Apply) const;

    // Предмет, сваренный по команде варки (зелье, зола или кипячёная вода),
    // уже в сумке. Рассылается из RunSimulationStep; окно котла показывает
    // его на витрине вместо поиска в сумке по времени создания -- поиск
    // промахивался, когда новое зелье сливалось с похожей стопкой (MergeStack
    // усредняет CreationTime).
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBrewCompleted, const FInventoryItem& /*Produced*/);
    FOnBrewCompleted OnBrewCompleted;

    // Сопоставляет команды сбора и варки из пакета с предметами, которые
    // Pipeline добавил в сумку (ContainerID 0): варке -- следующий результат
    // варки (Potion/Ash/BoiledWater), сбору -- следующий прочий предмет.
    // Раздельно, чтобы сбор без добычи не забирал себе зелье соседней варки.
    static void ForEachProducedItem(const TArray<FCommandEntry>& Commands, const FStateDelta& Delta,
        TFunctionRef<void(const FCommandEntry& Cmd, const FInventoryOperation& AddOp)> Visit);

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
    // CurrentGatheringTool: сам механизм работает уже сейчас, визуал
    // пристроек — отдельный, ещё не реализованный проход (уровень/актор,
    // не логика). Экономика (материалы + Molva) закрыта 2026-09-06 —
    // см. GardenNicheUnlockTypes.h, гейт проверяется в
    // AHerbalistPlayerController::SetGardenPlot ДО этого вызова
    // (RegisterGardenPlot сама остаётся честным, непроверяющим сеттером
    // мирового состояния — тот же принцип границы, что уже у ActivateWard/
    // PlantSeed: инвентарный/Molva-гейт в контроллере, состояние здесь).
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

    // Внесение перегноя (2026-09-04, "Перегной... применение сразу делаем")
    // -- поднимает Environment.Fertility клетки на FertilizerFertilityBonus
    // (HerbalistSettings.h), зажато в [0,1]. Владение предметом (есть ли
    // Перегной в инвентаре) проверяет AHerbalistPlayerController::ApplyFertilizer
    // -- тот же принцип границы, что и у PlantSeedInCell выше: эта функция
    // не трогает GameInstance, только мировое состояние клетки, поэтому
    // напрямую вызываема из автотестов. false + лог -- нет такой клетки.
    bool ApplyFertilizerToCell(const FIntPoint& CellCoord);

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

    // Сводка "разрастания поганых мест" по ВСЕЙ сетке разом (2026-09-06,
    // найдено по PIE-логу пользователя: "после трёх сборов и долгого
    // времени испортилась вся сетка") — GetSelectedCellInfo проверяет одну
    // клетку за раз, для наблюдения за волной заражения по 400 клеткам
    // пришлось бы кликать по каждой. Один вызов из консоли даёт снимок,
    // достаточный, чтобы отличить "заражение расползается" от чего-то
    // ещё по одному только логу, без визуального осмотра карты.
    FString GetGridCorruptionReport() const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress);

    // ---- Биомы ----
    // Суммы по биому для графа биомов и центры биомов -- из сводок чанков
    // (разметка мира, этап 7), а не обходом всех клеток.
    TMap<FName, FHerbalistBiomeFieldSum> GetBiomeFieldSums() const;
    TMap<FName, FVector> GetBiomeCenters() const;

    // Сводки всех чанков сетки. Живой чанк (в активной области; без центров
    // активности -- все, как у ForEachActiveCell) пересчитывается при каждом
    // запросе: только там идёт релаксация и влияние графа. Неживой берётся из
    // кэша, пока клетку в нём не пометит MarkCellDirty. Только игровой поток;
    // Func получает ссылку в кэш и сам сводки не запрашивает.
    void ForEachChunkSummary(TFunctionRef<void(const FHerbalistChunkSummary&)> Func) const;
    void ApplyBiomeInfluences(const TMap<FName, float>& MorokFields, const TMap<FName, float>& ZaryanaFields, float GlobalScale, float DeltaTime);

    // ---- Ресурсы ----
    // PlacementSlot -- слот места (этап 4 разметки мира); INDEX_NONE --
    // следующий свободный.
    void SpawnResourceActor(FName IngredientID, int32 X, int32 Y, const FVector& Offset = FVector::ZeroVector,
        class UIngredientRegistrySubsystem* IngredientSubsystemOverride = nullptr, int32 PlacementSlot = INDEX_NONE);

    // Ставит сохранённый ростер клетки (материализация чанка, загрузка сейва).
    // Каждый ресурс -- на место своего слота: собранный или не вставший при
    // заселении ресурс не сдвигает остальных. PlacementSlots короче списка --
    // недостающим выдаются следующие свободные (сейв до слотов).
    void SpawnResourceRoster(FGridCell& Cell, const TArray<FName>& IngredientIDs, const TArray<int32>& PlacementSlots,
        class UIngredientRegistrySubsystem* IngredientSubsystemOverride = nullptr);

    // ---- Восприятие ----
    const FPerceivedWorld* GetPerceivedWorld() const;
    const FPerceivedInventory* GetPerceivedInventory() const;

    // ---- Экология: восстановление клеток ----
    // OnlyChunk != nullptr — считать только клетки этого чанка (догон при
    // активации, CatchUpActivatedChunks). Обычный вызов из Tick оставляет
    // nullptr и идёт по всем активным клеткам, как раньше.
    void RegenerateCellParameters(float DeltaTime, const FIntPoint* OnlyChunk = nullptr);

    // Полное время зарастания: за сколько секунд HarvestStress 1.0 спадает до
    // нуля. Обратная величина к спаду, который считает RegenerateCellParameters
    // -- вынесена сюда (2026-09-12), потому что то же число понадобилось
    // отрастанию ресурсов (StartRegeneration). Два независимых выражения одной
    // величины разъезжаются при первой же правке любого множителя, а множителей
    // здесь три: биом, сезон и Лесное капище.
    //
    // Биомная версия (биом x сезон) отделена ради горячего цикла релаксации:
    // строка DataTable тянется раз на биом, а не раз на клетку каждый кадр,
    // и вызывающая сторона кэширует результат сама.
    float GetStressRecoverySecondsForBiome(EBiomeType Biome) const;

    // Та же величина для конкретной клетки: биомная, делённая на ускорение от
    // Лесного капища (15_Cycles_And_Shrines.md §15.5, "ускоряет заживление
    // клеток") -- ровно то, что RegenerateCellParameters накладывает поверх
    // биомного кэша. Дороже биомной на поиск доминирующего капища, поэтому в
    // горячем цикле не используется.
    float GetStressRecoverySecondsForCell(const FGridCell& Cell) const;

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
    // Календарь (2026-09-16, решение пользователя): год 365 суток, обычные
    // месяцы без високосных, сутки 0 -- 1 марта; сезон -- метеорологический,
    // по месяцам, как Meteorological Seasons у Ultra Dynamic Sky
    // (Core/Types/HerbalistCalendar.h).
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    ESeason GetSeason() const;

    // Сезон, как его называет игроку лор: осень -- часть Лета.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    ESeason GetLoreSeason() const;

    // Доля пройденного текущего сезона, [0,1): 0 в первый миг сезона.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    float GetSeasonProgress01() const;

    // День года от 1 марта [0, 365), месяц 1..12, число 1..31.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    int32 GetDayOfYear() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    int32 GetCalendarMonth() const;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    int32 GetCalendarDay() const;

    // Осень, сентябрь–ноябрь: Листовики (§16.2) и травы bAutumnOnly.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsAutumn() const;

    // Купальская ночь -- ночь на 24 июня (старый стиль): ночная фаза суток
    // 23 июня (§16.2, Купальские).
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    bool IsKupalaNight() const;

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
    // double (2026-09-16): год календаря -- 700 800 с; во float с 2^19 с (конец
    // ноября) кадр 1/60 с меньше половины шага точности, и часы вставали.
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Time")
    double GetGameClockSeconds() const { return GameClockSeconds; }
    void SetGameClockSeconds(double InSeconds) { GameClockSeconds = InSeconds; }

    // Ход часов за кадр; Tick() зовёт первым делом.
    void AdvanceGameClock(float DeltaTime) { GameClockSeconds += DeltaTime; }

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

    // Трёхглавый Змей (§4.4, Калинов мост, 2026-09-06) -- тот же
    // bManualRegistrationOnly/идемпотентный приём, что RegisterDomovoi выше,
    // но регистрируется не актором игрока, а самим SeedPointsOfInterest
    // (GridWorldManagerPOI.cpp) -- это не "хозяин" дома, а точка на карте.
    void RegisterZmeyGorynych(const FIntPoint& Cell);

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

    // ---- Домашние хранилища (DESIGN_Community_And_Homestead.md §2.2,
    // 2026-09-04) — буквальное расширение дома ("погреб выкопать"), не ниша
    // сада: прямой запрос пользователя. Постройка МГНОВЕННА при выполнении
    // условий (тот же принцип "мягкой прокачки" §2.2 — материалы + Respect
    // хозяина, не число опыта), не растянутый во времени процесс. Владение/
    // Respect Домового/списание материала — дело вызывающей стороны
    // (AHerbalistPlayerController::BuildHomeStorage, тот же класс границы,
    // что уже держат ActivateWard/PlantSeed): эта функция — только сам
    // эффект, спавн AStorageContainer у клетки-якоря дома, НЕ трогает
    // GameInstance, напрямую тестируема (тот же принцип, что уже
    // PlantSeedInCell/ApplyFertilizerToCell выше). Возвращает nullptr при
    // отказе (клетка вне сетки/спавн не удался) — вызывающая сторона уже
    // отчиталась причиной, здесь второго лога не требуется.
    AStorageContainer* SpawnHomeStorageContainer(const FIntPoint& AnchorCell, EStorageContainerType ContainerType);

    // Класс построенного хранилища (2026-09-14): Blueprint с окном переноса --
    // TransferWidgetClass у AStorageContainer задаётся только в Blueprint, голый
    // класс не открывался.
    UPROPERTY(EditAnywhere, Category = "Homestead")
    TSoftClassPtr<AStorageContainer> HomeStorageContainerClass = TSoftClassPtr<AStorageContainer>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_StorageContainer.BP_StorageContainer_C")));

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
    // (Gain × (Purity − Corruption)), по СУММЕ (не среднему, поправлено
    // 2026-09-05 по находке аудита) предложенных предметов — вызывающая
    // сторона (AHerbalistPlayerController::OfferToCommunity) списывает
    // ровно 1 единицу за каждый элемент этого массива, значит и вклад в
    // Molva обязан расти с их числом, не усредняться до "как будто дали
    // один раз" (для типового случая с одним предметом поведение не
    // изменилось). Возвращает применённое ΔMolva (для лога/обратной связи
    // вызывающей стороне), сам инвентарь не трогает — списание предметов
    // остаётся на вызывающей стороне, тем же разделением обязанностей, что
    // и у остальных Exec-путей этого класса.
    float OfferToCommunity(const TArray<FInventoryItem>& Items);

    // Ценность предмета для общины (§1.2) — Magnitude, взвешенный Purity и
    // обратной редкостью (1/IngredientTableRow::RarityWeight — уже
    // существующее понятие, не новая метрика). Нулевая/неизвестная
    // Ценность (не найден в реестре) — 0, не крах: тот же принцип
    // терпимости к отсутствующим данным, что у GetRow/Classify.
    float ComputeCommunityTradeValue(const FInventoryItem& Item) const;

    // Гарантированный минимум 1 (§1.2 изначально трактовалось как "не
    // магазин с ценниками, округление вниз, не отказ") оказался печатным
    // станком (аудит 2026-09-05): обмен самого дешёвого на самое дорогое и
    // обратно давал чистую прибыль из ничего на каждом цикле, независимо от
    // курса. §1.2 на деле про ДРУГОЕ — "курс считается формулой, не
    // хардкожен списком цен" — не про то, что сделка обязана состояться
    // при любом соотношении ценностей. Вынесено отдельной чистой функцией
    // (тестируется без реестра ингредиентов, недоступного в Editor-тестах):
    // Rate<1 -- сделка отказывает целиком, не округляется вверх за счёт
    // общины.
    static bool ComputeTradeReceivedCount(float Rate, int32& OutCount);

    // Обмен (§1.2) — курс ЦенностьA/ЦенностьB, домножен на (1 +
    // TradeMolvaRateBonus×Molva). OutReceived получает WantedIngredientID,
    // Count по ComputeTradeReceivedCount выше и State — из
    // CommunityIngredientQuality (реально увиденное общиной среднее
    // качество вида, аудит 2026-09-05), либо табличный BaseState, пока
    // община ни разу его не получала. false = ничего не найдено в реестре,
    // Offered.Count<=0, ИЛИ курс не тянет даже на 1 единицу (см. довод у
    // ComputeTradeReceivedCount).
    bool TryTradeWithCommunity(const FInventoryItem& Offered, FName WantedIngredientID, FInventoryItem& OutReceived) const;

    // Обновляет CommunityIngredientQuality реально отданной единицей (аудит
    // 2026-09-05). Публично и НЕ вызывается изнутри TryTradeWithCommunity
    // (та специально осталась const — чистый расчёт курса, никаких
    // мутаций): вызывающая сторона (AHerbalistPlayerController::
    // TradeWithCommunity) зовёт этот метод сама, ПОСЛЕ того как реально
    // списала предложенный товар — тот же принцип разделения "расчёт
    // отдельно от побочных эффектов", что уже держит OfferToCommunity
    // (не трогает инвентарь) в стороне от списания на контроллере.
    // OfferToCommunity — исключение: сама уже мутирует Molva, поэтому сама
    // же и пишет сюда, вызывающей стороне звать не нужно.
    void RecordCommunityIngredientQuality(FName IngredientID, const FRealState& State, int32 Count);

    // Публично только для теста на саму математику усреднения (аудит
    // 2026-09-05) — TryTradeWithCommunity не тестируется отдельным
    // автотестом (нужен IngredientRegistrySubsystem, недоступный в
    // Editor-тестах), но взвешенное среднее в RecordCommunityIngredientQuality
    // — чистая математика без реестра, тестируема напрямую. nullptr — вид,
    // которого община ни разу не получала (TryTradeWithCommunity в этом
    // случае честно откатывается на табличный BaseState).
    const FRealState* GetCommunityIngredientQualityForTest(FName IngredientID) const { return CommunityIngredientQuality.Find(IngredientID); }

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
    void CollectMemoryFragment(FName DefinitionID, bool bIsFalse, AHerbalistPlayerController* PC, const FIntPoint& Cell = HerbalistCore::InvalidCell());

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

    // Публично только для теста на утечку (аудит 2026-09-05): подтвердить,
    // что per-клеточные аккумуляторы TishinaLesaHoldSeconds/OjidanieBuriHoldSeconds
    // очищаются, как только соответствующий фрагмент собран, а не растут
    // молча до конца сессии.
    int32 GetTishinaLesaHoldMapNum() const { return TishinaLesaHoldSeconds.Num(); }
    int32 GetOjidanieBuriHoldMapNum() const { return OjidanieBuriHoldSeconds.Num(); }

    // Публично только для теста на дрейф (аудит 2026-09-05): подтвердить,
    // что порог опроса UpdateMemoryFragments вычитает CheckInterval, а не
    // обнуляет остаток накопленного DeltaTime.
    float GetFragmentStateCheckAccumulatorForTest() const { return FragmentStateCheckAccumulator; }

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
    // Возвращает число сохранённых клеток за сеткой: они отброшены (плитки
    // ландшафта убрали, этап 8 разметки мира).
    int32 ApplySaveCells(const TArray<FSavedCellState>& InCells);

    // Домашние хранилища (аудит 2026-09-05: "и их содержимое не сохраняются
    // вообще") — сами AStorageContainer нигде не отслеживаются постоянным
    // списком (ни здесь, ни на контроллере, см. BuildHomeStorage/
    // SpawnHomeStorageContainer выше): единственный источник истины —
    // TActorIterator по миру, в точности как уже делает BuildHomeStorage
    // при проверке "такой тип уже есть". Позиция не сохраняется отдельно —
    // контейнер всегда пересоздаётся у ТЕКУЩЕЙ клетки-якоря дома
    // (AAlchemyTableActor), той же логикой, что и исходный спавн. Только
    // построенные (bIsHomeStorage, 2026-09-14).
    TArray<FSavedHomeStorage> CaptureHomeStorages() const;
    void RestoreHomeStorages(const TArray<FSavedHomeStorage>& InStorages);

    // Сундуки и станции карты (2026-09-14) — содержимое по имени актора, сам
    // актор приходит с уровнем. См. FSavedPlacedContainer.
    TArray<FSavedPlacedContainer> CapturePlacedContainers() const;
    void RestorePlacedContainers(const TArray<FSavedPlacedContainer>& InContainers);

    // Выгрузка и загрузка контейнера карты World Partition
    // (AStorageContainer::EndPlay/BeginPlay): пока актора нет, содержимое
    // держит менеджер. Claim отдаёт и забывает.
    void StashPlacedContainerContents(FName ActorName, const TArray<FInventoryItem>& Items);
    bool ClaimPlacedContainerContents(FName ActorName, TArray<FInventoryItem>& OutItems);

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

    // Публично только для теста на ResetSessionOnlyWardTimers (аудит
    // 2026-09-05) — в остальном коде читается инлайн в GridWorldManagerZaryana.cpp
    // (GetZaryanaPerceivedState), отдельного публичного предиката не было
    // и не нужно было до сих пор.
    bool IsYouthAppleClarityBoostActiveForTest() const { return GameClockSeconds < YouthAppleClarityBoostExpiryGameSeconds; }

    // Шапка-невидимка (Баба-Яга) — временное, повторно используемое:
    // пока активно, подавляет НОВЫЕ проявления Низшего/Легендарного ранга
    // в НАСТОЯЩЕЙ зоне (2026-09-02, "чиним до настоящей зоны" — Chebyshev-
    // радиус InvisibilityCapRadiusMeters вокруг клетки игрока в момент
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
    // (WardConcealmentRadiusMeters) — общая проверка без геометрии + проверка
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

    // ---- Тиражные обереги (награда ритуалов перехода ярусов биомов,
    // RitualTypes.h::FRitualRecipeDefinition::GrantsIngredientID,
    // IngredientTableRow.h::bIsTieredWard) — В ОТЛИЧИЕ от трёх оберегов
    // выше (Плакун-камень/Громовая стрела/Куриный бог) у этих троих НЕТ
    // ТАЙМЕРА (прямой запрос: "как активировал/надел оберег, так он и
    // работает") — вместо GameClockSeconds-экспаери их сила зависит от
    // того, "в своём" ли биоме используется эффект (WardHomeBiomes на
    // карточке кристалла, TieredWardOutOfBiomeStrength, HerbalistSettings.h).
    // Ни Center, ни Radius не запоминаются на активации (в отличие от
    // ActivateWardConcealment/ActivateWardMorokReduction выше) — "дом"
    // оберега определяется биомом КЛЕТКИ, для которой спрашивают
    // (см. IsTieredConcealmentActive/GetTieredMorokReductionAmount ниже),
    // не расстоянием от места, где оберег был надет. ----

    // Переключает нужную пару bTiered*/Tiered*HomeBiomes по Type (BrewBoost/
    // EntityConceal/MorokReduction) — тот же диспетчер, что уже
    // AHerbalistPlayerController::ActivateWard делает для трёх старых
    // оберегов через switch, только без выбора конкретной Activate-функции:
    // здесь одна функция на все три эффекта, различаемые параметром.
    void ActivateTieredWard(EWardEffectType Type, const TArray<EBiomeType>& HomeBiomes);

    // EntityConceal (тираж) — Cell.Biome ЭТОЙ клетки в TieredConcealmentHomeBiomes
    // -> полная защита (тот же гейт проявления, что и IsWardConcealmentActive
    // выше, независимый и не взаимоисключающий источник); вне домашних биомов
    // -- защиты нет (радиус вокруг игрока схлопывается до нуля, из старой
    // Center+Radius геометрии здесь остаётся только "своя" клетка). См.
    // довод о выборе этой геометрии у ActivateTieredWard выше.
    bool IsTieredConcealmentActive(const FIntPoint& PlayerCell) const;

    // MorokReduction (тираж) — полный WardMorokReductionAmount, если Cell.Biome
    // в TieredMorokReductionHomeBiomes, иначе WardMorokReductionAmount *
    // TieredWardOutOfBiomeStrength (черновой коэффициент, HerbalistSettings.h).
    // 0.0f, если тиражный MorokReduction не активирован вовсе -- вызывающая
    // сторона (ComputePerceptionDistortion) просто вычитает результат, без
    // отдельной проверки bTieredMorokReductionActive.
    float GetTieredMorokReductionAmount(const FIntPoint& PlayerCell) const;

    // Аудит 2026-09-05: "таймеры оберегов/артефактов не сохраняются, а
    // GameClockSeconds откатывается назад при загрузке — активный оберег
    // может 'прожить' намного дольше заявленного окна". Сами таймеры
    // (Ward*/InvisibilityCap/YouthApple/Alkonost выше) осознанно НЕ
    // персистятся — задокументированное решение "короткое окно, часть
    // текущей сессии, не долгоживущий прогресс" (см. комментарии у полей).
    // Но реализация этого решения была неполной: LoadGame восстанавливает
    // GameClockSeconds (тот ДЕЙСТВИТЕЛЬНО персистится), а эти шесть
    // ExpiryGameSeconds-полей — нет, оставаясь на прежнем значении. Если
    // откат GameClockSeconds назад проносит его НИЖЕ уже стоящего
    // Expiry-значения (оберег истёк/не активировался вовсе в загруженной
    // точке, но Expiry всё ещё указывает на будущее ОТНОСИТЕЛЬНО нового
    // GameClockSeconds) — оберег читается как активный ещё раз, на полный
    // WardDurationSeconds, хотя по стоящему за решением намерению должен
    // был быть выключен. Явный сброс здесь — единственный способ реально
    // выполнить уже принятое "не переживает загрузку", независимо от того,
    // в какую сторону откатились часы. Вызывается из
    // UHerbalistSaveSubsystem::LoadGame.
    void ResetSessionOnlyWardTimers();

    // Тиражные обереги (аудит 2026-09-05, решение пользователя (а)) —
    // постоянная награда за завершённый ритуал, БЕЗ таймера, обязана
    // пережить перезагрузку так же, как AcquiredArtifacts/AcquiredFeathers
    // — в отличие от ResetSessionOnlyWardTimers выше (те шесть таймеров
    // ПРОДОЛЖАЮТ намеренно не персистится). Тот же принцип Capture/Restore,
    // что уже CaptureSaveCells/ApplySaveCells и CaptureHomeStorages/
    // RestoreHomeStorages.
    FSavedTieredWards CaptureTieredWards() const;
    void RestoreTieredWards(const FSavedTieredWards& InWards);

    // BrewBoost (тираж) не имеет отдельного публичного геттера силы, как
    // Concealment/MorokReduction выше (используется внутри GridWorldManagerAlchemy.cpp
    // напрямую по bTieredBrewBoostActive) -- этот accessor существует только
    // для проверки Capture/Restore round-trip в тестах.
    bool IsTieredBrewBoostActiveForTest() const { return bTieredBrewBoostActive; }

    // ---- Серебряный оберег (Ось Б §2.3, DESIGN_Community_And_Homestead.md,
    // 2026-09-06) — экипированный (AHerbalistPlayerController::EquipSilverWard),
    // НЕ расходуется, не резак вообще (см. комментарий у EGatheringTool,
    // HerbalistCoreTypes.h). В отличие от шести таймерных оберегов выше и
    // трёх тиражных — нет ни таймера, ни геометрии: постоянный, общий для
    // ВСЕЙ сетки источник подавления проявлений, действует наравне с
    // IsInvisibilityCapActive/IsWardConcealmentActive/IsTieredConcealmentActive
    // в геймплейной OR-цепочке гейта (см. UpdateEntityManifestations,
    // GridWorldManagerEntities.cpp) — не только во время сбора (решение
    // пользователя 2026-09-06: "общий источник, всегда активен"). Персистентен
    // (не session-only) — награда за находку в кургане, не эффект короткого
    // окна.
    bool IsSilverWardActive() const { return bSilverWardActive; }
    void SetSilverWardActive(bool bActive) { bSilverWardActive = bActive; }

    // ---- Курганы (DESIGN_Brewing_Situations_And_Lore.md §4.3 "Гнёздово",
    // DESIGN_Community_And_Homestead.md §2.3, 2026-09-06) — единственный
    // источник Костяного ножа/Серебряного оберега (артефакт-тир инструментов,
    // "не рыночный товар, а находка"). Ключ карты — клетка кургана, значение —
    // IngredientID награды; разграбленный курган удаляется из карты целиком
    // (не отдельный bool bLooted -- нечего проверять на второй попытке, курган
    // просто больше не в KurganSites). Засеивается один раз в InitializeCells
    // детерминированным WorldRNG (тот же генератор, что хозяева мест и якоря
    // в этой же функции) -- не отдельный, несинхронизированный источник
    // случайности. Никакого актора на уровне -- v1 консольный (LootKurgan),
    // тот же принцип, что у SetGardenPlot/ActivateWard: обнаружение места --
    // через GetSelectedCellInfo (GridWorldManagerDebug.cpp).
    void SeedKurganSites();
    bool LootKurgan(const FIntPoint& Cell, FName& OutGrantedIngredientID);
    const TMap<FIntPoint, FName>& GetKurganSites() const { return KurganSites; }
    // Ставит и акторы курганов (SyncKurganActors): загрузка сейва приходит сюда.
    void SetKurganSites(const TMap<FIntPoint, FName>& InSites);

    // Акторы курганов этого менеджера -- ровно по одному на курган KurganSites
    // (2026-09-14). Раньше актор спавнился только при посеве, и после загрузки
    // сейва акторы стояли на местах свежего посева, а разграблялись курганы сейва.
    void SyncKurganActors();

    // ---- Точки интереса, §4 (см. POITypes.h за общим доводом) ----
    // Общая точка входа сева -- сеет курганы (без изменения их поведения),
    // затем по одной клетке на оставшиеся виды, тем же детерминированным
    // WorldRNG-проходом, что уже SeedKurganSites, каждый раз исключая уже
    // занятые предыдущими видами клетки (GridWorldManagerPOI.cpp).
    UFUNCTION(Exec)
    void SeedPointsOfInterest();

    // Тотем (§4.2, Збручский идол как "похожий, но не тот же" объект,
    // решение 2026-09-06) -- запрос-только точка, ничего не грантит и не
    // меняет State клетки. Нижний ярус читается по Distortion клетки,
    // верхний -- виден только при высокой Purity клетки (см. довод у
    // GetTotemRevealText, GridWorldManagerPOI.cpp). Средний ярус ("состояние
    // игрока") НЕ реализован -- в проекте нет структуры, хранящей
    // собственное FRealState героя отдельно от клеток (см. тот же довод,
    // тем же честным пробелом, что уже Гнёздово-район у Курганов, §4.3) --
    // ROADMAP держит это открытым, не молчаливым упрощением.
    FIntPoint GetTotemSite() const { return TotemSite; }
    // Сеттеры Тотема, Светлояра и Горюч-камня ставят и актор-визуал: прежний
    // актор этого менеджера убирается, новый встаёт на место (загрузка сейва
    // оставляла актор на месте свежего посева, ревью 2026-09-14).
    void SetTotemSite(const FIntPoint& InSite);
    FString GetTotemRevealText() const;

    // Средний ярус (DESIGN_POI_Art_And_LevelDesign.md, "открытые вопросы —
    // решения", 2026-09-06) -- читается по Молве (уже существующий
    // continuous field, Molva), не по отдельному "состоянию игрока": та
    // самая структура, которой не хватало для честной реализации этого
    // яруса раньше (юнит 1/2), закрыта переиспользованием уже существующей
    // общинной переменной, не новой.
    bool IsTotemMiddleTierVisible() const;

    // Светлояр (§4.5) -- прямой потребитель уже существующей
    // GlobalPerceptionClarity, ничего нового не считает: город виден/слышен
    // выше BuyanGuardianClarityThreshold (тот же порог "высокой Clarity",
    // что уже гейтит стража Буяна -- Светлояр самим документом назван
    // "прямым предком Буяна в миниатюре", один и тот же порог, не два
    // рассинхронизированных числа с одинаковым смыслом).
    FIntPoint GetSvetloyarSite() const { return SvetloyarSite; }
    void SetSvetloyarSite(const FIntPoint& InSite);
    bool IsSvetloyarVisible() const;

    // Три звуковых уровня ПОВЕРХ самого порога видимости (DESIGN_POI_Art_
    // And_LevelDesign.md §2: "дальний звон / звон+пение / вспышка купола +
    // хор", 2026-09-06) -- 0, пока IsSvetloyarVisible()==false; иначе 1/2/3
    // по тому, сколько из оставшегося диапазона Clarity (от порога
    // видимости до 1.0) уже пройдено. Вынесено на менеджер, а не оставлено
    // внутри APOI_Svetloyar::Tick -- тот же довод, что и у остальных
    // POI-запросов (IsSvetloyarVisible/GetTotemRevealText): актор только
    // читает уже посчитанный факт, логика тестируется без спавна актора.
    int32 GetSvetloyarSoundTier() const;

    // Горюч-камень (§4.5) -- архетип "упрямого камня", не буквальный
    // Синь-камень Плещеева озера: то имя уже занято ward-кристаллом яруса 2
    // (RitualTypes.h::ZakatnayaOpushka), коллизия найдена и разведена
    // 2026-09-06 (решение пользователя: новый Landmark получает другое имя).
    // Аномально высокое сопротивление сдвигу TargetState -- множитель,
    // применяемый ТОЛЬКО к клетке этой точки при релаксации
    // (GridWorldManagerCore.cpp, см. довод у GoryuchKamenResistance рядом с
    // применением).
    FIntPoint GetGoryuchKamenSite() const { return GoryuchKamenSite; }
    void SetGoryuchKamenSite(const FIntPoint& InSite);

    // Счётчик попыток применить зелье к Горюч-камню (DESIGN_POI_Art_And_
    // LevelDesign.md §3: "звук глухого удара, без видимого следствия
    // после") -- сама клетка не меняется, поэтому актору нечего поллить в
    // Tick, кроме этого счётчика. Растёт в ApplyAlchemyResult
    // (GridWorldManagerAlchemy.cpp) каждый раз, когда
    // FApplyCommand::bTargetIsGoryuchKamen резолвится в true -- НЕ
    // персистентен (сессионный счётчик, не игровой факт), сброс между
    // сохранениями не нужен.
    int32 GetGoryuchKamenApplyAttemptCount() const { return GoryuchKamenApplyAttemptCount; }

    // Соловей-разбойник (§4.4) -- одноразовая (за заход) AoE-порча
    // Purity/Stability по площади при активации, если игрок не прошёл под
    // прикрытием одолень-травы (IsWardConcealmentActive у его клетки -- тот
    // же оберег, что уже даёт "скрытие" от бестиария, здесь просто другая
    // угроза читает тот же флаг). "Усмирение" плакун-травой -- НЕ отдельный
    // механизм: обычное Apply зелья на клетку точки той же формулой лечит
    // её Meta, что и любую другую клетку, специального отслеживания не
    // требуется.
    FIntPoint GetSoloveySite() const { return SoloveySite; }
    void SetSoloveySite(const FIntPoint& InSite) { SoloveySite = InSite; }
    bool IsSoloveyTriggered() const { return bSoloveyTriggered; }
    void SetSoloveyTriggered(bool bTriggered) { bSoloveyTriggered = bTriggered; }
    bool ActivateSolovey();

    // Усмирение плакун-травой (§4.4, DESIGN_POI_Art_And_LevelDesign.md §4,
    // 2026-09-06) — снимает угрозу НАВСЕГДА, отдельно от bSoloveyTriggered
    // (тот про "уже сработал один раз", этот про "больше никогда не
    // сработает"): игрок может усмирить Соловья, ни разу не пройдя мимо.
    // Резолвится вне Pipeline (AGridWorldManager::ApplyAlchemyResult
    // проверяет ингредиент riv_11 при TargetCell==SoloveySite), тот же
    // принцип, что и у bTargetIsGoryuchKamen.
    bool IsSoloveyCalmed() const { return bSoloveyCalmed; }
    void SetSoloveyCalmed(bool bCalmed) { bSoloveyCalmed = bCalmed; }
    void CalmSolovey();

    // Калинов мост / Трёхглавый Змей (§4.4) -- в отличие от остальных POI
    // выше, само взаимодействие НЕ живёт здесь: это Landmark (см.
    // RegisterZmeyGorynych) + диалог (DT_Dialogue, ЗмейГорыныч), доступный
    // через уже существующий AHerbalistPlayerController::TalkTo/
    // ChooseDialogueBranch, тем же путём, что Домовой. Координата хранится
    // только для сева (не занять эту клетку другим POI) и для отладочного
    // запроса "где искать" -- сам поиск собеседника всё равно идёт через
    // FindLandmarkAt(Cell), не через это поле.
    FIntPoint GetKalinovMostSite() const { return KalinovMostSite; }
    void SetKalinovMostSite(const FIntPoint& InSite) { KalinovMostSite = InSite; }

    // Цена ветки "Бой" (FDialogueBranch::bIsKalinovMostFight) -- удар по
    // Purity/Stability клетки Змея, вызывается из
    // AHerbalistPlayerController::ChooseDialogueBranch.
    void ApplyKalinovMostFightCost(const FIntPoint& Cell);

    // Сделка (FDialogueBranch::bIsKalinovMostDeal, 2026-09-06) -- "вооружает"
    // сделку выбором ветки диалога, реальная жертва -- отдельной командой
    // (см. TryPayKalinovMostToll ниже). Сессионный флаг, НЕ персистентен --
    // тот же класс "короткого окна между двумя действиями игрока", что уже
    // ResetSessionOnlyWardTimers описывает для оберегов: сохранение/загрузка
    // между выбором ветки и уплатой пошлины — редкий, не защищаемый явно
    // случай, разговор в любом случае придётся начать заново после загрузки
    // (CurrentDialogueID тоже не персистентен).
    void ArmKalinovMostDeal() { bKalinovMostDealPending = true; }
    bool IsKalinovMostDealPending() const { return bKalinovMostDealPending; }

    // Завершает уже вооружённую сделку: списывает названный артефакт из
    // AcquiredArtifacts безвозвратно. false, если сделка не вооружена
    // (диалоговая ветка не выбиралась) или названного артефакта нет во
    // владении -- в обоих случаях ничего не списывается.
    bool TryPayKalinovMostToll(FName ArtifactID);

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
        for (FGridCell& Cell : GetCellsInGridOrder()) Func(Cell);
    }

    template<typename TFunc>
    void ForEachCell(TFunc&& Func) const
    {
        for (const FGridCell& Cell : GetCellsInGridOrder()) Func(Cell);
    }

protected:
    // ---- Данные мира ----
    // Клетки страницами (этап 8). Таблица страниц -- прямоугольник страниц
    // сетки построчно от CellPageTableMin (в страницах); CellPageSize -- клеток
    // на сторону страницы, 0 -- одна страница во всю сетку (без разметки).
    TArray<FHerbalistCellPage> CellPages;
    FIntPoint CellPageTableMin = FIntPoint::ZeroValue;
    FIntPoint CellPageTableSize = FIntPoint::ZeroValue;
    int32 CellPageSize = 0;
    int32 LoadedCellCount = 0;
    FRandomStream WorldRNG;

    // Отклонения клеток выгруженных страниц от основы, по линейному индексу
    // сетки (этап 8в).
    TMap<int32, FSavedCellState> UnloadedCellDeltas;

    // Засеянные клетки выгруженных страниц (бит по линейному индексу) и ростеры
    // нетронутых засеянных клеток -- без полной дельты на каждую посещённую
    // клетку (ревью этапа 8в).
    TBitArray<> SeededCellMask;
    TMap<int32, FHerbalistCellRoster> UnloadedCellRosters;

    // Отрастания, сработавшие в выгруженной странице: времена отрастания по
    // линейному индексу -- завершаются при её загрузке.
    TMap<int32, TArray<float>> PendingRegrowthsOnLoad;

    // Страницы мест за убранными плитками ландшафта (2026-09-13): сетка
    // расширена до них при загрузке сейва, и они закреплены -- места живут без
    // земли. Остальные страницы расширения -- заполнитель
    // (IsCellInExtensionFiller). Первая клетка страницы; сбрасывается в
    // CreateCellPages, между загрузками копится -- сетка в сессии не сужается.
    TSet<FIntPoint> PinnedSitePages;

    // Ширина строки блоков фолбэка биомов, снятая в InitializeCells: расширение
    // сетки до страниц мест не меняет основу клеток фолбэка.
    int32 FallbackBiomeBlocksX = 0;

    // Сетка ландшафта при инициализации -- до расширения до страниц мест.
    FIntPoint LandscapeGridMinCell = FIntPoint::ZeroValue;
    FIntPoint LandscapeGridSize = FIntPoint::ZeroValue;

    // Материализованные или активные чанки изменились -- проверить, какие
    // страницы простаивают.
    bool bCellPageUnloadCheckPending = false;

    // Хэндл таймера GridCorruptionReportIntervalSeconds выше -- остановлен в
    // EndPlay, без этого таймер на уничтоженном акторе мог бы выстрелить в
    // persistent editor-мире между автотестами (та же причина, что уже
    // чистит менеджеры прежних тестов в TestWorldHelpers.h::SpawnAndBeginPlay).
    FTimerHandle GridCorruptionReportTimerHandle;

    // Хэндл таймера WorldStateMapUpdateIntervalSeconds -- та же причина
    // остановки в EndPlay, что у соседа выше.
    FTimerHandle WorldStateMapTimerHandle;

    // Показываемое состояние мира: то же, что в клетках, но догоняющее их с
    // ограниченной скоростью. Индекс -- построчно от угла окна карты
    // (этап 7). НЕ сохраняется намеренно -- это состояние
    // картинки, а не мира; при загрузке оно приравнивается к настоящему
    // (SnapWorldStateMapDisplayToWorld).
    // XYZW = Distortion / Corruption / HarvestStress / ShrineRestoration.
    TArray<FVector4f> DisplayedWorldState;

    // Окно карты (этап 7): где оно стоит и для какого угла окна посчитано
    // показываемое состояние. Пока окно не ставилось -- по центру сетки.
    FIntPoint WorldStateWindowMin = FIntPoint::ZeroValue;
    bool bWorldStateWindowPlaced = false;
    FIntPoint DisplayedWorldStateMin = FIntPoint::ZeroValue;

    // Угол окна для клетки зрителя: к ближайшему кратному тайла, окно не
    // выходит за сетку.
    FIntPoint ComputeWorldStateWindowMin(const FIntPoint& ViewerCell, const FIntPoint& Size) const;

    // Сторона тайла перецентровки -- восьмая часть окна разметки.
    int32 GetWorldStateWindowTile() const;

    // Клетка зрителя: точка обзора локального игрока 0, без него -- центр
    // первого чанка активности. false -- зрителя нет.
    bool GetWorldStateViewerCell(FIntPoint& OutCell) const;

    // Перенести показываемое состояние в окно с другим углом того же размера:
    // перекрытие сохраняет сглаживание, вошедшие клетки приравниваются к миру.
    void ShiftWorldStateMapDisplay(const FIntPoint& NewMin, const FIntPoint& Size);

    // Выгрузка карты: окно, шаг сглаживания DisplayStepSeconds, пиксели,
    // рамка. Таймер шагает на свой период, внеочередной вызов из Tick -- на 0.
    void UploadWorldStateMap(float DisplayStepSeconds);

    // ---- Ландшафт и кеш высот ----
    UPROPERTY()
    TObjectPtr<ALandscape> CachedLandscape;

    // Высоты клеток лежат в страницах (FHerbalistCellPage::Heights, этап 8).
    bool bCellHeightsCached = false;

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

    // Регионы воды -- тот же довод: по ним основа страницы при загрузке
    // заливает воду (вода только из регионов воды, 2026-09-13).
    UPROPERTY()
    TArray<TWeakObjectPtr<AWaterRegionVolume>> CachedWaterRegions;

    // ---- Вспомогательные данные ----
    TMap<int32, float> LastHarvestTimeMap;
    const float HarvestCooldown = 0.2f;

    double GameClockSeconds = 0.0;

    // Клетки, отклонившиеся от детерминированной генерации (DESIGN_World_State.md
    // §3 Вариант A + разбор открытых миров — Valheim/Skyrim и т.п. сохраняют
    // только тронутое, не весь мир). Липкая: раз клетка тронута, остаётся в
    // сейве даже после релаксации обратно к базе — дешевле и надёжнее, чем
    // сверять на выходе "а не совпало ли снова с базой ровно".
    TSet<int32> DirtyCellIndices;

    // Сводки чанков (разметка мира, этап 7): кэш неживых чанков и чанки, чья
    // сводка устарела. mutable -- кэш заполняется в const-запросах.
    mutable TMap<FIntPoint, FHerbalistChunkSummary> ChunkSummaries;
    mutable TSet<FIntPoint> StaleChunkSummaries;
    // Нарезка, под которую собран кэш (ревью этапа 7): без разметки размер
    // чанка читается из настроек и меняется на лету.
    mutable int32 ChunkSummaryChunkSize = 0;
    mutable FIntPoint ChunkSummaryMinCell = FIntPoint::ZeroValue;
    mutable FIntPoint ChunkSummaryGridSize = FIntPoint::ZeroValue;
    mutable bool bIteratingChunkSummaries = false;

    // Baseline на клетку (аудит 2026-09-05, решение пользователя: полноценный
    // откат вместо тихого игнорирования). Снимок КАЖДОЙ клетки сразу после
    // InitializeCells — до единого тика симуляции, до единого игрового
    // действия. Лежит в странице рядом с клеткой (Baselines, этап 8), не сохраняется
    // сам по себе: как и Biome/вода, это чистая функция RngBaseSeed +
    // расставленных на уровне ABiomeRegionVolume, пересчитывается заново при
    // каждом InitializeCells. Используется ТОЛЬКО в ApplySaveCells — клетка,
    // грязная СЕЙЧАС, но отсутствующая в самом сейве, по построению
    // DirtyCellIndices (см. довод там же) была нетронутой на момент
    // сохранения, то есть равнялась ровно этому снимку — не сохранённое
    // "среднее", а буквально то, чем была клетка, пока её не тронули.

    // Отслеживаемое среднее качество каждого вида, реально прошедшего через
    // общину (Подношение + предложенная сторона Обмена) — аудит 2026-09-05,
    // решение пользователя: желаемая сторона Обмена (см. TryTradeWithCommunity)
    // больше не оценивается по вечно чистому табличному BaseState, а по тому,
    // что община РЕАЛЬНО получала. Взвешено количеством реально отданных
    // единиц (Count=1 за слот у подношения — ровно то, что списывается,
    // см. довод у OfferToCommunity; полный Item.Count у обмена — ровно то,
    // что списывает TradeWithCommunity). Пока для вида нет ни одной записи
    // (община ещё не получала его вовсе) — честный фолбэк на BaseState в
    // TryTradeWithCommunity, не на нулевое состояние. Не сохраняется —
    // сессионное состояние, тем же допущением, что и у остального
    // общинного курса (Molva — исключение, персистится отдельно).
    TMap<FName, FRealState> CommunityIngredientQuality;
    TMap<FName, int32> CommunityIngredientSampleCount;

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
    // применения. InvalidCell() = никогда не применялась (тот же сентинел, что
    // уже ZaryanaCell использует для "не размещена").
    FIntPoint InvisibilityCapCenter = HerbalistCore::InvalidCell();

    // ---- Обереги (кристаллы Пещеры, §2.4, 2026-09-04) — тот же
    // GameClockSeconds-сентинел, что и все поля выше. Не персистятся
    // (Save/Load) — тот же сознательно принятый класс "короткого окна", что
    // уже не сохраняют InvisibilityCap/YouthApple/Alkonost выше: активация
    // — часть текущей игровой сессии, не долгоживущий прогресс. ----
    float WardBrewBoostExpiryGameSeconds = 0.0f;
    float WardConcealmentExpiryGameSeconds = 0.0f;
    FIntPoint WardConcealmentCenter = HerbalistCore::InvalidCell();
    float WardMorokReductionExpiryGameSeconds = 0.0f;
    FIntPoint WardMorokReductionCenter = HerbalistCore::InvalidCell();

    // ---- Тиражные обереги (награда ритуалов перехода ярусов биомов,
    // 2026-09-04, GridWorldManagerWards.cpp::ActivateTieredWard) -- НЕТ
    // ExpiryGameSeconds-поля, в отличие от пяти выше: "нет таймера" в
    // прямом запросе означает именно отсутствие срока действия, не
    // сентинел с огромным числом. В ОТЛИЧИЕ от остальных Ward-полей этого
    // блока -- ПЕРСИСТЯТСЯ (аудит 2026-09-05, решение пользователя (а):
    // постоянная награда за ритуал обязана пережить перезагрузку, см.
    // CaptureTieredWards/RestoreTieredWards выше). ----
    bool bTieredConcealmentActive = false;
    TArray<EBiomeType> TieredConcealmentHomeBiomes;
    bool bTieredMorokReductionActive = false;
    TArray<EBiomeType> TieredMorokReductionHomeBiomes;
    bool bTieredBrewBoostActive = false;
    TArray<EBiomeType> TieredBrewBoostHomeBiomes;

    // ---- Серебряный оберег (Ось Б §2.3, 2026-09-06) -- см. IsSilverWardActive
    // выше. Персистентен (Save/Load), тот же принцип, что тиражные обереги
    // выше -- находка в кургане, не эффект короткого окна.
    bool bSilverWardActive = false;

    // ---- Курганы (§2.3/§4.3 DESIGN_Brewing_Situations_And_Lore.md,
    // 2026-09-06) -- см. SeedKurganSites/LootKurgan выше. Персистентна
    // (Save/Load): разграбленный курган не должен "возрождаться" при загрузке.
    TMap<FIntPoint, FName> KurganSites;

    // Содержимое контейнеров карты, чей актор сейчас выгружен World Partition
    // (2026-09-14): выгрузка уничтожает актор, и сундук, от которого отошли,
    // возвращался пустым, а сейв вдали от него терял его содержимое. Ключ --
    // имя актора, см. FSavedPlacedContainer.
    TMap<FName, TArray<FInventoryItem>> PendingPlacedContainerContents;

    // ---- Точки интереса, §4 (см. POITypes.h) -- по одной клетке на вид,
    // HerbalistCore::InvalidCell() значит "не размещена" (та же сигнальная величина,
    // что уже ZaryanaCell/InvisibilityCapCenter/WardConcealmentCenter выше
    // в этом файле, не новая придуманная). Персистентны (Save/Load) --
    // детерминированный сев зависит от порядка вызовов WorldRNG, повторный
    // сев при загрузке дал бы другие клетки, если что-то ещё в
    // InitializeCells успело измениться между сохранением и загрузкой.
    FIntPoint TotemSite = HerbalistCore::InvalidCell();
    FIntPoint SvetloyarSite = HerbalistCore::InvalidCell();
    FIntPoint GoryuchKamenSite = HerbalistCore::InvalidCell();
    FIntPoint SoloveySite = HerbalistCore::InvalidCell();
    bool bSoloveyTriggered = false;
    bool bSoloveyCalmed = false;
    FIntPoint KalinovMostSite = HerbalistCore::InvalidCell();
    int32 GoryuchKamenApplyAttemptCount = 0;
    bool bKalinovMostDealPending = false;

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
    // InvalidCell() = не размещена — SetZaryanaCellIfUnset (AAlchemyTableActor::
    // BeginPlay) или ручная расстановка в редакторе задают реальное значение.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Zaryana")
    FIntPoint ZaryanaCell = HerbalistCore::InvalidCell();

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

    // Страницы под текущую сетку и разметку, все загружены (этап 8).
    void CreateCellPages();
    FHerbalistCellPage* FindCellPage(int32 X, int32 Y);
    const FHerbalistCellPage* FindCellPage(int32 X, int32 Y) const;
    const FSavedCellState* FindCellBaselineByGridIndex(int32 GridIndex) const;

    // ---- Выгрузка и загрузка страниц (этап 8в, DESIGN_World_Layout.md §6) ----
    FHerbalistCellBaseContext MakeCellBaseContext() const;

    // Основа клетки -- чистая функция координаты: биом и веса по регионам,
    // блочный фолбэк, умолчания, вода по регионам воды.
    // Возвращает регион, заявивший клетку (nullptr -- блочный фолбэк).
    ABiomeRegionVolume* BuildCellBase(int32 X, int32 Y, FGridCell& OutCell, const FHerbalistCellBaseContext& Context) const;
    void ApplyWaterToCell(FGridCell& Cell, const UWaterTypeRegistrySubsystem* WaterSubsystem) const;

    // Клетка выгруженной страницы: основа плюс её дельта, без акторов. false --
    // клетка загружена или вне сетки.
    bool BuildUnloadedCell(int32 X, int32 Y, FGridCell& OutCell, const FHerbalistCellBaseContext& Context) const;

    void CacheCellHeightsForPage(FHerbalistCellPage& Page);
    // Акторы мест (курганы, точки интереса, маркеры якорей и хозяйства) на
    // странице -- на землю. Спавн на выгруженной странице или до загрузки
    // ландшафта под ней ставит их на высоту 0 (ревью 2026-09-14).
    void PlaceSiteActorsOnGround(const FHerbalistCellPage& Page);
    void LoadCellPage(FHerbalistCellPage& Page);
    void UnloadCellPage(FHerbalistCellPage& Page);
    void EnsureChunkPagesLoaded(const FIntPoint& Chunk);

    // Выгрузить страницы без материализованных и активных чанков -- только
    // когда земля известна и материализация ведётся.
    void UnloadIdleCellPagesIfPending();
    bool IsCellPageIdle(const FHerbalistCellPage& Page) const;

    // На странице -- хозяин места, легендарный якорь или капище: их логика идёт
    // вдали от игрока, и страница не выгружается (ревью этапа 8в).
    bool IsCellPagePinned(const FHerbalistCellPage& Page) const;

    // Страницы, задевающие чанк (без разметки -- единственная страница).
    void ForEachChunkPage(const FIntPoint& Chunk, TFunctionRef<void(FHerbalistCellPage&)> Func);

    // Кэш сводок собран под размер чанка и прямоугольник сетки -- выставить ключ
    // до записи в кэш (ревью этапа 8в).
    void EnsureChunkSummaryCacheKey() const;
    bool IsChunkInLoadedPage(const FIntPoint& Chunk) const;
    void GetPageChunkRange(const FHerbalistCellPage& Page, FIntPoint& OutMinChunk, FIntPoint& OutMaxChunk) const;

    // Разметка при старте игры: пересчёт, применение, если поля разошлись с
    // ней, сверка с ландшафтом и ячейками стриминга, строка в лог.
    void InitializeWorldLayoutForPlay();
    void VerifyWorldLayoutAgainstWorld(TArray<FString>& OutWarnings) const;

    // ---- Маркеры состояния ----
    void MarkCellDirty(int32 X, int32 Y)
    {
        // Координата за краем попала бы на клетку соседней строки (ревью этапа 6).
        if (!IsCellInGrid(X, Y)) return;
        DirtyCellIndices.Add(GetCellIndex(X, Y));
        // Сводка чанка устарела (этап 7): неживой чанк иначе отдал бы кэш.
        StaleChunkSummaries.Add(GetChunkCoordForCell(X, Y));
    }

    // Сводка одного чанка обходом его клеток (этап 7).
    FHerbalistChunkSummary BuildChunkSummary(const FIntPoint& Chunk) const;

    // Клетки созданы или заменены целиком -- все сводки считаются заново.
    void InvalidateAllChunkSummaries()
    {
        ChunkSummaries.Reset();
        StaleChunkSummaries.Reset();
    }

    // Локальный индекс в массивах клеток для глобальной координаты (этап 6).
    inline int32 GetCellIndex(int32 X, int32 Y) const
    {
        const FIntPoint Min = GetGridMinCell();
        return (Y - Min.Y) * GridSizeX + (X - Min.X);
    }

    // Общая точка записи FSavedCellState в живую клетку — используется и
    // обычным восстановлением из сейва (ApplySaveCells), и откатом клеток,
    // тронутых после сейва, к снимку страницы (аудит 2026-09-05). Определение —
    // GridWorldManagerSave.cpp.
    void ApplyCellStateAndRespawnResources(FGridCell& Cell, const FSavedCellState& Saved);

    // Поля клетки из сейва или дельты без ростера ресурсов (этап 8в): общая
    // часть загрузки сейва и сборки клетки выгруженной страницы.
    static void CopySavedCellFields(FGridCell& Cell, const FSavedCellState& Saved);

    // Обратная операция: снимок клетки в FSavedCellState (X/Y/State/
    // TargetState/HarvestStress/Memory/ManifestedEntityID/bEternallyPure/
    // PlantedSpeciesID/bResourcesSeeded/ResourceIngredientIDs). Общая для
    // CaptureSaveCells (реальный сейв, GridWorldManagerSave.cpp) и
    // InitializeCells (снимки страниц, GridWorldManagerCore.cpp) — одна
    // формула, не две разные копии одной идеи. Static — чистая функция от
    // параметра, this не трогает.
    static FSavedCellState CaptureCellState(const FGridCell& Cell);

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

    // Накопитель шага восстановления клеток (см. CellRegenerationStepSeconds).
    float CellRegenerationAccumulator = 0.0f;

    // Поколение таймеров отрастания: загрузка сейва его сдвигает, и таймеры,
    // поставленные до неё, срабатывают вхолостую (2026-09-14).
    int32 RegrowthTimerGeneration = 0;
    // Сколько таймеров отрастания поставлено за жизнь менеджера -- для автотестов.
    int32 RegrowthTimersScheduled = 0;

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

    // Материализованные чанки (2026-09-12) -- см. UpdateMaterializedChunks.
    TSet<FIntPoint> MaterializedChunks;
    // Отслеживание включается с первым непустым набором центров или с первой
    // известной землёй; до этого (headless-тесты, кадры до появления игрока в
    // мире без стриминга) спавн идёт как раньше.
    bool bMaterializationTracked = false;
    // Прямоугольники загруженной земли на текущий кадр (XY, мировые).
    TArray<FBox2D> GroundCoverage;
    bool bGroundCoverageKnown = false;
    bool bLoggedEmptyGroundCoverage = false;
    TOptional<TArray<FBox2D>> GroundCoverageOverride;
    // Отпечаток земли, по которому последний раз собран набор чанков: пока
    // прямоугольники те же, пересобирать нечего.
    uint32 CoverageCacheHash = 0;
    bool bCoverageCacheValid = false;
    void RefreshGroundCoverage();
    void CollectGroundCoveredChunks(TSet<FIntPoint>& OutChunks) const;

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
    int32 SelectedX = HerbalistCore::InvalidCell().X, SelectedY = HerbalistCore::InvalidCell().Y;
};