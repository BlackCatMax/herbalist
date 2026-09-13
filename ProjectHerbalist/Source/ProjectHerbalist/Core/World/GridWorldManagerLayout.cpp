// Core/World/GridWorldManagerLayout.cpp
//
// Разметка мира на менеджере (2026-09-12, DESIGN_World_Layout.md, этап 1):
// сбор исходных величин в редакторе, пересчёт разметки, применение к сетке,
// сверка с тем, что видно в игре, и одна строка в лог. Решатель --
// Core/World/WorldLayout.h.
//
// Начало сетки при выведенной разметке -- GetGridOrigin(), а не положение
// актора: у C++-класса менеджера нет корневого компонента, и
// SetActorLocation у заспавненного менеджера молча ничего не делает (нашёл
// тест BakedSourceShapesGridAtBeginPlay, подтвердило ревью). Актор разметка не
// двигает вовсе -- заодно нет перемещения внутри PreSave и нет положения,
// которое отмена в редакторе не вернула бы.
//
// Незапечённый менеджер (ландшафта в исходных величинах нет) ведёт себя как
// до разметки: GridSizeX/GridSizeY/CellSize -- ручные, начало -- положение
// актора, чанк -- из настроек. Так живут все автотесты, спавнящие менеджер в
// пустом месте.

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "HerbalistLogChannels.h"
#include "Landscape.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/ObjectSaveContext.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeCellData.h"

#if WITH_EDITOR
#include "LandscapeInfo.h"
#include "LandscapeStreamingProxy.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#endif

float AGridWorldManager::GetLongestLocalMechanicMeters()
{
    // Самый дальнобойный локальный механизм -- разрежение сущностей: соседей
    // одного вида он ищет на MinSpacingMeters. Та же выборка, что у теста
    // ActiveRadiusCoversTheLongestLocalMechanic: добавят карточку с большей
    // дистанцией -- чанк подстроится сам.
    float Longest = 0.0f;
    for (const FAmbientEntityDefinition& Definition : GetAmbientEntityDefinitions())
    {
        Longest = FMath::Max(Longest, Definition.MinSpacingMeters);
    }
    return Longest;
}

int32 AGridWorldManager::GetChunkSizeInCells() const
{
    if (ResolvedLayout.bValid && ResolvedLayout.ChunkSizeInCells > 0)
    {
        return ResolvedLayout.ChunkSizeInCells;
    }
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    return FMath::Max(1, Settings ? Settings->ChunkSizeInCells : 32);
}

bool AGridWorldManager::ResolveWorldLayout(TArray<FString>& OutWarnings)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RadiusMeters = Settings ? Settings->ActiveSimulationRadiusMeters : -1.0f;
    ResolvedLayout = FWorldLayoutSolver::Resolve(BakedLayoutSource, LayoutOverrides, RadiusMeters,
        GetLongestLocalMechanicMeters(), OutWarnings);
    return ResolvedLayout.bValid;
}

FVector2D AGridWorldManager::GetLayoutGridOrigin() const
{
    return ResolvedLayout.Anchor + FVector2D(ResolvedLayout.MinCell) * ResolvedLayout.CellSizeCm;
}

FVector AGridWorldManager::GetGridOrigin() const
{
    if (ResolvedLayout.bValid)
    {
        const FVector2D Origin = GetLayoutGridOrigin();
        return FVector(Origin.X, Origin.Y, GetActorLocation().Z);
    }
    return GetActorLocation();
}

void AGridWorldManager::ApplyResolvedLayout()
{
    if (!ResolvedLayout.bValid)
    {
        return;
    }
    CellSize = static_cast<float>(ResolvedLayout.CellSizeCm);
    GridSizeX = ResolvedLayout.GridSize.X;
    GridSizeY = ResolvedLayout.GridSize.Y;
}

bool AGridWorldManager::IsGridMatchingResolvedLayout() const
{
    if (!ResolvedLayout.bValid)
    {
        return true;
    }
    return FMath::IsNearlyEqual(static_cast<double>(CellSize), ResolvedLayout.CellSizeCm, 0.01)
        && GridSizeX == ResolvedLayout.GridSize.X
        && GridSizeY == ResolvedLayout.GridSize.Y;
}

