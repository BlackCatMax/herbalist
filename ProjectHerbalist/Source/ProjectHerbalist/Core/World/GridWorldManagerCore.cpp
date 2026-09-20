// Core/World/GridWorldManagerCore.cpp
#include "Core/World/GridWorldManager.h"
#include "Misc/PackageName.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "Engine/OverlapResult.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "Core/World/HomesteadMarkerActor.h"
#include "Core/World/KurganActor.h"
#include "Core/World/POIActors.h"
#include "Core/Entities/LegendaryAnchorMarkerActor.h"
#include "Core/Entities/LandmarkTypes.h"
#include "EngineUtils.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Subsystems/WaterTypeRegistrySubsystem.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/BiomeRow.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Save/HerbalistSaveTypes.h"
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

    // Высоты -- в странице рядом с клетками (этап 8): страница загружается и
    // выгружается вместе с ними.
    int32 CachedCount = 0;
    for (FHerbalistCellPage& Page : CellPages)
    {
        if (Page.bLoaded)
        {
            CacheCellHeightsForPage(Page);
            CachedCount += Page.Cells.Num();
        }
    }
    bCellHeightsCached = true;

    if (CachedLandscape)
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("Cached %d cell heights from landscape"), CachedCount);
    }
}

float AGridWorldManager::GetCellHeight(int32 X, int32 Y) const
{
    // Границы до умножения (найдено ревью этапа 6а): координата вне сетки, в
    // том числе InvalidCell, переполняла int32 в Y * GridSizeX.
    if (!IsCellInGrid(X, Y))
    {
        return 0.f;
    }
    const FHerbalistCellPage* Page = FindCellPage(X, Y);
    if (!Page || !Page->bLoaded)
    {
        return 0.f;
    }
    const int32 LocalIndex = Page->GetLocalIndex(X, Y);
    return Page->Heights.IsValidIndex(LocalIndex) ? Page->Heights[LocalIndex] : 0.f;
}

FVector AGridWorldManager::GetCellWorldPositionFlat(int32 X, int32 Y) const
{
    const FIntPoint Min = GetGridMinCell();
    return GetGridOrigin() + FVector((X - Min.X) * CellSize, (Y - Min.Y) * CellSize, 0.f);
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
    OutX = HerbalistCore::InvalidCell().X;
    OutY = HerbalistCore::InvalidCell().Y;

    const FVector LocalLoc = WorldPos - GetGridOrigin();
    const FIntPoint Min = GetGridMinCell();
    const int32 X = Min.X + FMath::FloorToInt(LocalLoc.X / CellSize);
    const int32 Y = Min.Y + FMath::FloorToInt(LocalLoc.Y / CellSize);

    if (IsCellInGrid(X, Y))
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
    // Проверка клеток, не только сравнение с GridSizeX/GridSizeY (2026-09-02):
    // GridSizeX/GridSizeY -- это НАМЕРЕНИЕ (EditAnywhere-свойство актора,
    // валидно сразу после конструктора), а клетки (страницы) создаются только
    // в InitializeCells() (BeginPlay). Актор, размещённый на уровне, но ещё
    // не прошедший BeginPlay в этой конкретной игровой сессии (например,
    // редакторский предпросмотр без Play, или другой AGridWorldManager,
    // случайно найденный через TActorIterator раньше "правильного") имел бы
    // валидный по GridSizeX/Y индекс, но ни одной клетки -- падение с
    // Array index out of bounds вместо честного nullptr. Нашёл
    // AHerbalistResourceActor::RegisterOnCell(), вызываемый из BeginPlay
    // ресурсного актора, спавненного PCG-графом раньше, чем менеджер успел
    // инициализировать сетку.
    // Границы до индекса (ревью этапа 6): координата далеко за краем, в том
    // числе InvalidCell, переполняла бы int32 в GetCellIndex.
    if (!IsCellInGrid(X, Y))
    {
        return nullptr;
    }
    FHerbalistCellPage* Page = FindCellPage(X, Y);
    if (!Page || !Page->bLoaded)
    {
        return nullptr;
    }
    const int32 LocalIndex = Page->GetLocalIndex(X, Y);
    return Page->Cells.IsValidIndex(LocalIndex) ? &Page->Cells[LocalIndex] : nullptr;
}

FIntPoint AGridWorldManager::GetChunkCoordForCell(int32 CellX, int32 CellY) const
{
    const int32 ChunkSize = GetChunkSizeInCells();
    // FloorDiv, не целочисленное деление: у отрицательных координат (с этапа 6
    // разметки мира -- клетки западнее и южнее начала сетки World Partition) обычное
    // деление тянет к нулю и склеивает чанк -1 с чанком 0.
    // Целочисленно, без float (ревью этапа 6): точно на любых координатах и
    // дешевле на горячем пути IsCellActive.
    return FIntPoint(HerbalistCore::FloorDivCoord(CellX, ChunkSize), HerbalistCore::FloorDivCoord(CellY, ChunkSize));
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

void AGridWorldManager::SetResourceSlots(UHerbalistResourceSlots* InSlots)
{
    ResourceSlotsAsset = InSlots;
    ResourceSlotsByCell.Reset();
    bResourceSlotsIndexed = false;
}

const TArray<FHerbalistResourceSlot>* AGridWorldManager::FindCellResourceSlots(int32 X, int32 Y) const
{
    if (!ResourceSlotsAsset)
    {
        return nullptr;
    }
    if (!bResourceSlotsIndexed)
    {
        bResourceSlotsIndexed = true;
        ResourceSlotsByCell.Reset();
        for (const FHerbalistResourceSlotSet& Set : ResourceSlotsAsset->Sets)
        {
            for (const FHerbalistResourceSlot& Slot : Set.Slots)
            {
                int32 CellX = 0;
                int32 CellY = 0;
                if (WorldPositionToCell(Slot.Location, CellX, CellY))
                {
                    ResourceSlotsByCell.FindOrAdd(FIntPoint(CellX, CellY)).Add(Slot);
                }
            }
        }
    }
    return ResourceSlotsByCell.Find(FIntPoint(X, Y));
}

bool AGridWorldManager::HasResourceSlots(int32 X, int32 Y) const
{
    const TArray<FHerbalistResourceSlot>* Slots = FindCellResourceSlots(X, Y);
    return Slots && Slots->Num() > 0;
}

bool AGridWorldManager::GetAssignedResourceSlot(int32 X, int32 Y, int32 PlacementSlot, FHerbalistResourceSlot& OutSlot) const
{
    const TArray<FHerbalistResourceSlot>* Slots = FindCellResourceSlots(X, Y);
    if (!Slots || Slots->Num() == 0 || PlacementSlot < 0)
    {
        return false;
    }
    OutSlot = (*Slots)[MakeResourceSlotOrder(X, Y, Slots->Num())[PlacementSlot % Slots->Num()]];
    return true;
}

TArray<int32> AGridWorldManager::MakeResourceSlotOrder(int32 X, int32 Y, int32 NumSlots) const
{
    TArray<int32> Order;
    Order.Reserve(NumSlots);
    for (int32 Index = 0; Index < NumSlots; ++Index)
    {
        Order.Add(Index);
    }
    FRandomStream OrderRng = MakeCellRandomStream(X, Y, FWorldLayoutSolver::ECellRandomPurpose::ResourceSlotOrder);
    for (int32 Index = Order.Num() - 1; Index > 0; --Index)
    {
        Order.Swap(Index, OrderRng.RandRange(0, Index));
    }
    return Order;
}

bool AGridWorldManager::FindResourceSlotPosition(int32 X, int32 Y, int32 PlacementSlot, bool bAquaticSpecies, FVector& OutPosition) const
{
    const TArray<FHerbalistResourceSlot>* Slots = FindCellResourceSlots(X, Y);
    if (!Slots || Slots->Num() == 0 || PlacementSlot < 0)
    {
        return false;
    }

    // Слот под живым ресурсом клетки занят: IsSpawnPointBlocked свои игровые
    // акторы не считает. Места до числа слотов и так получают разные слоты;
    // проверка нужна по кругу (мест больше, чем слотов) и после перезапекания.
    const FGridCell* Cell = GetCellConst(X, Y);
    auto IsTakenByResource = [Cell](const FVector& Location)
    {
        if (!Cell)
        {
            return false;
        }
        for (const TWeakObjectPtr<AHerbalistResourceActor>& Resource : Cell->ResourceActors)
        {
            if (Resource.IsValid() && FVector::DistSquared2D(Resource->GetActorLocation(), Location) < 1.0f)
            {
                return true;
            }
        }
        return false;
    };

    const TArray<int32> Order = MakeResourceSlotOrder(X, Y, Slots->Num());
    for (int32 Step = 0; Step < Order.Num(); ++Step)
    {
        const FHerbalistResourceSlot& Slot = (*Slots)[Order[(PlacementSlot + Step) % Order.Num()]];
        if (HerbalistResourceSlots::SlotSuitsSpecies(Slot.Kind, bAquaticSpecies)
            && !IsTakenByResource(Slot.Location) && !IsSpawnPointBlocked(Slot.Location))
        {
            OutPosition = Slot.Location;
            return true;
        }
    }
    return false;
}

bool AGridWorldManager::FindResourcePosition(int32 X, int32 Y, int32 PlacementSlot, bool bAquaticSpecies, FRandomStream& PlacementRng,
    FVector& OutPosition, bool& bOutFromSlot) const
{
    bOutFromSlot = false;
    if (HasResourceSlots(X, Y))
    {
        if (FindResourceSlotPosition(X, Y, PlacementSlot, bAquaticSpecies, OutPosition))
        {
            bOutFromSlot = true;
            return true;
        }
        if (bAquaticSpecies)
        {
            return false;
        }
    }
    return FindFreeSpawnPositionInCell(X, Y, GetResourceJitterRadius(), PlacementRng, OutPosition);
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

            // Ищем именно ЗЕМЛЮ, а не первое попадание сверху (2026-09-03,
            // найдено в PIE: одна травинка висела в воздухе). Одиночный
            // трейс возвращал крону дерева или верх валуна -- растение
            // садилось на них, а проверка занятости этого не ловила: сфера
            // над кроной действительно пуста. Берём первый хит, который
            // принадлежит ландшафту; если ландшафта под точкой нет вовсе --
            // оставляем высоту клетки из кэша, она тоже с ландшафта.
            TArray<FHitResult> Hits;
            World->LineTraceMultiByChannel(Hits, Start, End, ECC_WorldStatic, TraceParams);
            for (const FHitResult& Hit : Hits)
            {
                const AActor* HitActor = Hit.GetActor();
                if (HitActor && HitActor->IsA<ALandscapeProxy>())
                {
                    Candidate.Z = Hit.ImpactPoint.Z;
                    break;
                }
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
    const float Jitter = GetResourceJitterRadius();
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float TraceHalf = Settings ? FMath::Max(0.0f, Settings->SpawnTraceHalfHeight) : 5000.0f;

    int32 Considered = 0, Free = 0, Blocked = 0;
    const FIntPoint GridMin = GetGridMinCell();
    for (int32 Y = 0; Y < GridSizeY && Considered < PreviewMaxCells; ++Y)
    {
        for (int32 X = 0; X < GridSizeX && Considered < PreviewMaxCells; ++X)
        {
            FVector Base = GetCellWorldPositionFlat(GridMin.X + X, GridMin.Y + Y);

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

            // Тот же поиск именно земли, что и в FindFreeSpawnPositionInCell.
            // Сама точка -- пример из потока превью, а не будущее место ресурса:
            // места берутся из потоков слотов клеток (этап 4 разметки мира).
            FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(HerbalistPreviewGround), false);
            TraceParams.AddIgnoredActor(this);
            TArray<FHitResult> Hits;
            World->LineTraceMultiByChannel(Hits, Candidate + FVector(0, 0, TraceHalf), Candidate - FVector(0, 0, TraceHalf), ECC_WorldStatic, TraceParams);
            for (const FHitResult& Hit : Hits)
            {
                const AActor* HitActor = Hit.GetActor();
                if (HitActor && HitActor->IsA<ALandscapeProxy>())
                {
                    Candidate.Z = Hit.ImpactPoint.Z;
                    break;
                }
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

    // С разметкой радиус урезан до дальности загрузки и кратен чанку (найдено
    // ревью: здесь брался радиус прямо из настроек и мог выйти за загруженные
    // страницы). Тот же итог, что показывает лог разметки.
    if (ResolvedLayout.bValid && ResolvedLayout.EffectiveSimulationRadiusMeters >= 0.0)
    {
        const double ChunkMeters = ResolvedLayout.GetChunkSizeCm() / 100.0;
        return ChunkMeters > 0.0 ? FMath::RoundToInt32(ResolvedLayout.EffectiveSimulationRadiusMeters / ChunkMeters) : 0;
    }

    const int32 ChunkSize = GetChunkSizeInCells();
    const float ChunkSpanCm = FMath::Max(KINDA_SMALL_NUMBER, CellSize * ChunkSize);
    // Floor, не ceil: радиус меньше одного чанка честно означает "только свой
    // чанк" (0), а не "и соседние тоже".
    return FMath::FloorToInt((RadiusMeters * 100.0f) / ChunkSpanCm);
}

float AGridWorldManager::GetEntityManifestationJitterRadius() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Fraction = Settings ? Settings->EntityManifestationJitterFraction : 0.3f;
    return CellSize * Fraction;
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

void AGridWorldManager::ForEachCellInChunk(const FIntPoint& Chunk, TFunctionRef<void(FGridCell&)> Func)
{
    // Те же границы чанка, что уже считает SetChunkResourcesActive --
    // GetCell() сам отбрасывает координаты вне сетки и загруженных страниц, так
    // что клэмпить MaxX/MaxY здесь не нужно: последний чанк ряда, не
    // кратного ChunkSizeInCells, просто получит меньше валидных клеток.
    const int32 ChunkSize = GetChunkSizeInCells();

    const int32 MinX = Chunk.X * ChunkSize;
    const int32 MinY = Chunk.Y * ChunkSize;

    for (int32 Y = MinY; Y < MinY + ChunkSize; ++Y)
    {
        for (int32 X = MinX; X < MinX + ChunkSize; ++X)
        {
            if (FGridCell* Cell = GetCell(X, Y))
            {
                Func(*Cell);
            }
        }
    }
}

void AGridWorldManager::ForEachActiveCell(TFunctionRef<void(FGridCell&)> Func)
{
    const int32 Radius = GetActiveRadiusInChunks();

    // Те же две ветки "активно всё", что и у IsCellActive выше -- решаются
    // ОДИН раз для всего вызова, а не 250 000 раз внутри цикла.
    if (Radius < 0 || ActiveChunkCenters.Num() == 0)
    {
        for (FGridCell& Cell : GetCellsInGridOrder())
        {
            Func(Cell);
        }
        return;
    }

    // НЕ переиспользуем member ActiveChunks -- тот кэш актуален только
    // после CatchUpActivatedChunks() этого кадра (обычный путь Tick), а эта
    // функция вызывается и напрямую, без прогона Tick (см. предупреждение
    // у объявления в .h). Считаем ту же геометрию заново тем же общим
    // хелпером -- дёшево: центров и радиус в чанках всегда мало.
    for (const FIntPoint& Chunk : ComputeChunksWithinRadius(ActiveChunkCenters, Radius))
    {
        ForEachCellInChunk(Chunk, Func);
    }
}

void AGridWorldManager::UpdateActiveChunkCenters()
{
    ActiveChunkCenters.Reset();

    UWorld* World = GetWorld();
    if (!World) return;

    // Центр -- чанк в координатах сетки, В ТОМ ЧИСЛЕ за её пределами
    // (2026-09-12). Раньше источник вне сетки не давал центра вовсе: стоя в
    // паре метров за краем, игрок не видел ресурсов у самого края, а ушедший
    // далеко -- оставлял за собой вечно висящие акторы (центров нет, см.
    // CatchUpActivatedChunks). Чанки за краем отсекает
    // ComputeChunksWithinRadius.
    auto AddCenterFromWorldLocation = [this](const FVector& WorldLocation)
    {
        ActiveChunkCenters.AddUnique(WorldPositionToChunk(WorldLocation));
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
    const int32 ChunkSize = GetChunkSizeInCells();

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
                    // Возврат игрока: поднимаем ровно то, что стояло. Список
                    // забирается целиком ДО спавна (2026-09-12): спавн в
                    // нематериализованную клетку сам дописывает в
                    // DormantResourceIDs, и правка массива посреди обхода
                    // была бы неопределённым поведением.
                    TArray<FName> ToWake = MoveTemp(Cell->DormantResourceIDs);
                    TArray<int32> ToWakeSlots = MoveTemp(Cell->DormantResourceSlots);
                    Cell->DormantResourceIDs.Reset();
                    Cell->DormantResourceSlots.Reset();
                    SpawnResourceRoster(*Cell, ToWake, ToWakeSlots);
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

                    AddDormantResource(*Cell, Actor->GetIngredientID(), Actor->GetPlacementSlot());
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

void AGridWorldManager::DespawnChunkEntities(const FIntPoint& Chunk)
{
    // Не мутирует сами клетки (не FGridCell& в лямбде понадобился бы, будь
    // тут запись Cell.ManifestedEntityID) -- только уничтожает актора и
    // отпускает слабую ссылку. ManifestedEntityID остаётся как был: это
    // "что должно проявиться", не физическое присутствие.
    ForEachCellInChunk(Chunk, [](FGridCell& Cell)
    {
        if (AHerbalistEntityActor* Actor = Cell.ManifestedEntityActor.Get())
        {
            Actor->Destroy();
        }
        // TWeakObjectPtr сам обнулится после Destroy() -- явный Reset() не
        // нужен, но не вредит и снимает вопрос "а точно ли обнулился".
        Cell.ManifestedEntityActor.Reset();
    });
}

void AGridWorldManager::GetGridChunkRange(FIntPoint& OutMinChunk, FIntPoint& OutMaxChunk) const
{
    const FIntPoint Min = GetGridMinCell();
    OutMinChunk = GetChunkCoordForCell(Min.X, Min.Y);
    OutMaxChunk = GridSizeX > 0 && GridSizeY > 0
        ? GetChunkCoordForCell(Min.X + GridSizeX - 1, Min.Y + GridSizeY - 1)
        : OutMinChunk - FIntPoint(1, 1);
}

TSet<FIntPoint> AGridWorldManager::ComputeChunksWithinRadius(const TArray<FIntPoint>& Centers, int32 Radius) const
{
    // Только чанки, у которых есть клетки (2026-09-12): центр теперь может
    // лежать за краем сетки, и без отсечения игрок, гуляющий вне сетки,
    // плодил бы записи ChunkLastSimulatedGameTime для пустых чанков. Границы --
    // глобальные координаты чанков (этап 6 разметки мира).
    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    GetGridChunkRange(MinChunk, MaxChunk);

    TSet<FIntPoint> Result;
    const int32 ChunkCells = GetChunkSizeInCells();
    for (const FIntPoint& Center : Centers)
    {
        const int32 MinX = FMath::Max(Center.X - Radius, MinChunk.X);
        const int32 MaxX = FMath::Min(Center.X + Radius, MaxChunk.X);
        const int32 MinY = FMath::Max(Center.Y - Radius, MinChunk.Y);
        const int32 MaxY = FMath::Min(Center.Y + Radius, MaxChunk.Y);
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                // Чанк страницы-заполнителя не активируется (ревью 2026-09-13):
                // игрок у края ландшафта со стороны расширения загрузил бы
                // страницу без земли, симулировал её и записал в сейв.
                if (IsCellInExtensionFiller(X * ChunkCells, Y * ChunkCells))
                {
                    continue;
                }
                Result.Add(FIntPoint(X, Y));
            }
        }
    }
    return Result;
}

void AGridWorldManager::CatchUpActivatedChunks()
{
    const int32 Radius = GetActiveRadiusInChunks();

    // Механизм выключен (или источников нет) — активно всё, простаивать
    // нечему, догонять нечего.
    if (Radius < 0 || ActiveChunkCenters.Num() == 0)
    {
        // Активные чанки ушли -- страницы под ними могли опустеть (этап 8в).
        bCellPageUnloadCheckPending |= PreviousActiveChunks.Num() > 0;
        ActiveChunks.Reset();
        PreviousActiveChunks.Reset();
        // Раньше здесь был просто return, и всё материализованное вокруг
        // последней позиции игрока навсегда оставалось в мире (2026-09-12,
        // "ресурсы остались там, откуда вы ушли, висящими в воздухе").
        // Материализация ведётся отдельно и сама решает, что усыпить.
        UpdateMaterializedChunks();
        UnloadIdleCellPagesIfPending();
        return;
    }

    ActiveChunks = ComputeChunksWithinRadius(ActiveChunkCenters, Radius);

    const float Now = GameClockSeconds;
    for (const FIntPoint& Chunk : ActiveChunks)
    {
        // Страница чанка -- до догона: он читает клетки (этап 8в). Уже
        // активный чанк выгруженным не бывает.
        if (!PreviousActiveChunks.Contains(Chunk))
        {
            EnsureChunkPagesLoaded(Chunk);
        }

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

    // Сущности уходят на границе симуляции (2026-09-12, второй заход): ресурсы
    // стоят, пока под ними загружена земля, а поведение сущностей считается
    // только в активных чанках -- за этой границей актор стоял бы
    // замороженным. ManifestedEntityID остаётся, и актор проявится снова, как
    // только чанк опять станет активным (см. DespawnChunkEntities в .h).
    for (const FIntPoint& Chunk : PreviousActiveChunks)
    {
        if (!ActiveChunks.Contains(Chunk))
        {
            DespawnChunkEntities(Chunk);
            bCellPageUnloadCheckPending = true;
        }
    }

    PreviousActiveChunks = ActiveChunks;

    // Материализация/усыпление акторов ресурсов -- через
    // UpdateMaterializedChunks: по загруженной земле, а не по активности.
    UpdateMaterializedChunks();
    UnloadIdleCellPagesIfPending();
}

FIntPoint AGridWorldManager::WorldPositionToChunk(const FVector& WorldPos) const
{
    const int32 ChunkSize = GetChunkSizeInCells();
    const double ChunkSpan = FMath::Max(static_cast<double>(CellSize) * ChunkSize, UE_DOUBLE_KINDA_SMALL_NUMBER);
    // От мировой точки глобальной клетки (0,0) (этап 6 разметки мира). Для точек
    // внутри сетки совпадает с GetChunkCoordForCell(WorldPositionToCell):
    const FVector Local = WorldPos - GetCellWorldPositionFlat(0, 0);
    // floor(floor(x / c) / n) == floor(x / (c * n)).
    return FIntPoint(FMath::FloorToInt(Local.X / ChunkSpan), FMath::FloorToInt(Local.Y / ChunkSpan));
}

bool AGridWorldManager::IsChunkMaterialized(const FIntPoint& Chunk) const
{
    // Стриминг выключен или игрока ещё не было -- как до механизма.
    if (!bMaterializationTracked || GetActiveRadiusInChunks() < 0)
    {
        return true;
    }
    return MaterializedChunks.Contains(Chunk);
}

bool AGridWorldManager::IsCellMaterialized(const FGridCell& Cell) const
{
    return IsChunkMaterialized(GetChunkCoordForCell(Cell.X, Cell.Y));
}

bool AGridWorldManager::IsChunkGroundLoaded(const FIntPoint& Chunk) const
{
    if (!bGroundCoverageKnown)
    {
        return true;
    }

    const int32 ChunkSize = GetChunkSizeInCells();
    const double Span = static_cast<double>(CellSize) * ChunkSize;
    // Чанки считаются от мировой точки глобальной клетки (0,0) (этап 6).
    const FVector Origin = GetCellWorldPositionFlat(0, 0);
    const FVector2D Min(Origin.X + Chunk.X * Span, Origin.Y + Chunk.Y * Span);
    const FVector2D Max = Min + FVector2D(Span, Span);

    // Углы и центр: прямоугольники земли (прокси ландшафта, ячейки World
    // Partition) много крупнее чанка, так что дыру посередине чанка при
    // покрытых углах даёт только прямоугольник мельче самого чанка.
    // Включительные границы -- чанк на стыке двух прямоугольников покрыт обоими.
    const FVector2D Points[] = { Min, FVector2D(Max.X, Min.Y), FVector2D(Min.X, Max.Y), Max, (Min + Max) * 0.5 };
    for (const FVector2D& Point : Points)
    {
        const bool bCovered = GroundCoverage.ContainsByPredicate([&Point](const FBox2D& Box)
        {
            return Point.X >= Box.Min.X && Point.X <= Box.Max.X && Point.Y >= Box.Min.Y && Point.Y <= Box.Max.Y;
        });
        if (!bCovered)
        {
            return false;
        }
    }
    return true;
}

void AGridWorldManager::RefreshGroundCoverage()
{
    GroundCoverage.Reset();
    bGroundCoverageKnown = false;

    if (GroundCoverageOverride.IsSet())
    {
        GroundCoverage = GroundCoverageOverride.GetValue();
        bGroundCoverageKnown = true;
        return;
    }

    UWorld* World = GetWorld();
    const UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
    if (!World || !World->IsGameWorld() || !WorldPartition || !WorldPartition->IsStreamingEnabled()
        || !WorldPartition->RuntimeHash)
    {
        return;
    }
    bGroundCoverageKnown = true;

    // Земля -- это загруженный ландшафт (2026-09-12, второй заход). Ячейки
    // World Partition для этого не годятся: крупный актор (регион-сплайн,
    // водоём) попадает в ячейку старшего уровня размером в сотни метров, и
    // она активна целиком, пока задевает дальность загрузки, -- в том числе
    // там, где плитки ландшафта уже выгружены. Пока ресурсы жили в радиусе
    // симуляции, края таких ячеек до них не доходили; ресурсы до границы
    // земли повисли бы как раз на них. Компоненты прокси регистрируются,
    // только когда их ячейка видима, -- ровно "земля есть".
    bool bWorldHasLandscape = false;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        bWorldHasLandscape = true;
        FBox Bounds(ForceInit);
        for (const TObjectPtr<ULandscapeComponent>& Component : It->LandscapeComponents)
        {
            if (Component && Component->IsRegistered())
            {
                Bounds += Component->Bounds.GetBox();
            }
        }
        if (Bounds.IsValid)
        {
            GroundCoverage.Add(FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)));
        }
    }

    // Карта без ландшафта (пол из мешей, как L_PlaytestPaint) -- землю держат
    // ячейки World Partition; оговорка про старшие уровни выше здесь
    // остаётся. RuntimeHash напрямую, а не UWorldPartition::IsStreamingCompleted:
    // тот на каждый вызов сдвигает StreamingStateEpoch и заставляет партишен
    // заново пересчитывать источники.
    if (!bWorldHasLandscape)
    {
        WorldPartition->RuntimeHash->ForEachStreamingCells([this](const UWorldPartitionRuntimeCell* Cell)
        {
            // HLOD-ячейки вблизи деактивированы по замыслу (их заменяют
            // настоящие), всегда загруженные не привязаны к месту -- ни те,
            // ни другие не говорят, есть ли земля в конкретной точке.
            if (Cell && !Cell->GetIsHLOD() && !Cell->IsAlwaysLoaded()
                && Cell->GetCurrentState() == EWorldPartitionRuntimeCellState::Activated)
            {
                const FBox Bounds = Cell->GetCellBounds();
                GroundCoverage.Add(FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)));
            }
            return true;
        });
    }

    if (GroundCoverage.Num() == 0 && !bLoggedEmptyGroundCoverage)
    {
        UE_LOG(LogHerbalistWorld, Log,
            TEXT("[Streaming] Загруженной земли пока нет (%s) -- ресурсы и сущности не материализуются, пока она не загрузится"),
            bWorldHasLandscape ? TEXT("ландшафт") : TEXT("ячейки World Partition"));
        bLoggedEmptyGroundCoverage = true;
    }
}

