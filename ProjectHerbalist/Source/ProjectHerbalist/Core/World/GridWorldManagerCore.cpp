// Core/World/GridWorldManagerCore.cpp
#include "Core/World/GridWorldManager.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Engine/OverlapResult.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "EngineUtils.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/BiomeRow.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/World/WaterRegionVolume.h"
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
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartition.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

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

FVector AGridWorldManager::GetSpawnPositionWithinBiome(int32 X, int32 Y, float JitterRadius, FRandomStream& Rng) const
{
    FVector BasePos = GetCellWorldPositionFlat(X, Y);
    BasePos.Z = GetCellHeight(X, Y);

    if (JitterRadius <= 0.0f) return BasePos;

    // Реальные регионы есть только если клетку что-то покрыло (BiomeWeights
    // непуст) — на блочном фолбэке (или в тестовом окружении без волюмов на
    // уровне) CachedBiomeRegions либо пуст, либо не имеет смысла проверять:
    // старое поведение (джиттер без проверки формы) не меняется.
    const FGridCell* Cell = GetCellConst(X, Y);
    const bool bHasRealRegions = Cell && Cell->BiomeWeights.Num() > 0 && CachedBiomeRegions.Num() > 0;

    FVector Candidate = BasePos;
    const int32 MaxAttempts = bHasRealRegions ? 5 : 1;
    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        const FVector Offset(Rng.FRandRange(-JitterRadius, JitterRadius), Rng.FRandRange(-JitterRadius, JitterRadius), 0.0f);
        Candidate = BasePos + Offset;
        if (!bHasRealRegions) break;

        // Проверяем только регионы биома(ов), которыми реально помечена эта
        // клетка (Cell->BiomeWeights) — не все регионы уровня подряд: точка
        // может технически лежать внутри чужого, соседнего сплайна и всё
        // равно быть неправильным ответом для ЭТОЙ клетки.
        bool bInsideMatchingRegion = false;
        for (const TWeakObjectPtr<ABiomeRegionVolume>& RegionPtr : CachedBiomeRegions)
        {
            ABiomeRegionVolume* Region = RegionPtr.Get();
            if (!Region) continue;

            bool bBiomeMatches = false;
            for (const FBiomeWeightEntry& Entry : Cell->BiomeWeights)
            {
                if (Entry.Biome == Region->Biome) { bBiomeMatches = true; break; }
            }
            if (bBiomeMatches && Region->IsPointInside(Candidate))
            {
                bInsideMatchingRegion = true;
                break;
            }
        }
        if (bInsideMatchingRegion) break;
        // Иначе -- следующая попытка передобирает Offset заново; после
        // MaxAttempts неудачных попыток возвращаем последний кандидат как
        // есть (лучше видимый, но не идеально вписанный джиттер, чем
        // отказ спавнить вовсе).
    }
    return Candidate;
}

bool AGridWorldManager::IsCellClaimedByBiomeRegion(const FGridCell& Cell) const
{
    // На уровне вообще нет ни одного ABiomeRegionVolume -- блочный фолбэк
    // остаётся ЕДИНСТВЕННЫМ источником биома для всей сетки (тестовое
    // окружение, сцены без PCG-авторства), а не заплаткой для нескольких
    // клеток. Старое поведение не меняется.
    if (CachedBiomeRegions.Num() == 0) return true;

    // Регионы на уровне есть -- клетка "заявлена" только если реально
    // попала хотя бы в один (тот же признак, что уже использует
    // GetSpawnPositionWithinBiome). Пустой BiomeWeights здесь означает
    // блочный фолбэк красил клетку каким-то биомом ради математики
    // релаксации/восстановления -- это не то же самое, что "здесь должен
    // быть контент этого биома".
    return Cell.BiomeWeights.Num() > 0;
}

ABiomeRegionVolume* AGridWorldManager::GetClaimingRegion(const FGridCell& Cell) const
{
    if (Cell.BiomeWeights.Num() == 0 || CachedBiomeRegions.Num() == 0) return nullptr;

    const FVector CellWorldPos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
    for (const TWeakObjectPtr<ABiomeRegionVolume>& RegionPtr : CachedBiomeRegions)
    {
        ABiomeRegionVolume* Region = RegionPtr.Get();
        if (!Region) continue;

        bool bBiomeMatches = false;
        for (const FBiomeWeightEntry& Entry : Cell.BiomeWeights)
        {
            if (Entry.Biome == Region->Biome) { bBiomeMatches = true; break; }
        }
        if (bBiomeMatches && Region->IsPointInside(CellWorldPos))
        {
            return Region;
        }
    }
    return nullptr;
}

FGridCell* AGridWorldManager::GetCell(int32 X, int32 Y)
{
    // Cells.IsValidIndex, не только сравнение с GridSizeX/GridSizeY (2026-09-02):
    // GridSizeX/GridSizeY -- это НАМЕРЕНИЕ (EditAnywhere-свойство актора,
    // валидно сразу после конструктора), а Cells реально заполняется только
    // в InitializeCells() (BeginPlay). Актор, размещённый на уровне, но ещё
    // не прошедший BeginPlay в этой конкретной игровой сессии (например,
    // редакторский предпросмотр без Play, или другой AGridWorldManager,
    // случайно найденный через TActorIterator раньше "правильного") имел бы
    // валидный по GridSizeX/Y индекс, но Cells.Num()==0 -- падение с
    // Array index out of bounds вместо честного nullptr. Нашёл
    // AHerbalistResourceActor::RegisterOnCell(), вызываемый из BeginPlay
    // ресурсного актора, спавненного PCG-графом раньше, чем менеджер успел
    // инициализировать сетку.
    const int32 Index = Y * GridSizeX + X;
    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY && Cells.IsValidIndex(Index))
        return &Cells[Index];
    return nullptr;
}

FIntPoint AGridWorldManager::GetChunkCoordForCell(int32 CellX, int32 CellY) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 ChunkSize = FMath::Max(1, Settings ? Settings->ChunkSizeInCells : 32);
    // FloorDiv, не целочисленное деление: у отрицательных координат (их не
    // бывает в текущей сетке, но GetCell честно принимает любые) обычное
    // деление тянет к нулю и склеивает чанк -1 с чанком 0.
    return FIntPoint(FMath::FloorToInt(static_cast<float>(CellX) / ChunkSize),
                     FMath::FloorToInt(static_cast<float>(CellY) / ChunkSize));
}

bool AGridWorldManager::IsSpawnPointBlocked(const FVector& Point) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    if (!Settings || !Settings->bRejectOccupiedSpawnPoints) return false;

    UWorld* World = GetWorld();
    if (!World) return false;

    const float Clearance = FMath::Max(0.0f, Settings->SpawnClearanceRadius);
    if (Clearance <= 0.0f) return false;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(HerbalistSpawnClearance), /*bTraceComplex=*/false);
    Params.AddIgnoredActor(this);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    // Центр сферы приподнят на её радиус: иначе сфера, стоящая ровно на
    // поверхности, наполовину утоплена в неё и цепляет любую статику пола.
    const FVector Centre = Point + FVector(0.0f, 0.0f, Clearance);

    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity, ObjectParams,
        FCollisionShape::MakeSphere(Clearance), Params);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        const AActor* Other = Overlap.GetActor();
        if (!Other) continue;

        // Ландшафт занятостью не считается -- он под КАЖДОЙ точкой мира.
        // Проверка именно по ALandscapeProxy, а не по одному закэшированному
        // ALandscape: в World Partition ландшафт разбит на десятки
        // ALandscapeStreamingProxy, и игнорирование только «главного» делало
        // занятой всю сетку целиком (поймано тестом до коммита).
        if (Other->IsA<ALandscapeProxy>()) continue;

        // Свои же игровые акторы -- не преграда: у ресурса есть широкая
        // сфера взаимодействия для сбора, и считать её «занятым местом»
        // значило бы запретить двум травам расти рядом.
        if (Other->IsA<AHerbalistResourceActor>()) continue;
        if (Other->IsA<AHerbalistEntityActor>()) continue;

        return true;
    }
    return false;
}

