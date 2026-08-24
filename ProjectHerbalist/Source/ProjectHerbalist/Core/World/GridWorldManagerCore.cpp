// Core/World/GridWorldManagerCore.cpp
#include "Core/World/GridWorldManager.h"
#include "Landscape.h"
#include "EngineUtils.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/BiomeRow.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "TimerManager.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/PerceptionComponent.h"
#include "Templates/TypeHash.h"
#include "Core/Types/HerbalistCoreMath.h"

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ (ЛАНДШАФТ)
// ============================================================================

void AGridWorldManager::FindAndCacheLandscape()
{
    if (CachedLandscape) return;
    UWorld* World = GetWorld();
    if (!World) return;

    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        CachedLandscape = *It;
        break;
    }
    if (!CachedLandscape)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("No Landscape found in level. Grid cells will use flat Z."));
    }
    else
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("Landscape found: %s"), *CachedLandscape->GetName());
    }
}

void AGridWorldManager::CacheCellHeights()
{
    FindAndCacheLandscape();
    const int32 TotalCells = GridSizeX * GridSizeY;
    CachedCellHeights.SetNum(TotalCells);

    if (!CachedLandscape)
    {
        for (int32 i = 0; i < TotalCells; ++i) CachedCellHeights[i] = 0.f;
        return;
    }

    FVector GridOrigin = GetActorLocation();
    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
        {
            FVector WorldPoint(GridOrigin.X + X * CellSize, GridOrigin.Y + Y * CellSize, 0.f);
            TOptional<float> OptHeight = CachedLandscape->GetHeightAtLocation(WorldPoint);
            float Z = OptHeight.IsSet() ? OptHeight.GetValue() : 0.f;
            int32 Idx = Y * GridSizeX + X;
            CachedCellHeights[Idx] = Z;
        }
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("Cached %d cell heights from landscape"), TotalCells);
}

float AGridWorldManager::GetCellHeight(int32 X, int32 Y) const
{
    int32 Idx = Y * GridSizeX + X;
    if (CachedCellHeights.IsValidIndex(Idx))
        return CachedCellHeights[Idx];
    return 0.f;
}

FVector AGridWorldManager::GetCellWorldPositionFlat(int32 X, int32 Y) const
{
    return GetActorLocation() + FVector(X * CellSize, Y * CellSize, 0.f);
}

FVector AGridWorldManager::GetCellWorldPosition(int32 X, int32 Y) const
{
    FVector Flat = GetCellWorldPositionFlat(X, Y);
    float Z = GetCellHeight(X, Y);
    // Центр отладочного бокса на уровне ландшафта
    return FVector(Flat.X, Flat.Y, Z);
}

bool AGridWorldManager::WorldPositionToCell(const FVector& WorldPos, int32& OutX, int32& OutY) const
{
    OutX = -1;
    OutY = -1;

    const FVector LocalLoc = WorldPos - GetActorLocation();
    const int32 X = FMath::FloorToInt(LocalLoc.X / CellSize);
    const int32 Y = FMath::FloorToInt(LocalLoc.Y / CellSize);

    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY)
    {
        OutX = X;
        OutY = Y;
        return true;
    }
    return false;
}

FGridCell* AGridWorldManager::GetCell(int32 X, int32 Y)
{
    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY)
        return &Cells[Y * GridSizeX + X];
    return nullptr;
}

const FGridCell* AGridWorldManager::GetCellConst(int32 X, int32 Y) const
{
    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY)
        return &Cells[Y * GridSizeX + X];
    return nullptr;
}

// ============================================================================
// БИОМЫ
// ============================================================================

TArray<FGridBiomeSample> AGridWorldManager::GetBiomeSamples() const
{
    TArray<FGridBiomeSample> Samples;
    for (const FGridCell& Cell : Cells)
    {
        FGridBiomeSample Sample;
        Sample.BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        Sample.MorokValue = Cell.State.Meta.Distortion;
        Sample.ZaryanaValue = 1.f - Cell.State.Meta.Distortion;
        Samples.Add(Sample);
    }
    return Samples;
}