void AGridWorldManager::CollectGroundCoveredChunks(TSet<FIntPoint>& OutChunks) const
{
    OutChunks.Reset();

    const int32 ChunkSize = GetChunkSizeInCells();
    const double Span = FMath::Max(static_cast<double>(CellSize) * ChunkSize, UE_DOUBLE_KINDA_SMALL_NUMBER);
    // Чанки считаются от мировой точки глобальной клетки (0,0) (этап 6).
    const FVector Origin = GetCellWorldPositionFlat(0, 0);
    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    GetGridChunkRange(MinChunk, MaxChunk);

    // Обходятся только чанки сетки, задетые хотя бы одним прямоугольником;
    // покрытие решает та же IsChunkGroundLoaded (углы и центр), что и у
    // одиночного вопроса. Клэмп до перевода в int: ячейки старших уровней
    // бывают размером с весь мир.
    for (const FBox2D& Box : GroundCoverage)
    {
        const double LowX = static_cast<double>(MinChunk.X) - 1.0;
        const double LowY = static_cast<double>(MinChunk.Y) - 1.0;
        const double HighX = static_cast<double>(MaxChunk.X) + 1.0;
        const double HighY = static_cast<double>(MaxChunk.Y) + 1.0;
        const int32 MinX = FMath::Max(FMath::FloorToInt(FMath::Clamp((Box.Min.X - Origin.X) / Span, LowX, HighX)), MinChunk.X);
        const int32 MaxX = FMath::Min(FMath::FloorToInt(FMath::Clamp((Box.Max.X - Origin.X) / Span, LowX, HighX)), MaxChunk.X);
        const int32 MinY = FMath::Max(FMath::FloorToInt(FMath::Clamp((Box.Min.Y - Origin.Y) / Span, LowY, HighY)), MinChunk.Y);
        const int32 MaxY = FMath::Min(FMath::FloorToInt(FMath::Clamp((Box.Max.Y - Origin.Y) / Span, LowY, HighY)), MaxChunk.Y);
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const FIntPoint Chunk(X, Y);
                if (!OutChunks.Contains(Chunk) && IsChunkGroundLoaded(Chunk))
                {
                    OutChunks.Add(Chunk);
                }
            }
        }
    }
}

void AGridWorldManager::UpdateMaterializedChunks()
{
    if (GetActiveRadiusInChunks() < 0)
    {
        return;   // стриминг выключен: всё заселено при старте, усыплять нечего
    }

    RefreshGroundCoverage();

    const bool bHaveCentres = ActiveChunkCenters.Num() > 0;
    if (!bHaveCentres && !bMaterializationTracked && !bGroundCoverageKnown)
    {
        return;   // игрока ещё не было и земля неизвестна -- прежнее поведение
    }
    bMaterializationTracked = true;

    // Кандидаты. Земля известна -- всё, что на ней стоит; радиус симуляции ни
    // при чём (решение пользователя 2026-09-12: "ресурсы пропадают раньше" --
    // радиус ~100 м, ландшафт L_TestDev грузится на 252 м). Земля неизвестна
    // (вне игрового мира, без стриминга) -- активные чанки, а в кадр без
    // центров то, что уже материализовано: не сбрасываем, иначе ресурсы
    // моргали бы и вставали на новые места.
    TSet<FIntPoint> Candidates;
    if (bGroundCoverageKnown)
    {
        // Земля грузится и выгружается редко, а вопрос задаётся каждый кадр:
        // пока прямоугольники и сетка те же, набор уже верный.
        const FVector Origin = GetGridOrigin();
        uint32 Hash = GetTypeHash(GetChunkSizeInCells());
        Hash = HashCombineFast(Hash, GetTypeHash(CellSize));
        Hash = HashCombineFast(Hash, GetTypeHash(GridSizeX));
        Hash = HashCombineFast(Hash, GetTypeHash(GridSizeY));
        Hash = HashCombineFast(Hash, GetTypeHash(Origin.X));
        Hash = HashCombineFast(Hash, GetTypeHash(Origin.Y));
        Hash = HashCombineFast(Hash, GetTypeHash(GroundCoverage.Num()));
        for (const FBox2D& Box : GroundCoverage)
        {
            Hash = HashCombineFast(Hash, GetTypeHash(Box.Min.X));
            Hash = HashCombineFast(Hash, GetTypeHash(Box.Min.Y));
            Hash = HashCombineFast(Hash, GetTypeHash(Box.Max.X));
            Hash = HashCombineFast(Hash, GetTypeHash(Box.Max.Y));
        }
        if (bCoverageCacheValid && Hash == CoverageCacheHash)
        {
            return;
        }
        CoverageCacheHash = Hash;
        bCoverageCacheValid = true;
        CollectGroundCoveredChunks(Candidates);
    }
    else
    {
        bCoverageCacheValid = false;
        Candidates = bHaveCentres ? ActiveChunks : MaterializedChunks;
    }

    TArray<FIntPoint> ToSleep;
    for (const FIntPoint& Chunk : MaterializedChunks)
    {
        if (!Candidates.Contains(Chunk))
        {
            ToSleep.Add(Chunk);
        }
    }
    for (const FIntPoint& Chunk : ToSleep)
    {
        MaterializedChunks.Remove(Chunk);
        SetChunkResourcesActive(Chunk, false);

        // Земля ушла -- заглушки сущностей на ней тоже (найдено пользователем
        // 2026-09-03: они оставались висеть в мире). На границе симуляции их
        // отдельно снимает CatchUpActivatedChunks.
        DespawnChunkEntities(Chunk);
    }

    for (const FIntPoint& Chunk : Candidates)
    {
        if (!MaterializedChunks.Contains(Chunk))
        {
            // Страница -- до спавна: ростер из её дельт встаёт спящим и
            // просыпается здесь же (этап 8в).
            EnsureChunkPagesLoaded(Chunk);
            // Страница могла загрузиться раньше земли (активный чанк, телепорт)
            // -- высоты досчитываются до спавна (ревью этапа 8в).
            if (CellPageSize > 0)
            {
                ForEachChunkPage(Chunk, [this](FHerbalistCellPage& Page)
                {
                    if (Page.bLoaded && !Page.bHeightsComplete)
                    {
                        CacheCellHeightsForPage(Page);
                    }
                });
            }
            // Сначала в набор, потом спавн: спавн сам сверяется с набором.
            MaterializedChunks.Add(Chunk);
            SetChunkResourcesActive(Chunk, true);
        }
    }

    // Набор материализованных чанков пересчитан -- страницы без земли и без
    // активных чанков выгружаются (этап 8в, UnloadIdleCellPagesIfPending).
    bCellPageUnloadCheckPending = true;
}

const FGridCell* AGridWorldManager::GetCellConst(int32 X, int32 Y) const
{
    // Границы до индекса (ревью этапа 6): координата далеко за краем, в том
    // числе InvalidCell, переполняла бы int32 в GetCellIndex.
    if (!IsCellInGrid(X, Y))
    {
        return nullptr;
    }
    const FHerbalistCellPage* Page = FindCellPage(X, Y);
    if (!Page || !Page->bLoaded)
    {
        return nullptr;
    }
    const int32 LocalIndex = Page->GetLocalIndex(X, Y);
    return Page->Cells.IsValidIndex(LocalIndex) ? &Page->Cells[LocalIndex] : nullptr;
}

FGridCell* AGridWorldManager::GetCellByGridIndex(int32 GridIndex)
{
    return const_cast<FGridCell*>(static_cast<const AGridWorldManager*>(this)->GetCellByGridIndex(GridIndex));
}

const FGridCell* AGridWorldManager::GetCellByGridIndex(int32 GridIndex) const
{
    if (GridIndex < 0 || GridIndex >= GetGridCellCount())
    {
        return nullptr;
    }
    const FIntPoint GridMin = GetGridMinCell();
    return GetCellConst(GridMin.X + GridIndex % GridSizeX, GridMin.Y + GridIndex / GridSizeX);
}

void AGridWorldManager::CreateCellPages()
{
    // Повторная инициализация (InitializeCells вызывается и из Blueprint):
    // акторы клеток прежних страниц не должны остаться в мире без клетки,
    // которая могла бы их убрать (ревью этапа 8б).
    for (FHerbalistCellPage& OldPage : CellPages)
    {
        for (FGridCell& Cell : OldPage.Cells)
        {
            for (const TWeakObjectPtr<AHerbalistResourceActor>& ResourceActor : Cell.ResourceActors)
            {
                if (ResourceActor.IsValid() && ResourceActor->WasSpawnedByGrid())
                {
                    ResourceActor->Destroy();
                }
            }
            if (AHerbalistEntityActor* EntityActor = Cell.ManifestedEntityActor.Get())
            {
                EntityActor->Destroy();
            }
        }
    }
    CellPages.Reset();
    CellPageTableMin = FIntPoint::ZeroValue;
    CellPageTableSize = FIntPoint::ZeroValue;
    CellPageSize = 0;
    LoadedCellCount = 0;
    bCellHeightsCached = false;
    PinnedSitePages.Reset();
    if (GridSizeX <= 0 || GridSizeY <= 0)
    {
        return;
    }

    // Страница -- из разметки (ячейка стриминга в целых клетках); без разметки
    // -- одна страница во всю сетку, как единый массив до этапа 8.
    const FIntPoint GridMin = GetGridMinCell();
    const FIntPoint GridEnd = GridMin + FIntPoint(GridSizeX, GridSizeY);
    CellPageSize = ResolvedLayout.bValid ? FMath::Max(ResolvedLayout.PageSizeInCells, 0) : 0;
    if (CellPageSize <= 0)
    {
        CellPageTableSize = FIntPoint(1, 1);
        FHerbalistCellPage& Page = CellPages.AddDefaulted_GetRef();
        Page.MinCell = GridMin;
        Page.Size = FIntPoint(GridSizeX, GridSizeY);
    }
    else
    {
        CellPageTableMin = FIntPoint(HerbalistCore::FloorDivCoord(GridMin.X, CellPageSize), HerbalistCore::FloorDivCoord(GridMin.Y, CellPageSize));
        const FIntPoint TableMax(HerbalistCore::FloorDivCoord(GridEnd.X - 1, CellPageSize), HerbalistCore::FloorDivCoord(GridEnd.Y - 1, CellPageSize));
        CellPageTableSize = TableMax - CellPageTableMin + FIntPoint(1, 1);
        CellPages.SetNum(CellPageTableSize.X * CellPageTableSize.Y);

        // Страница -- три массива. При мелкой странице (ручной размер) их
        // накладные расходы сравнимы с самими клетками (ревью этапа 8б); порог --
        // впятеро больше страниц, чем у сетки-предела 4 млн клеток при странице
        // 14 клеток (20 тыс.).
        if (CellPages.Num() > 100000)
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Pages] %d страниц по %d клеток на сторону -- накладные расходы страниц велики, проверь размер страницы разметки"),
                CellPages.Num(), CellPageSize);
        }
        for (int32 PageY = 0; PageY < CellPageTableSize.Y; ++PageY)
        {
            for (int32 PageX = 0; PageX < CellPageTableSize.X; ++PageX)
            {
                FHerbalistCellPage& Page = CellPages[PageY * CellPageTableSize.X + PageX];
                const FIntPoint PageMin((CellPageTableMin.X + PageX) * CellPageSize, (CellPageTableMin.Y + PageY) * CellPageSize);
                // Разметка расширяет сетку до целых страниц; обрезка -- защита
                // сетки, изменённой после разметки (например из Blueprint).
                Page.MinCell = FIntPoint(FMath::Max(PageMin.X, GridMin.X), FMath::Max(PageMin.Y, GridMin.Y));
                const FIntPoint PageEnd(FMath::Min(PageMin.X + CellPageSize, GridEnd.X), FMath::Min(PageMin.Y + CellPageSize, GridEnd.Y));
                Page.Size = PageEnd - Page.MinCell;
            }
        }
    }

    for (FHerbalistCellPage& Page : CellPages)
    {
        Page.Cells.SetNum(Page.Size.X * Page.Size.Y);
        Page.bLoaded = true;
        LoadedCellCount += Page.Cells.Num();
    }
}