bool AGridWorldManager::FindFreeSpawnPositionInCell(int32 X, int32 Y, float JitterRadius, FRandomStream& Rng, FVector& OutPosition) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 MaxAttempts = Settings ? FMath::Max(1, Settings->MaxSpawnPlacementAttempts) : 8;
    const bool bTrace = Settings ? Settings->bTraceSpawnToGround : true;
    const float TraceHalf = Settings ? FMath::Max(0.0f, Settings->SpawnTraceHalfHeight) : 5000.0f;

    UWorld* World = GetWorld();

    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        // Джиттер внутри формы биома — прежняя логика, не тронута.
        FVector Candidate = GetSpawnPositionWithinBiome(X, Y, JitterRadius, Rng);

        // Посадка на поверхность. Высота клетки — приближение по её ЦЕНТРУ;
        // при крупной клетке сдвинутая точка может быть заметно выше или
        // ниже, поэтому ищем поверхность именно под кандидатом.
        if (bTrace && World && TraceHalf > 0.0f)
        {
            const FVector Start = Candidate + FVector(0.0f, 0.0f, TraceHalf);
            const FVector End   = Candidate - FVector(0.0f, 0.0f, TraceHalf);

            FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(HerbalistSpawnGround), /*bTraceComplex=*/false);
            TraceParams.AddIgnoredActor(this);

            FHitResult Hit;
            if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, TraceParams))
            {
                Candidate.Z = Hit.ImpactPoint.Z;
            }
        }

        if (!IsSpawnPointBlocked(Candidate))
        {
            OutPosition = Candidate;
            return true;
        }
    }

    // Свободного места в клетке не нашлось. Честный отказ: пустая клетка
    // лучше, чем трава внутри валуна.
    return false;
}

void AGridWorldManager::PreviewResourceSpawnPoints()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ClearResourceSpawnPreview();
    FindAndCacheLandscape();

    // Превью работает и без InitializeCells (в редакторе, до запуска игры),
    // поэтому не читает Cells: позиция клетки считается из GridSizeX/Y и
    // CellSize, а форма биома проверяется прямо у волюмов на уровне.
    TArray<ABiomeRegionVolume*> Regions;
    for (TActorIterator<ABiomeRegionVolume> It(World); It; ++It)
    {
        if (ABiomeRegionVolume* Region = *It)
        {
            Region->UpdateCachedPoints();
            Regions.Add(Region);
        }
    }

    FRandomStream PreviewRng(RngBaseSeed);
    const float Jitter = CellSize * 0.3f;
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float TraceHalf = Settings ? FMath::Max(0.0f, Settings->SpawnTraceHalfHeight) : 5000.0f;

    int32 Considered = 0, Free = 0, Blocked = 0;
    for (int32 Y = 0; Y < GridSizeY && Considered < PreviewMaxCells; ++Y)
    {
        for (int32 X = 0; X < GridSizeX && Considered < PreviewMaxCells; ++X)
        {
            FVector Base = GetCellWorldPositionFlat(X, Y);

            // Только клетки внутри нарисованных регионов — остальной мир
            // ресурсов биома и не получит (см. IsCellClaimedByBiomeRegion).
            if (Regions.Num() > 0)
            {
                bool bInside = false;
                for (ABiomeRegionVolume* Region : Regions)
                {
                    if (Region && Region->IsPointInside(Base)) { bInside = true; break; }
                }
                if (!bInside) continue;
            }
            ++Considered;

            FVector Candidate = Base + FVector(PreviewRng.FRandRange(-Jitter, Jitter), PreviewRng.FRandRange(-Jitter, Jitter), 0.0f);

            FHitResult Hit;
            FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(HerbalistPreviewGround), false);
            TraceParams.AddIgnoredActor(this);
            if (World->LineTraceSingleByChannel(Hit, Candidate + FVector(0, 0, TraceHalf), Candidate - FVector(0, 0, TraceHalf), ECC_WorldStatic, TraceParams))
            {
                Candidate.Z = Hit.ImpactPoint.Z;
            }

            const bool bBlocked = IsSpawnPointBlocked(Candidate);
            bBlocked ? ++Blocked : ++Free;

            DrawDebugSphere(World, Candidate, FMath::Max(8.0f, CellSize * 0.06f), 8,
                bBlocked ? FColor::Red : FColor::Green, /*bPersistent=*/true, -1.0f, 0, 2.0f);
        }
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Preview] Точек показано: %d (свободно %d, занято %d). Красные -- туда ресурс не встанет."),
        Considered, Free, Blocked);
}

void AGridWorldManager::ClearResourceSpawnPreview()
{
    if (UWorld* World = GetWorld())
    {
        FlushPersistentDebugLines(World);
    }
}

int32 AGridWorldManager::GetActiveRadiusInChunks() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RadiusMeters = Settings ? Settings->ActiveSimulationRadiusMeters : -1.0f;
    if (RadiusMeters < 0.0f) return -1;   // механизм выключен

    const int32 ChunkSize = FMath::Max(1, Settings ? Settings->ChunkSizeInCells : 32);
    const float ChunkSpanCm = FMath::Max(KINDA_SMALL_NUMBER, CellSize * ChunkSize);
    // Floor, не ceil: радиус меньше одного чанка честно означает "только свой
    // чанк" (0), а не "и соседние тоже".
    return FMath::FloorToInt((RadiusMeters * 100.0f) / ChunkSpanCm);
}

bool AGridWorldManager::IsCellActive(const FGridCell& Cell) const
{
    const int32 Radius = GetActiveRadiusInChunks();

    // -1 -- механизм выключен, активно всё (поведение до 2026-09-03).
    if (Radius < 0) return true;

    // Радиус задан, но источников нет вовсе (нет игрока, headless-тест без
    // явной установки центров) -- считаем всё активным, а не всё мёртвым:
    // тихо остановившаяся симуляция хуже, чем не включившаяся оптимизация.
    if (ActiveChunkCenters.Num() == 0) return true;

    const FIntPoint CellChunk = GetChunkCoordForCell(Cell.X, Cell.Y);
    for (const FIntPoint& Center : ActiveChunkCenters)
    {
        // Чебышёв -- тот же принцип соседства, что уже у радиуса капища.
        if (FMath::Max(FMath::Abs(CellChunk.X - Center.X), FMath::Abs(CellChunk.Y - Center.Y)) <= Radius)
        {
            return true;
        }
    }
    return false;
}

void AGridWorldManager::UpdateActiveChunkCenters()
{
    ActiveChunkCenters.Reset();

    UWorld* World = GetWorld();
    if (!World) return;

    auto AddCenterFromWorldLocation = [this](const FVector& WorldLocation)
    {
        int32 X, Y;
        if (WorldPositionToCell(WorldLocation, X, Y))
        {
            ActiveChunkCenters.AddUnique(GetChunkCoordForCell(X, Y));
        }
    };

    // Основной путь: спрашиваем сам World Partition, вокруг чего он сейчас
    // стримит уровень. Так сетка «подчиняется партишену» буквально — она
    // следует тем же источникам, что и загрузка мира, включая любые
    // будущие (второй игрок, камера, транспорт), без правок здесь.
    // UWorldPartition::GetStreamingSources() -- публичный аксессор уже
    // посчитанного партишеном списка (одноимённый метод у
    // UWorldPartitionSubsystem закрыт, это не он).
    if (const UWorldPartition* WorldPartition = World->GetWorldPartition())
    {
        for (const FWorldPartitionStreamingSource& Source : WorldPartition->GetStreamingSources())
        {
            AddCenterFromWorldLocation(Source.Location);
        }
    }

    // Фолбэк для уровней без партишена (и для PIE до того, как источники
    // зарегистрируются): позиция пешки игрока.
    if (ActiveChunkCenters.Num() == 0)
    {
        if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
        {
            AddCenterFromWorldLocation(PlayerPawn->GetActorLocation());
        }
    }
}