void AGridWorldManager::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // Итог разметки не сохраняется (Transient) -- пересчитывается здесь, до
    // BeginPlay любого актора уровня: ресурсы и места силы, стартующие раньше
    // менеджера, уже ищут свои клетки по размеру и началу сетки.
    TArray<FString> Warnings;
    if (ResolveWorldLayout(Warnings))
    {
        ApplyResolvedLayout();
    }
}

void AGridWorldManager::VerifyWorldLayoutAgainstWorld(TArray<FString>& OutWarnings) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Ландшафт: родительский ALandscape не загружается пространственно, квад и
    // компонент видны и в собранной игре.
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        const ALandscape* Landscape = *It;
        const double QuadCm = FMath::Abs(Landscape->GetActorScale3D().X);
        if (!FMath::IsNearlyEqual(QuadCm, BakedLayoutSource.QuadSizeCm, 0.01)
            || Landscape->ComponentSizeQuads != BakedLayoutSource.ComponentSizeQuads)
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Ландшафт в игре (квад %.0f см, компонент %d) не совпадает с запечённым (квад %.0f см, компонент %d) -- пересохраните разметку"),
                QuadCm, Landscape->ComponentSizeQuads, BakedLayoutSource.QuadSizeCm, BakedLayoutSource.ComponentSizeQuads));
        }
        break;
    }

    // Сетка стриминга. В собранной игре CellSize и Origin разбиения не
    // существуют (данные редактора), зато у каждой ячейки есть имя разбиения
    // и уровень LH-сетки: ячейка уровня L в 2^L раз шире ячейки уровня 0, а её
    // угол лежит на целом числе таких ячеек от начала сетки.
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!BakedLayoutSource.bHasStreamingGrid || !WorldPartition || !WorldPartition->RuntimeHash
        || !WorldPartition->IsStreamingEnabled() || !World->IsGameWorld())
    {
        return;
    }

    const FName GridName = BakedLayoutSource.StreamingGridName;
    const double BakedCellCm = BakedLayoutSource.StreamingCellSizeCm;
    const FVector2D BakedOrigin = BakedLayoutSource.StreamingGridOrigin;
    int32 CheckedCells = 0;
    double MismatchedCellCm = -1.0;
    bool bOriginMismatch = false;
    WorldPartition->RuntimeHash->ForEachStreamingCells([&](const UWorldPartitionRuntimeCell* Cell)
    {
        if (!Cell || Cell->GetIsHLOD() || !Cell->IsSpatiallyLoaded() || !Cell->RuntimeCellData)
        {
            return true;
        }
        const UWorldPartitionRuntimeCellData* Data = Cell->RuntimeCellData;
        // У пространственно не загружаемых ячеек уровень -- MAX_int32.
        if (Data->GridName != GridName || Data->HierarchicalLevel < 0 || Data->HierarchicalLevel > 24)
        {
            return true;
        }
        const FBox Bounds = Cell->GetCellBounds();
        if (!Bounds.IsValid)
        {
            return true;
        }
        ++CheckedCells;
        const double LevelCellCm = Bounds.GetSize().X;
        const double LevelZeroCm = LevelCellCm / static_cast<double>(1 << Data->HierarchicalLevel);
        if (!FMath::IsNearlyEqual(LevelZeroCm, BakedCellCm, 0.5))
        {
            MismatchedCellCm = LevelZeroCm;
            return false;
        }
        const double RemainderX = FMath::Fmod(Bounds.Min.X - BakedOrigin.X, LevelCellCm);
        const double RemainderY = FMath::Fmod(Bounds.Min.Y - BakedOrigin.Y, LevelCellCm);
        auto IsOnGrid = [LevelCellCm](double Remainder)
        {
            const double Absolute = FMath::Abs(Remainder);
            return Absolute <= 0.5 || FMath::Abs(Absolute - LevelCellCm) <= 0.5;
        };
        if (!IsOnGrid(RemainderX) || !IsOnGrid(RemainderY))
        {
            bOriginMismatch = true;
            return false;
        }
        return true;
    });

    if (MismatchedCellCm >= 0.0)
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Ячейка разбиения %s в игре %.0f см, запечённая -- %.0f см: сетка World Partition изменилась -- пересохраните разметку"),
            *GridName.ToString(), MismatchedCellCm, BakedCellCm));
    }
    else if (bOriginMismatch)
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Ячейки разбиения %s не лежат на запечённом начале сетки (%.0f, %.0f) -- начало изменилось, пересохраните разметку"),
            *GridName.ToString(), BakedOrigin.X, BakedOrigin.Y));
    }
    else if (CheckedCells == 0)
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Ячеек разбиения %s в игре не найдено -- имя разбиения ландшафта изменилось?"), *GridName.ToString()));
    }
}