int32 AGridWorldManager::EnsureGridCoversSites(const TArray<FIntPoint>& Sites)
{
    // Места за убранными плитками ландшафта (решение пользователя 2026-09-13:
    // «они должны жить, даже будучи отстримленными»). Плитки убирают в
    // редакторе между сессиями, поэтому расширение нужно при загрузке сейва:
    // сетка растёт до целых страниц с местами, страницы мест собираются из
    // основы (регионы или фолбэк; земли нет) и закрепляются. Без разметки сетка
    // ручная, сейв другого размера отклонён раньше.
    if (CellPageSize <= 0 || CellPages.Num() == 0)
    {
        return 0;
    }

    FIntPoint TableMin = CellPageTableMin;
    FIntPoint TableMax = CellPageTableMin + CellPageTableSize - FIntPoint(1, 1);
    TSet<FIntPoint> SitePages;
    for (const FIntPoint& Site : Sites)
    {
        if (!HerbalistCore::IsValidCell(Site) || IsCellInGrid(Site.X, Site.Y))
        {
            continue;
        }
        const FIntPoint SitePage(HerbalistCore::FloorDivCoord(Site.X, CellPageSize), HerbalistCore::FloorDivCoord(Site.Y, CellPageSize));
        SitePages.Add(SitePage);
        TableMin = FIntPoint(FMath::Min(TableMin.X, SitePage.X), FMath::Min(TableMin.Y, SitePage.Y));
        TableMax = FIntPoint(FMath::Max(TableMax.X, SitePage.X), FMath::Max(TableMax.Y, SitePage.Y));
    }
    if (SitePages.Num() == 0)
    {
        return 0;
    }

    // Проверка заполнителя смотрит первую клетку чанка -- это верно, только
    // если чанк делит страницу (ревью 2026-09-13). Ручной размер чанка, не
    // делящий страницу, разметка лишь предупреждает; расширение с ним не строим.
    const int32 ChunkCells = GetChunkSizeInCells();
    if (ChunkCells <= 0 || CellPageSize % ChunkCells != 0)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] Чанк %d кл. не делит страницу %d кл. -- места за краем сетки не восстановлены"),
            ChunkCells, CellPageSize);
        return 0;
    }

    // С разметкой сетка -- целые страницы; сетку, изменённую после разметки
    // (обрезанные страницы), не расширяем.
    const FIntPoint OldGridMin = GetGridMinCell();
    const int32 OldSizeX = GridSizeX;
    if (OldGridMin != CellPageTableMin * CellPageSize || FIntPoint(GridSizeX, GridSizeY) != CellPageTableSize * CellPageSize)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] Сетка %dx%d от (%d, %d) не кратна страницам -- места за её краем не восстановлены"),
            GridSizeX, GridSizeY, OldGridMin.X, OldGridMin.Y);
        return 0;
    }

    // Предел сетки разметки (ревью): место за километры от ландшафта -- скорее
    // сейв другой карты с тем же отпечатком, чем убранная плитка, и прямоугольник
    // до него не должен съесть память. Считается в int64 до первой записи: из
    // битого сейва координаты бывают такими, что размер в клетках не влезает в
    // int32.
    const int64 NewCellsX = (static_cast<int64>(TableMax.X) - TableMin.X + 1) * CellPageSize;
    const int64 NewCellsY = (static_cast<int64>(TableMax.Y) - TableMin.Y + 1) * CellPageSize;
    if (NewCellsX * NewCellsY > FWorldLayoutSolver::MaxGridCells)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] Места за краем сетки потребовали бы %lldx%lld клеток (предел %lld) -- не восстановлены"),
            NewCellsX, NewCellsY, FWorldLayoutSolver::MaxGridCells);
        return 0;
    }
    const FIntPoint NewTableSize = TableMax - TableMin + FIntPoint(1, 1);
    const FIntPoint NewGridMin = TableMin * CellPageSize;
    const FIntPoint NewGridSize = NewTableSize * CellPageSize;
    const auto RemapIndex = [OldGridMin, OldSizeX, NewGridMin, NewSizeX = NewGridSize.X](int32 OldIndex)
    {
        const int32 X = OldGridMin.X + OldIndex % OldSizeX;
        const int32 Y = OldGridMin.Y + OldIndex / OldSizeX;
        return (Y - NewGridMin.Y) * NewSizeX + (X - NewGridMin.X);
    };

    // Линейные индексы клеток зависят от угла и ширины сетки.
    TSet<int32> RemappedDirty;
    RemappedDirty.Reserve(DirtyCellIndices.Num());
    for (int32 Index : DirtyCellIndices)
    {
        RemappedDirty.Add(RemapIndex(Index));
    }
    DirtyCellIndices = MoveTemp(RemappedDirty);

    const auto RemapKeys = [&RemapIndex](auto& Map)
    {
        std::remove_reference_t<decltype(Map)> Remapped;
        Remapped.Reserve(Map.Num());
        for (auto& Pair : Map)
        {
            Remapped.Add(RemapIndex(Pair.Key), MoveTemp(Pair.Value));
        }
        Map = MoveTemp(Remapped);
    };
    RemapKeys(UnloadedCellDeltas);
    RemapKeys(UnloadedCellRosters);
    RemapKeys(PendingRegrowthsOnLoad);

    TBitArray<> RemappedSeeded(false, NewGridSize.X * NewGridSize.Y);
    for (TConstSetBitIterator<> It(SeededCellMask); It; ++It)
    {
        RemappedSeeded[RemapIndex(It.GetIndex())] = true;
    }
    SeededCellMask = MoveTemp(RemappedSeeded);

    // Таблица страниц: прежние страницы переезжают вместе с клетками (буферы
    // клеток не копируются), новые -- пустые и выгруженные.
    TArray<FHerbalistCellPage> NewPages;
    NewPages.SetNum(NewTableSize.X * NewTableSize.Y);
    for (int32 PageY = 0; PageY < NewTableSize.Y; ++PageY)
    {
        for (int32 PageX = 0; PageX < NewTableSize.X; ++PageX)
        {
            const FIntPoint PageCoord(TableMin.X + PageX, TableMin.Y + PageY);
            FHerbalistCellPage& NewPage = NewPages[PageY * NewTableSize.X + PageX];
            const FIntPoint OldLocal = PageCoord - CellPageTableMin;
            if (OldLocal.X >= 0 && OldLocal.X < CellPageTableSize.X && OldLocal.Y >= 0 && OldLocal.Y < CellPageTableSize.Y)
            {
                NewPage = MoveTemp(CellPages[OldLocal.Y * CellPageTableSize.X + OldLocal.X]);
            }
            else
            {
                NewPage.MinCell = PageCoord * CellPageSize;
                NewPage.Size = FIntPoint(CellPageSize, CellPageSize);
            }
        }
    }
    CellPages = MoveTemp(NewPages);
    CellPageTableMin = TableMin;
    CellPageTableSize = NewTableSize;

    // Отпечаток разметки границ не содержит (решение 14) -- сейв остаётся
    // совместимым. Сводки ключуются глобальными чанками и от расширения не
    // устаревают: ключ кэша переносится на новый прямоугольник, иначе следующий
    // шаг графа пересобрал бы все выгруженные чанки из основы (ревью). Окно
    // карты встанет заново.
    const bool bSummaryCacheCurrent = ChunkSummaryChunkSize == GetChunkSizeInCells()
        && ChunkSummaryMinCell == OldGridMin && ChunkSummaryGridSize == FIntPoint(OldSizeX, GridSizeY);
    ResolvedLayout.MinCell = NewGridMin;
    ResolvedLayout.GridSize = NewGridSize;
    GridSizeX = NewGridSize.X;
    GridSizeY = NewGridSize.Y;
    bWorldStateWindowPlaced = false;
    if (bSummaryCacheCurrent)
    {
        ChunkSummaryMinCell = NewGridMin;
        ChunkSummaryGridSize = NewGridSize;
    }

    for (const FIntPoint& SitePage : SitePages)
    {
        const FIntPoint PageMinCell = SitePage * CellPageSize;
        PinnedSitePages.Add(PageMinCell);
        if (FHerbalistCellPage* Page = FindCellPage(PageMinCell.X, PageMinCell.Y); Page && !Page->bLoaded)
        {
            LoadCellPage(*Page);
        }
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Layout] Сетка расширена до %dx%d от (%d, %d): %d страниц мест за краем ландшафта закреплены"),
        GridSizeX, GridSizeY, NewGridMin.X, NewGridMin.Y, SitePages.Num());
    return SitePages.Num();
}

bool AGridWorldManager::IsCellInExtensionFiller(int32 X, int32 Y) const
{
    // Страница-заполнитель (ревью 2026-09-13): расширение растит сетку
    // прямоугольником, и между краем ландшафта и местом встают страницы без
    // земли и без мест. Их клетки -- не часть мира: сохранённые клетки там
    // отбрасываются, как за убранными плитками (решение 14), а сводки не входят
    // в биомный граф и отчёты.
    if (CellPageSize <= 0 || !IsCellInGrid(X, Y))
    {
        return false;
    }
    if (X >= LandscapeGridMinCell.X && X < LandscapeGridMinCell.X + LandscapeGridSize.X
        && Y >= LandscapeGridMinCell.Y && Y < LandscapeGridMinCell.Y + LandscapeGridSize.Y)
    {
        return false;
    }
    const FIntPoint PageMinCell(HerbalistCore::FloorDivCoord(X, CellPageSize) * CellPageSize, HerbalistCore::FloorDivCoord(Y, CellPageSize) * CellPageSize);
    return !PinnedSitePages.Contains(PageMinCell);
}

FHerbalistCellPage* AGridWorldManager::FindCellPage(int32 X, int32 Y)
{
    return const_cast<FHerbalistCellPage*>(static_cast<const AGridWorldManager*>(this)->FindCellPage(X, Y));
}

const FHerbalistCellPage* AGridWorldManager::FindCellPage(int32 X, int32 Y) const
{
    if (CellPages.Num() == 0)
    {
        return nullptr;
    }
    if (CellPageSize <= 0)
    {
        return &CellPages[0];
    }
    const int32 PageX = HerbalistCore::FloorDivCoord(X, CellPageSize) - CellPageTableMin.X;
    const int32 PageY = HerbalistCore::FloorDivCoord(Y, CellPageSize) - CellPageTableMin.Y;
    if (PageX < 0 || PageX >= CellPageTableSize.X || PageY < 0 || PageY >= CellPageTableSize.Y)
    {
        return nullptr;
    }
    return &CellPages[PageY * CellPageTableSize.X + PageX];
}

const FSavedCellState* AGridWorldManager::FindCellBaselineByGridIndex(int32 GridIndex) const
{
    if (GridIndex < 0 || GridIndex >= GetGridCellCount())
    {
        return nullptr;
    }
    const FIntPoint GridMin = GetGridMinCell();
    const int32 X = GridMin.X + GridIndex % GridSizeX;
    const int32 Y = GridMin.Y + GridIndex / GridSizeX;
    const FHerbalistCellPage* Page = FindCellPage(X, Y);
    if (!Page || !Page->bLoaded)
    {
        return nullptr;
    }
    const int32 LocalIndex = Page->GetLocalIndex(X, Y);
    return Page->Baselines.IsValidIndex(LocalIndex) ? &Page->Baselines[LocalIndex] : nullptr;
}

bool AGridWorldManager::GetCellBaselineCellForTests(int32 GridIndex, FIntPoint& OutCell) const
{
    const FSavedCellState* Baseline = FindCellBaselineByGridIndex(GridIndex);
    if (!Baseline)
    {
        return false;
    }
    OutCell = FIntPoint(Baseline->X, Baseline->Y);
    return true;
}

bool AGridWorldManager::GetCellPageBoundsForTests(int32 X, int32 Y, FIntPoint& OutMinCell, FIntPoint& OutSize) const
{
    const FHerbalistCellPage* Page = IsCellInGrid(X, Y) ? FindCellPage(X, Y) : nullptr;
    if (!Page)
    {
        return false;
    }
    OutMinCell = Page->MinCell;
    OutSize = Page->Size;
    return true;
}

// ============================================================================
// БИОМЫ
// ============================================================================

FHerbalistChunkSummary AGridWorldManager::BuildChunkSummary(const FIntPoint& Chunk) const
{
    FHerbalistChunkSummary Summary;
    const int32 ChunkSize = GetChunkSizeInCells();
    TOptional<FHerbalistCellBaseContext> UnloadedCellContext;
    const int32 MinX = Chunk.X * ChunkSize;
    const int32 MinY = Chunk.Y * ChunkSize;

    for (int32 Y = MinY; Y < MinY + ChunkSize; ++Y)
    {
        for (int32 X = MinX; X < MinX + ChunkSize; ++X)
        {
            FGridCell UnloadedCell;
            const FGridCell* Cell = GetCellConst(X, Y);
            if (!Cell && IsCellInGrid(X, Y))
            {
                // Клетка выгруженной страницы (этап 8в) -- основа плюс дельта.
                if (!UnloadedCellContext.IsSet())
                {
                    UnloadedCellContext = MakeCellBaseContext();
                }
                if (BuildUnloadedCell(X, Y, UnloadedCell, UnloadedCellContext.GetValue()))
                {
                    Cell = &UnloadedCell;
                }
            }
            if (!Cell)
            {
                continue;
            }
            ++Summary.CellCount;

            // Морок как ОТКЛОНЕНИЕ Distortion от дефолта биома (2026-09-07,
            // вторым заходом, симметрично Заряне ниже). Абсолютный уровень
            // здесь не годился по двум причинам, обе проверены:
            //  1) у поля не было якоря на природе биома -- Болото со своими
            //     0.70 медленно уезжало туда, куда скажет затухающее поле;
            //  2) при настоящей таблице биомов ветка Морока дёргала ВСЕ 400
            //     клеток каждый шаг (падал Save.BiomeInfluencesWithZeroFieldsStaySparse),
            //     потому что "ведро" всегда тянуло Distortion от дефолта к нулю.
            // На отклонениях покой даёт ровно ноль -- ни записи, ни дрейфа.
            //
            // Отдельно: НЕЛЬЗЯ было оставить абсолют и просто релаксировать поле
            // к дефолту биома (более дешёвая на вид альтернатива). Диффузия по
            // рёбрам (PropagateWaves) переносит АБСОЛЮТНЫЕ значения со скоростью
            // MorokLeak (0.15 за шаг = 0.75/с), что на два порядка сильнее
            // релаксации (0.01/с): соседние биомы просто усреднились бы, и
            // Болото перестало бы отличаться от Поймы. На отклонениях в покое
            // переносить нечего -- диффундирует только реальный избыток.
            const FRealState BiomeDefault = GetCellDefaultState(*Cell);

            const int32 BiomeIndex = static_cast<int32>(Cell->Biome);
            if (!ensureMsgf(BiomeIndex < HerbalistBiomeTypeCount, TEXT("EBiomeType %d вне HerbalistBiomeTypeCount"), BiomeIndex))
            {
                continue;
            }
            FHerbalistBiomeFieldSum& BiomeSum = Summary.Biomes[BiomeIndex];
            BiomeSum.MorokSum += Cell->State.Meta.Distortion - BiomeDefault.Meta.Distortion;

            // Заряна как ОТКЛОНЕНИЕ Stability от дефолта биома (2026-09-07,
            // выбор пользователя, вариант "а" из ROADMAP.md; было
            // `1 - Distortion`). Прежнее определение делало Заряну зеркалом
            // Морока, а не самостоятельной величиной, и ветка Заряны в
            // ApplyBiomeInfluences тянула Purity/Stability к f(Distortion)
            // безотносительно их собственных дефолтов: Тайга с природным
            // Purity 0.8 уезжала к 0.375 без единой внешней причины
            // (замер: 0.70 -> 0.55 за 300с, MATH_REFERENCE.md §6.2).
            //
            // Знаковое отклонение, не абсолютный уровень: ноль означает "биом
            // в своей природе", плюс/минус -- насколько его увели от неё. Тогда
            // затухание поля (UpdateMemories) означает возврат к дефолту биома,
            // а не сползание к нулю. Читается и применяется в ОДНОЙ системе
            // отсчёта (см. ApplyBiomeInfluences) -- ровно то, чего не хватало
            // сломанной правке 2026-09-07 по Distortion (двойной счёт).
            BiomeSum.ZaryanaSum += Cell->State.Meta.Stability - BiomeDefault.Meta.Stability;
            BiomeSum.PositionSum += GetCellWorldPositionFlat(Cell->X, Cell->Y);
            ++BiomeSum.CellCount;

            const float Distortion = Cell->State.Meta.Distortion;
            Summary.DegradingCount += Cell->Memory.bDegrading ? 1 : 0;
            Summary.DistortionSum += Distortion;
            Summary.DistortionMin = FMath::Min(Summary.DistortionMin, Distortion);
            Summary.DistortionMax = FMath::Max(Summary.DistortionMax, Distortion);
            if (!Cell->bIsWater)
            {
                ++Summary.LandCellCount;
                Summary.LandDistortionSum += Distortion;
            }
            Summary.DistanceWithHistorySum += HerbalistCore::Math::DistanceWithHistory(Cell->State, Cell->Memory.AverageCoherence);
        }
    }
    return Summary;
}

void AGridWorldManager::EnsureChunkSummaryCacheKey() const
{
    // Кэш собран под размер чанка и прямоугольник сетки (ревью этапа 7): без
    // разметки размер чанка читается из настроек и меняется на лету.
    const int32 ChunkSize = GetChunkSizeInCells();
    const FIntPoint GridMin = GetGridMinCell();
    const FIntPoint GridSize(GridSizeX, GridSizeY);
    if (ChunkSummaryChunkSize != ChunkSize || ChunkSummaryMinCell != GridMin || ChunkSummaryGridSize != GridSize)
    {
        ChunkSummaries.Reset();
        StaleChunkSummaries.Reset();
        ChunkSummaryChunkSize = ChunkSize;
        ChunkSummaryMinCell = GridMin;
        ChunkSummaryGridSize = GridSize;
    }
}

void AGridWorldManager::ForEachChunkSummary(TFunctionRef<void(const FHerbalistChunkSummary&)> Func) const
{
    check(IsInGameThread());
    if (!HasCellPages())
    {
        return;
    }

    // Func получает ссылку в ChunkSummaries: запрос сводок изнутри перестроил
    // бы таблицу под ней (ревью этапа 7).
    check(!bIteratingChunkSummaries);
    TGuardValue<bool> IterationGuard(bIteratingChunkSummaries, true);

    EnsureChunkSummaryCacheKey();

    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    GetGridChunkRange(MinChunk, MaxChunk);

    // Те же две ветки «живо всё», что у ForEachActiveCell: без центров
    // активности релаксация идёт по всей сетке, и кэш не нужен.
    const int32 Radius = GetActiveRadiusInChunks();
    const bool bAllLive = Radius < 0 || ActiveChunkCenters.Num() == 0;
    const TSet<FIntPoint> LiveChunks = bAllLive ? TSet<FIntPoint>() : ComputeChunksWithinRadius(ActiveChunkCenters, Radius);

    const int32 ChunkCells = GetChunkSizeInCells();
    for (int32 ChunkY = MinChunk.Y; ChunkY <= MaxChunk.Y; ++ChunkY)
    {
        for (int32 ChunkX = MinChunk.X; ChunkX <= MaxChunk.X; ++ChunkX)
        {
            const FIntPoint Chunk(ChunkX, ChunkY);
            // Страница-заполнитель расширения сетки -- не мир: её основа размыла
            // бы биомный граф и отчёты (ревью 2026-09-13). Чанк делит страницу,
            // поэтому хватает его первой клетки.
            if (IsCellInExtensionFiller(ChunkX * ChunkCells, ChunkY * ChunkCells))
            {
                continue;
            }
            // Чанк выгруженной страницы и без центров активности не живой (этап
            // 8в): у него последняя сводка, а не сборка основы на каждый запрос.
            if (bAllLive && IsChunkInLoadedPage(Chunk))
            {
                Func(BuildChunkSummary(Chunk));
                continue;
            }

            const FHerbalistChunkSummary* Cached = ChunkSummaries.Find(Chunk);
            if (!Cached || LiveChunks.Contains(Chunk) || StaleChunkSummaries.Contains(Chunk))
            {
                FHerbalistChunkSummary& Slot = ChunkSummaries.FindOrAdd(Chunk);
                Slot = BuildChunkSummary(Chunk);
                StaleChunkSummaries.Remove(Chunk);
                Cached = &Slot;
            }
            Func(*Cached);
        }
    }
}

TMap<FName, FHerbalistBiomeFieldSum> AGridWorldManager::GetBiomeFieldSums() const
{
    FHerbalistBiomeFieldSum Totals[HerbalistBiomeTypeCount];
    ForEachChunkSummary([&Totals](const FHerbalistChunkSummary& Summary)
    {
        for (int32 BiomeIndex = 0; BiomeIndex < HerbalistBiomeTypeCount; ++BiomeIndex)
        {
            const FHerbalistBiomeFieldSum& Part = Summary.Biomes[BiomeIndex];
            FHerbalistBiomeFieldSum& Total = Totals[BiomeIndex];
            Total.MorokSum += Part.MorokSum;
            Total.ZaryanaSum += Part.ZaryanaSum;
            Total.PositionSum += Part.PositionSum;
            Total.CellCount += Part.CellCount;
        }
    });

    // Наружу -- только биомы, у которых есть клетки, как у прежнего обхода.
    TMap<FName, FHerbalistBiomeFieldSum> Sums;
    for (int32 BiomeIndex = 0; BiomeIndex < HerbalistBiomeTypeCount; ++BiomeIndex)
    {
        if (Totals[BiomeIndex].CellCount > 0)
        {
            Sums.Add(FBiomeDefaults::BiomeTypeToName(static_cast<EBiomeType>(BiomeIndex)), Totals[BiomeIndex]);
        }
    }
    return Sums;
}

TMap<FName, FVector> AGridWorldManager::GetBiomeCenters() const
{
    TMap<FName, FVector> Centers;
    for (const TPair<FName, FHerbalistBiomeFieldSum>& Pair : GetBiomeFieldSums())
    {
        if (Pair.Value.CellCount > 0)
        {
            Centers.Add(Pair.Key, Pair.Value.PositionSum / Pair.Value.CellCount);
        }
    }
    return Centers;
}