void AGridWorldManager::SetChunkResourcesActive(const FIntPoint& Chunk, bool bActive)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 ChunkSize = FMath::Max(1, Settings ? Settings->ChunkSizeInCells : 32);

    const int32 MinX = Chunk.X * ChunkSize;
    const int32 MinY = Chunk.Y * ChunkSize;

    for (int32 Y = MinY; Y < MinY + ChunkSize; ++Y)
    {
        for (int32 X = MinX; X < MinX + ChunkSize; ++X)
        {
            FGridCell* Cell = GetCell(X, Y);
            if (!Cell) continue;

            if (bActive)
            {
                if (!Cell->bResourcesSeeded)
                {
                    // Первая активация клетки за сессию -- обычный бросок
                    // кубика, тот же, что раньше делала InitializeCells.
                    SpawnResourcesInCell(*Cell);
                    Cell->bResourcesSeeded = true;
                }
                else
                {
                    // Возврат игрока: поднимаем ровно то, что стояло.
                    for (FName IngredientID : Cell->DormantResourceIDs)
                    {
                        SpawnResourceActor(IngredientID, X, Y);
                    }
                    Cell->DormantResourceIDs.Reset();
                }
            }
            else
            {
                for (const TWeakObjectPtr<AHerbalistResourceActor>& Ptr : Cell->ResourceActors)
                {
                    AHerbalistResourceActor* Actor = Ptr.Get();
                    if (!Actor) continue;

                    // Чужие акторы (PCG-граф) сетке не принадлежат -- их
                    // стримит сам World Partition, трогать нельзя.
                    if (!Actor->WasSpawnedByGrid()) continue;

                    Cell->DormantResourceIDs.Add(Actor->GetIngredientID());
                    Actor->Destroy();
                }
                Cell->ResourceActors.RemoveAll([](const TWeakObjectPtr<AHerbalistResourceActor>& Ptr)
                {
                    return !Ptr.IsValid();
                });
            }
        }
    }
}

void AGridWorldManager::CatchUpActivatedChunks()
{
    const int32 Radius = GetActiveRadiusInChunks();

    // Механизм выключен (или источников нет) — активно всё, простаивать
    // нечему, догонять нечего.
    if (Radius < 0 || ActiveChunkCenters.Num() == 0)
    {
        ActiveChunks.Reset();
        PreviousActiveChunks.Reset();
        return;
    }

    ActiveChunks.Reset();
    for (const FIntPoint& Center : ActiveChunkCenters)
    {
        for (int32 dy = -Radius; dy <= Radius; ++dy)
        {
            for (int32 dx = -Radius; dx <= Radius; ++dx)
            {
                ActiveChunks.Add(FIntPoint(Center.X + dx, Center.Y + dy));
            }
        }
    }

    const float Now = GameClockSeconds;
    for (const FIntPoint& Chunk : ActiveChunks)
    {
        // Чанк, не встречавшийся ни разу, простаивал с момента инициализации
        // сетки — не «с этой секунды». Иначе дальний мир стоял бы
        // замороженным до первого визита, и клетка, испорченная до ухода
        // игрока, не восстановилась бы никогда.
        float* Last = &ChunkLastSimulatedGameTime.FindOrAdd(Chunk, GridInitGameClock);

        // Догон только для тех, кто ТОЛЬКО ЧТО стал активным. Для уже
        // активных этот же интервал считает обычный проход в Tick — иначе
        // релаксация шла бы дважды за кадр.
        if (!PreviousActiveChunks.Contains(Chunk))
        {
            const float Elapsed = Now - *Last;
            if (Elapsed > KINDA_SMALL_NUMBER)
            {
                RegenerateCellParameters(Elapsed, &Chunk);
                UE_LOG(LogHerbalistWorld, Verbose, TEXT("[Streaming] Chunk (%d,%d) caught up %.1f s"), Chunk.X, Chunk.Y, Elapsed);
            }
        }
        *Last = Now;
    }

    // Материализация/усыпление ресурсов по смене активности (юнит 3).
    for (const FIntPoint& Chunk : ActiveChunks)
    {
        if (!PreviousActiveChunks.Contains(Chunk))
        {
            SetChunkResourcesActive(Chunk, true);
        }
    }
    for (const FIntPoint& Chunk : PreviousActiveChunks)
    {
        if (!ActiveChunks.Contains(Chunk))
        {
            SetChunkResourcesActive(Chunk, false);
        }
    }

    PreviousActiveChunks = ActiveChunks;
}