void AGridWorldManager::InitializeWorldLayoutForPlay()
{
    TArray<FString> Warnings;
    const bool bValid = ResolveWorldLayout(Warnings);
    const UWorld* World = GetWorld();
    const bool bGameWorld = World && World->IsGameWorld();

    if (!bValid)
    {
        // Незапечённый менеджер -- все автотесты и карты без ландшафта. В
        // игровом мире это стоит видеть, в мире автотеста -- шум.
        if (bGameWorld)
        {
            UE_LOG(LogHerbalistWorld, Log,
                TEXT("[Layout] Разметка мира не выведена -- сетка ручная: %dx%d по %.0f см от (%.0f, %.0f)"),
                GridSizeX, GridSizeY, CellSize, GetActorLocation().X, GetActorLocation().Y);
            if (BakedLayoutSource.bHasLandscape)
            {
                for (const FString& Warning : Warnings)
                {
                    UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] %s"), *Warning);
                }
            }
        }
        return;
    }

    if (!IsGridMatchingResolvedLayout())
    {
        Warnings.Add(FString::Printf(
            TEXT("Поля сетки на акторе (%dx%d по %.0f см) разошлись с разметкой -- применена разметка; нажмите «Сверить с World Partition» и сохраните карту"),
            GridSizeX, GridSizeY, CellSize));
    }
    ApplyResolvedLayout();
    VerifyWorldLayoutAgainstWorld(Warnings);

    if (bGameWorld)
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Layout] %s"), *FWorldLayoutSolver::Describe(ResolvedLayout));
        for (const FString& Warning : Warnings)
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] %s"), *Warning);
        }
    }
}

int32 AGridWorldManager::GetCellRadius(float Meters) const
{
    return FWorldLayoutSolver::MetersToCellRadius(Meters, CellSize);
}

FRandomStream AGridWorldManager::MakeCellRandomStream(int32 X, int32 Y, FWorldLayoutSolver::ECellRandomPurpose Purpose, int32 Salt) const
{
    // Координаты клеток уже глобальные (этап 6).
    return FRandomStream(FWorldLayoutSolver::MakeCellSeed(RngBaseSeed, FIntPoint(X, Y), Purpose, Salt));
}

void AGridWorldManager::SyncWithWorldPartition()
{
#if WITH_EDITOR
    // Разметка задаёт номера клеток (этап 6): пересчёт на уже созданной сетке
    // перенумеровал бы живые клетки (найдено ревью). Сверка -- в редакторе,
    // до запуска игры.
    const UWorld* CurrentWorld = GetWorld();
    if ((CurrentWorld && CurrentWorld->IsGameWorld()) || Cells.Num() > 0)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] Сверка с World Partition недоступна у запущенной сетки: разметка задаёт номера её клеток"));
        return;
    }

    TArray<FString> Warnings;
    const bool bChanged = SyncWorldLayoutFromWorld(GetWorld(), Warnings, /*bMarkModified=*/true);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Layout] Сверка с World Partition%s: %s"),
        bChanged ? TEXT(" (разметка изменилась)") : TEXT(""), *FWorldLayoutSolver::Describe(ResolvedLayout));
    for (const FString& Warning : Warnings)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] %s"), *Warning);
    }
#else
    UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] Сверка с World Partition доступна только в редакторе"));
#endif
}

void AGridWorldManager::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
#if WITH_EDITOR
    // До Super::PreSave: движок обновляет дескриптор внешнего актора из
    // UObject::PreSave (OnObjectPreSave), и поля, изменённые позже, попали бы
    // в дескриптор только при следующем сохранении. Кук и процедурные
    // сохранения пропускаются -- там мир не тот, что видит автор.
    const UWorld* World = GetWorld();
    if (bSyncLayoutOnSave && !ObjectSaveContext.IsCooking() && !ObjectSaveContext.IsProceduralSave()
        && !IsTemplate() && World && World->WorldType == EWorldType::Editor)
    {
        TArray<FString> Warnings;
        if (SyncWorldLayoutFromWorld(GetWorld(), Warnings, /*bMarkModified=*/false))
        {
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Layout] Разметка пересчитана при сохранении: %s"),
                *FWorldLayoutSolver::Describe(ResolvedLayout));
        }
        for (const FString& Warning : Warnings)
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Layout] %s"), *Warning);
        }
    }