void AGridWorldManager::ApplyBiomeInfluences(const TMap<FName, float>& MorokFields,
                                             const TMap<FName, float>& ZaryanaFields,
                                             float GlobalScale,
                                             float DeltaTime)
{
    // Единственный писатель в состояние клеток — ApplyStateDelta(). BiomeGraph не
    // мутирует Cells напрямую, а собирает Delta.TargetStateNudges, точно как Pipeline
    // собирает Delta.WorldChanges — так FStateDelta действительно единственный
    // источник изменений мира (Single Writer, Causal Execution Spec).
    FStateDelta Delta;

    // Стриминг сетки (2026-09-03): поля биом-графа применяются только к
    // активным клеткам. Сам граф считается всегда и целиком — он живёт на
    // уровне узлов, а не клеток, поэтому дальний мир продолжает меняться;
    // неактивные клетки просто не получают его вклад, пока не станут
    // активными (догон — юнит 2). ForEachActiveCell (не полный проход +
    // IsCellActive-фильтр внутри) — правка того же дня: при активном
    // радиусе в несколько чанков полный skip-scan 250 000 клеток каждый
    // тик обходился на два порядка дороже, чем реальная работа под ним.
    const UHerbalistSettings* Settings = GetHerbalistSettings();

    ForEachActiveCell([&](FGridCell& Cell)
    {
        // Испорченный полюс бистабильности управляет Distortion/Corruption/
        // Purity/Stability САМ (02_GDD/12_Biome_Change.md §12.10: "выход
        // только прямым действием игрока") — если бы "дырявое ведро" ниже
        // продолжало тянуть значение к своему равновесию поверх этого, оно
        // подрывало бы ровно то свойство, ради которого полюс существует.
        // Тот же принцип, что уже исключает bEternallyPure из этой функции.
        if (Cell.Memory.bDegrading) return;

        FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell.Biome);
        FRealState NewTarget = Cell.TargetState;
        bool bChanged = false;

        // 1. Влияние Морока на Distortion -- "дырявое ведро" (2026-09-07,
        // прямое решение пользователя после найденного вживую бага: раньше
        // это было непрерывное сложение MorokField*0.1 каждый шаг, без
        // единого вычитания -- растило Distortion к потолку 1.0 без единой
        // внешней причины, вопреки канону "Distortion — устойчивый уровень
        // биома, не накопитель", §12.10). ApplyFieldsToGrid всегда кладёт
        // запись в MorokFields для КАЖДОГО узла графа, включая нулевые поля —
        // значит "if (MorokField)" (есть ли запись) равносильно "всегда", не
        // "поле реально что-то сдвигает". Правка: сравниваем итог с уже
        // стоящим TargetState, метим грязной только при реальном изменении
        // (AUDIT_AND_REFACTORING_PLAN.md §7.1).
        //
        // Декей ведёт К САМОМУ MorokField, НЕ к "BiomeDefault + MorokField"
        // (первая версия этой правки, 2026-09-07, оказалась битой -- найдено
        // по PIE-логу пользователя: рост Distortion не остановился вовсе,
        // той же скоростью, что и до фикса). Причина: MorokField УЖЕ сам по
        // себе на той же шкале, что Distortion -- RecalculateFieldsFromGrid
        // (BiomeGraphSubsystem.cpp) тянет его к среднему Distortion клеток
        // биома. Добавление BiomeDefault ПОВЕРХ этого было двойным счётом
        // одной и той же величины: MorokField сходится к ~BiomeDefault, а
        // цель клетки тогда становилась ~2×BiomeDefault -- и этот раздутый
        // результат немедленно поднимал средний Distortion биома, откуда
        // RecalculateFieldsFromGrid тянул MorokField ещё выше -- тот же
        // класс замкнутого контура с положительной обратной связью, что
        // вчера чинился в PropagateWaves, просто на соседнем стыке
        // конвейера. Цель без BiomeDefault -- честная неподвижная точка:
        // если Distortion==MorokField у всех клеток биома разом, среднее
        // по сетке равно MorokField, и RecalculateFieldsFromGrid ничего не
        // сдвигает -- рост возможен только если что-то РЕАЛЬНОЕ (контагион,
        // варка) сдвинет хоть одну клетку первым.
        const float* MorokField = MorokFields.Find(BiomeID);
        if (MorokField)
        {
            float PushRate = Settings ? Settings->MorokDistortionPushRate : 0.01f;
            const float DecayRate = Settings ? Settings->MorokDistortionDecayRate : 0.01f;

            // Эффект 3, Каменное (Стрибог, §15.5): "не пускает Морок" — глушит
            // вклад MorokField в локальный Distortion на (1 − 0.4×Restoration),
            // только в радиусе капища.
            const FShrine* DominantShrine = Shrines.Num() > 0
                ? HerbalistCore::Shrine::FindDominantShrine(FIntPoint(Cell.X, Cell.Y), Shrines, GetCellRadius(Settings ? Settings->ShrineInfluenceRadiusMeters : 30.0f))
                : nullptr;
            if (DominantShrine && DominantShrine->Type == EShrineType::Stone && DominantShrine->Restoration > 0.0f)
            {
                const float Dampening = Settings ? Settings->ShrineStoneMorokDampening : 0.4f;
                PushRate *= (1.0f - FMath::Clamp(Dampening * DominantShrine->Restoration, 0.0f, 1.0f));
            }

            // Отклонение от дефолта биома, а не абсолют (2026-09-07, второй
            // заход): в покое отклонение равно нулю, шаг равен нулю, записи
            // нет -- и Distortion остаётся ровно на природе своего биома.
            // Раньше "ведро" тянуло абсолютный Distortion к затухающему полю,
            // то есть к нулю, и заодно метило грязными все 400 клеток каждый
            // шаг при настоящих дефолтах биомов.
            const FRealState MorokBiomeDefault = GetCellDefaultState(Cell);
            const float DistortionDeviation = NewTarget.Meta.Distortion - MorokBiomeDefault.Meta.Distortion;
            const float NewDistortionDeviation = DistortionDeviation
                + (*MorokField * PushRate * GlobalScale - DecayRate * DistortionDeviation) * DeltaTime;
            const float NewDistortion = FMath::Clamp(MorokBiomeDefault.Meta.Distortion + NewDistortionDeviation, 0.f, 1.f);
            if (!FMath::IsNearlyEqual(NewDistortion, NewTarget.Meta.Distortion, KINDA_SMALL_NUMBER))
            {
                NewTarget.Meta.Distortion = NewDistortion;
                bChanged = true;
            }
        }

        // 2. Влияние Заряны на Stability/Purity -- "дырявое ведро" по
        // ОТКЛОНЕНИЮ от дефолта биома (2026-09-07, выбор пользователя,
        // вариант "а"). ZaryanaField теперь тоже отклонение
        // (BuildChunkSummary выше), то есть читается и применяется в одной
        // системе отсчёта -- двойного счёта, погубившего аналогичную
        // правку по Distortion, здесь нет.
        //
        // Равновесие: StabilityDeviation -> ZaryanaField (вес 1.0),
        // PurityDeviation -> 0.5*ZaryanaField (тот же относительный вес,
        // что был до правки). Затухание ZaryanaField в ноль означает
        // возврат обеих осей к дефолтам СВОЕГО биома -- Тайга остаётся
        // Тайгой, а не уезжает к f(Distortion), как было раньше.
        const float* ZaryanaField = ZaryanaFields.Find(BiomeID);
        if (ZaryanaField)
        {
            const float PushRate = Settings ? Settings->ZaryanaEffectPushRate : 0.01f;
            const float DecayRate = Settings ? Settings->ZaryanaEffectDecayRate : 0.01f;

            const FRealState BiomeDefault = GetCellDefaultState(Cell);

            const float StabilityDeviation = NewTarget.Meta.Stability - BiomeDefault.Meta.Stability;
            const float NewStabilityDeviation = StabilityDeviation
                + (*ZaryanaField * PushRate * GlobalScale - DecayRate * StabilityDeviation) * DeltaTime;
            const float NewStability = FMath::Clamp(BiomeDefault.Meta.Stability + NewStabilityDeviation, 0.f, 1.f);

            const float PurityDeviation = NewTarget.Meta.Purity - BiomeDefault.Meta.Purity;
            const float NewPurityDeviation = PurityDeviation
                + (*ZaryanaField * PushRate * 0.5f * GlobalScale - DecayRate * PurityDeviation) * DeltaTime;
            const float NewPurity = FMath::Clamp(BiomeDefault.Meta.Purity + NewPurityDeviation, 0.f, 1.f);

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
    });

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

#if WITH_EDITORONLY_DATA
    // Менеджер держит симуляцию всего мира и не должен выгружаться вместе со
    // своей ячейкой стриминга, когда игрок уходит (найдено ревью разметки
    // мира, 2026-09-12). Размещённый на L_TestDev экземпляр значение не
    // переопределяет -- подхватит новое значение по умолчанию.
    bIsSpatiallyLoaded = false;
#endif
}

void AGridWorldManager::BeginPlay()
{
    Super::BeginPlay();
    WorldRNG.Initialize(RngBaseSeed);

    // Разметка мира -- до клеток: она задаёт их размер, число и начало.
    InitializeWorldLayoutForPlay();

    // Слоты ресурсов карты (этап 5б) -- до заселения клеток. Только в игровом
    // мире: автотесты в редакторном мире ставят свои через SetResourceSlots и
    // не должны подхватывать настоящие слоты открытой карты.
    if (GetWorld() && GetWorld()->IsGameWorld() && !ResourceSlotsAsset)
    {
        const FString SlotsPackage = HerbalistResourceSlots::AssetPackagePathForMap(GetWorld()->GetOutermost()->GetName());
        const FString SlotsObject = SlotsPackage + TEXT(".") + FPackageName::GetShortName(SlotsPackage);
        if (UHerbalistResourceSlots* Loaded = LoadObject<UHerbalistResourceSlots>(nullptr, *SlotsObject, nullptr, LOAD_NoWarn | LOAD_Quiet))
        {
            SetResourceSlots(Loaded);
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Slots] Слоты ресурсов %s: %d"), *SlotsPackage, Loaded->CountSlots());
        }
    }

    if (!HasCellPages())
    {
        InitializeCells();
    }

    // Автоснимок GetGridCorruptionReport() (2026-09-06) -- см. довод у
    // GridCorruptionReportTimerHandle в заголовке. GridCorruptionReportIntervalSeconds<=0
    // выключает его совсем (EditAnywhere -- на случай, если периодический
    // лог когда-нибудь понадобится реже/чаще или не понадобится вовсе).
    if (GridCorruptionReportIntervalSeconds > 0.0f)
    {
        GetWorldTimerManager().SetTimer(GridCorruptionReportTimerHandle, [this]()
        {
            UE_LOG(LogHerbalistWorld, Log, TEXT("Grid corruption (auto): %s"), *GetGridCorruptionReport());
        }, GridCorruptionReportIntervalSeconds, true);
    }

    // Карта состояния мира в текстуру (2026-09-07, "план A") -- довод в
    // шапке GridWorldManagerWorldStateMap.cpp, обоснование периода у
    // WorldStateMapUpdateIntervalSeconds в заголовке. Первая выгрузка --
    // сразу, не через период: иначе первую секунду после загрузки уровня
    // материалы читали бы пустую (чёрную) карту, то есть показывали бы
    // нетронутый мир там, где он на самом деле уже испорчен.
    if (WorldStateMapUpdateIntervalSeconds > 0.0f)
    {
        UpdateWorldStateMap();
        GetWorldTimerManager().SetTimer(WorldStateMapTimerHandle, [this]()
        {
            UpdateWorldStateMap();
        }, WorldStateMapUpdateIntervalSeconds, true);
    }
}

void AGridWorldManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(GridCorruptionReportTimerHandle);
    GetWorldTimerManager().ClearTimer(WorldStateMapTimerHandle);
    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МИРА
// ============================================================================

namespace
{
    // Взвешенная сумма состояний. Доли BiomeWeights в сумме дают 1, а смесь
    // одного биома с долей 1 совпадает с его состоянием точно.
    void AddWeightedCellState(FRealState& Sum, const FRealState& State, float Weight)
    {
        Sum.Magnitude += State.Magnitude * Weight;
        Sum.Direction.Body += State.Direction.Body * Weight;
        Sum.Direction.Mind += State.Direction.Mind * Weight;
        Sum.Direction.Spirit += State.Direction.Spirit * Weight;
        Sum.Direction.Nature += State.Direction.Nature * Weight;
        Sum.Meta.Distortion += State.Meta.Distortion * Weight;
        Sum.Meta.Stability += State.Meta.Stability * Weight;
        Sum.Meta.Purity += State.Meta.Purity * Weight;
        Sum.Meta.Potency += State.Meta.Potency * Weight;
        Sum.Meta.Resonance += State.Meta.Resonance * Weight;
        Sum.Meta.Corruption += State.Meta.Corruption * Weight;
    }
}

FName AGridWorldManager::RollWaterTypeForCell(const FGridCell& Cell, const UWaterTypeRegistrySubsystem* WaterSubsystem) const
{
    return RollWaterTypeForBiome(Cell.X, Cell.Y, Cell.Biome, WaterSubsystem);
}

FName AGridWorldManager::RollWaterTypeForBiome(int32 X, int32 Y, EBiomeType Biome, const UWaterTypeRegistrySubsystem* WaterSubsystem) const
{
    // Поток клетки, не общий WorldRNG (этап 4 разметки мира): тип воды не
    // зависит от того, сколько клеток и в каком порядке залито раньше. У всех
    // биомов клетки поток один и тот же.
    if (!WaterSubsystem)
    {
        return NAME_None;
    }
    FRandomStream WaterRng = MakeCellRandomStream(X, Y, FWorldLayoutSolver::ECellRandomPurpose::WaterType);
    return WaterSubsystem->GetRandomWaterType(Biome, WaterRng);
}

FRealState AGridWorldManager::RollWaterStateForCell(const FGridCell& Cell, const UWaterTypeRegistrySubsystem* WaterSubsystem) const
{
    // Решение пользователя 2026-09-13: вода берёт воду биома, в котором лежит,
    // а на границах биомов смешивается. Состояние -- смесь состояний типов воды
    // биомов клетки по долям BiomeWeights; у клетки без долей (блочный фолбэк)
    // -- вода одного Cell.Biome.
    const auto WaterStateForBiome = [this, &Cell, WaterSubsystem](EBiomeType Biome)
    {
        FRealState State = FBiomeDefaults::GetDefaultWaterState(Biome);
        if (WaterSubsystem)
        {
            if (const FWaterTypeRow* WaterRow = WaterSubsystem->GetWaterType(RollWaterTypeForBiome(Cell.X, Cell.Y, Biome, WaterSubsystem)))
            {
                State.Meta.Purity      = WaterRow->BasePurity;
                State.Meta.Distortion  = WaterRow->BaseDistortion;
                State.Meta.Stability   = WaterRow->BaseStability;
                State.Meta.Potency     = WaterRow->BasePotency;
                State.Meta.Corruption  = WaterRow->BaseCorruption;
            }
        }
        return State;
    };

    if (Cell.BiomeWeights.Num() == 0)
    {
        return WaterStateForBiome(Cell.Biome);
    }
    FRealState Mixed;
    for (const FBiomeWeightEntry& Entry : Cell.BiomeWeights)
    {
        AddWeightedCellState(Mixed, WaterStateForBiome(Entry.Biome), Entry.Weight);
    }
    return Mixed;
}

FRealState AGridWorldManager::GetCellDefaultState(const FGridCell& Cell)
{
    // Умолчание, к которому клетку тянут Морок, Заряна и выход из испорченного
    // полюса. У суши -- умолчание доминирующего биома. У воды -- смесь по тем же
    // долям, что при заливке, но из умолчаний воды биомов: иначе вода на стыке
    // сползала бы к воде доминирующего биома. Без реестра воды совпадает с
    // заливкой точно; с реестром заливка берёт величины строк типа воды, и
    // клетка тянется от них к умолчаниям -- так было и до смеси (ревью 2026-09-13).
    if (!Cell.bIsWater)
    {
        return FBiomeDefaults::GetDefaultState(Cell.Biome);
    }
    if (Cell.BiomeWeights.Num() == 0)
    {
        return FBiomeDefaults::GetDefaultWaterState(Cell.Biome);
    }
    FRealState Mixed;
    for (const FBiomeWeightEntry& Entry : Cell.BiomeWeights)
    {
        AddWeightedCellState(Mixed, FBiomeDefaults::GetDefaultWaterState(Entry.Biome), Entry.Weight);
    }
    return Mixed;
}