const FGridCell* AGridWorldManager::GetCellConst(int32 X, int32 Y) const
{
    const int32 Index = Y * GridSizeX + X;
    if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY && Cells.IsValidIndex(Index))
        return &Cells[Index];
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
        // Стриминг сетки (2026-09-03): поля биом-графа применяются только к
        // активным клеткам. Сам граф считается всегда и целиком — он живёт
        // на уровне узлов, а не клеток, поэтому дальний мир продолжает
        // меняться; неактивные клетки просто не получают его вклад, пока не
        // станут активными (догон — юнит 2).
        if (!IsCellActive(Cell)) continue;

        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        FRealState NewTarget = Cell.TargetState;
        bool bChanged = false;

        // 1. Влияние Морока (увеличивает Distortion). ApplyFieldsToGrid
        // (BiomeGraphSubsystem.cpp) всегда кладёт запись в MorokFields для
        // КАЖДОГО узла графа, включая нулевые поля — значит "if (MorokField)"
        // (есть ли запись) было равносильно "всегда", а не "поле реально
        // что-то сдвигает". На каждом шаге симуляции (StepSimulation тикается
        // из Tick(), не редкое событие) это метило ВСЮ сетку грязной с первых
        // секунд сессии — обесценивая липкий DirtyCellIndices, вокруг
        // которого построена вся система сохранений (AUDIT_AND_REFACTORING_PLAN.md
        // §7.1, подтверждено автотестом Herbalist.Save.BiomeInfluencesWithZeroFieldsStaySparse).
        // Правка: сравниваем итог с уже стоящим TargetState, метим грязной
        // только при реальном изменении.
        const float* MorokField = MorokFields.Find(BiomeID);
        if (MorokField)
        {
            float MorokInfluence = *MorokField * 0.1f * GlobalScale;

            // Эффект 3, Каменное (Стрибог, §15.5): "не пускает Морок" — глушит
            // вклад MorokField в локальный Distortion на (1 − 0.4×Restoration),
            // только в радиусе капища.
            const UHerbalistSettings* ShrineSettings = GetHerbalistSettings();
            const FShrine* DominantShrine = Shrines.Num() > 0
                ? HerbalistCore::Shrine::FindDominantShrine(FIntPoint(Cell.X, Cell.Y), Shrines, ShrineSettings ? ShrineSettings->ShrineInfluenceRadius : 3)
                : nullptr;
            if (DominantShrine && DominantShrine->Type == EShrineType::Stone && DominantShrine->Restoration > 0.0f)
            {
                const float Dampening = ShrineSettings ? ShrineSettings->ShrineStoneMorokDampening : 0.4f;
                MorokInfluence *= (1.0f - FMath::Clamp(Dampening * DominantShrine->Restoration, 0.0f, 1.0f));
            }

            const float NewDistortion = FMath::Clamp(NewTarget.Meta.Distortion + MorokInfluence, 0.f, 1.f);
            if (!FMath::IsNearlyEqual(NewDistortion, NewTarget.Meta.Distortion, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Distortion = NewDistortion;
                bChanged = true;
            }
        }

        // 2. Влияние Заряны (повышает Stability и Purity) — та же поправка.
        const float* ZaryanaField = ZaryanaFields.Find(BiomeID);
        if (ZaryanaField)
        {
            const float ZaryanaInfluence = *ZaryanaField * 0.05f * GlobalScale;
            const float NewStability = FMath::Clamp(NewTarget.Meta.Stability + ZaryanaInfluence, 0.f, 1.f);
            const float NewPurity    = FMath::Clamp(NewTarget.Meta.Purity    + ZaryanaInfluence * 0.5f, 0.f, 1.f);
            if (!FMath::IsNearlyEqual(NewStability, NewTarget.Meta.Stability, KINDA_SMALL_NUMBER) ||
                !FMath::IsNearlyEqual(NewPurity, NewTarget.Meta.Purity, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Stability = NewStability;
                NewTarget.Meta.Purity = NewPurity;
                bChanged = true;
            }
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

    // Точка отсчёта простоя чанков (2026-09-03, стриминг): чанк, который
    // игрок не посещал ни разу, простаивал именно с этого момента.
    GridInitGameClock = GameClockSeconds;
    ChunkLastSimulatedGameTime.Reset();
    ActiveChunks.Reset();
    PreviousActiveChunks.Reset();

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    UWaterTypeRegistrySubsystem* WaterSubsystem = GameInstance ? GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>() : nullptr;

    // Общая точка заливки клетки водой -- раньше было продублировано в двух
    // местах блока случайной воды ниже, теперь ещё и в явных регионах воды
    // (2026-09-02) -- один источник истины на все три случая. Читает уже
    // выставленный Cell.Biome -- вода поверх болота получает болотный
    // WaterTypeID, поверх тундры -- тундровый, автоматически.
    auto ApplyWaterToCell = [WaterSubsystem, this](FGridCell& Cell)
    {
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
    };

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

    // PCG-биомы (2026-08-31) -- собираем ABiomeRegionVolume, расставленные
    // в уровне, один раз до цикла по клеткам. Явно пересчитываем кэш точек
    // каждого региона (Region->UpdateCachedPoints()), не полагаясь на то,
    // что его собственный BeginPlay уже отработал -- UE не гарантирует
    // порядок BeginPlay между акторами уровня.
    TArray<ABiomeRegionVolume*> BiomeRegions;
    for (TActorIterator<ABiomeRegionVolume> It(GetWorld()); It; ++It)
    {
        ABiomeRegionVolume* Region = *It;
        if (!Region) continue;
        // AWaterRegionVolume -- IS-A ABiomeRegionVolume в C++, но семантически
        // не земляной биом (2026-09-02) -- собирается отдельным проходом ниже,
        // не должен застолбить долю в Cell.BiomeWeights со своим унаследованным
        // (неиспользуемым) дефолтным Biome.
        if (Region->IsA<AWaterRegionVolume>()) continue;
        Region->UpdateCachedPoints();
        BiomeRegions.Add(Region);
    }
    if (BiomeRegions.Num() == 0)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("InitializeCells: ни одного ABiomeRegionVolume не найдено в уровне -- вся сетка идёт по блочному фолбэку (5x5)"));
    }

    // Явные регионы воды (2026-09-02, прямой запрос пользователя) -- вода
    // как отдельный, вручную нарисованный "биом", всегда побеждающий (вес 1,
    // не размешивается с земляными регионами) для флага bIsWater конкретной
    // клетки, но НЕ подменяющий собой Cell.Biome (тот резолвится обычным
    // путём выше, из земляных регионов) -- WaterTypeID ниже берётся именно
    // из уже определённого Cell.Biome, поэтому вода поверх болота
    // автоматически получает болотный тип воды, поверх тундры -- тундровый.
    TArray<AWaterRegionVolume*> WaterRegions;
    for (TActorIterator<AWaterRegionVolume> It(GetWorld()); It; ++It)
    {
        AWaterRegionVolume* Region = *It;
        if (!Region) continue;
        Region->UpdateCachedPoints();
        WaterRegions.Add(Region);
    }

    // Сохраняем для GetSpawnPositionWithinBiome — тот же список пригодится
    // во время игры (регенерация ресурсов, проявление сущностей), не
    // только здесь при старте.
    CachedBiomeRegions.Reset(BiomeRegions.Num());
    for (ABiomeRegionVolume* Region : BiomeRegions)
    {
        CachedBiomeRegions.Add(Region);
    }

    // Блочная раскраска 5x5 остаётся ФОЛБЭКОМ для клеток вне всех
    // размещённых регионов (не отменена, не заменена целиком) -- система
    // деградирует плавно, пока авторская расстановка регионов неполная,
    // вместо краха/единственного дефолтного биома на пробелах.
    const int32 BlockSize = 5;
    const int32 BlocksX = GridSizeX / BlockSize;
    const int32 BlocksY = GridSizeY / BlockSize;
    int32 FallbackCellCount = 0;

    // Регион, реально заявивший каждую клетку (2026-09-02, для
    // пер-региональной плотности воды ниже) -- параллельно Cells, тот же
    // индекс. nullptr -- клетка вне всех регионов (блочный фолбэк) ИЛИ
    // регионов на уровне нет вовсе.
    TArray<ABiomeRegionVolume*> CellRegion;
    CellRegion.SetNumZeroed(TotalCells);

    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
        {
            int32 Index = Y * GridSizeX + X;
            FGridCell& Cell = Cells[Index];

            // Какие регионы содержат клетку -- равная доля на каждый
            // (0.5/0.5 на двух, 1/3 на трёх и т.д., без авторского
            // "усиления" региона -- вертикальный срез v1, прямое решение
            // пользователя). MatchingRegions -- тот же индекс, что и
            // Cell.BiomeWeights, чтобы не пересчитывать IsPointInside ещё
            // раз при определении CellRegion[Index] ниже.
            Cell.BiomeWeights.Reset();
            TArray<ABiomeRegionVolume*> MatchingRegions;
            const FVector CellWorldPos = GetCellWorldPositionFlat(X, Y);
            for (ABiomeRegionVolume* Region : BiomeRegions)
            {
                if (Region && Region->IsPointInside(CellWorldPos))
                {
                    Cell.BiomeWeights.Add(FBiomeWeightEntry{ Region->Biome, 1.0f });
                    MatchingRegions.Add(Region);
                }
            }

            EBiomeType biome;
            if (Cell.BiomeWeights.Num() > 0)
            {
                const float Share = 1.0f / Cell.BiomeWeights.Num();
                for (FBiomeWeightEntry& Entry : Cell.BiomeWeights)
                {
                    Entry.Weight = Share;
                }

                // Доминанта -- наибольший вес; при точном равенстве (v1: у
                // равных долей ВСЕГДА равенство) -- меньший порядковый
                // номер EBiomeType. Не "первый по порядку акторов" --
                // порядок TActorIterator зависит от порядка в Outliner/
                // пересохранения уровня, скрытая невоспроизводимая
                // зависимость, которой у старой блочной формулы не было.
                int32 BestIndex = 0;
                biome = Cell.BiomeWeights[0].Biome;
                float BestWeight = Cell.BiomeWeights[0].Weight;
                for (int32 i = 1; i < Cell.BiomeWeights.Num(); ++i)
                {
                    const FBiomeWeightEntry& Entry = Cell.BiomeWeights[i];
                    const bool bStrictlyBetter = Entry.Weight > BestWeight + KINDA_SMALL_NUMBER;
                    const bool bTieBrokenByOrdinal = FMath::IsNearlyEqual(Entry.Weight, BestWeight, KINDA_SMALL_NUMBER)
                        && static_cast<uint8>(Entry.Biome) < static_cast<uint8>(biome);
                    if (bStrictlyBetter || bTieBrokenByOrdinal)
                    {
                        biome = Entry.Biome;
                        BestWeight = Entry.Weight;
                        BestIndex = i;
                    }
                }
                CellRegion[Index] = MatchingRegions[BestIndex];
            }
            else
            {
                biome = AllBiomes[( (Y / BlockSize) * BlocksX + (X / BlockSize) ) % AllBiomes.Num()];
                ++FallbackCellCount;
            }

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

            // Явный регион воды (2026-09-02) -- вес всегда 1, безусловно
            // заливает клетку поверх уже определённого Cell.Biome, не
            // участвует в вероятностной WaterDensity-раскладке ниже.
            for (AWaterRegionVolume* WaterRegion : WaterRegions)
            {
                if (WaterRegion && WaterRegion->IsPointInside(CellWorldPos))
                {
                    ApplyWaterToCell(Cell);
                    break;
                }
            }
        }
    }

    if (BiomeRegions.Num() > 0 && FallbackCellCount > 0)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("InitializeCells: %d/%d клеток (%.0f%%) вне всех ABiomeRegionVolume -- для них взят блочный фолбэк"),
            FallbackCellCount, TotalCells, TotalCells > 0 ? 100.0f * FallbackCellCount / TotalCells : 0.0f);
    }

    // ------------------------------------------------------------------------
    // Размещение водоёмов -- пер-региональная плотность
    // (ABiomeRegionVolume::WaterDensity, 2026-09-02, прямой запрос
    // пользователя "настройки под все дела в этих волюмах"). Было: единое
    // TotalCells/5 (=20%) без учёта биома вообще (степь и болото заливались
    // одинаково). Каждый регион -- свой пул клеток (плюс отдельный
    // "фолбэк"-пул: клетки вне всех регионов, включая случай "регионов на
    // уровне нет вовсе" -- у него дефолт 0.2, то же число, что раньше было
    // TotalCells/5 для всей сетки). Блок (1x1..2x2, как раньше) не
    // пересекает границу пула -- случайная точка-затравка берётся ИЗ
    // списка клеток пула, не из всей сетки: на большой сетке с маленьким
    // регионом равномерный бросок по всей сетке почти никогда не попадал бы
    // в маленький пул, сходимость была бы неприемлемо медленной.
    //
    // БЕЗ регионов на уровне -- прежний алгоритм побитово (тот же порядок и
    // число обращений к WorldRNG), не через пул: этот путь используют
    // практически ВСЕ существующие тесты (SpawnAndBeginPlay без регионов),
    // многие неявно зависят от точной раскладки воды при дефолтном
    // RngBaseSeed. Найдено регрессией: единый пуловый алгоритм ниже даёт
    // другую раскладку даже с одним пулом на всю сетку (иной порядок
    // потребления Rng -- сид-затравка через RandRange по списку клеток
    // пула, а не W/H/StartX/StartY подряд), что различным образом смещает,
    // какие именно клетки становятся водой -- и это меняет исход конкретных
    // тестов, рассчитанных на то, что определённая клетка ОСТАЁТСЯ водой
    // (например, Herbalist.AmbientEntity.DecorativeEntitiesManifestWithoutEffect
    // держится на воде на клетке вне явно заданных, защищающей её от
    // ошибочной сухопутной сущности).
    // ------------------------------------------------------------------------
    // Затравка из уже проставленного bIsWater -- явные регионы воды
    // (2026-09-02) могли заранее залить часть клеток безусловно, до того,
    // как этот блок вообще начал работать; без явных регионов на уровне
    // (сегодняшний путь абсолютного большинства тестов) Cell.bIsWater
    // сейчас false у всех, так что это ничем не отличается от Init(false, ...).
    TArray<bool> IsWaterAlready;
    IsWaterAlready.SetNumUninitialized(TotalCells);
    for (int32 Idx = 0; Idx < TotalCells; ++Idx)
    {
        IsWaterAlready[Idx] = Cells[Idx].bIsWater;
    }

    if (BiomeRegions.Num() == 0)
    {
        int32 TargetWaterCount = FMath::Max(TotalCells / 5, 1);
        int32 PlacedWater = 0;

        while (PlacedWater < TargetWaterCount)
        {
            int32 W = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
            int32 H = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
            int32 StartX = WorldRNG.RandRange(0, GridSizeX - W);
            int32 StartY = WorldRNG.RandRange(0, GridSizeY - H);

            bool bAreaFree = true;
            for (int32 dy = 0; dy < H && bAreaFree; ++dy)
                for (int32 dx = 0; dx < W; ++dx)
                    if (IsWaterAlready[(StartY + dy) * GridSizeX + (StartX + dx)])
                        { bAreaFree = false; break; }

            if (!bAreaFree) continue;

            for (int32 dy = 0; dy < H; ++dy)
            {
                for (int32 dx = 0; dx < W; ++dx)
                {
                    int32 Idx = (StartY + dy) * GridSizeX + (StartX + dx);
                    ApplyWaterToCell(Cells[Idx]);
                    IsWaterAlready[Idx] = true;
                    PlacedWater++;
                }
            }
            if (PlacedWater >= TargetWaterCount) break;
        }
    }
    else
    {
    TMap<ABiomeRegionVolume*, TArray<int32>> CellsByPool;
    for (int32 Idx = 0; Idx < TotalCells; ++Idx)
    {
        CellsByPool.FindOrAdd(CellRegion[Idx]).Add(Idx);
    }

    for (const TPair<ABiomeRegionVolume*, TArray<int32>>& PoolPair : CellsByPool)
    {
        ABiomeRegionVolume* PoolRegion = PoolPair.Key;
        const TArray<int32>& PoolCells = PoolPair.Value;
        if (PoolCells.Num() == 0) continue;

        const float Density = PoolRegion ? PoolRegion->WaterDensity : 0.2f;
        const int32 TargetForPool = FMath::Clamp(FMath::RoundToInt(PoolCells.Num() * Density), 0, PoolCells.Num());
        if (TargetForPool <= 0) continue;

        int32 PlacedInPool = 0;
        // Запас попыток -- маленький пул с плотной застройкой блоками
        // сходится медленнее одной большой сетки, но не бесконечно:
        // безопасный предел вместо потенциального зависания.
        const int32 MaxAttempts = PoolCells.Num() * 20 + 50;
        for (int32 Attempt = 0; PlacedInPool < TargetForPool && Attempt < MaxAttempts; ++Attempt)
        {
            const int32 SeedIdx = PoolCells[WorldRNG.RandRange(0, PoolCells.Num() - 1)];
            const int32 StartX = SeedIdx % GridSizeX;
            const int32 StartY = SeedIdx / GridSizeX;

            const int32 W = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
            const int32 H = (WorldRNG.FRand() < 0.5f) ? 1 : 2;
            if (StartX + W > GridSizeX || StartY + H > GridSizeY) continue;

            // Свободна ли область И целиком внутри того же пула -- блок не
            // пересекает границу региона.
            bool bAreaFree = true;
            for (int32 dy = 0; dy < H && bAreaFree; ++dy)
            {
                for (int32 dx = 0; dx < W; ++dx)
                {
                    const int32 Idx = (StartY + dy) * GridSizeX + (StartX + dx);
                    if (IsWaterAlready[Idx] || CellRegion[Idx] != PoolRegion) { bAreaFree = false; break; }
                }
            }
            if (!bAreaFree) continue;

            for (int32 dy = 0; dy < H; ++dy)
            {
                for (int32 dx = 0; dx < W; ++dx)
                {
                    const int32 Idx = (StartY + dy) * GridSizeX + (StartX + dx);
                    ApplyWaterToCell(Cells[Idx]);
                    IsWaterAlready[Idx] = true;
                    ++PlacedInPool;
                }
            }
        }
    }
    }

    // ========================================================================
    // ВАЖНО: сначала кешируем высоты ландшафта, потом спавним ресурсы
    // ========================================================================
    CacheCellHeights();

    // Спавним ресурсы во всех клетках -- в т.ч. водных (2026-09-02, водные
    // растения): SpawnResourcesInCell сама решает, из какого пула брать
    // кандидата (аквапул для bIsWater, обычный земляной иначе), водная
    // клетка без ни единого зарегистрированного bGrowsOnWater-растения
    // просто не получит ничего (GetRandomResourceForAquaticBiome вернёт
    // NAME_None), как и раньше.
    // Со включённым стримингом (ActiveChunkRadius >= 0) массового заселения
    // при старте НЕ происходит: клетка получает ресурсы при первой
    // активации своего чанка (SetChunkResourcesActive). Иначе на мире 5x5 км
    // старт означал бы десятки тысяч акторов разом. При выключенном
    // стриминге -- прежнее поведение, побайтово тот же порядок WorldRNG.
    {
        const bool bStreamingEnabled = GetActiveRadiusInChunks() >= 0;
        if (!bStreamingEnabled)
        {
            for (FGridCell& Cell : Cells)
            {
                SpawnResourcesInCell(Cell);
                Cell.bResourcesSeeded = true;
            }
        }
    }

    // Вертикальный срез проявления сущностей (16_Entity_Manifestation) —
    // авто-расстановка тестовых клеток-обиталищ.
    SeedTestLandmarks();
    SeedLegendaryAnchors();

    SetActorTickEnabled(true);
}