TMap<FName, FVector> AGridWorldManager::GetBiomeCenters() const
{
    TMap<FName, FVector> Centers;
    TMap<FName, int32> Counts;
    for (const FGridCell& Cell : Cells)
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        FVector Pos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
        Centers.FindOrAdd(BiomeID) += Pos;
        Counts.FindOrAdd(BiomeID)++;
    }
    for (auto& Pair : Centers)
    {
        int32 Cnt = Counts[Pair.Key];
        if (Cnt > 0) Pair.Value /= Cnt;
    }
    return Centers;
}

void AGridWorldManager::ApplyBiomeInfluences(const TMap<FName, float>& MorokFields,
                                             const TMap<FName, float>& ZaryanaFields,
                                             float GlobalScale)
{
    // Единственный писатель в состояние клеток — ApplyStateDelta(). BiomeGraph не
    // мутирует Cells напрямую, а собирает Delta.TargetStateNudges, точно как Pipeline
    // собирает Delta.WorldChanges — так FStateDelta действительно единственный
    // источник изменений мира (Single Writer, Causal Execution Spec).
    FStateDelta Delta;

    for (const FGridCell& Cell : Cells)
    {
        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        FRealState NewTarget = Cell.TargetState;
        bool bChanged = false;

        // 1. Влияние Морока (увеличивает Distortion)
        const float* MorokField = MorokFields.Find(BiomeID);
        if (MorokField)
        {
            float MorokInfluence = *MorokField * 0.1f * GlobalScale;
            NewTarget.Meta.Distortion = FMath::Clamp(NewTarget.Meta.Distortion + MorokInfluence, 0.f, 1.f);
            bChanged = true;
        }

        // 2. Влияние Заряны (повышает Stability и Purity)
        const float* ZaryanaField = ZaryanaFields.Find(BiomeID);
        if (ZaryanaField)
        {
            float ZaryanaInfluence = *ZaryanaField * 0.05f * GlobalScale;
            NewTarget.Meta.Stability = FMath::Clamp(NewTarget.Meta.Stability + ZaryanaInfluence, 0.f, 1.f);
            NewTarget.Meta.Purity    = FMath::Clamp(NewTarget.Meta.Purity    + ZaryanaInfluence * 0.5f, 0.f, 1.f);
            bChanged = true;
        }

        if (bChanged)
        {
            Delta.TargetStateNudges.Add(FIntPoint(Cell.X, Cell.Y), NewTarget);
        }
    }

    ApplyStateDelta(Delta);
}

// ============================================================================
// ЖИЗНЕННЫЙ ЦИКЛ
// ============================================================================

AGridWorldManager::AGridWorldManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    PerceptionComponent = CreateDefaultSubobject<UPerceptionComponent>(TEXT("PerceptionComp"));
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    WorldRNG.Initialize(RngBaseSeed);

    if (Cells.Num() == 0)
    {
        InitializeCells();
    }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МИРА
// ============================================================================