void AGridWorldManager::InitializeCells()
{
    const int32 TotalCells = GetGridCellCount();
    CreateCellPages();
    FallbackBiomeBlocksX = GridSizeX / HerbalistFallbackBiomeBlockCells;
    LandscapeGridMinCell = GetGridMinCell();
    LandscapeGridSize = FIntPoint(GridSizeX, GridSizeY);
    if (TotalCells == 0)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("InitializeCells: сетка %d x %d пуста -- клеток нет"), GridSizeX, GridSizeY);
        return;
    }

    // Точка отсчёта простоя чанков (2026-09-03, стриминг): чанк, который
    // игрок не посещал ни разу, простаивал именно с этого момента.
    GridInitGameClock = GameClockSeconds;
    ChunkLastSimulatedGameTime.Reset();
    ActiveChunks.Reset();
    PreviousActiveChunks.Reset();

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    // Заливка водой и основа клетки -- члены менеджера (ApplyWaterToCell,
    // BuildCellBase, этап 8в): те же функции собирают страницу при загрузке.

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

    // Регионы воды (2026-09-02, прямой запрос пользователя) -- с 2026-09-13
    // единственный источник воды (решение пользователя: пятна воды убраны).
    // Вода побеждает всегда (вес 1, не размешивается с земляными регионами)
    // для флага bIsWater, но НЕ подменяет Cell.Biome: тот резолвится из
    // земляных регионов, а вода берёт воду биомов клетки
    // (RollWaterStateForCell). Список нужен и основе страницы при загрузке.
    CachedWaterRegions.Reset();
    for (TActorIterator<AWaterRegionVolume> It(GetWorld()); It; ++It)
    {
        AWaterRegionVolume* Region = *It;
        if (!Region) continue;
        Region->UpdateCachedPoints();
        CachedWaterRegions.Add(Region);
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
    // вместо краха/единственного дефолтного биома на пробелах. Сама формула --
    // в BuildCellBase.
    const FHerbalistCellBaseContext BaseContext = MakeCellBaseContext();
    int32 FallbackCellCount = 0;
    int32 WaterCellCount = 0;

    // Координаты клеток -- глобальные, от начала сетки World Partition (этап 6
    // разметки мира); X, Y цикла -- локальный индекс в массиве.
    const FIntPoint GridMin = GetGridMinCell();
    for (int32 Y = 0; Y < GridSizeY; ++Y)
    {
        for (int32 X = 0; X < GridSizeX; ++X)
        {
            int32 Index = Y * GridSizeX + X;
            FGridCell& Cell = *GetCellByGridIndex(Index);

            // Основа клетки, вода из регионов воды включительно -- та же
            // функция, что собирает страницу при загрузке (этап 8в).
            if (!BuildCellBase(GridMin.X + X, GridMin.Y + Y, Cell, BaseContext))
            {
                ++FallbackCellCount;
            }
            WaterCellCount += Cell.bIsWater ? 1 : 0;
        }
    }

    // Сколько воды дали регионы воды (2026-09-14, по PIE-логу пользователя):
    // без этой строки из лога не видно, заработал ли поставленный регион.
    UE_LOG(LogHerbalistWorld, Log, TEXT("InitializeCells: регионов воды %d, водных клеток %d"), CachedWaterRegions.Num(), WaterCellCount);

    if (BiomeRegions.Num() > 0 && FallbackCellCount > 0)
    {
        // Формулировка «взят блочный фолбэк» читалась как безобидная
        // информация, хотя следствие тяжёлое: SpawnResourcesInCell и
        // SeedTestLandmarks выходят на таких клетках сразу
        // (IsCellClaimedByBiomeRegion), то есть там НЕЧЕГО СОБИРАТЬ и
        // сбор молча ничего не делает. Пишем следствие прямо.
        UE_LOG(LogHerbalistWorld, Warning, TEXT("InitializeCells: %d/%d клеток (%.1f%%) вне всех ABiomeRegionVolume -- блочный фолбэк даёт им биом для математики, но НЕ контент: ресурсы и хозяева мест там не появятся, собирать нечего. Расширь регионы или добавь новые."),
            FallbackCellCount, TotalCells, TotalCells > 0 ? 100.0f * FallbackCellCount / TotalCells : 0.0f);
    }

    // Дельты, засеянные клетки и отложенные отрастания прошлой сетки к этой не
    // относятся (этап 8в).
    UnloadedCellDeltas.Reset();
    SeededCellMask.Init(false, TotalCells);
    UnloadedCellRosters.Reset();
    PendingRegrowthsOnLoad.Reset();

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
    // стриминге -- заселение всей сетки сразу, из потоков клеток (этап 4
    // разметки мира), общий WorldRNG не расходуется.
    {
        const bool bStreamingEnabled = GetActiveRadiusInChunks() >= 0;
        if (!bStreamingEnabled)
        {
            for (FGridCell& Cell : GetCellsInGridOrder())
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

    // Точки интереса (§4, 2026-09-06) — детерминированная расстановка общим
    // WorldRNG, тем же, что хозяева мест и якоря выше (тип воды и ресурсы --
    // потоки клеток); ДО снимков страниц
    // ниже, тот же довод: baseline должен увидеть уже финальную, а не
    // частично засеянную сетку (курганы/Тотем/Светлояр/Горюч-камень/Соловей
    // сами по себе не трогают Cell.State при севе, но порядок здесь общий
    // принцип для всего в этой функции). SeedPointsOfInterest вызывает
    // SeedKurganSites первым же шагом (POITypes.h за доводом каркаса).
    SeedPointsOfInterest();

    // Baseline на клетку (аудит 2026-09-05, решение пользователя: полноценный
    // откат при загрузке вместо тихого игнорирования пост-сейвовых правок).
    // Снимаем ПРЯМО ЗДЕСЬ -- после биома/воды/высот/начального ростера
    // ресурсов (или его отсутствия при включённом стриминге), но ДО
    // SetActorTickEnabled(true) ниже: ни один тик симуляции, ни одно
    // действие игрока ещё не могли ничего сдвинуть. SeedTestLandmarks/
    // SeedLegendaryAnchors выше не трогают Cell.ManifestedEntityID (это
    // поле пишет только UpdateEntityManifestations, тикового происхождения,
    // см. GridWorldManagerEntities.cpp) -- порядок относительно них не важен.
    for (FHerbalistCellPage& Page : CellPages)
    {
        Page.Baselines.Reset(Page.Cells.Num());
        for (const FGridCell& Cell : Page.Cells)
        {
            Page.Baselines.Add(CaptureCellState(Cell));
        }
    }

    // Клетки созданы заново -- сводки чанков считаются заново (этап 7).
    InvalidateAllChunkSummaries();

    SetActorTickEnabled(true);
}

// ============================================================================
// РЕСУРСЫ
// ============================================================================

FHerbalistCellBaseContext AGridWorldManager::MakeCellBaseContext() const
{
    FHerbalistCellBaseContext Context;
    Context.AllBiomes = FBiomeDefaults::GetAllBiomeTypes();
    if (Context.AllBiomes.Num() == 0)
    {
        Context.AllBiomes = {
            EBiomeType::Tundra, EBiomeType::Taiga, EBiomeType::MixedForest,
            EBiomeType::BroadleafForest, EBiomeType::ForestSteppe,
            EBiomeType::Steppe, EBiomeType::Floodplain, EBiomeType::Bog
        };
    }
    const UGameInstance* GameInstance = GetGameInstance();
    Context.WaterSubsystem = GameInstance ? GameInstance->GetSubsystem<UWaterTypeRegistrySubsystem>() : nullptr;
    Context.BlocksX = FallbackBiomeBlocksX > 0 ? FallbackBiomeBlocksX : GridSizeX / HerbalistFallbackBiomeBlockCells;
    return Context;
}

void AGridWorldManager::ApplyWaterToCell(FGridCell& Cell, const UWaterTypeRegistrySubsystem* WaterSubsystem) const
{
    // Общая точка заливки клетки водой -- регионы воды при старте и при
    // загрузке страницы (2026-09-02, членом менеджера с этапа 8в). Читает уже
    // выставленные Cell.Biome и доли BiomeWeights: тип воды для сбора -- от
    // доминирующего биома, состояние -- смесь воды биомов клетки по долям
    // (решение пользователя 2026-09-13).
    Cell.bIsWater = true;
    Cell.WaterTypeID = RollWaterTypeForCell(Cell, WaterSubsystem);
    const FRealState WaterState = RollWaterStateForCell(Cell, WaterSubsystem);
    Cell.State = WaterState;
    Cell.TargetState = WaterState;
    Cell.HarvestStress = 0.0f;
    Cell.ResourceActors.Empty();
}

ABiomeRegionVolume* AGridWorldManager::BuildCellBase(int32 X, int32 Y, FGridCell& OutCell, const FHerbalistCellBaseContext& Context) const
{
    // Какие регионы содержат клетку -- равная доля на каждый (0.5/0.5 на двух,
    // 1/3 на трёх и т.д., без авторского "усиления" региона -- вертикальный
    // срез v1, прямое решение пользователя). MatchingRegions -- тот же индекс,
    // что и BiomeWeights. Регионы не грузятся пространственно (этап 5), и
    // список тот же, что при старте.
    OutCell.BiomeWeights.Reset();
    TArray<ABiomeRegionVolume*, TInlineAllocator<4>> MatchingRegions;
    const FVector CellWorldPos = GetCellWorldPositionFlat(X, Y);
    for (const TWeakObjectPtr<ABiomeRegionVolume>& WeakRegion : CachedBiomeRegions)
    {
        ABiomeRegionVolume* Region = WeakRegion.Get();
        if (Region && Region->IsPointInside(CellWorldPos))
        {
            OutCell.BiomeWeights.Add(FBiomeWeightEntry{ Region->Biome, 1.0f });
            MatchingRegions.Add(Region);
        }
    }

    EBiomeType Biome = EBiomeType::Tundra;
    ABiomeRegionVolume* ClaimingRegion = nullptr;
    if (OutCell.BiomeWeights.Num() > 0)
    {
        const float Share = 1.0f / OutCell.BiomeWeights.Num();
        for (FBiomeWeightEntry& Entry : OutCell.BiomeWeights)
        {
            Entry.Weight = Share;
        }

        // Доминанта -- наибольший вес; при точном равенстве (v1: у равных
        // долей ВСЕГДА равенство) -- меньший порядковый номер EBiomeType. Не
        // "первый по порядку акторов" -- порядок TActorIterator зависит от
        // порядка в Outliner/пересохранения уровня, скрытая невоспроизводимая
        // зависимость, которой у старой блочной формулы не было.
        int32 BestIndex = 0;
        Biome = OutCell.BiomeWeights[0].Biome;
        float BestWeight = OutCell.BiomeWeights[0].Weight;
        for (int32 Index = 1; Index < OutCell.BiomeWeights.Num(); ++Index)
        {
            const FBiomeWeightEntry& Entry = OutCell.BiomeWeights[Index];
            const bool bStrictlyBetter = Entry.Weight > BestWeight + KINDA_SMALL_NUMBER;
            const bool bTieBrokenByOrdinal = FMath::IsNearlyEqual(Entry.Weight, BestWeight, KINDA_SMALL_NUMBER)
                && static_cast<uint8>(Entry.Biome) < static_cast<uint8>(Biome);
            if (bStrictlyBetter || bTieBrokenByOrdinal)
            {
                Biome = Entry.Biome;
                BestWeight = Entry.Weight;
                BestIndex = Index;
            }
        }
        ClaimingRegion = MatchingRegions[BestIndex];
    }
    else
    {
        // Блоки 5 x 5 -- от глобальной координаты (ревью этапа 6): при
        // расширении ландшафта сетка начинается с другой клетки, а блок клетки
        // остаётся прежним. Деление с округлением вниз -- у клеток западнее
        // начала сетки World Partition координаты отрицательные.
        const int32 BlockSize = HerbalistFallbackBiomeBlockCells;
        const int32 BlockX = HerbalistCore::FloorDivCoord(X, BlockSize);
        const int32 BlockY = HerbalistCore::FloorDivCoord(Y, BlockSize);
        const int32 BiomeCount = Context.AllBiomes.Num();
        Biome = Context.AllBiomes[((BlockY * Context.BlocksX + BlockX) % BiomeCount + BiomeCount) % BiomeCount];
    }

    OutCell.Biome         = Biome;
    OutCell.State         = FBiomeDefaults::GetDefaultState(Biome);
    OutCell.TargetState   = OutCell.State;
    OutCell.Environment   = FBiomeDefaults::GetDefaultEnvironment(Biome);
    OutCell.Memory        = FMemoryState();
    OutCell.X             = X;
    OutCell.Y             = Y;
    OutCell.HarvestStress = 0.0f;
    OutCell.bIsWater      = false;
    OutCell.WaterTypeID   = NAME_None;

    // Регион воды заливает клетку поверх биома. Вода -- только из регионов
    // воды (решение пользователя 2026-09-13, пятна воды убраны).
    for (const TWeakObjectPtr<AWaterRegionVolume>& WeakWaterRegion : CachedWaterRegions)
    {
        const AWaterRegionVolume* WaterRegion = WeakWaterRegion.Get();
        if (WaterRegion && WaterRegion->IsPointInside(CellWorldPos))
        {
            ApplyWaterToCell(OutCell, Context.WaterSubsystem);
            break;
        }
    }
    return ClaimingRegion;
}

bool AGridWorldManager::BuildUnloadedCell(int32 X, int32 Y, FGridCell& OutCell, const FHerbalistCellBaseContext& Context) const
{
    if (!IsCellInGrid(X, Y))
    {
        return false;
    }
    const FHerbalistCellPage* Page = FindCellPage(X, Y);
    if (!Page || Page->bLoaded)
    {
        return false;
    }
    BuildCellBase(X, Y, OutCell, Context);
    if (const FSavedCellState* Delta = UnloadedCellDeltas.Find(GetCellIndex(X, Y)))
    {
        CopySavedCellFields(OutCell, *Delta);
        OutCell.bResourcesSeeded = Delta->bResourcesSeeded;
    }
    return true;
}

void AGridWorldManager::CacheCellHeightsForPage(FHerbalistCellPage& Page)
{
    // Страница грузится вслед за землёй под ней, поэтому высоты читаются уже
    // по загруженному ландшафту (этап 8в; раньше дальние клетки получали 0).
    Page.Heights.Reset();
    Page.Heights.SetNumZeroed(Page.Cells.Num());
    // Без ландшафта нули -- правда; с ландшафтом высота известна, только когда
    // компонент под клеткой загружен (ревью этапа 8в).
    Page.bHeightsComplete = true;
    if (CachedLandscape)
    {
        for (int32 LocalY = 0; LocalY < Page.Size.Y; ++LocalY)
        {
            for (int32 LocalX = 0; LocalX < Page.Size.X; ++LocalX)
            {
                FVector WorldPoint = GetCellWorldPositionFlat(Page.MinCell.X + LocalX, Page.MinCell.Y + LocalY);
                WorldPoint.Z = 0.f;
                const TOptional<float> OptHeight = CachedLandscape->GetHeightAtLocation(WorldPoint);
                Page.Heights[LocalY * Page.Size.X + LocalX] = OptHeight.IsSet() ? OptHeight.GetValue() : 0.f;
                Page.bHeightsComplete &= OptHeight.IsSet();
            }
        }
    }
    // Неполные высоты -- нули под частью клеток; актор переставится, когда
    // материализация чанка их досчитает.
    if (Page.bHeightsComplete)
    {
        PlaceSiteActorsOnGround(Page);
    }
}

namespace
{
    template<typename ActorType>
    void PlacePageActorsOnGround(const AGridWorldManager& Manager, UWorld& World, const FHerbalistCellPage& Page)
    {
        for (TActorIterator<ActorType> It(&World); It; ++It)
        {
            const FIntPoint Cell = It->GetGridCell();
            if (Cell.X < Page.MinCell.X || Cell.X >= Page.MinCell.X + Page.Size.X
                || Cell.Y < Page.MinCell.Y || Cell.Y >= Page.MinCell.Y + Page.Size.Y)
            {
                continue;
            }
            const FVector Ground = Manager.GetCellWorldPosition(Cell.X, Cell.Y);
            // Не над своей клеткой -- актор другого менеджера (или сдвинут
            // намеренно), по высоте не трогаем.
            if (FVector2D(It->GetActorLocation()).Equals(FVector2D(Ground), 1.0))
            {
                It->SetActorLocation(Ground);
            }
        }
    }
}

void AGridWorldManager::PlaceSiteActorsOnGround(const FHerbalistCellPage& Page)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    PlacePageActorsOnGround<AKurganActor>(*this, *World, Page);
    PlacePageActorsOnGround<APOI_Totem>(*this, *World, Page);
    PlacePageActorsOnGround<APOI_Svetloyar>(*this, *World, Page);
    PlacePageActorsOnGround<APOI_GoryuchKamen>(*this, *World, Page);
    PlacePageActorsOnGround<ALegendaryAnchorMarkerActor>(*this, *World, Page);
    PlacePageActorsOnGround<AHomesteadMarkerActor>(*this, *World, Page);
}

void AGridWorldManager::GetPageChunkRange(const FHerbalistCellPage& Page, FIntPoint& OutMinChunk, FIntPoint& OutMaxChunk) const
{
    OutMinChunk = GetChunkCoordForCell(Page.MinCell.X, Page.MinCell.Y);
    OutMaxChunk = GetChunkCoordForCell(Page.MinCell.X + Page.Size.X - 1, Page.MinCell.Y + Page.Size.Y - 1);
}

void AGridWorldManager::LoadCellPage(FHerbalistCellPage& Page)
{
    // Порядок (DESIGN_World_Layout.md §6): основа -> снимок для отката -> высоты
    // -> дельты -> отложенные отрастания. Догон за простой делает
    // CatchUpActivatedChunks по времени чанка: активный чанк выгруженным не
    // бывает, а неактивный не симулируется и загруженным.
    const FHerbalistCellBaseContext Context = MakeCellBaseContext();
    const int32 CellCount = Page.Size.X * Page.Size.Y;
    Page.Cells.SetNum(CellCount);
    Page.Baselines.Reset(CellCount);
    for (int32 LocalY = 0; LocalY < Page.Size.Y; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Page.Size.X; ++LocalX)
        {
            FGridCell& Cell = Page.Cells[LocalY * Page.Size.X + LocalX];
            BuildCellBase(Page.MinCell.X + LocalX, Page.MinCell.Y + LocalY, Cell, Context);
            Page.Baselines.Add(CaptureCellState(Cell));
        }
    }
    Page.bLoaded = true;
    LoadedCellCount += CellCount;
    CacheCellHeightsForPage(Page);

    const bool bHasCellExtras = UnloadedCellDeltas.Num() > 0 || UnloadedCellRosters.Num() > 0 || PendingRegrowthsOnLoad.Num() > 0;
    for (FGridCell& Cell : Page.Cells)
    {
        const int32 GridIndex = GetCellIndex(Cell.X, Cell.Y);
        if (SeededCellMask.IsValidIndex(GridIndex) && SeededCellMask[GridIndex])
        {
            Cell.bResourcesSeeded = true;
            SeededCellMask[GridIndex] = false;
        }
        if (!bHasCellExtras)
        {
            continue;
        }

        // Чанк ещё не материализован -- ростер встаёт спящим и просыпается
        // вместе с чанком.
        if (const FSavedCellState* Delta = UnloadedCellDeltas.Find(GridIndex))
        {
            ApplyCellStateAndRespawnResources(Cell, *Delta);
            UnloadedCellDeltas.Remove(GridIndex);
        }
        else if (FHerbalistCellRoster* Roster = UnloadedCellRosters.Find(GridIndex))
        {
            const FHerbalistCellRoster Moved = MoveTemp(*Roster);
            UnloadedCellRosters.Remove(GridIndex);
            SpawnResourceRoster(Cell, Moved.IngredientIDs, Moved.PlacementSlots);
        }
        if (TArray<float>* Regrowths = PendingRegrowthsOnLoad.Find(GridIndex))
        {
            const TArray<float> RegrowthTimes = MoveTemp(*Regrowths);
            PendingRegrowthsOnLoad.Remove(GridIndex);
            for (float RegrowthTime : RegrowthTimes)
            {
                CompleteRegrowth(Cell, RegrowthTime);
            }
        }
    }

    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    GetPageChunkRange(Page, MinChunk, MaxChunk);
    for (int32 ChunkY = MinChunk.Y; ChunkY <= MaxChunk.Y; ++ChunkY)
    {
        for (int32 ChunkX = MinChunk.X; ChunkX <= MaxChunk.X; ++ChunkX)
        {
            StaleChunkSummaries.Add(FIntPoint(ChunkX, ChunkY));
        }
    }
}

void AGridWorldManager::UnloadCellPage(FHerbalistCellPage& Page)
{
    // Последние сводки чанков страницы -- до выброса клеток (DESIGN §9). Ключ
    // кэша -- до записи: иначе первый же запрос сводок сбросил бы их (ревью
    // этапа 8в).
    EnsureChunkSummaryCacheKey();
    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    GetPageChunkRange(Page, MinChunk, MaxChunk);
    for (int32 ChunkY = MinChunk.Y; ChunkY <= MaxChunk.Y; ++ChunkY)
    {
        for (int32 ChunkX = MinChunk.X; ChunkX <= MaxChunk.X; ++ChunkX)
        {
            const FIntPoint Chunk(ChunkX, ChunkY);
            ChunkSummaries.FindOrAdd(Chunk) = BuildChunkSummary(Chunk);
            StaleChunkSummaries.Remove(Chunk);
        }
    }

    for (const FGridCell& Cell : Page.Cells)
    {
        // Сущности уже сняты на границе активной области, ресурсы усыплены
        // материализацией; оставшееся -- на всякий случай, не висеть в мире.
        if (AHerbalistEntityActor* EntityActor = Cell.ManifestedEntityActor.Get())
        {
            EntityActor->Destroy();
        }

        // Ростер -- только свой: чужие акторы (PCG-граф) стримит World
        // Partition, при загрузке страницы сетка их не пересоздаёт.
        FHerbalistCellRoster Roster;
        for (const TWeakObjectPtr<AHerbalistResourceActor>& ResourceActor : Cell.ResourceActors)
        {
            if (ResourceActor.IsValid() && ResourceActor->WasSpawnedByGrid())
            {
                Roster.IngredientIDs.Add(ResourceActor->GetIngredientID());
                Roster.PlacementSlots.Add(ResourceActor->GetPlacementSlot());
                ResourceActor->Destroy();
            }
        }
        Roster.IngredientIDs.Append(Cell.DormantResourceIDs);
        for (int32 Index = 0; Index < Cell.DormantResourceIDs.Num(); ++Index)
        {
            Roster.PlacementSlots.Add(Cell.DormantResourceSlots.IsValidIndex(Index) ? Cell.DormantResourceSlots[Index] : INDEX_NONE);
        }

        const int32 GridIndex = GetCellIndex(Cell.X, Cell.Y);
        if (Cell.bResourcesSeeded)
        {
            SeededCellMask[GridIndex] = true;
        }

        // Полная дельта -- только у отклонившихся от основы: тронутая клетка и
        // проявление сущности (пишется без пометки). Нетронутая засеянная
        // клетка -- бит засева и, если растения есть, ростер: вид засеянного
        // ресурса зависит от условий в момент заселения, и основа его не
        // воспроизводит (ревью этапа 8в: полная дельта на каждую посещённую
        // клетку -- десятки мегабайт на исследованном L_TestDev).
        if (DirtyCellIndices.Contains(GridIndex) || !Cell.ManifestedEntityID.IsNone())
        {
            FSavedCellState Delta = CaptureCellState(Cell);
            Delta.ResourceIngredientIDs = MoveTemp(Roster.IngredientIDs);
            Delta.ResourceSlots = MoveTemp(Roster.PlacementSlots);
            UnloadedCellDeltas.Add(GridIndex, MoveTemp(Delta));
        }
        else if (Roster.IngredientIDs.Num() > 0)
        {
            UnloadedCellRosters.Add(GridIndex, MoveTemp(Roster));
        }
    }

    LoadedCellCount -= Page.Cells.Num();
    Page.Cells.Empty();
    Page.Heights.Empty();
    Page.Baselines.Empty();
    Page.bLoaded = false;
}

void AGridWorldManager::EnsureChunkPagesLoaded(const FIntPoint& Chunk)
{
    if (CellPageSize <= 0)
    {
        return;   // одна страница во всю сетку -- всегда загружена
    }
    ForEachChunkPage(Chunk, [this](FHerbalistCellPage& Page)
    {
        if (!Page.bLoaded)
        {
            LoadCellPage(Page);
        }
    });
}