// ============================================================================
// РЕСУРСЫ
// ============================================================================

void AGridWorldManager::SpawnResourcesInCell(FGridCell& Cell)
{
    // PCG-биомы (2026-09-02, прямое требование пользователя) -- клетка вне
    // всех размещённых на уровне ABiomeRegionVolume не спавнит ресурсы,
    // даже если блочный фолбэк формально приписал ей какой-то биом. Без
    // регионов на уровне вовсе (тесты, сцены без PCG-авторства) -- проверка
    // всегда true, ничего не меняется.
    if (!IsCellClaimedByBiomeRegion(Cell)) return;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    // Одно и то же окно условий для всех 1-3 ресурсов этого вызова (та же
    // клетка, тот же момент) — читается один раз, не на каждой итерации.
    FHarvestContext Context;
    Context.Season = GetSeason();
    Context.bLateSummer = IsLateSummer();
    Context.TimeOfDay = IsDawn() ? EHarvestTimeWindow::Dawn
        : IsDusk() ? EHarvestTimeWindow::Dusk
        : IsNight() ? EHarvestTimeWindow::Night
        : EHarvestTimeWindow::Day;
    Context.MoonPhase = GetMoonPhase();
    Context.bDryWeather = !IsRainy() && !IsBlizzard();

    // Сад (§2.4): клетка с пристройкой — кандидаты из EGardenNiche, не из
    // AllowedBiomes. Пусто (None) для подавляющего большинства клеток мира
    // — обычный путь ниже не меняется вовсе для них. Не применяется на воде
    // (2026-09-02) -- пристройка сада не подделывает нишу для водных клеток,
    // это отдельный, пока не заведённый случай.
    const EGardenNiche* PlotNiche = Cell.bIsWater ? nullptr : GardenPlots.Find(FIntPoint(Cell.X, Cell.Y));

    // Диапазон количества ресурсов -- пер-региональная настройка
    // (ABiomeRegionVolume::MinResourcesPerCell/MaxResourcesPerCell,
    // 2026-09-02), если клетка реально заявлена регионом; без регионов на
    // уровне (тесты, сцены без PCG) -- GetClaimingRegion возвращает
    // nullptr, дефолт 1-3 не меняется.
    ABiomeRegionVolume* ClaimingRegion = GetClaimingRegion(Cell);

    // Регион, отданный PCG-графу (2026-09-03), C++ не заселяет вовсе --
    // иначе к разбросу графа добавился бы второй, клеточный набор внахлёст.
    if (ClaimingRegion && !ClaimingRegion->bSpawnResourcesFromGrid) return;

    const int32 MinRes = ClaimingRegion ? FMath::Min(ClaimingRegion->MinResourcesPerCell, ClaimingRegion->MaxResourcesPerCell) : 1;
    const int32 MaxRes = ClaimingRegion ? FMath::Max(ClaimingRegion->MinResourcesPerCell, ClaimingRegion->MaxResourcesPerCell) : 3;
    int32 NumResources = WorldRNG.RandRange(MinRes, MaxRes);
    for (int32 i = 0; i < NumResources; ++i)
    {
        FName IngredientID = NAME_None;
        if (IngredientSubsystem)
        {
            // Водные растения (2026-09-02, прямой запрос пользователя):
            // "если у биома есть водные растения, то они разрешены к
            // размещению на поверхности воды, и вода одновременно доступна" --
            // отдельный, не смешанный с земляным пул (bGrowsOnWater),
            // тот же принцип отбора по Cell.BiomeWeights земляного биома
            // под водой, что и обычный GetRandomResourceForBiome.
            if (Cell.bIsWater)
            {
                IngredientID = IngredientSubsystem->GetRandomResourceForAquaticBiome(Cell, Context, WorldRNG);
            }
            else
            {
                IngredientID = (PlotNiche && *PlotNiche != EGardenNiche::None)
                    ? IngredientSubsystem->GetRandomResourceForNiche(Cell, *PlotNiche, Context, WorldRNG)
                    : IngredientSubsystem->GetRandomResourceForBiome(Cell, Context, WorldRNG);
            }
        }
        if (IngredientID.IsNone()) continue;

        // Позиция внутри формы биома (2026-09-02) + посадка на поверхность и
        // отбраковка занятых точек (2026-09-03). Свободного места нет --
        // клетка остаётся пустой: лучше так, чем трава внутри валуна.
        FVector SpawnPos;
        if (!FindFreeSpawnPositionInCell(Cell.X, Cell.Y, CellSize * 0.3f, WorldRNG, SpawnPos))
        {
            UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnResourcesInCell: клетка (%d,%d) занята, ресурс пропущен"), Cell.X, Cell.Y);
            continue;
        }
        SpawnPos.Z += 5.0f;   // небольшой подъём над поверхностью, тот же, что и раньше

        const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
        if (!Row) continue;

        // Класс из строки (2026-09-03) -- пусто = базовый, см.
        // FIngredientTableRow::ResourceActorClass.
        TSubclassOf<AHerbalistResourceActor> ClassToSpawn = Row->ResourceActorClass;
        if (!ClassToSpawn) ClassToSpawn = AHerbalistResourceActor::StaticClass();

        AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(ClassToSpawn, SpawnPos, FRotator::ZeroRotator);
        if (NewActor)
        {
            // Регистрация в Cell.ResourceActors теперь делает сам Init()
            // (2026-09-02) -- единая точка входа для любого источника спавна,
            // не только этого C++-пути.
            NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, Cell.X, Cell.Y, Row->Resilience, Row->bIronAverse, Row->bDelicate);
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

    // Offset по умолчанию (ZeroVector) -- единственный реальный вызывающий
    // (ApplySaveCells, восстановление после загрузки) никогда не передаёт
    // собственный сдвиг явно: раньше это молча означало "ровно в центре
    // клетки" (та же "дебажная сетка", что и была у остальных ресурсов) --
    // теперь дефолт использует ту же позицию внутри формы биома, что и
    // SpawnResourcesInCell. Явный ненулевой Offset вызывающей стороны
    // по-прежнему уважается как есть -- контракт параметра не сломан.
    FVector SpawnPos;
    if (Offset.IsNearlyZero())
    {
        // Тот же поиск свободной точки, что и при первичном заселении
        // (2026-09-03). Отросшее/восстановленное из сейва растение не должно
        // оказаться внутри камня, поставленного там, где оно раньше росло.
        if (!FindFreeSpawnPositionInCell(X, Y, CellSize * 0.3f, WorldRNG, SpawnPos))
        {
            UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnResourceActor: клетка (%d,%d) занята, %s не поставлен"), X, Y, *IngredientID.ToString());
            return;
        }
        SpawnPos.Z += 5.0f;
    }
    else
    {
        SpawnPos = GetCellWorldPositionFlat(X, Y);
        SpawnPos.Z = GetCellHeight(X, Y) + 5.0f;
        SpawnPos += Offset;
    }

    TSubclassOf<AHerbalistResourceActor> ClassToSpawn = Row->ResourceActorClass;
    if (!ClassToSpawn) ClassToSpawn = AHerbalistResourceActor::StaticClass();

    AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(ClassToSpawn, SpawnPos, FRotator::ZeroRotator);
    if (NewActor)
    {
        // Регистрация в Cell.ResourceActors теперь делает сам Init() (2026-09-02).
        NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, X, Y, Row->Resilience, Row->bIronAverse, Row->bDelicate);
        UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnResourceActor: %s at cell (%d,%d) Z=%.1f"), *IngredientID.ToString(), X, Y, SpawnPos.Z);
    }
}