void AGridWorldManager::InitializeCells()
{
    const int32 TotalCells = GridSizeX * GridSizeY;
    Cells.SetNum(TotalCells);

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance ? GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>() : nullptr;

    // Собираем все типы биомов
    TArray<EBiomeType> AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
    if (AllBiomes.Num() == 0)
    {
        AllBiomes = {
            EBiomeType::Tundra, EBiomeType::Taiga, EBiomeType::MixedForest,
            EBiomeType::BroadleafForest, EBiomeType::ForestSteppe,
            EBiomeType::Steppe, EBiomeType::Floodplain, EBiomeType::Bog
        };
    }

    // Закрашиваем сетку блоками по 5x5 клеток
    const int32 BlockSize = 5;
    const int32 BlocksX = GridSizeX / BlockSize;
    const int32 BlocksY = GridSizeY / BlockSize;

    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
        {
            int32 Index = Y * GridSizeX + X;
            EBiomeType biome = AllBiomes[( (Y / BlockSize) * BlocksX + (X / BlockSize) ) % AllBiomes.Num()];

            FGridCell& Cell = Cells[Index];
            Cell.Biome        = biome;
            Cell.State        = FBiomeDefaults::GetDefaultState(biome);
            Cell.TargetState  = Cell.State;
            Cell.Environment  = FBiomeDefaults::GetDefaultEnvironment(biome);
            Cell.Memory       = FMemoryState();
            Cell.X            = X;
            Cell.Y            = Y;
            Cell.HarvestStress = 0.0f;
            Cell.bIsWater     = false;
            Cell.WaterTypeID  = NAME_None;
        }
    }

    // ------------------------------------------------------------------------
    // Размещение водоёмов (≈20% клеток)
    // ------------------------------------------------------------------------
    int32 TargetWaterCount = FMath::Max(TotalCells / 5, 1);
    TArray<bool> IsWaterAlready;
    IsWaterAlready.Init(false, TotalCells);
    int32 PlacedWater = 0;

    while (PlacedWater < TargetWaterCount)
    {
        int32 W = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
        int32 H = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
        int32 StartX = WorldRNG.RandRange(0, GridSizeX - W);
        int32 StartY = WorldRNG.RandRange(0, GridSizeY - H);

        // Проверяем, свободна ли область
        bool bAreaFree = true;
        for (int32 dy = 0; dy < H && bAreaFree; ++dy)
            for (int32 dx = 0; dx < W; ++dx)
                if (IsWaterAlready[(StartY + dy) * GridSizeX + (StartX + dx)])
                    { bAreaFree = false; break; }

        if (!bAreaFree) continue;

        // Заливаем область водой
        for (int32 dy = 0; dy < H; ++dy)
        {
            for (int32 dx = 0; dx < W; ++dx)
            {
                int32 Idx = (StartY + dy) * GridSizeX + (StartX + dx);
                FGridCell& Cell = Cells[Idx];
                Cell.bIsWater = true;
                Cell.WaterTypeID = WaterSubsystem ? WaterSubsystem->GetRandomWaterType(Cell.Biome, WorldRNG) : NAME_None;

                FRealState waterState = FBiomeDefaults::GetDefaultWaterState(Cell.Biome);
                if (WaterSubsystem)
                {
                    if (const FWaterTypeRow* WaterRow = WaterSubsystem->GetWaterType(Cell.WaterTypeID))
                    {
                        waterState.Meta.Purity      = WaterRow->BasePurity;
                        waterState.Meta.Distortion  = WaterRow->BaseDistortion;
                        waterState.Meta.Stability   = WaterRow->BaseStability;
                        waterState.Meta.Potency     = WaterRow->BasePotency;
                        waterState.Meta.Corruption  = WaterRow->BaseCorruption;
                    }
                }
                Cell.State = waterState;
                Cell.TargetState = waterState;
                Cell.HarvestStress = 0.0f;
                Cell.ResourceActors.Empty();
                IsWaterAlready[Idx] = true;
                PlacedWater++;
            }
        }
        if (PlacedWater >= TargetWaterCount) break;
    }

    // ========================================================================
    // ВАЖНО: сначала кешируем высоты ландшафта, потом спавним ресурсы
    // ========================================================================
    CacheCellHeights();

    // Спавним ресурсы во всех не-водных клетках
    for (FGridCell& Cell : Cells)
    {
        if (!Cell.bIsWater)
        {
            SpawnResourcesInCell(Cell);
        }
    }

    // Вертикальный срез проявления сущностей (16_Entity_Manifestation) —
    // авто-расстановка тестовых клеток-обиталищ.
    SeedTestLandmarks();

    SetActorTickEnabled(true);
}

// ============================================================================
// РЕСУРСЫ
// ============================================================================