void AGridWorldManager::ForEachChunkPage(const FIntPoint& Chunk, TFunctionRef<void(FHerbalistCellPage&)> Func)
{
    if (CellPages.Num() == 0 || GridSizeX <= 0 || GridSizeY <= 0)
    {
        return;
    }
    if (CellPageSize <= 0)
    {
        Func(CellPages[0]);
        return;
    }
    const int32 ChunkSize = GetChunkSizeInCells();
    const FIntPoint GridMin = GetGridMinCell();
    const FIntPoint GridMax = GridMin + FIntPoint(GridSizeX - 1, GridSizeY - 1);
    const FIntPoint CellMin(FMath::Max(Chunk.X * ChunkSize, GridMin.X), FMath::Max(Chunk.Y * ChunkSize, GridMin.Y));
    const FIntPoint CellMax(FMath::Min(Chunk.X * ChunkSize + ChunkSize - 1, GridMax.X), FMath::Min(Chunk.Y * ChunkSize + ChunkSize - 1, GridMax.Y));
    if (CellMin.X > CellMax.X || CellMin.Y > CellMax.Y)
    {
        return;
    }
    for (int32 PageY = HerbalistCore::FloorDivCoord(CellMin.Y, CellPageSize); PageY <= HerbalistCore::FloorDivCoord(CellMax.Y, CellPageSize); ++PageY)
    {
        for (int32 PageX = HerbalistCore::FloorDivCoord(CellMin.X, CellPageSize); PageX <= HerbalistCore::FloorDivCoord(CellMax.X, CellPageSize); ++PageX)
        {
            const int32 CellX = FMath::Max(PageX * CellPageSize, CellMin.X);
            const int32 CellY = FMath::Max(PageY * CellPageSize, CellMin.Y);
            if (FHerbalistCellPage* Page = FindCellPage(CellX, CellY))
            {
                Func(*Page);
            }
        }
    }
}

bool AGridWorldManager::IsCellPagePinned(const FHerbalistCellPage& Page) const
{
    // Места, чья логика идёт своим чередом вдали от игрока (ревью этапа 8в):
    // хозяева мест и легендарные якоря копят и тратят благосклонность, капища
    // гасят утечку Морока по графу. Таких страниц единицы.
    auto InPage = [&Page](const FIntPoint& Cell)
    {
        return Cell.X >= Page.MinCell.X && Cell.X < Page.MinCell.X + Page.Size.X
            && Cell.Y >= Page.MinCell.Y && Cell.Y < Page.MinCell.Y + Page.Size.Y;
    };
    // Страница места за убранными плитками ландшафта (2026-09-13): под ней нет
    // земли, и без закрепления место не жило бы никогда.
    if (PinnedSitePages.Contains(Page.MinCell))
    {
        return true;
    }
    for (const FEntityLandmark& Landmark : EntityLandmarks)
    {
        if (InPage(Landmark.Cell))
        {
            return true;
        }
    }
    for (const FShrine& Shrine : Shrines)
    {
        // И страницы четырёх прямых соседей: гашение утечки Морока на стыке
        // биомов читает их клетки (CollectBorderShrineDamping), и у капища на
        // краю страницы сосед на выгруженной странице молча выпадал (ревью
        // 2026-09-13).
        if (HerbalistCore::IsValidCell(Shrine.Cell)
            && (InPage(Shrine.Cell) || InPage(Shrine.Cell + FIntPoint(1, 0)) || InPage(Shrine.Cell - FIntPoint(1, 0))
                || InPage(Shrine.Cell + FIntPoint(0, 1)) || InPage(Shrine.Cell - FIntPoint(0, 1))))
        {
            return true;
        }
    }
    for (const TPair<FName, FIntPoint>& Anchor : LegendaryAnchors)
    {
        if (InPage(Anchor.Value))
        {
            return true;
        }
    }
    // Точки интереса (ревью 2026-09-13): под ними может не оказаться земли --
    // плитку убрали внутри ландшафта, и страница иначе не грузилась бы никогда.
    for (const FIntPoint& Site : { GetTotemSite(), GetSvetloyarSite(), GetGoryuchKamenSite(), GetSoloveySite(), GetKalinovMostSite() })
    {
        if (HerbalistCore::IsValidCell(Site) && InPage(Site))
        {
            return true;
        }
    }
    return false;
}

bool AGridWorldManager::IsCellPageIdle(const FHerbalistCellPage& Page) const
{
    if (IsCellPagePinned(Page))
    {
        return false;
    }
    FIntPoint MinChunk;
    FIntPoint MaxChunk;
    GetPageChunkRange(Page, MinChunk, MaxChunk);
    for (int32 ChunkY = MinChunk.Y; ChunkY <= MaxChunk.Y; ++ChunkY)
    {
        for (int32 ChunkX = MinChunk.X; ChunkX <= MaxChunk.X; ++ChunkX)
        {
            const FIntPoint Chunk(ChunkX, ChunkY);
            if (MaterializedChunks.Contains(Chunk) || ActiveChunks.Contains(Chunk))
            {
                return false;
            }
        }
    }
    return true;
}

void AGridWorldManager::UnloadIdleCellPagesIfPending()
{
    if (!bCellPageUnloadCheckPending)
    {
        return;
    }
    bCellPageUnloadCheckPending = false;

    // Только когда страниц много, стриминг сетки включён и земля известна:
    // без этого «земли нет» неотличимо от «не знаем» (редактор, карта без
    // World Partition, автотесты без покрытия).
    if (CellPageSize <= 0 || GetActiveRadiusInChunks() < 0 || !bGroundCoverageKnown || !bMaterializationTracked)
    {
        return;
    }
    for (FHerbalistCellPage& Page : CellPages)
    {
        if (Page.bLoaded && IsCellPageIdle(Page))
        {
            UnloadCellPage(Page);
        }
    }
}

bool AGridWorldManager::IsChunkInLoadedPage(const FIntPoint& Chunk) const
{
    if (CellPageSize <= 0)
    {
        return CellPages.Num() > 0 && CellPages[0].bLoaded;
    }
    const int32 ChunkSize = GetChunkSizeInCells();
    const FIntPoint GridMin = GetGridMinCell();
    const int32 CellX = FMath::Clamp(Chunk.X * ChunkSize, GridMin.X, GridMin.X + GridSizeX - 1);
    const int32 CellY = FMath::Clamp(Chunk.Y * ChunkSize, GridMin.Y, GridMin.Y + GridSizeY - 1);
    const FHerbalistCellPage* Page = FindCellPage(CellX, CellY);
    return Page && Page->bLoaded;
}

FHarvestContext AGridWorldManager::BuildHarvestContextForCell(const FGridCell& Cell) const
{
    // Одно и то же окно условий для всех ресурсов одного вызова (та же
    // клетка, тот же момент) — читается один раз, не на каждой итерации.
    // Вынесено из SpawnResourcesInCell (2026-09-04): StartRegeneration
    // теперь тоже считает это окно -- на момент отрастания время могло уже
    // уйти вперёд (сезон/луна/погода), и это правильно, не баг: отросшее
    // растение подчиняется условиям МОМЕНТА отрастания, не момента сбора.
    FHarvestContext Context;
    Context.Season = GetSeason();
    Context.TimeOfDay = IsDawn() ? EHarvestTimeWindow::Dawn
        : IsDusk() ? EHarvestTimeWindow::Dusk
        : IsNight() ? EHarvestTimeWindow::Night
        : EHarvestTimeWindow::Day;
    Context.MoonPhase = GetMoonPhase();
    Context.bDryWeather = !IsRainy() && !IsBlizzard();

    // Высота клетки для высотного пояса карточек (2026-09-03). Известна
    // только когда высоты реально закэшированы с ландшафта: без ландшафта
    // GetCellHeight отдаёт 0 для всей сетки, и гейт по такой "высоте" был бы
    // вымыслом -- в этом случае он просто не применяется.
    //
    // Без деления на 100 (переименовано в AltitudeCentimeters тем же днём) --
    // GetCellHeight уже в сантиметрах, как и Min/MaxAltitudeCentimeters
    // карточки. Раньше здесь стояла конвертация в метры, а поля карточки
    // заполнялись в редакторе сантиметрами по привычке (стандартная единица
    // UE для любого другого расстояния) -- пояс сравнивался с числом в 100
    // раз меньше настоящей высоты и никогда не совпадал.
    Context.bAltitudeKnown = CachedLandscape != nullptr && bCellHeightsCached;
    Context.AltitudeCentimeters = Context.bAltitudeKnown ? GetCellHeight(Cell.X, Cell.Y) : 0.0f;

    // Хозяева трав (решение пользователя 2026-09-20): Respect ближайшего
    // экземпляра каждого Основного биомов клетки. Хозяин чужого биома траву
    // этой клетки не множит; Домовой (ручная регистрация) -- не хозяин трав.
    TMap<FName, int32> NearestDistSq;
    for (const FEntityLandmark& Landmark : EntityLandmarks)
    {
        const FLandmarkDefinition* Def = FindLandmarkDefinition(Landmark.EntityID);
        if (!Def || Def->bManualRegistrationOnly) continue;
        const bool bCellBiome = Cell.BiomeWeights.Num() == 0
            ? Def->Biome == Cell.Biome
            : Cell.BiomeWeights.ContainsByPredicate([Def](const FBiomeWeightEntry& Entry) { return Entry.Biome == Def->Biome; });
        if (!bCellBiome) continue;
        const int32 DistSq = FMath::Square(Landmark.Cell.X - Cell.X) + FMath::Square(Landmark.Cell.Y - Cell.Y);
        const int32* Best = NearestDistSq.Find(Landmark.EntityID);
        if (!Best || DistSq < *Best)
        {
            NearestDistSq.Add(Landmark.EntityID, DistSq);
            Context.HostRespect.Add(Landmark.EntityID, Landmark.Respect);
        }
    }

    return Context;
}

bool AGridWorldManager::SpawnOneResourceInCell(FGridCell& Cell, const FHarvestContext& Context,
    const EGardenNiche* PlotNiche, ABiomeRegionVolume* ClaimingRegion, UIngredientRegistrySubsystem* IngredientSubsystem)
{
    return SpawnOneResourceInCell(Cell, Context, PlotNiche, ClaimingRegion, IngredientSubsystem, WorldRNG, AllocatePlacementSlot(Cell));
}

bool AGridWorldManager::SpawnOneResourceInCell(FGridCell& Cell, const FHarvestContext& Context,
    const EGardenNiche* PlotNiche, ABiomeRegionVolume* ClaimingRegion, UIngredientRegistrySubsystem* IngredientSubsystem,
    FRandomStream& SpeciesRng, int32 PlacementSlot)
{
    FName IngredientID = NAME_None;

    // Посадка (PlantSeed, DESIGN_Community_And_Homestead.md §2.4, 2026-09-04)
    // -- клетка с явно посаженным видом обходит вероятностный выбор ЦЕЛИКОМ,
    // включая PickWeightedResource и все его окна (сезон/время/луна/погода/
    // высота): "здесь посажено именно это", не "склоняется к этому в среднем
    // чаще". Не применяется на воде -- тот же принцип, что и у PlotNiche чуть
    // ниже (пристройка сада не подделывает нишу для водных клеток);
    // PlantSeedInCell в любом случае требует зарегистрированную пристройку на
    // этой клетке, но поле проверяется здесь напрямую, не полагаясь на то,
    // как оно было проставлено. Персистентно: ни первичное заселение, ни
    // StartRegeneration (оба вызывают именно эту функцию на той же Cell) не
    // сбрасывают Cell.PlantedSpeciesID -- посадка переживает отрастание после
    // сбора, не разовый эффект.
    // Слоты ресурсов (этап 5б): слот места выбирается раньше вида и не
    // зависит от состояния клетки (GetAssignedResourceSlot). Вода и кромка
    // берут водный пул, суша -- земной без водных видов. Земной вид вместо
    // водного -- когда водный пул пуст или водному в проснувшейся клетке
    // нет свободного слота (свой занят, отрастание дало индекс по кругу):
    // иначе клетка у воды навсегда теряла бы ресурс. Исключение -- вода в
    // водной клетке: земному там места нет, как и до слотов. Пристройка сада
    // (PlotNiche) и посадка -- как до слотов, раньше слота.
    FHerbalistResourceSlot AssignedSlot;
    const bool bHasSlot = GetAssignedResourceSlot(Cell.X, Cell.Y, PlacementSlot, AssignedSlot);
    const bool bAquaticSpot = bHasSlot ? AssignedSlot.Kind != EResourceSlotKind::Land : Cell.bIsWater;
    const bool bWaterSurface = bHasSlot ? AssignedSlot.Kind == EResourceSlotKind::Water : Cell.bIsWater;
    const bool bHasPlotNiche = PlotNiche && *PlotNiche != EGardenNiche::None;

    if (!Cell.bIsWater && !Cell.PlantedSpeciesID.IsNone())
    {
        IngredientID = Cell.PlantedSpeciesID;
    }
    else if (IngredientSubsystem)
    {
        // Водные растения (2026-09-02, прямой запрос пользователя):
        // "если у биома есть водные растения, то они разрешены к
        // размещению на поверхности воды, и вода одновременно доступна" --
        // отдельный, не смешанный с земляным пул (bGrowsOnWater),
        // тот же принцип отбора по Cell.BiomeWeights земляного биома
        // под водой, что и обычный GetRandomResourceForBiome.
        if (!Cell.bIsWater && bHasPlotNiche)
        {
            IngredientID = IngredientSubsystem->GetRandomResourceForNiche(Cell, *PlotNiche, Context, SpeciesRng);
        }
        else if (bAquaticSpot)
        {
            IngredientID = IngredientSubsystem->GetRandomResourceForAquaticBiome(Cell, Context, SpeciesRng);
            FVector FreeAquaticSlot;
            if (!IngredientID.IsNone() && bHasSlot && IsCellMaterialized(Cell)
                && !FindResourceSlotPosition(Cell.X, Cell.Y, PlacementSlot, /*bAquaticSpecies=*/true, FreeAquaticSlot))
            {
                IngredientID = NAME_None;
            }
            if (IngredientID.IsNone() && !(bWaterSurface && Cell.bIsWater))
            {
                IngredientID = bHasSlot
                    ? IngredientSubsystem->GetRandomResourceForLandBiome(Cell, Context, SpeciesRng)
                    : IngredientSubsystem->GetRandomResourceForBiome(Cell, Context, SpeciesRng);
            }
        }
        else if (bHasPlotNiche)
        {
            IngredientID = IngredientSubsystem->GetRandomResourceForNiche(Cell, *PlotNiche, Context, SpeciesRng);
        }
        else
        {
            IngredientID = bHasSlot
                ? IngredientSubsystem->GetRandomResourceForLandBiome(Cell, Context, SpeciesRng)
                : IngredientSubsystem->GetRandomResourceForBiome(Cell, Context, SpeciesRng);
        }
    }
    if (IngredientID.IsNone()) return false;

    // Спящая клетка (2026-09-12): выросшее запоминается, а не ставится в мир
    // -- под клеткой может не быть загруженной земли. Место подберётся при
    // материализации чанка (SetChunkResourcesActive).
    if (!IsCellMaterialized(Cell))
    {
        AddDormantResource(Cell, IngredientID, PlacementSlot);
        return true;
    }

    // Позиция внутри формы биома (2026-09-02) + посадка на поверхность и
    // отбраковка занятых точек (2026-09-03). Свободного места нет --
    // клетка остаётся пустой: лучше так, чем трава внутри валуна.
    // Поток мест -- свой у каждого слота (этап 4 разметки мира).
    const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(IngredientID) : nullptr;
    if (!Row)
    {
        return false;
    }

    FRandomStream PlacementRng = MakeCellRandomStream(Cell.X, Cell.Y, FWorldLayoutSolver::ECellRandomPurpose::ResourcePlacement, PlacementSlot);
    FVector SpawnPos;
    bool bFromSlot = false;
    if (!FindResourcePosition(Cell.X, Cell.Y, PlacementSlot, Row->bGrowsOnWater, PlacementRng, SpawnPos, bFromSlot))
    {
        UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnOneResourceInCell: клетка (%d,%d) занята, ресурс пропущен"), Cell.X, Cell.Y);
        return false;
    }
    SpawnPos.Z += 5.0f;   // небольшой подъём над поверхностью, тот же, что и раньше

    // Случайная трансформация (2026-09-03, "как у PCG в ноде Transform")
    // -- скейл/поворот/доп.смещение из настроек региона. Без региона
    // (блочный фолбэк, тесты) -- нейтральный дефолт FRandomPlacementTransform,
    // поведение не меняется. Доп.смещение применяется к SpawnPos ДО
    // Init() -- Init() получает Location параметром отдельно от
    // фактического Transform актора, оба должны совпадать.
    const FRandomPlacementTransform PlacementXform = ClaimingRegion
        ? ClaimingRegion->RollPlacementTransform(SpawnPos, PlacementRng)
        : FRandomPlacementTransform();
    // Слот -- точное место, найденное графом (на воде, у кромки): сдвиг
    // региона увёл бы растение с него.
    if (!bFromSlot)
    {
        SpawnPos += PlacementXform.PositionOffset;
    }

    // Класс из строки (2026-09-03) -- пусто = базовый, см.
    // FIngredientTableRow::ResourceActorClass.
    TSubclassOf<AHerbalistResourceActor> ClassToSpawn = Row->ResourceActorClass;
    if (!ClassToSpawn) ClassToSpawn = AHerbalistResourceActor::StaticClass();

    AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(ClassToSpawn, SpawnPos, PlacementXform.Rotation);
    if (!NewActor) return false;

    NewActor->SetActorScale3D(FVector(PlacementXform.UniformScale));
    // Регистрация в Cell.ResourceActors теперь делает сам Init()
    // (2026-09-02) -- единая точка входа для любого источника спавна,
    // не только этого C++-пути.
    NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, Cell.X, Cell.Y, Row->Resilience, Row->bIronAverse, Row->bDelicate);
    NewActor->SetPlacementSlot(PlacementSlot);
    UE_LOG(LogHerbalistWorld, Verbose, TEXT("Spawned %s at cell (%d,%d) with Z=%.1f"), *IngredientID.ToString(), Cell.X, Cell.Y, SpawnPos.Z);
    return true;
}

void AGridWorldManager::SpawnResourcesInCell(FGridCell& Cell, UIngredientRegistrySubsystem* IngredientSubsystemOverride)
{
    // PCG-биомы (2026-09-02, прямое требование пользователя) -- клетка вне
    // всех размещённых на уровне ABiomeRegionVolume не спавнит ресурсы,
    // даже если блочный фолбэк формально приписал ей какой-то биом. Без
    // регионов на уровне вовсе (тесты, сцены без PCG-авторства) -- проверка
    // всегда true, ничего не меняется.
    if (!IsCellClaimedByBiomeRegion(Cell)) return;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = IngredientSubsystemOverride ? IngredientSubsystemOverride
        : GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    const FHarvestContext Context = BuildHarvestContextForCell(Cell);

    // Сад (§2.4): клетка с пристройкой — кандидаты из EGardenNiche, не из
    // AllowedBiomes. Пусто (None) для подавляющего большинства клеток мира
    // — обычный путь ниже не меняется вовсе для них. Не применяется на воде
    // (2026-09-02) -- пристройка сада не подделывает нишу для водных клеток,
    // это отдельный, пока не заведённый случай.
    const EGardenNiche* PlotNiche = Cell.bIsWater ? nullptr : GardenPlots.Find(FIntPoint(Cell.X, Cell.Y));

    // Плотность -- пер-региональная настройка, ресурсов на 100 м²
    // (ABiomeRegionVolume::MinResourcesPer100SquareMeters/Max..., с 2026-09-12
    // на площадь, а не на клетку), если клетка реально заявлена регионом; без
    // регионов на уровне (тесты, сцены без PCG) -- GetClaimingRegion
    // возвращает nullptr, дефолт 1-3 на 100 м².
    ABiomeRegionVolume* ClaimingRegion = GetClaimingRegion(Cell);

    // Регион, отданный PCG-графу (2026-09-03), C++ не заселяет вовсе --
    // иначе к разбросу графа добавился бы второй, клеточный набор внахлёст.
    if (ClaimingRegion && !ClaimingRegion->bSpawnResourcesFromGrid) return;

    const float MinPer100SquareMeters = ClaimingRegion ? ClaimingRegion->MinResourcesPer100SquareMeters : 1.0f;
    const float MaxPer100SquareMeters = ClaimingRegion ? ClaimingRegion->MaxResourcesPer100SquareMeters : 3.0f;

    // Затухание плотности к границе региона (2026-09-03, "как у PCG в ноде
    // Transform"). 0 (дефолт) -- множитель всегда 1, поведение не меняется.
    // Множитель входит в бросок ДО розыгрыша дробной части (2026-09-12, ревью
    // этапа 2 разметки мира): округление уже целого числа ресурсов делало
    // затухание ступенькой (1 x 0.6 -> 1, 1 x 0.4 -> 0) и ломало среднее на
    // площадь -- сильнее всего на мелких клетках.
    float DensityScale = 1.0f;
    if (ClaimingRegion && ClaimingRegion->DensityFalloffStrength > 0.0f)
    {
        const float T = ClaimingRegion->GetNormalizedDistanceFromCenter(GetCellWorldPositionFlat(Cell.X, Cell.Y));
        DensityScale = FMath::Lerp(1.0f, 1.0f - T, ClaimingRegion->DensityFalloffStrength);
    }
    // Потоки клетки, не общий WorldRNG (этап 4 разметки мира): число, виды и
    // места ресурсов не зависят от порядка обхода и от бросков других клеток.
    // i-й ресурс -- вид из потока с солью i и слот места i. Слоты независимы:
    // ресурс, не нашедший места, не сдвигает следующих, а собранный -- тех,
    // что остались, когда ростер ставится заново.
    FRandomStream CountRng = MakeCellRandomStream(Cell.X, Cell.Y, FWorldLayoutSolver::ECellRandomPurpose::ResourceCount);
    const int32 NumResources = RollResourceCount(MinPer100SquareMeters, MaxPer100SquareMeters, CellSize, CountRng, DensityScale);

    for (int32 i = 0; i < NumResources; ++i)
    {
        FRandomStream SpeciesRng = MakeCellRandomStream(Cell.X, Cell.Y, FWorldLayoutSolver::ECellRandomPurpose::ResourceSpecies, i);
        SpawnOneResourceInCell(Cell, Context, PlotNiche, ClaimingRegion, IngredientSubsystem, SpeciesRng, i);
    }
}