void AGridWorldManager::RegisterGardenPlot(const FIntPoint& Cell, EGardenNiche Niche)
{
    if (Niche == EGardenNiche::None)
    {
        GardenPlots.Remove(Cell);
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Garden] Plot at (%d,%d) cleared"), Cell.X, Cell.Y);
        return;
    }
    GardenPlots.Add(Cell, Niche);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Garden] Plot at (%d,%d) set to niche %d"), Cell.X, Cell.Y, (int32)Niche);
}

void AGridWorldManager::StartRegeneration(FGridCell& Cell)
{
    // Время возрождения -- пер-региональная настройка
    // (ABiomeRegionVolume::ResourceRegrowthTimeSeconds, 2026-09-02), если
    // клетка реально заявлена регионом; без регионов на уровне -- глобальный
    // ResourceRegrowthTime, как раньше.
    ABiomeRegionVolume* ClaimingRegion = GetClaimingRegion(Cell);
    const float RegrowthTime = ClaimingRegion ? ClaimingRegion->ResourceRegrowthTimeSeconds : ResourceRegrowthTime;

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, &Cell]()
    {
        // Водные растения (2026-09-02) возрождаются тем же путём, что и
        // земляные -- SpawnResourcesInCell сама решает пул (аквапул для
        // bIsWater), раньше вода была исключена целиком.
        SpawnResourcesInCell(Cell);
        // В отличие от исходного броска в InitializeCells (тот безопасно
        // переигрывается заново из RngBaseSeed), это отросшее — не то же
        // самое, что дало бы InitializeCells на старте. Сейв должен его помнить.
        MarkCellDirty(Cell.X, Cell.Y);
    }, RegrowthTime, false);
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
    Cmd.Harvest.bIronAverse   = Actor->GetIsIronAverse();
    Cmd.Harvest.bDelicate     = Actor->GetIsDelicate();
    Cmd.Harvest.Tool          = PC ? PC->CurrentGatheringTool : EGatheringTool::BareHands;
    QueueCommand(Cmd);

    // Водные растения (2026-09-02) возрождаются тем же путём -- вода
    // раньше была исключена из этого гейта целиком.
    if (Cell->ResourceActors.Num() == 0)
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