void AGridWorldManager::SpawnResourcesInCell(FGridCell& Cell)
{
    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    int32 NumResources = WorldRNG.RandRange(1, 3);
    for (int32 i = 0; i < NumResources; ++i)
    {
        FName IngredientID = IngredientSubsystem ? IngredientSubsystem->GetRandomResourceForBiome(Cell.Biome, Cell.State, WorldRNG) : NAME_None;
        if (IngredientID.IsNone()) continue;

        FVector Offset = FVector(WorldRNG.FRandRange(-CellSize * 0.3f, CellSize * 0.3f),
                                 WorldRNG.FRandRange(-CellSize * 0.3f, CellSize * 0.3f),
                                 0);
        // Плоская позиция (X,Y) плюс высота ландшафта + небольшой подъём (5 см)
        FVector SpawnPos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
        SpawnPos.Z = GetCellHeight(Cell.X, Cell.Y) + 5.0f;
        SpawnPos += Offset;

        const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
        if (!Row) continue;

        AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
        if (NewActor)
        {
            NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, Cell.X, Cell.Y, Row->Resilience);
            Cell.ResourceActors.Add(NewActor);
            UE_LOG(LogHerbalistWorld, Verbose, TEXT("Spawned %s at cell (%d,%d) with Z=%.1f"), *IngredientID.ToString(), Cell.X, Cell.Y, SpawnPos.Z);
        }
    }
}

void AGridWorldManager::SpawnResourceActor(FName IngredientID, int32 X, int32 Y, const FVector& Offset)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
    if (!Row) return;

    FVector SpawnPos = GetCellWorldPositionFlat(X, Y);
    SpawnPos.Z = GetCellHeight(X, Y) + 5.0f;
    SpawnPos += Offset;

    AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(AHerbalistResourceActor::StaticClass(), SpawnPos, FRotator::ZeroRotator);
    if (NewActor)
    {
        NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, X, Y, Row->Resilience);
        Cell->ResourceActors.Add(NewActor);
        UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnResourceActor: %s at cell (%d,%d) Z=%.1f"), *IngredientID.ToString(), X, Y, SpawnPos.Z);
    }
}

void AGridWorldManager::StartRegeneration(FGridCell& Cell)
{
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, &Cell]()
    {
        if (!Cell.bIsWater)
        {
            SpawnResourcesInCell(Cell);
            // В отличие от исходного броска в InitializeCells (тот безопасно
            // переигрывается заново из RngBaseSeed), это отросшее — не то же
            // самое, что дало бы InitializeCells на старте. Сейв должен его помнить.
            MarkCellDirty(Cell.X, Cell.Y);
        }
    }, ResourceRegrowthTime, false);
}

void AGridWorldManager::OnResourceCollected(AHerbalistResourceActor* Actor)
{
    if (!Actor) return;

    FGridCell* Cell = GetCell(Actor->GetGridX(), Actor->GetGridY());
    if (!Cell) return;

    // Удаляем актор из клетки
    Cell->ResourceActors.Remove(Actor);
    MarkCellDirty(Cell->X, Cell->Y);   // ростер ресурсов отличается от начального броска

    // Обновляем глобальное искажение для игрока (тултип). ComputePerceptionDistortion
    // учитывает не только Memory.AccumulatedDistortion, но и ночную надбавку
    // (Морочники) и местные проявления (Низший уровень, Гнильники).
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (PC && Cell)
    {
        PC->CurrentGlobalDistortion = ComputePerceptionDistortion(Cell->X, Cell->Y);
    }

    // Только команда в пайплайн – никакого прямого добавления!
    FCommandEntry Cmd;
    Cmd.Primitive             = ECommandPrimitive::Harvest;
    Cmd.Harvest.TargetCell    = FIntPoint(Cell->X, Cell->Y);
    Cmd.Harvest.IngredientID  = Actor->GetIngredientID();
    Cmd.Harvest.Amount        = 1;
    Cmd.Harvest.BaseState     = Actor->GetBaseState();
    Cmd.Harvest.Resilience    = Actor->GetResilience();
    Cmd.Harvest.MoonPhase     = GetMoonPhase();
    QueueCommand(Cmd);

    if (Cell->ResourceActors.Num() == 0 && !Cell->bIsWater)
    {
        StartRegeneration(*Cell);
    }
}