int32 AGridWorldManager::RollResourceCount(float MinPer100SquareMeters, float MaxPer100SquareMeters,
    double CellSizeCm, FRandomStream& Rng, float DensityScale)
{
    // NaN в любом входе даёт ноль, а не -2^30 из FloorToInt32 (найдено ревью).
    if (!FMath::IsFinite(MinPer100SquareMeters) || !FMath::IsFinite(MaxPer100SquareMeters)
        || !FMath::IsFinite(DensityScale) || !FMath::IsFinite(CellSizeCm))
    {
        return 0;
    }
    const double Low = FMath::Max(0.0, static_cast<double>(FMath::Min(MinPer100SquareMeters, MaxPer100SquareMeters)));
    const double High = FMath::Max(0.0, static_cast<double>(FMath::Max(MinPer100SquareMeters, MaxPer100SquareMeters)));
    // Площадь клетки в сотнях квадратных метров: клетка 10 м -- ровно 1.
    const double CellAreaIn100SquareMeters = FMath::Square(FMath::Max(CellSizeCm, 0.0) / 100.0) / 100.0;
    const double Expected = FMath::Clamp(
        Rng.FRandRange(Low, High) * CellAreaIn100SquareMeters * FMath::Max(static_cast<double>(DensityScale), 0.0),
        0.0, static_cast<double>(MaxResourcesPerCell));
    // Дробная часть -- вероятность ещё одного ресурса. Округление вместо
    // розыгрыша сломало бы плотность на мелких клетках: 0.81..2.43 на клетку
    // 9 м ещё терпимо, а 0.04 на клетку 2 м округлялось бы в пустой мир.
    const int32 Whole = FMath::FloorToInt32(Expected);
    return FMath::Min(Whole + (Rng.FRand() < (Expected - Whole) ? 1 : 0), MaxResourcesPerCell);
}

int32 AGridWorldManager::AllocatePlacementSlot(const FGridCell& Cell)
{
    int32 MaxSlot = INDEX_NONE;
    for (const TWeakObjectPtr<AHerbalistResourceActor>& Ptr : Cell.ResourceActors)
    {
        if (const AHerbalistResourceActor* Actor = Ptr.Get())
        {
            MaxSlot = FMath::Max(MaxSlot, Actor->GetPlacementSlot());
        }
    }
    for (int32 Slot : Cell.DormantResourceSlots)
    {
        MaxSlot = FMath::Max(MaxSlot, Slot);
    }
    return MaxSlot + 1;
}

void AGridWorldManager::AddDormantResource(FGridCell& Cell, FName IngredientID, int32 PlacementSlot)
{
    // Список ID могли дописать без слотов (сейв до слотов, тесты) -- слоты
    // выравниваются по нему, чтобы индексы не разъехались.
    while (Cell.DormantResourceSlots.Num() < Cell.DormantResourceIDs.Num())
    {
        Cell.DormantResourceSlots.Add(INDEX_NONE);
    }
    Cell.DormantResourceIDs.Add(IngredientID);
    Cell.DormantResourceSlots.Add(PlacementSlot);
}

void AGridWorldManager::SpawnResourceRoster(FGridCell& Cell, const TArray<FName>& IngredientIDs, const TArray<int32>& PlacementSlots,
    UIngredientRegistrySubsystem* IngredientSubsystemOverride)
{
    for (int32 Index = 0; Index < IngredientIDs.Num(); ++Index)
    {
        const int32 Slot = PlacementSlots.IsValidIndex(Index) ? PlacementSlots[Index] : INDEX_NONE;
        SpawnResourceActor(IngredientIDs[Index], Cell.X, Cell.Y, FVector::ZeroVector, IngredientSubsystemOverride, Slot);
    }
}

void AGridWorldManager::SpawnResourceActor(FName IngredientID, int32 X, int32 Y, const FVector& Offset,
    UIngredientRegistrySubsystem* IngredientSubsystemOverride, int32 PlacementSlot)
{
    FGridCell* Cell = GetCell(X, Y);
    if (!Cell) return;

    // Слот выдаётся до проверки материализации: спящий ресурс хранит его, и
    // при пробуждении встаёт туда же, куда встал бы сейчас.
    const int32 PlacementSlotIndex = PlacementSlot != INDEX_NONE ? PlacementSlot : AllocatePlacementSlot(*Cell);

    // Спящая клетка (2026-09-12): загрузка сейва и прочие вызовы не ставят
    // актор туда, где под ним может не быть земли, -- ресурс запоминается и
    // появится при материализации чанка.
    if (!IsCellMaterialized(*Cell))
    {
        AddDormantResource(*Cell, IngredientID, PlacementSlotIndex);
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = IngredientSubsystemOverride ? IngredientSubsystemOverride
        : GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
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
    // Случайная трансформация (2026-09-03, см. довод у SpawnResourcesInCell)
    // -- только на пути автопоиска места, не когда вызывающая сторона
    // передала свой Offset явно: тот контракт про ТОЧНУЮ позицию, добавлять
    // туда случайность значило бы его нарушить.
    FRandomPlacementTransform PlacementXform;
    if (Offset.IsNearlyZero())
    {
        // Тот же поиск свободной точки, что и при первичном заселении
        // (2026-09-03). Отросшее/восстановленное из сейва растение не должно
        // оказаться внутри камня, поставленного там, где оно раньше росло.
        // Место -- из потока клетки с солью слота (этап 4 разметки мира), того
        // же, что при первичном заселении.
        FRandomStream PlacementRng = MakeCellRandomStream(X, Y, FWorldLayoutSolver::ECellRandomPurpose::ResourcePlacement, PlacementSlotIndex);
        bool bFromSlot = false;
        if (!FindResourcePosition(X, Y, PlacementSlotIndex, Row->bGrowsOnWater, PlacementRng, SpawnPos, bFromSlot))
        {
            UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnResourceActor: клетка (%d,%d) занята, %s не поставлен"), X, Y, *IngredientID.ToString());
            return;
        }
        SpawnPos.Z += 5.0f;

        if (ABiomeRegionVolume* ClaimingRegion = GetClaimingRegion(*Cell))
        {
            PlacementXform = ClaimingRegion->RollPlacementTransform(SpawnPos, PlacementRng);
            if (!bFromSlot)
            {
                SpawnPos += PlacementXform.PositionOffset;
            }
        }
    }
    else
    {
        SpawnPos = GetCellWorldPositionFlat(X, Y);
        SpawnPos.Z = GetCellHeight(X, Y) + 5.0f;
        SpawnPos += Offset;
    }

    TSubclassOf<AHerbalistResourceActor> ClassToSpawn = Row->ResourceActorClass;
    if (!ClassToSpawn) ClassToSpawn = AHerbalistResourceActor::StaticClass();

    AHerbalistResourceActor* NewActor = GetWorld()->SpawnActor<AHerbalistResourceActor>(ClassToSpawn, SpawnPos, PlacementXform.Rotation);
    if (NewActor)
    {
        NewActor->SetActorScale3D(FVector(PlacementXform.UniformScale));
        // Регистрация в Cell.ResourceActors теперь делает сам Init() (2026-09-02).
        NewActor->Init(IngredientID, Row->DisplayName, Row->ResourceMesh, Row->BaseState, SpawnPos, this, X, Y, Row->Resilience, Row->bIronAverse, Row->bDelicate);
        NewActor->SetPlacementSlot(PlacementSlotIndex);
        UE_LOG(LogHerbalistWorld, Verbose, TEXT("SpawnResourceActor: %s at cell (%d,%d) Z=%.1f"), *IngredientID.ToString(), X, Y, SpawnPos.Z);
    }
}

void AGridWorldManager::RegisterGardenPlot(const FIntPoint& Cell, EGardenNiche Niche)
{
    // Маркер-актор "Обставления" (ROADMAP.md, 2026-09-06) — найден по
    // клетке+виду через TActorIterator, тот же приём, что уже
    // AHerbalistPlayerController::BuildHomeStorage использует против
    // дублей. Не отдельная TMap-привязка клетка->актор -- клеток с
    // пристройкой физически мало (садовые грядки), линейный поиск дешевле
    // лишней книги учёта.
    AHomesteadMarkerActor* Existing = nullptr;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AHomesteadMarkerActor> It(World); It; ++It)
        {
            if (It->GetKind() == EHomesteadMarkerKind::GardenPristroyka && It->GetGridCell() == Cell)
            {
                Existing = *It;
                break;
            }
        }
    }

    if (Niche == EGardenNiche::None)
    {
        GardenPlots.Remove(Cell);
        if (Existing) Existing->Destroy();
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Garden] Plot at (%d,%d) cleared"), Cell.X, Cell.Y);
        return;
    }

    GardenPlots.Add(Cell, Niche);
    if (Existing)
    {
        Existing->SetNiche(Niche);
    }
    else if (UWorld* World = GetWorld())
    {
        if (AHomesteadMarkerActor* Marker = World->SpawnActor<AHomesteadMarkerActor>(
            AHomesteadMarkerActor::StaticClass(), GetCellWorldPosition(Cell.X, Cell.Y), FRotator::ZeroRotator))
        {
            Marker->Init(Cell, EHomesteadMarkerKind::GardenPristroyka, Niche);
        }
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Garden] Plot at (%d,%d) set to niche %d"), Cell.X, Cell.Y, (int32)Niche);
}

bool AGridWorldManager::PlantSeedInCell(const FIntPoint& CellCoord, FName SpeciesID, EGardenNiche SpeciesNiche)
{
    // Посадка (PlantSeed, DESIGN_Community_And_Homestead.md §2.4, 2026-09-04)
    // -- в отличие от RegisterGardenPlot выше (какую нишу подделывает
    // пристройка), это про то, КАКОЙ КОНКРЕТНО вид посажен в уже
    // существующую пристройку. Резолв SpeciesNiche (IngredientTableRow::
    // GardenNiche растения) -- дело вызывающей стороны (AHerbalistPlayerController::
    // PlantSeed через IngredientRegistrySubsystem), тот же принцип границы
    // "инвентарный поиск + резолв ряда в контроллере, мировое состояние
    // здесь", что уже держат ActivateWard/OfferToCommunity -- эта функция
    // не обращается к GameInstance вовсе, поэтому напрямую вызываема из
    // автотестов (GameInstanceSubsystem недоступен в Editor-мире тестов).
    FGridCell* Cell = GetCell(CellCoord.X, CellCoord.Y);
    if (!Cell)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Garden] PlantSeedInCell: no cell at (%d,%d)"), CellCoord.X, CellCoord.Y);
        return false;
    }

    const EGardenNiche* PlotNiche = GardenPlots.Find(CellCoord);
    if (!PlotNiche || *PlotNiche == EGardenNiche::None)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Garden] PlantSeedInCell: (%d,%d) has no garden plot registered (see SetGardenPlot)"), CellCoord.X, CellCoord.Y);
        return false;
    }

    // Тот же класс валидации, что уже RegisterGardenPlot/SetGardenPlot --
    // отказ с логом, не тихая подмена: сажать степную траву в Погребе не
    // должно молча "сработать как-нибудь".
    if (*PlotNiche != SpeciesNiche)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Garden] PlantSeedInCell: (%d,%d) is niche %d, species %s needs niche %d, refused"),
            CellCoord.X, CellCoord.Y, (int32)*PlotNiche, *SpeciesID.ToString(), (int32)SpeciesNiche);
        return false;
    }

    Cell->PlantedSpeciesID = SpeciesID;
    MarkCellDirty(CellCoord.X, CellCoord.Y);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Garden] PlantSeedInCell: (%d,%d) planted with %s"), CellCoord.X, CellCoord.Y, *SpeciesID.ToString());
    return true;
}

bool AGridWorldManager::ApplyFertilizerToCell(const FIntPoint& CellCoord)
{
    FGridCell* Cell = GetCell(CellCoord.X, CellCoord.Y);
    if (!Cell)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Fertilizer] ApplyFertilizerToCell: no cell at (%d,%d)"), CellCoord.X, CellCoord.Y);
        return false;
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Bonus = Settings ? Settings->FertilizerFertilityBonus : 0.1f;
    Cell->Environment.Fertility = FMath::Clamp(Cell->Environment.Fertility + Bonus, 0.0f, 1.0f);
    MarkCellDirty(CellCoord.X, CellCoord.Y);

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Fertilizer] ApplyFertilizerToCell: (%d,%d) Fertility now %.3f"),
        CellCoord.X, CellCoord.Y, Cell->Environment.Fertility);
    return true;
}

float AGridWorldManager::GetRegrowthDelaySeconds(const FGridCell& Cell) const
{
    // Базовое время -- пер-региональная настройка
    // (ABiomeRegionVolume::ResourceRegrowthTimeSeconds, 2026-09-02), если
    // клетка реально заявлена регионом; без регионов на уровне -- глобальный
    // ResourceRegrowthTime.
    const ABiomeRegionVolume* ClaimingRegion = GetClaimingRegion(Cell);
    // Не ниже 0.1 с -- ClampMin региона и ResourceRegrowthTime действует только
    // в редакторе, а таймер с нулём не ставится, и место ждало бы вечно.
    const float BaseSeconds = FMath::Max(ClaimingRegion ? ClaimingRegion->ResourceRegrowthTimeSeconds : ResourceRegrowthTime, 0.1f);

    // Истощённая клетка отращивает дольше (2026-09-12, прямой запрос: "на
    // клетках с высоким стрессом растения должны восстанавливаться
    // медленнее"). До этого HarvestStress на отрастание не влиял вовсе.
    //
    // Надбавка не подобрана, а выведена из уже существующих констант --
    // свободных параметров здесь нет:
    //
    //     T = T_база + HarvestStress x (HarvestStressIncrement x ПолноеЗарастание)
    //
    // Одной фразой: полностью истощённая клетка отдаёт следующее растение
    // только после того, как земля отпустила ровно один сбор. При дефолтах
    // (база 420 с, сутки 32 мин, зарастание 7 суток = 13440 с, шаг сбора 0.1,
    // биом и сезон 1.0): стресс 0 -- 7 мин без изменений, 0.5 -- 18 мин,
    // 1.0 -- 29 мин (потолок 4.2x).
    //
    // Форма аддитивная, а не T/(1-стресс): вторая расходится при стрессе 1.0
    // и потребовала бы произвольного потолка -- ровно того числа с потолка,
    // которого вывод выше избегает. Выбрано явно (вариант A), ужесточать --
    // по результатам игры.
    //
    // ПолноеЗарастание -- та же функция, что гонит спад HarvestStress в
    // RegenerateCellParameters, не копия, поэтому биом, сезон и Лесное
    // капище наследуются сами: болото держит след дольше -- и отрастает
    // дольше, зимой дольше, весной быстрее.
    //
    // Про одно отставание: OnResourceCollected ставит сбор в очередь
    // (QueueCommand) и зовёт StartRegeneration СРАЗУ, а HarvestStress растёт
    // позже, в ProcessHarvestCommand. Значит здесь виден стресс ДО текущего
    // сбора. Это намеренно: запрошено свойство КЛЕТКИ ("на клетках с высоким
    // стрессом"), а не штраф за само действие, и нетронутая клетка обязана
    // отрастить за базовое время.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float StressStep = Settings ? Settings->HarvestStressIncrement : 0.1f;
    const float Stress = GetCurrentHarvestStress(Cell);

    return BaseSeconds + Stress * StressStep * GetStressRecoverySecondsForCell(Cell);
}

void AGridWorldManager::StartRegeneration(FGridCell& Cell)
{
    // Поресурсно, не по клетке (2026-09-04, "а можно отрастание сделать
    // поресурсно, а не по клеткам?"). Раньше OnResourceCollected звал эту
    // функцию только когда Cell.ResourceActors пустел ДО НУЛЯ, и тогда она
    // отращивала целую новую пачку (WorldRNG.RandRange(Min,Max) заново) --
    // клетка с 3 ресурсами, из которой собрали один, не отращивала ничего,
    // пока не соберут оставшиеся два. Теперь один вызов = один собранный
    // слот = один таймер на один новый ресурс: собрали один из трёх --
    // отрастает именно один, остальные два не тронуты.
    //
    // Время возрождения -- базовое пер-региональное плюс надбавка за
    // истощение клетки, см. GetRegrowthDelaySeconds.
    const float RegrowthTime = GetRegrowthDelaySeconds(Cell);

    // Наблюдаемый счётчик (2026-09-04) -- см. комментарий у поля в
    // HerbalistCoreTypes.h. Растёт здесь, падает в CompleteRegrowth; неудачная
    // попытка ставит новую (2026-09-14), и место остаётся в счёте.
    ++Cell.PendingRegrowthCount;
    ScheduleRegrowthTimer(FIntPoint(Cell.X, Cell.Y), RegrowthTime);
}

void AGridWorldManager::ScheduleRegrowthTimer(const FIntPoint& Coord, float RegrowthTime)
{
    // Координата, а не ссылка на клетку (этап 8в): за минуты ожидания страница
    // клетки может выгрузиться, и ссылка указывала бы в освобождённую память.
    // Поколение (2026-09-14): загрузка сейва перезапускает отрастания из сейва,
    // а таймер, поставленный до неё, отрастил бы растение поверх загруженного.
    // Слабая лямбда (ревью 2026-09-14): таймер, переживший менеджер (Destroy в
    // автотестах, смена уровня), не вызывается у мёртвого объекта.
    const int32 Generation = RegrowthTimerGeneration;
    ++RegrowthTimersScheduled;
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, Coord, RegrowthTime, Generation]()
    {
        OnRegrowthTimer(Coord, RegrowthTime, Generation);
    }), RegrowthTime, false);
}

void AGridWorldManager::OnRegrowthTimer(const FIntPoint& Coord, float RegrowthTime, int32 Generation)
{
    if (Generation != RegrowthTimerGeneration)
    {
        return;
    }
    if (FGridCell* LiveCell = GetCell(Coord.X, Coord.Y))
    {
        CompleteRegrowth(*LiveCell, RegrowthTime);
    }
    else if (IsCellInGrid(Coord.X, Coord.Y))
    {
        // Страница выгружена -- отрастание завершится при её загрузке.
        PendingRegrowthsOnLoad.FindOrAdd(GetCellIndex(Coord.X, Coord.Y)).Add(RegrowthTime);
    }
}