void AGridWorldManager::RegenerateCellParameters(float DeltaTime, const FIntPoint* OnlyChunk)
{
    const float RegenerationRate = 0.0005f;   // 0.05% в секунду
    const float DeltaRegen = RegenerationRate * DeltaTime;

    // Спад HarvestStress: клетка со стрессом 1.0 полностью зарастает за
    // StressRecoveryGameDays игровых суток, умноженные на множитель биома
    // (болото со стоячей водой держит след дольше, пойма промывает быстрее).
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RecoveryDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    const float DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f;

    // Сезонный множитель (15_Cycles_And_Shrines.md §15.4): Весна — "временный
    // бонус к скорости зарастания клеток во всех биомах", Зима — "клетки
    // заживают медленнее" — отсюда умножение на биомный множитель ниже, а не
    // замена его. Лето — намеренно 1.0 (см. комментарий у GetSeason() в
    // GridWorldManagerEntities.cpp). Множитель здесь — время полного
    // восстановления (RecoveryDays × DaySeconds × Multiplier ниже), не
    // скорость: Весна < 1.0 (короче срок = быстрее), Зима > 1.0 (длиннее
    // срок = медленнее) — см. оговорку у HerbalistSettings.h.
    float SeasonMultiplier = 1.0f;
    switch (GetSeason())
    {
    case ESeason::Spring: SeasonMultiplier = Settings ? Settings->SpringStressRecoveryMultiplier : 0.7f; break;
    case ESeason::Winter: SeasonMultiplier = Settings ? Settings->WinterStressRecoveryMultiplier : 1.6f; break;
    default: break;
    }

    // Множитель биома зависит только от типа, а не от клетки — тянем строку
    // DataTable один раз на биом, а не 400 раз за кадр. Сезон одинаков для
    // всей сетки в рамках одного вызова, поэтому безопасно замкнуть его в
    // тот же кэш через SeasonMultiplier из внешней области видимости.
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
        Multiplier *= SeasonMultiplier;
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
        // Стриминг сетки (2026-09-03): релаксация считается только в
        // активных чанках. Неактивная клетка не «портится» и не «чинится»,
        // пока до неё никому нет дела; при активации получит догон за всё
        // пропущенное время (юнит 2) — экспоненциальная форма сходимости
        // делает такой единичный шаг точным, а не приближённым.
        if (!IsCellActive(Cell)) continue;

        // Догон одного конкретного чанка (CatchUpActivatedChunks) — остальные
        // клетки в этом вызове не трогаем, у них своё время.
        if (OnlyChunk && GetChunkCoordForCell(Cell.X, Cell.Y) != *OnlyChunk) continue;

        // Перо Жар-птицы (16_Entity_Manifestation.md §16.4, 2026-09-02) —
        // клетка, помеченная навечно чистой, полностью исключена из этой
        // функции: ни бистабильная релаксация, ни заражение соседей, ни
        // обычная релаксация к TargetState её не трогают — заморожена на
        // текущем значении, как и просит §16.4 ("заморозь TargetState/
        // State на текущем значении"). Заражение соседей всё ещё может
        // толкать её TargetState ИЗВНЕ (см. guard у Neighbor ниже, отдельно).
        if (Cell.bEternallyPure) continue;

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

        // Заражение соседей (2026-08-30, "разрастание поганых мест") — пока
        // клетка в испорченном полюсе, она непрерывно толкает TargetState
        // четырёх прямых соседей по сетке в ту же сторону, что и её
        // собственный полюс. Пересекает границу биома намеренно (прямое
        // решение пользователя) — заражение не спрашивает биом соседа, тем
        // же принципом, что уже диффузия Морока по биомному графу, только
        // на уровне клеток сетки. Сравнение перед записью — тот же §7.1
        // паттерн, что у ночного/зимнего нуджа выше в этом файле: без него
        // сосед у уже насыщенного полюса грязнился бы каждый кадр без
        // реального изменения.
        if (Cell.Memory.bDegrading)
        {
            const float ContagionRate = Settings ? Settings->ContagionSpreadRate : 0.01f;
            if (ContagionRate > 0.0f)
            {
                static const FIntPoint ContagionOffsets[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
                for (const FIntPoint& Offset : ContagionOffsets)
                {
                    FGridCell* Neighbor = GetCell(Cell.X + Offset.X, Cell.Y + Offset.Y);
                    if (!Neighbor || Neighbor->bEternallyPure) continue;

                    const float NewCorruption = FMath::Clamp(Neighbor->TargetState.Meta.Corruption + ContagionRate * DeltaTime, 0.0f, 1.0f);
                    const float NewPurity     = FMath::Clamp(Neighbor->TargetState.Meta.Purity     - ContagionRate * DeltaTime, 0.0f, 1.0f);
                    const float NewDistortion = FMath::Clamp(Neighbor->TargetState.Meta.Distortion + ContagionRate * DeltaTime, 0.0f, 1.0f);
                    const float NewStability  = FMath::Clamp(Neighbor->TargetState.Meta.Stability  - ContagionRate * DeltaTime, 0.0f, 1.0f);

                    if (!FMath::IsNearlyEqual(NewCorruption, Neighbor->TargetState.Meta.Corruption, KINDA_SMALL_NUMBER) ||
                        !FMath::IsNearlyEqual(NewPurity,     Neighbor->TargetState.Meta.Purity,     KINDA_SMALL_NUMBER) ||
                        !FMath::IsNearlyEqual(NewDistortion, Neighbor->TargetState.Meta.Distortion, KINDA_SMALL_NUMBER) ||
                        !FMath::IsNearlyEqual(NewStability,  Neighbor->TargetState.Meta.Stability,  KINDA_SMALL_NUMBER))
                    {
                        Neighbor->TargetState.Meta.Corruption = NewCorruption;
                        Neighbor->TargetState.Meta.Purity     = NewPurity;
                        Neighbor->TargetState.Meta.Distortion = NewDistortion;
                        Neighbor->TargetState.Meta.Stability  = NewStability;
                        MarkCellDirty(Neighbor->X, Neighbor->Y);
                    }
                }
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
        float StabilityDeltaRegen = DeltaRegen;   // эффект 3, Родовое — см. ниже
        float DirectionRateMultiplier = 1.0f;
        const FShrine* DominantShrine = Shrines.Num() > 0
            ? HerbalistCore::Shrine::FindDominantShrine(FIntPoint(Cell.X, Cell.Y), Shrines, Settings ? Settings->ShrineInfluenceRadius : 3)
            : nullptr;
        if (DominantShrine && DominantShrine->Restoration != 0.0f)
        {
            const float SafeInfluence = FMath::Clamp(DominantShrine->Restoration, -0.95f, 1.0f);
            const bool bApproachingS0 = HerbalistCore::Math::Distance(T, FAlatyr::S0) < HerbalistCore::Math::Distance(S, FAlatyr::S0);
            const float Modulation = bApproachingS0 ? (1.0f + SafeInfluence) : (1.0f / (1.0f + SafeInfluence));
            CellDeltaRegen *= Modulation;
            DirectionRateMultiplier = Modulation;

            // Эффект 3, Родовое (Дажьбог, §15.5): "усиливает пуллинг
            // Stability" — только Stability-ось релаксации, поверх уже
            // посчитанной Modulation, не вместо неё.
            StabilityDeltaRegen = CellDeltaRegen;
            if (DominantShrine->Type == EShrineType::Ancestral)
            {
                StabilityDeltaRegen *= Settings ? Settings->ShrineAncestralStabilityMultiplier : 1.5f;
            }
        }

        // Эффект 3, Водное (Мокошь, §15.5): "подтягивает Purity воды к 1.0
        // пропорционально Restoration" — локальный непрерывный нудж
        // TargetState в радиусе капища (не мутация общего DefaultWaterState
        // биома — эффект капища всегда локален, тот же принцип, что и у
        // остальных четырёх типов). Сравнение перед записью — тот же §7.1
        // паттерн, что у ночного/зимнего нуджа: без него клетка с уже
        // насыщенной Purity=1.0 грязнилась бы каждый кадр без изменения.
        if (Cell.bIsWater && DominantShrine && DominantShrine->Type == EShrineType::Water && DominantShrine->Restoration > 0.0f)
        {
            const float PullRate = Settings ? Settings->ShrineWaterPurityPullRate : 0.02f;
            const float NewTargetPurity = FMath::Clamp(
                Cell.TargetState.Meta.Purity + PullRate * DominantShrine->Restoration * DeltaTime, 0.0f, 1.0f);
            if (!FMath::IsNearlyEqual(NewTargetPurity, Cell.TargetState.Meta.Purity, KINDA_SMALL_NUMBER))
            {
                Cell.TargetState.Meta.Purity = NewTargetPurity;
                MarkCellDirty(Cell.X, Cell.Y);
            }
        }

        // 1. Отклонение по Meta + Magnitude — единым шагом на все семь осей
        // вместо прежних четырёх. bChanged нужен только для раннего выхода
        // из релаксации Direction ниже, спад HarvestStress идёт независимо.
        bool bChanged = false;
        bChanged |= MoveToward(S.Meta.Distortion, T.Meta.Distortion, CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Purity,     T.Meta.Purity,     CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Stability,  T.Meta.Stability,  StabilityDeltaRegen);
        bChanged |= MoveToward(S.Meta.Potency,    T.Meta.Potency,    CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Resonance,  T.Meta.Resonance,  CellDeltaRegen);
        bChanged |= MoveToward(S.Meta.Corruption, T.Meta.Corruption, CellDeltaRegen);
        bChanged |= MoveToward(S.Magnitude,       T.Magnitude,       CellDeltaRegen);

        // 2. Спад HarvestStress — медленный, зависит от биома (см. выше).
        // Эффект 3, Лесное (Велес, §15.5): "ускоряет заживление клеток" —
        // StressRecoveryMultiplier биома делится на (1+0.5×Restoration),
        // что для decay-в-секунду (обратно пропорционален Multiplier)
        // эквивалентно умножению на тот же коэффициент. Локально — только
        // в радиусе капища, GetStressDecay(Biome) сам по себе биомный, не
        // клеточный кэш, эффект капища накладывается поверх здесь.
        const bool bStressDecaying = Cell.HarvestStress > 0.0f;
        float StressDecayRate = GetStressDecay(Cell.Biome);
        if (DominantShrine && DominantShrine->Type == EShrineType::Forest && DominantShrine->Restoration > 0.0f)
        {
            const float HealBonus = Settings ? Settings->ShrineForestHealBonus : 0.5f;
            StressDecayRate *= (1.0f + HealBonus * DominantShrine->Restoration);
        }
        Cell.HarvestStress = FMath::Max(Cell.HarvestStress - StressDecayRate * DeltaTime, 0.0f);

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