FRealState AGridWorldManager::CollectWater(int32 X, int32 Y)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell || !Cell->bIsWater) return FRealState();

    // Формируем команду Harvest для воды
    FCommandEntry Cmd;
    Cmd.Primitive             = ECommandPrimitive::Harvest;
    Cmd.Harvest.TargetCell    = FIntPoint(X, Y);
    Cmd.Harvest.IngredientID  = Cell->WaterTypeID;
    Cmd.Harvest.Amount        = 1;
    QueueCommand(Cmd);

    // Возвращаем пустое состояние — реальный сбор произойдёт через новый пайплайн
    return FRealState();
}

// ============================================================================
// SNAPSHOT / DELTA
// ============================================================================

FWorldSnapshot AGridWorldManager::CaptureState() const
{
    FWorldSnapshot Snapshot;
    for (const FGridCell& Cell : Cells)
    {
        Snapshot.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
    }

    // Сид пайплайна выводится из (RngBaseSeed, TickIndex), а не из WorldRNG:
    // WorldRNG используется генерацией мира/ресурсов и продвигается нерегулярно
    // (только при спавне/восстановлении ресурсов), из-за чего Pipeline получал бы
    // один и тот же "случайный" джиттер для всех Harvest/Apply команд между такими
    // событиями. HashCombine даёт детерминированный, но уникальный на каждый тик сид,
    // который к тому же переживает Trace/Replay (он хранится в самом снапшоте).
    Snapshot.TickIndex = CurrentTickID;
    Snapshot.WorldSeed = static_cast<int32>(HashCombine(static_cast<uint32>(RngBaseSeed), static_cast<uint32>(CurrentTickID)));
    Snapshot.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    Snapshot.Shrines = Shrines;
    return Snapshot;
}

void AGridWorldManager::ApplyStateDelta(const FStateDelta& Delta)
{
    for (const auto& Pair : Delta.WorldChanges)
    {
        const FIntPoint& Coord = Pair.Key;
        const FGridCell& NewCellData = Pair.Value;

        FGridCell* Cell = GetCell(Coord.X, Coord.Y);
        if (Cell)
        {
            Cell->State       = NewCellData.State;
            Cell->TargetState = NewCellData.State;
            Cell->Biome       = NewCellData.Biome;
            Cell->bIsWater    = NewCellData.bIsWater;
            Cell->WaterTypeID = NewCellData.WaterTypeID;
            Cell->HarvestStress = NewCellData.HarvestStress;     // добавить
            Cell->Memory        = NewCellData.Memory;            // добавить
            MarkCellDirty(Coord.X, Coord.Y);
        }
    }

    // Мягкие правки TargetState (BiomeGraph и подобные continuous-field источники) —
    // трогают только цель релаксации, не сам State.
    for (const auto& Pair : Delta.TargetStateNudges)
    {
        if (FGridCell* Cell = GetCell(Pair.Key.X, Pair.Key.Y))
        {
            Cell->TargetState = Pair.Value;
            MarkCellDirty(Pair.Key.X, Pair.Key.Y);
        }
    }
}

// ============================================================================
// COMMAND ALGEBRA
// ============================================================================

void AGridWorldManager::QueueCommand(const FCommandEntry& Cmd)
{
    PendingCommands.Add(Cmd);
}

// ============================================================================
// ВОСПРИЯТИЕ
// ============================================================================

const FPerceivedWorld* AGridWorldManager::GetPerceivedWorld() const
{
    return PerceptionComponent ? &PerceptionComponent->GetPerceivedWorld() : nullptr;
}

const FPerceivedInventory* AGridWorldManager::GetPerceivedInventory() const
{
    return PerceptionComponent ? &PerceptionComponent->GetPerceivedInventory() : nullptr;
}

// ============================================================================
// ОТЛАДОЧНАЯ ОТРИСОВКА
// ============================================================================

#if WITH_EDITOR
void AGridWorldManager::DrawGridDebug()
{
    if (!bEnableDebugDraw) return;
    for (const FGridCell& Cell : Cells)
    {
        FVector Center = GetCellWorldPosition(Cell.X, Cell.Y);
        FVector Extent = FVector(CellSize / 2.0f, CellSize / 2.0f, CellHeight / 2.0f);
        FColor Color;
        if (Cell.bIsWater)
        {
            Color = FColor::White;
        }
        else
        {
            float Distortion = Cell.State.Meta.Distortion;
            Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Distortion).ToFColor(false);
        }
        DrawDebugBox(GetWorld(), Center, Extent, Color, false, 0.0f, 0, BorderThickness);
    }
}
#endif