#endif
    Super::PreSave(ObjectSaveContext);
}

#if WITH_EDITOR

namespace
{
    // Чтение приватного свойства-структуры через рефлексию. Константной
    // перегрузки ContainerPtrToValuePtr в 5.7 нет, объект не изменяется.
    const FVector* ReadVectorProperty(const UObject* Object, const TCHAR* PropertyName)
    {
        const FStructProperty* Property = Object ? FindFProperty<FStructProperty>(Object->GetClass(), PropertyName) : nullptr;
        if (!Property || Property->Struct != TBaseStructure<FVector>::Get())
        {
            return nullptr;
        }
        return Property->ContainerPtrToValuePtr<FVector>(const_cast<UObject*>(Object));
    }

    // Сетки старого UWorldPartitionRuntimeSpatialHash: Grids -- приватный
    // массив данных редактора. Разбирается через рефлексию по именам полей
    // FSpatialHashRuntimeGrid (GridName: FName, CellSize: int32, LoadingRange:
    // float, Origin: FVector2D).
    bool ReadSpatialHashGrid(const UWorldPartitionRuntimeSpatialHash* SpatialHash, FName WantedGrid,
        FHerbalistWorldLayoutSource& OutSource)
    {
        const FArrayProperty* GridsProperty = FindFProperty<FArrayProperty>(UWorldPartitionRuntimeSpatialHash::StaticClass(), TEXT("Grids"));
        const FStructProperty* GridStruct = GridsProperty ? CastField<FStructProperty>(GridsProperty->Inner) : nullptr;
        if (!GridStruct)
        {
            return false;
        }
        const FNameProperty* NameProperty = FindFProperty<FNameProperty>(GridStruct->Struct, TEXT("GridName"));
        const FIntProperty* CellSizeProperty = FindFProperty<FIntProperty>(GridStruct->Struct, TEXT("CellSize"));
        const FFloatProperty* RangeProperty = FindFProperty<FFloatProperty>(GridStruct->Struct, TEXT("LoadingRange"));
        const FStructProperty* OriginProperty = FindFProperty<FStructProperty>(GridStruct->Struct, TEXT("Origin"));
        if (!NameProperty || !CellSizeProperty || !RangeProperty)
        {
            return false;
        }

        FScriptArrayHelper Grids(GridsProperty, GridsProperty->ContainerPtrToValuePtr<void>(const_cast<UWorldPartitionRuntimeSpatialHash*>(SpatialHash)));
        for (int32 Index = 0; Index < Grids.Num(); ++Index)
        {
            const uint8* Grid = Grids.GetRawPtr(Index);
            const FName Name = NameProperty->GetPropertyValue_InContainer(Grid);
            if (!WantedGrid.IsNone() && Name != WantedGrid)
            {
                continue;
            }
            OutSource.bHasStreamingGrid = true;
            OutSource.StreamingGridName = Name;
            OutSource.StreamingCellSizeCm = CellSizeProperty->GetPropertyValue_InContainer(Grid);
            OutSource.StreamingLoadingRangeCm = RangeProperty->GetPropertyValue_InContainer(Grid);
            if (OriginProperty && OriginProperty->Struct == TBaseStructure<FVector2D>::Get())
            {
                OutSource.StreamingGridOrigin = *OriginProperty->ContainerPtrToValuePtr<FVector2D>(const_cast<uint8*>(Grid));
            }
            return true;
        }
        return false;
    }
}