void AGridWorldManager::CompleteRegrowth(FGridCell& Cell, float RegrowthTime)
{
    // Тело таймера StartRegeneration (вынесено 2026-09-12 ради прямой
    // проверки). Спящая клетка актор не получает: SpawnOneResourceInCell
    // сама кладёт выросшее в DormantResourceIDs. Раньше таймер ставил
    // ресурс в клетку, от которой игрок уже ушёл, и тот висел там вечно.
    Cell.PendingRegrowthCount = FMath::Max(Cell.PendingRegrowthCount - 1, 0);

    // Регион мог перестать заявлять клетку или переключиться на PCG-граф
    // за время ожидания (минуты, не тики) -- та же проверка, что
    // SpawnResourcesInCell делает для первичного заселения, здесь нужна
    // явно: SpawnOneResourceInCell её не делает вовсе (её вызывающая
    // сторона решает, применимо ли расти тут в принципе).
    if (!IsCellClaimedByBiomeRegion(Cell)) return;

    ABiomeRegionVolume* Region = GetClaimingRegion(Cell);
    if (Region && !Region->bSpawnResourcesFromGrid) return;

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    // Окно условий на МОМЕНТ отрастания, не на момент сбора (см.
    // комментарий у BuildHarvestContextForCell) -- за 5-10 минут
    // ожидания сезон/луна/погода могли уже смениться.
    const FHarvestContext Context = BuildHarvestContextForCell(Cell);
    const EGardenNiche* PlotNiche = Cell.bIsWater ? nullptr : GardenPlots.Find(FIntPoint(Cell.X, Cell.Y));

    // Повторная попытка (решение пользователя 2026-09-14: «если растение не
    // вернулось -- пробовать снова»). Растение возвращается с вероятностью
    // 1 - HarvestStress, стресс -- на момент срабатывания таймера (решено
    // 2026-09-12: штраф истощения -- меньше растений, а не другой набор трав).
    // Неудача -- броска или места -- не теряет место: через время отрастания при
    // новом стрессе бросок повторяется. На истощённой клетке растений в каждый
    // момент меньше, по мере заживления земли возвращаются все. Стресс -- на
    // сейчас: в спящем чанке он в клетке заморожен до догона (ревью 2026-09-14).
    // Без стресса бросок не нужен -- последовательность WorldRNG мира не
    // сдвигается зря.
    const float ReturnChance = 1.0f - GetCurrentHarvestStress(Cell);
    const bool bReturns = ReturnChance >= 1.0f || WorldRNG.FRand() < ReturnChance;
    if (!bReturns || !SpawnOneResourceInCell(Cell, Context, PlotNiche, Region, IngredientSubsystem))
    {
        StartRegeneration(Cell);
        return;
    }
    {
        // В отличие от исходного броска в InitializeCells (тот безопасно
        // переигрывается заново из RngBaseSeed), это отросшее — не то же
        // самое, что дало бы InitializeCells на старте. Сейв должен его помнить.
        MarkCellDirty(Cell.X, Cell.Y);

        // Найдено 2026-09-06 (прямой запрос пользователя: "проверь что с
        // отрастанием ресурсов") -- реальный спавн логируется только на
        // Verbose (SpawnResourceActor, невидим по умолчанию), а таймер по
        // умолчанию 420с (7 минут) -- ни разу не подтверждалось видимым
        // логом, что отрастание вообще срабатывает. Одна строка на Log,
        // именно на факт УСПЕШНОГО отрастания (не на попытку -- ранние
        // return выше уже покрыты собственными путями, спамить на каждую
        // клетку сетки при обычной игре не должно).
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Regrowth] Cell (%d,%d) regrew a resource after %.0fs%s"),
            Cell.X, Cell.Y, RegrowthTime, IsCellMaterialized(Cell) ? TEXT("") : TEXT(" (chunk asleep, kept as dormant)"));
    }
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
    // Намерение сбора (DESIGN_Community_And_Homestead.md §2.4, PlantSeed,
    // 2026-09-04) -- тот же принцип чтения контроллера, что Tool выше.
    Cmd.Harvest.bForPlanting  = PC && PC->CurrentHarvestIntent == EHarvestIntent::Seed;
    QueueCommand(Cmd);

    // Поресурсно (2026-09-04) -- каждый собранный ресурс запускает СВОЙ
    // таймер отрастания сразу, не дожидаясь, пока опустеет вся клетка.
    // Раньше гейт "Num() == 0" означал, что клетка с несколькими ресурсами
    // (плотность региона поднята выше дефолтных 1-3) вообще не
    // отращивала ничего, пока не соберут буквально всё до последнего --
    // на практике, с широким Min/Max, это почти никогда не наступало.
    StartRegeneration(*Cell);
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
    for (const FGridCell& Cell : GetCellsInGridOrder())
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
    Snapshot.WorldSeed = GetCurrentWorldSeed();
    Snapshot.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    Snapshot.Shrines = Shrines;
    Snapshot.CellSizeCm = CellSize;
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
    for (const FGridCell& Cell : GetCellsInGridOrder())
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

// Сколько секунд клетка со стрессом 1.0 зарастает полностью. Было
// лямбдой внутри RegenerateCellParameters до 2026-09-12; вынесено, когда
// то же число понадобилось StartRegeneration (см. там).
//
// Множители здесь — множители ВРЕМЕНИ, не скорости: больше = дольше
// заживает. Биом — FBiomeRow::StressRecoveryMultiplier (болото со стоячей
// водой держит след дольше, пойма промывает быстрее). Сезон —
// 15_Cycles_And_Shrines.md §15.4: Весна "временный бонус к скорости
// зарастания клеток во всех биомах" (< 1.0, короче срок), Зима
// "клетки заживают медленнее" (> 1.0), Лето и Осень намеренно 1.0 (осень
// нейтральна, решение пользователя 2026-09-16). Сезон УМНОЖАЕТ
// биомный множитель, а не заменяет его.
float AGridWorldManager::GetStressRecoverySecondsForBiome(EBiomeType Biome) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RecoveryDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    const float DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f;

    float Multiplier = 1.0f;
    if (const FBiomeRow* Row = FBiomeDefaults::GetBiomeRow(Biome))
    {
        Multiplier = FMath::Max(Row->StressRecoveryMultiplier, 0.05f);
    }

    switch (GetSeason())
    {
    case ESeason::Spring: Multiplier *= Settings ? Settings->SpringStressRecoveryMultiplier : 0.7f; break;
    case ESeason::Winter: Multiplier *= Settings ? Settings->WinterStressRecoveryMultiplier : 1.6f; break;
    default: break;
    }

    return FMath::Max(RecoveryDays * DaySeconds * Multiplier, KINDA_SMALL_NUMBER);
}

// То же для конкретной клетки. Эффект 3, Лесное капище (Велес, §15.5):
// "ускоряет заживление клеток" — в RegenerateCellParameters это записано как
// умножение СКОРОСТИ на (1 + HealBonus×Restoration); здесь величина
// обратная — время, поэтому ДЕЛЕНИЕ на тот же коэффициент.
float AGridWorldManager::GetStressRecoverySecondsForCell(const FGridCell& Cell) const
{
    float Seconds = GetStressRecoverySecondsForBiome(Cell.Biome);

    if (Shrines.Num() > 0)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const FShrine* DominantShrine = HerbalistCore::Shrine::FindDominantShrine(
            FIntPoint(Cell.X, Cell.Y), Shrines, GetCellRadius(Settings ? Settings->ShrineInfluenceRadiusMeters : 30.0f));
        if (DominantShrine && DominantShrine->Type == EShrineType::Forest && DominantShrine->Restoration > 0.0f)
        {
            const float HealBonus = Settings ? Settings->ShrineForestHealBonus : 0.5f;
            Seconds /= FMath::Max(1.0f + HealBonus * DominantShrine->Restoration, KINDA_SMALL_NUMBER);
        }
    }

    return FMath::Max(Seconds, KINDA_SMALL_NUMBER);
}

float AGridWorldManager::GetCurrentHarvestStress(const FGridCell& Cell) const
{
    // Навечно чистая клетка (Перо Жар-птицы) из релаксации исключена, её стресс
    // не спадает никогда -- а сбор его поднимает. Иначе после десятка сборов
    // шанс вернуться стал бы 0 навсегда (ревью 2026-09-14).
    if (Cell.bEternallyPure)
    {
        return 0.0f;
    }
    const float Stress = FMath::Clamp(Cell.HarvestStress, 0.0f, 1.0f);
    // Стриминг выключен или источников нет -- считается весь мир, стресс в
    // клетке свежий.
    if (Stress <= 0.0f || GetActiveRadiusInChunks() < 0 || ActiveChunkCenters.Num() == 0)
    {
        return Stress;
    }
    // Чанк считался и в прошлом проходе -- спад уже в клетке. Только что
    // активированный ещё не догнан: страница грузится до догона
    // (CatchUpActivatedChunks), и отложенные отрастания срабатывают в ней.
    const FIntPoint Chunk = GetChunkCoordForCell(Cell.X, Cell.Y);
    if (IsCellActive(Cell) && PreviousActiveChunks.Contains(Chunk))
    {
        return Stress;
    }
    // Тот же линейный спад, что в RegenerateCellParameters: полное зарастание --
    // GetStressRecoverySecondsForCell (биом, сезон, Лесное капище).
    const float* LastSimulated = ChunkLastSimulatedGameTime.Find(Chunk);
    const float Elapsed = FMath::Max(GameClockSeconds - (LastSimulated ? *LastSimulated : GridInitGameClock), 0.0f);
    return FMath::Max(Stress - Elapsed / GetStressRecoverySecondsForCell(Cell), 0.0f);
}

void AGridWorldManager::RegenerateCellParameters(float DeltaTime, const FIntPoint* OnlyChunk)
{
    const float DeltaRegen = StateRelaxationPerSecond * DeltaTime;

    // Спад HarvestStress: клетка со стрессом 1.0 полностью зарастает за
    // StressRecoveryGameDays игровых суток, умноженные на множитель биома
    // (болото со стоячей водой держит след дольше, пойма промывает быстрее).
    const UHerbalistSettings* Settings = GetHerbalistSettings();

    // Множитель биома зависит только от типа, а не от клетки — тянем строку
    // DataTable один раз на биом, а не 400 раз за кадр. Сезон одинаков для всей
    // сетки в рамках одного вызова и читается внутри
    // GetStressRecoverySecondsForBiome, поэтому кэш по одному биому
    // остаётся верным на всю длину вызова.
    TMap<EBiomeType, float> StressDecayPerSecond;
    auto GetStressDecay = [&](EBiomeType Biome) -> float
    {
        if (const float* Cached = StressDecayPerSecond.Find(Biome))
        {
            return *Cached;
        }
        const float Decay = 1.0f / FMath::Max(GetStressRecoverySecondsForBiome(Biome), KINDA_SMALL_NUMBER);
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

    // Радиусы в клетках -- один раз на вызов, а не на каждую клетку.
    const int32 ShrineRadiusCells = GetCellRadius(Settings ? Settings->ShrineInfluenceRadiusMeters : 30.0f);
    const int32 SoloveyRadiusCells = GetCellRadius(Settings ? Settings->SoloveyCorruptionRadiusMeters : 30.0f);

    // Стриминг сетки (2026-09-03): релаксация считается только в активных
    // чанках. Неактивная клетка не «портится» и не «чинится», пока до неё
    // никому нет дела; при активации получит догон за всё пропущенное
    // время (CatchUpActivatedChunks) — единичный догоняющий шаг здесь точен,
    // а не приближён, но НЕ по той причине, что была написана раньше
    // ("экспоненциальная форма сходимости"): MoveToward ниже делает ЛИНЕЙНЫЙ
    // шаг фиксированного размера и клампится ровно в цель, поэтому N шагов
    // по dt и один шаг по N*dt дают одно и то же (2026-09-07, проверка
    // математики). У экспоненциальной формы такой эквивалентности как раз
    // НЕ было бы -- если релаксацию когда-нибудь переведут на неё, догон
    // придётся считать иначе. Направление идёт к нормированной цели
    // экспоненциально, через точную долю 1-exp(-k*dt), у которой то же
    // свойство (2026-09-14; раньше там стоял шаг Эйлера k*dt к сырой цели, и
    // догон перелетал её или расходился с непрерывным счётом).
    //
    // Тело вынесено в лямбду, а выбор ОБХОДА — наружу: раньше оба режима
    // (обычный тик и догон одного чанка) шли одним и тем же полным
    // `for (Cell : Cells) { if (!Active) continue; if (chunk mismatch)
    // continue; ... }` — при 250 000 клеток это стократная переплата что
    // при обычном тике (активны сотни-тысячи), что при догоне ОДНОГО чанка
    // (активны десятки-сотни). ForEachActiveCell/ForEachCellInChunk идут
    // прямо по нужному диапазону, не касаясь остальной сетки вовсе.
    auto ProcessCell = [&](FGridCell& Cell)
    {
        // Перо Жар-птицы (16_Entity_Manifestation.md §16.4, 2026-09-02) —
        // клетка, помеченная навечно чистой, полностью исключена из этой
        // функции: ни бистабильная релаксация, ни заражение соседей, ни
        // обычная релаксация к TargetState её не трогают — заморожена на
        // текущем значении, как и просит §16.4 ("заморозь TargetState/
        // State на текущем значении"). Заражение соседей всё ещё может
        // толкать её TargetState ИЗВНЕ (см. guard у Neighbor ниже, отдельно).
        // return, не continue -- тело функции стало лямбдой (ForEachActiveCell/
        // ForEachCellInChunk, 2026-09-03), continue вне реального цикла не
        // компилируется (C2044); return из void-лямбды даёт тот же эффект
        // "пропустить эту клетку и перейти к следующей".
        if (Cell.bEternallyPure) return;

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
                // возвращается к здоровому умолчанию клетки (у воды -- смесь
                // умолчаний воды её биомов, GetCellDefaultState).
                const FRealState Healthy = GetCellDefaultState(Cell);
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
            // Ставка на опорную клетку, пересчитанная на клетку сетки: фронт в
            // метрах (решение пользователя 12, довод у ContagionSpreadRate).
            const float ReferenceRate = Settings ? Settings->ContagionSpreadRate : 0.00045f;
            const float ContagionRate = CellSize > 0.0f
                ? ReferenceRate * UHerbalistSettings::ContagionReferenceCellMeters * 100.0f / CellSize
                : ReferenceRate;
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

                    // Точное сравнение, не KINDA_SMALL_NUMBER (ревью 2026-09-13):
                    // функция идёт каждый кадр с реальным DeltaTime, и толчок за
                    // кадр на клетке 9 м -- 0.0005 × 1/60 с ≈ 8e-6, меньше эпсилона.
                    // С эпсилоном запись пропускалась бы каждый кадр, и фронт в игре
                    // стоял бы. Насыщенный сосед (зажат в 0 или 1) не пишется и так.
                    if (NewCorruption != Neighbor->TargetState.Meta.Corruption ||
                        NewPurity     != Neighbor->TargetState.Meta.Purity     ||
                        NewDistortion != Neighbor->TargetState.Meta.Distortion ||
                        NewStability  != Neighbor->TargetState.Meta.Stability)
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
            ? HerbalistCore::Shrine::FindDominantShrine(FIntPoint(Cell.X, Cell.Y), Shrines, ShrineRadiusCells)
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
            // Точное сравнение -- та же причина, что у заражения выше (ревью
            // 2026-09-13): при Restoration ниже ~0.3 толчок за кадр 1/60 с
            // меньше KINDA_SMALL_NUMBER и с эпсилоном не записывался бы.
            if (NewTargetPurity != Cell.TargetState.Meta.Purity)
            {
                Cell.TargetState.Meta.Purity = NewTargetPurity;
                MarkCellDirty(Cell.X, Cell.Y);
            }
        }

        // Горюч-камень (§4.5, "упрямый камень", 2026-09-06, юнит 2/2 POI) —
        // аномальное сопротивление сдвигу TargetState "в любую сторону, не
        // только к лучшему": та же точка приложения, что уже модуляция
        // капища выше (CellDeltaRegen/StabilityDeltaRegen/
        // DirectionRateMultiplier — насколько БЫСТРО State следует за
        // TargetState), но множитель СИММЕТРИЧЕН (не различает "к S0" или
        // "от S0", в отличие от капища) и применяется только к одной
        // конкретной клетке, не всей сетке — не тот класс риска, что полная
        // миграция Morok/Zaryana на read-time оверлей (ROADMAP.md,
        // "Архитектурный долг"): контагион/гистерезис/капища по-прежнему
        // пишут в TargetState этой клетки как обычно, сопротивление гасит
        // только скорость, с которой State за ней угонится.
        if (HerbalistCore::IsValidCell(GoryuchKamenSite) && Cell.X == GoryuchKamenSite.X && Cell.Y == GoryuchKamenSite.Y)
        {
            const float Resistance = Settings ? Settings->GoryuchKamenShiftResistance : 0.5f;
            CellDeltaRegen *= Resistance;
            StabilityDeltaRegen *= Resistance;
            DirectionRateMultiplier *= Resistance;
        }

        // Соловей-разбойник, постоянная зона порчи (§4.4, DESIGN_POI_Art_
        // And_LevelDesign.md §4, 2026-09-06) — "земля... визибельно
        // порченая... постоянный, не разовый признак". Потолок
        // TargetState.Meta.Purity в радиусе точки, пока не усмирён
        // (bSoloveyCalmed) плакун-травой — не растущий нудж, просто снимает
        // TargetState обратно к потолку, если он выше; сравнение перед
        // записью, тот же §7.1 паттерн, что у контагиона выше в этой функции.
        if (HerbalistCore::IsValidCell(SoloveySite) && !bSoloveyCalmed)
        {
            const int32 SoloveyRadius = SoloveyRadiusCells;
            const int32 SoloveyDist = FMath::Max(FMath::Abs(Cell.X - SoloveySite.X), FMath::Abs(Cell.Y - SoloveySite.Y));
            if (SoloveyDist <= SoloveyRadius)
            {
                const float Ceiling = Settings ? Settings->SoloveyAmbientPurityCeiling : 0.5f;
                if (Cell.TargetState.Meta.Purity > Ceiling)
                {
                    Cell.TargetState.Meta.Purity = Ceiling;
                    MarkCellDirty(Cell.X, Cell.Y);
                }
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
        // Цель -- доли осей (ревью 2026-09-14). У TargetState.Direction сумма
        // не обязана быть 1: ночной толчок Spirit, Низшие и хозяева мест
        // прибавляют к оси без нормировки (ApplyLandmarkAxisNudge). Состояние же
        // нормируется каждый шаг и до сырой цели не доходило никогда: отклонение
        // не падало ниже порога, клетка вечно считалась тронутой, а догон
        // спящего чанка расходился с непрерывным счётом (NormalizeSum делал шаг
        // нелинейным). Скорость теперь k, а не k на сумму осей цели.
        FDirection TargetDir = T.Direction;
        TargetDir.NormalizeSum();
        const float DirDeviation = FMath::Abs(S.Direction.Body - TargetDir.Body)
                                  + FMath::Abs(S.Direction.Mind - TargetDir.Mind)
                                  + FMath::Abs(S.Direction.Spirit - TargetDir.Spirit)
                                  + FMath::Abs(S.Direction.Nature - TargetDir.Nature);
        if (DirDeviation > KINDA_SMALL_NUMBER)
        {
            // Доля пути к цели за DeltaTime -- точное решение затухания
            // dS/dt = k(T-S), а не шаг Эйлера k*dt (2026-09-14): догон
            // проснувшегося чанка зовёт эту функцию одним шагом на всё время
            // сна (CatchUpActivatedChunks), и при k*dt > 1 направление
            // перелетало цель, упиралось в клампы и нормализовалось в
            // произвольное значение. На обычном шаге 0.1 с доля отличается от
            // Эйлера на (k*dt)^2/2 -- 5e-7 при k = 0.01; к нормированной цели N
            // шагов по dt дают тот же итог, что один по N*dt.
            const float DirRate = 0.01f * DirectionRateMultiplier;
            const float DirAlpha = 1.0f - FMath::Exp(-DirRate * DeltaTime);
            S.Direction.Body   = FMath::Clamp(S.Direction.Body   + (TargetDir.Body   - S.Direction.Body)   * DirAlpha, 0.0f, 1.0f);
            S.Direction.Mind   = FMath::Clamp(S.Direction.Mind   + (TargetDir.Mind   - S.Direction.Mind)   * DirAlpha, 0.0f, 1.0f);
            S.Direction.Spirit = FMath::Clamp(S.Direction.Spirit + (TargetDir.Spirit - S.Direction.Spirit) * DirAlpha, 0.0f, 1.0f);
            S.Direction.Nature = FMath::Clamp(S.Direction.Nature + (TargetDir.Nature - S.Direction.Nature) * DirAlpha, 0.0f, 1.0f);
            S.Direction.NormalizeSum();
        }

        // 4. Синхронизация памяти клетки (для графа и тултипа)
        Cell.Memory.AccumulatedDistortion = S.Meta.Distortion;

        // Сохранения (Core/Save/): релаксация — единственный писатель State/
        // HarvestStress вне ApplyStateDelta, помечаем клетку тронутой отдельно.
        // Переход гистерезиса -- тоже изменение (ревью этапа 7): у клетки,
        // стоящей ровно на цели, иначе флаг распада не попал бы ни в сейв, ни
        // в сводку чанка.
        if (bChanged || DirDeviation > KINDA_SMALL_NUMBER || bStressDecaying || Cell.Memory.bDegrading != bWasDegrading)
        {
            MarkCellDirty(Cell.X, Cell.Y);
        }
    };

    if (OnlyChunk)
    {
        ForEachCellInChunk(*OnlyChunk, ProcessCell);
    }
    else
    {
        ForEachActiveCell(ProcessCell);
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
                // Абсолютный уровень, не поле (2026-09-07): MorokField теперь
                // знаковое отклонение, и зелёный означал бы "биом в своей
                // природе" одинаково и для Тайги, и для Болота -- отладка
                // перестала бы показывать, где реально грязно.
                const float AmbientMorok = Graph->GetAmbientMorok(Pair.Key);
                FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, AmbientMorok).ToFColor(false);
                DrawDebugSphere(World, *Pos, 30.0f, 12, Color, false, 0.0f, 0, 2.0f);
                DrawDebugString(World, *Pos + FVector(0, 0, 50.0f), Pair.Key.ToString(), nullptr, FColor::White, 0.0f, true, 1.2f);
            }
        }
    }

    if (bShowCellDistortion || bShowCellInfluence)
    {
        for (const FGridCell& Cell : GetCellsInGridOrder())
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