// ============================================================================
// ЭКОЛОГИЯ: ВОССТАНОВЛЕНИЕ ПАРАМЕТРОВ КЛЕТОК
// ============================================================================

void AGridWorldManager::RegenerateCellParameters(float DeltaTime)
{
    const float RegenerationRate = 0.0005f;   // 0.05% в секунду
    const float DeltaRegen = RegenerationRate * DeltaTime;

    // Спад HarvestStress: клетка со стрессом 1.0 полностью зарастает за
    // StressRecoveryGameDays игровых суток, умноженные на множитель биома
    // (болото со стоячей водой держит след дольше, пойма промывает быстрее).
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RecoveryDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    const float DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f;

    // Множитель зависит только от типа биома, а не от клетки — тянем строку
    // DataTable один раз на биом, а не 400 раз за кадр.
    TMap<EBiomeType, float> StressDecayPerSecond;
    auto GetStressDecay = [&](EBiomeType Biome) -> float
    {
        if (const float* Cached = StressDecayPerSecond.Find(Biome))
        {
            return *Cached;
        }
        float Multiplier = 1.0f;
        if (const FBiomeRow* Row = FBiomeDefaults::GetBiomeRow(Biome))
        {
            Multiplier = FMath::Max(Row->StressRecoveryMultiplier, 0.05f);
        }
        const float Decay = 1.0f / FMath::Max(RecoveryDays * DaySeconds * Multiplier, KINDA_SMALL_NUMBER);
        StressDecayPerSecond.Add(Biome, Decay);
        return Decay;
    };

    // Шаг 1 новой модели хранения (DESIGN_World_State.md §5): State хранится
    // как отклонение от TargetState, а не как независимая величина — сама
    // релаксация сводится к затуханию этого отклонения, и клетки с нулевым
    // отклонением можно не трогать вовсе. Раньше здесь были явные if/else
    // Min/Max по четырём осям из семи (Potency/Resonance/Corruption вообще
    // не восстанавливались к TargetState — молчаливый пробел, закрытый этим
    // же проходом заодно, без отдельной задачи). Direction/TargetState как
    // хранимые поля пока остаются (полный переход на процедурную базу —
    // отдельные, более крупные шаги 2-3 того же плана), меняется только
    // форма релаксации внутри них.
    auto MoveToward = [](float& Current, float Target, float Step) -> bool
    {
        if (Current == Target) return false;
        if (Current < Target) Current = FMath::Min(Current + Step, Target);
        else                  Current = FMath::Max(Current - Step, Target);
        return true;
    };

    const float DegradeCenter = Settings ? Settings->BiomeDegradeCenterCorruption : 0.75f;
    const float DegradeMargin = Settings ? Settings->BiomeDegradeMargin : 0.10f;

    for (FGridCell& Cell : Cells)
    {
        // Бистабильная релаксация (обсуждение в сессии 2026-08-24) — общий
        // случай того, что раньше делали только Гнильники для Болота. Гистерезис
        // на Corruption клетки решает, куда сама релаксация её тянет: выше порога
        // входа цель — испорченный полюс (само восстановление невозможно, только
        // усугубляет), ниже порога выхода — здоровое умолчание биома/воды. Порог
        // меняется только на переходе (не каждый тик), иначе TargetState дёргался
        // бы туда-обратно на каждом пересечении границы гистерезиса.
        const bool bWasDegrading = Cell.Memory.bDegrading;
        Cell.Memory.bDegrading = HerbalistCore::Math::PassesHysteresisThreshold(
            bWasDegrading, Cell.State.Meta.Corruption, DegradeCenter, DegradeMargin);

        if (Cell.Memory.bDegrading != bWasDegrading)
        {
            if (Cell.Memory.bDegrading)
            {
                Cell.TargetState.Meta.Corruption = 1.0f;
                Cell.TargetState.Meta.Purity     = 0.0f;
                Cell.TargetState.Meta.Distortion = 1.0f;
                Cell.TargetState.Meta.Stability  = 0.0f;
            }
            else
            {
                // Игрок продавил Corruption ниже порога выхода — цель
                // возвращается к здоровому умолчанию биома (или воды,
                // у неё отдельная таблица default-состояний).
                const FRealState Healthy = Cell.bIsWater
                    ? FBiomeDefaults::GetDefaultWaterState(Cell.Biome)
                    : FBiomeDefaults::GetDefaultState(Cell.Biome);
                Cell.TargetState.Meta.Corruption = Healthy.Meta.Corruption;
                Cell.TargetState.Meta.Purity     = Healthy.Meta.Purity;
                Cell.TargetState.Meta.Distortion = Healthy.Meta.Distortion;
                Cell.TargetState.Meta.Stability  = Healthy.Meta.Stability;
            }
        }

        FRealState& S = Cell.State;
        const FRealState& T = Cell.TargetState;

        // Капище, эффект 1 (§15.5: "снижают сопротивление среды... повышают
        // устойчивость изменений"). Релаксация всегда движется к TargetState —
        // "приближает ли шаг к S0" определяется тем, ближе ли сама TargetState
        // к S0, чем текущая State, без гипотетического пробного шага. Clamp
        // на -0.95 — Influence может быть отрицательным (осквернённое капище,
        // Restoration < 0), (1+Influence) не должен доходить до нуля в знаменателе.
        float CellDeltaRegen = DeltaRegen;
        float DirectionRateMultiplier = 1.0f;
        if (Shrines.Num() > 0)
        {
            const float Influence = HerbalistCore::Shrine::GetInfluenceAt(
                FIntPoint(Cell.X, Cell.Y), Shrines, Settings ? Settings->ShrineInfluenceRadius : 3);
            if (Influence != 0.0f)
            {
                const float SafeInfluence = FMath::Clamp(Influence, -0.95f, 1.0f);
                const bool bApproachingS0 = HerbalistCore::Math::Distance(T, FAlatyr::S0) < HerbalistCore::Math::Distance(S, FAlatyr::S0);
                const float Modulation = bApproachingS0 ? (1.0f + SafeInfluence) : (1.0f / (1.0f + SafeInfluence));
                CellDeltaRegen *= Modulation;
                DirectionRateMultiplier = Modulation;
            }
        }

        // 1. Отклонение по Meta + Magnitude — единым шагом на все семь осей
        // вместо прежних четырёх. bChanged нужен только для раннего выхода
        // из релаксации Direction ниже, спад HarvestStress идёт независимо.
        bool bChanged = false;
        bChanged |= MoveToward(S.Meta.Distortion, T.Meta.Distortion, CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Purity,     T.Meta.Purity,     CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Stability,  T.Meta.Stability,  CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Potency,    T.Meta.Potency,    CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Resonance,  T.Meta.Resonance,  CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Corruption, T.Meta.Corruption, CellDeltaRegen);
        bChanged |= MoveToward(S.Magnitude,       T.Magnitude,       CellDeltaRegen);

        // 2. Спад HarvestStress — медленный, зависит от биома (см. выше)
        const bool bStressDecaying = Cell.HarvestStress > 0.0f;
        Cell.HarvestStress = FMath::Max(Cell.HarvestStress - GetStressDecay(Cell.Biome) * DeltaTime, 0.0f);

        // 3. Направление — та же идея (движение к отклонению-нулю), но
        // Direction нормализуется отдельно (NormalizeSum), поэтому не
        // укладывается в MoveToward построчно; пропускаем полностью, если
        // отклонение уже пренебрежимо мало — это и есть "не обходить клетки
        // с нулевым отклонением" из плана.
        const FDirection& TargetDir = T.Direction;
        const float DirDeviation = FMath::Abs(S.Direction.Body - TargetDir.Body)
                                  + FMath::Abs(S.Direction.Mind - TargetDir.Mind)
                                  + FMath::Abs(S.Direction.Spirit - TargetDir.Spirit)
                                  + FMath::Abs(S.Direction.Nature - TargetDir.Nature);
        if (DirDeviation > KINDA_SMALL_NUMBER)
        {
            const float DirRate = 0.01f * DirectionRateMultiplier;
            S.Direction.Body   = FMath::Clamp(S.Direction.Body   + (TargetDir.Body   - S.Direction.Body) * DirRate * DeltaTime, 0.0f, 1.0f);
            S.Direction.Mind   = FMath::Clamp(S.Direction.Mind   + (TargetDir.Mind   - S.Direction.Mind) * DirRate * DeltaTime, 0.0f, 1.0f);
            S.Direction.Spirit = FMath::Clamp(S.Direction.Spirit + (TargetDir.Spirit - S.Direction.Spirit) * DirRate * DeltaTime, 0.0f, 1.0f);
            S.Direction.Nature = FMath::Clamp(S.Direction.Nature + (TargetDir.Nature - S.Direction.Nature) * DirRate * DeltaTime, 0.0f, 1.0f);
            S.Direction.NormalizeSum();
        }

        // 4. Синхронизация памяти клетки (для графа и тултипа)
        Cell.Memory.AccumulatedDistortion = S.Meta.Distortion;

        // Сохранения (Core/Save/): релаксация — единственный писатель State/
        // HarvestStress вне ApplyStateDelta, помечаем клетку тронутой отдельно.
        if (bChanged || DirDeviation > KINDA_SMALL_NUMBER || bStressDecaying)
        {
            MarkCellDirty(Cell.X, Cell.Y);
        }
    }
}