FHerbalistWorldLayoutSource AGridWorldManager::GatherWorldLayoutSource(UWorld* World, TArray<FString>& OutWarnings)
{
    FHerbalistWorldLayoutSource Source;
    if (!World)
    {
        return Source;
    }

    // ---- Ландшафт ----
    // Родительский ALandscape не загружается пространственно (у него
    // CanChangeIsSpatiallyLoadedFlag = false), его трансформ и размеры в
    // квадах есть всегда.
    ALandscape* Landscape = nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (!Landscape)
        {
            Landscape = *It;
            continue;
        }
        if (!FMath::IsNearlyEqual(FMath::Abs(It->GetActorScale3D().X), FMath::Abs(Landscape->GetActorScale3D().X), 0.01)
            || It->ComponentSizeQuads != Landscape->ComponentSizeQuads)
        {
            Source.bLandscapesDisagree = true;
        }
    }

    if (Landscape)
    {
        const FVector Scale = Landscape->GetActorScale3D();
        const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();

        // Полный экстент в квадах -- из ULandscapeInfo. В отличие от
        // GetCompleteBounds, он не смешивает загруженные прокси с дескрипторами
        // и не зависит от того, что сейчас подгружено в редакторе.
        const ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
        const FIntRect Extent = Info ? Info->GetCompleteLandscapeExtent() : FIntRect();
        FVector CornerA = FVector::ZeroVector;
        FVector CornerB = FVector::ZeroVector;
        bool bHasExtent = false;
        if (Extent.Width() > 0 && Extent.Height() > 0)
        {
            CornerA = LandscapeToWorld.TransformPosition(FVector(Extent.Min.X, Extent.Min.Y, 0.0));
            CornerB = LandscapeToWorld.TransformPosition(FVector(Extent.Max.X, Extent.Max.Y, 0.0));
            bHasExtent = true;
        }
        else
        {
            const FBox Bounds = Landscape->GetCompleteBounds();
            if (Bounds.IsValid)
            {
                CornerA = Bounds.Min;
                CornerB = Bounds.Max;
                bHasExtent = true;
                OutWarnings.Add(TEXT("Экстент ландшафта в квадах не прочитан -- границы взяты по GetCompleteBounds"));
            }
        }

        if (bHasExtent)
        {
            Source.bHasLandscape = true;
            Source.QuadSizeCm = FMath::Abs(Scale.X);
            Source.ComponentSizeQuads = Landscape->ComponentSizeQuads;
            const FVector Vertex = LandscapeToWorld.TransformPosition(FVector::ZeroVector);
            Source.LandscapeOrigin = FVector2D(Vertex.X, Vertex.Y);
            Source.LandscapeMin = FVector2D(FMath::Min(CornerA.X, CornerB.X), FMath::Min(CornerA.Y, CornerB.Y));
            Source.LandscapeMax = FVector2D(FMath::Max(CornerA.X, CornerB.X), FMath::Max(CornerA.Y, CornerB.Y));

            if (Scale.X < 0.0 || Scale.Y < 0.0)
            {
                OutWarnings.Add(TEXT("Масштаб ландшафта отрицательный -- квад взят по модулю"));
            }
            if (!FMath::IsNearlyEqual(FMath::Abs(Scale.X), FMath::Abs(Scale.Y), 0.01))
            {
                OutWarnings.Add(TEXT("Квады ландшафта не квадратные -- разметка считает по масштабу X"));
            }
            if (!Landscape->GetActorRotation().IsNearlyZero())
            {
                OutWarnings.Add(TEXT("Ландшафт повёрнут -- сетка Herbalist его поворот не учитывает"));
            }
        }
        else
        {
            OutWarnings.Add(TEXT("Границы ландшафта пусты -- разметка не выводится"));
        }
    }

    // ---- World Partition ----
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition || !WorldPartition->RuntimeHash)
    {
        return Source;
    }
    if (!WorldPartition->IsStreamingEnabled())
    {
        OutWarnings.Add(TEXT("Стриминг World Partition на карте выключен -- страница равна компоненту ландшафта"));
        return Source;
    }

    // Разбиение, в которое попадает ЗЕМЛЯ. Родительский ALandscape сам в
    // ячейки стриминга не попадает -- попадают его прокси, и RuntimeGrid у
    // каждого прокси свой. Берём самое частое значение по дескрипторам прокси;
    // None -- разбиение по умолчанию (первое в списке).
    FName LandscapeGrid = Landscape ? Landscape->GetRuntimeGrid() : NAME_None;
    TMap<FName, int32> ProxyGrids;
    FWorldPartitionHelpers::ForEachActorDescInstance<ALandscapeStreamingProxy>(WorldPartition,
        [&ProxyGrids](const FWorldPartitionActorDescInstance* ActorDescInstance)
    {
        ++ProxyGrids.FindOrAdd(ActorDescInstance->GetRuntimeGrid());
        return true;
    });
    if (!ProxyGrids.IsEmpty())
    {
        int32 BestCount = 0;
        for (const TPair<FName, int32>& Pair : ProxyGrids)
        {
            if (Pair.Value > BestCount)
            {
                BestCount = Pair.Value;
                LandscapeGrid = Pair.Key;
            }
        }
        if (ProxyGrids.Num() > 1)
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Прокси ландшафта лежат в %d разных разбиениях -- взято самое частое: %s"),
                ProxyGrids.Num(), *LandscapeGrid.ToString()));
        }
    }

    if (const UWorldPartitionRuntimeHashSet* HashSet = Cast<UWorldPartitionRuntimeHashSet>(WorldPartition->RuntimeHash))
    {
        const URuntimePartition* Partition = HashSet->ResolveRuntimePartition(LandscapeGrid, /*bMainPartitionLayer=*/true);
        if (const URuntimePartitionLHGrid* LHGrid = Cast<URuntimePartitionLHGrid>(Partition))
        {
            Source.bHasStreamingGrid = true;
            Source.StreamingGridName = LHGrid->Name;
            Source.StreamingCellSizeCm = LHGrid->GetCellSize();
            Source.StreamingLoadingRangeCm = LHGrid->LoadingRange;
            // Origin -- приватное поле данных редактора без геттера.
            if (const FVector* Origin = ReadVectorProperty(LHGrid, TEXT("Origin")))
            {
                Source.StreamingGridOrigin = FVector2D(Origin->X, Origin->Y);
            }
            else
            {
                OutWarnings.Add(TEXT("Начало LH-сетки не прочитано -- взят ноль"));
            }
        }
        else if (Partition)
        {
            OutWarnings.Add(FString::Printf(
                TEXT("Разбиение ландшафта %s -- не LH-сетка, её ячейки разметка не понимает"), *Partition->Name.ToString()));
        }
        else
        {
            OutWarnings.Add(FString::Printf(TEXT("Разбиение %s не найдено"), *LandscapeGrid.ToString()));
        }
    }
    else if (const UWorldPartitionRuntimeSpatialHash* SpatialHash = Cast<UWorldPartitionRuntimeSpatialHash>(WorldPartition->RuntimeHash))
    {
        if (!ReadSpatialHashGrid(SpatialHash, LandscapeGrid, Source))
        {
            OutWarnings.Add(TEXT("Сетка SpatialHash, в которой лежит ландшафт, не прочитана"));
        }
    }
    else
    {
        OutWarnings.Add(FString::Printf(
            TEXT("Хэш World Partition %s не поддерживается разметкой"), *WorldPartition->RuntimeHash->GetClass()->GetName()));
    }

    return Source;
}