void AGridWorldManager::DrawBiomeGraphDebug()
{
#if WITH_EDITOR
    if (!bShowBiomeGraph && !bShowCellDistortion && !bShowCellInfluence) return;

    UWorld* World = GetWorld();
    if (!World) return;

    UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
    if (!Graph) return;

    if (bShowBiomeGraph)
    {
        const TMap<FName, FVector>& Centers = Graph->GetCachedBiomeCenters();
        const TArray<FBiomeGraphEdge>& Edges = Graph->GetEdges();
        const TMap<FName, FBiomeGraphNode>& Nodes = Graph->GetNodes();

        for (const FBiomeGraphEdge& Edge : Edges)
        {
            const FVector* FromPos = Centers.Find(Edge.FromBiome);
            const FVector* ToPos   = Centers.Find(Edge.ToBiome);
            if (FromPos && ToPos)
            {
                DrawDebugLine(World, *FromPos, *ToPos, FColor::Yellow, false, 0.0f, 0, 2.0f);
            }
        }

        for (const auto& Pair : Nodes)
        {
            const FVector* Pos = Centers.Find(Pair.Key);
            if (Pos)
            {
                const FBiomeGraphNode& Node = Pair.Value;
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Node.MorokField).ToFColor(false);
                DrawDebugSphere(World, *Pos, 30.0f, 12, Color, false, 0.0f, 0, 2.0f);
                DrawDebugString(World, *Pos + FVector(0, 0, 50.0f), Pair.Key.ToString(), nullptr, FColor::White, 0.0f, true, 1.2f);
            }
        }
    }

    if (bShowCellDistortion || bShowCellInfluence)
    {
        for (const FGridCell& Cell : Cells)
        {
            FVector Pos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
            Pos.Z = GetCellHeight(Cell.X, Cell.Y) + 30.0f;
            if (bShowCellDistortion)
            {
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Cell.State.Meta.Distortion).ToFColor(false);
                DrawDebugString(World, Pos, FString::Printf(TEXT("%.2f"), Cell.State.Meta.Distortion), nullptr, Color, 0.0f, true);
            }
            if (bShowCellInfluence)
            {
                float Influence = Cell.TargetState.Meta.Distortion - Cell.State.Meta.Distortion;
                FColor Color = Influence > 0 ? FColor::Red : (Influence < 0 ? FColor::Blue : FColor::White);
                DrawDebugString(World, Pos + FVector(0, 0, 30.0f), FString::Printf(TEXT("Δ%.2f"), Influence), nullptr, Color, 0.0f, true);
            }
        }
    }
#endif
}