bool AGridWorldManager::SyncWorldLayoutFromWorld(UWorld* World, TArray<FString>& OutWarnings, bool bMarkModified)
{
    const FHerbalistWorldLayoutSource NewSource = GatherWorldLayoutSource(World, OutWarnings);
    const bool bSourceChanged = !FWorldLayoutSolver::IsSameSource(NewSource, BakedLayoutSource);

    // Итог пересчитывается всегда: он не сохраняется, а настройки радиуса
    // могли измениться. Меняется ли что-то на акторе -- исходные величины и
    // поля сетки.
    const FHerbalistWorldLayoutSource PreviousSource = BakedLayoutSource;
    if (bSourceChanged)
    {
        BakedLayoutSource = NewSource;
    }
    ResolveWorldLayout(OutWarnings);
    const bool bGridChanged = ResolvedLayout.bValid && !IsGridMatchingResolvedLayout();
    if (!bSourceChanged && !bGridChanged)
    {
        return false;
    }

    if (bMarkModified)
    {
        // Modify() до изменения полей, чтобы отмена в редакторе вернула прежнее.
        // Актор не двигается -- положение корневого компонента не трогается.
        BakedLayoutSource = PreviousSource;
        Modify();
        BakedLayoutSource = bSourceChanged ? NewSource : PreviousSource;
        TArray<FString> Ignored;
        ResolveWorldLayout(Ignored);
    }
    ApplyResolvedLayout();
    return true;
}

#endif // WITH_EDITOR
