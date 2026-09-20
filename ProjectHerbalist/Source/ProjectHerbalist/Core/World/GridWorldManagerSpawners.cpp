// GridWorldManagerSpawners.cpp
//
// Спавнеры Низших (DESIGN_Entity_Spawners.md, решения пользователя
// 2026-09-20). Этап 1: спавнер выбирает вид и выпускает бродящих особей,
// эффекта на клетки у них ещё нет -- его по-прежнему даёт клеточный путь в
// GridWorldManagerEntities.cpp, пока выключен UHerbalistSettings::
// bUseAmbientSpawners. Оба пути разом не работают: с включённым флагом
// клеточный цикл Низших пропускается.
#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntitySpawner.h"
#include "Core/Entities/AmbientEntityActor.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Engine/World.h"
#include "Templates/TypeHash.h"

namespace
{
    // Клетка-кандидат своего квадрата. Детерминированно от координат блока --
    // тот же приём «сиды от клетки», что уже у ресурсов: тот же мир даёт те
    // же спавнеры, хранить их не нужно.
    FIntPoint GetAutoSpawnerCellForBlock(const FIntPoint& Block, int32 BlockCells)
    {
        const uint32 Hash = HashCombine(GetTypeHash(Block), 20260920u);
        const int32 OffsetX = static_cast<int32>(Hash % static_cast<uint32>(BlockCells));
        const int32 OffsetY = static_cast<int32>((Hash / 7919u) % static_cast<uint32>(BlockCells));
        return FIntPoint(Block.X * BlockCells + OffsetX, Block.Y * BlockCells + OffsetY);
    }
}

void AGridWorldManager::RegisterAmbientSpawner(AAmbientEntitySpawner* Spawner)
{
    if (!Spawner) return;
    ManualAmbientSpawners.AddUnique(Spawner);
}

void AGridWorldManager::UnregisterAmbientSpawner(AAmbientEntitySpawner* Spawner)
{
    ManualAmbientSpawners.RemoveAll([Spawner](const TWeakObjectPtr<AAmbientEntitySpawner>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Spawner;
    });
}

void AGridWorldManager::DespawnSpawnerIndividuals(FAmbientSpawnerRuntime& Spawner)
{
    for (TWeakObjectPtr<AAmbientEntityActor>& Individual : Spawner.Individuals)
    {
        if (AAmbientEntityActor* Actor = Individual.Get())
        {
            Actor->Destroy();
        }
    }
    Spawner.Individuals.Reset();
}

// Один вид на спавнер: среди подходящих карточек побеждает та, чьё временное
// условие реже (решение пользователя 2026-09-19 «редкое вытесняет частое»,
// GetAmbientTemporalShare). Прежний вид сравнивается с порогом удержания, а
// не входа, -- гистерезис тот же, что у клеточного пути, просто хранится у
// спавнера, а не у клетки.
FName AGridWorldManager::ChooseAmbientSpeciesForSpawner(const FAmbientSpawnerRuntime& Spawner) const
{
    const FGridCell* Cell = GetCellConst(Spawner.CenterCell.X, Spawner.CenterCell.Y);
    if (!Cell) return NAME_None;

    const AAmbientEntitySpawner* Manual = Spawner.ManualSpawner.Get();
    FName Best = NAME_None;
    float BestShare = TNumericLimits<float>::Max();
    for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
    {
        if (Manual && Manual->AllowedEntityIDs.Num() > 0 && !Manual->AllowedEntityIDs.Contains(Def.EntityID))
        {
            continue;
        }
        if (!IsAmbientCardEligible(*Cell, Def, Spawner.ActiveEntityID == Def.EntityID))
        {
            continue;
        }
        const float Share = GetAmbientTemporalShare(Def);
        // Строгое "реже" -- при равной редкости держится первый по SortOrder,
        // тот же тай-брейк, что и у клеточного пути.
        if (Share < BestShare)
        {
            BestShare = Share;
            Best = Def.EntityID;
        }
    }
    return Best;
}

void AGridWorldManager::UpdateSpawnerIndividuals(FAmbientSpawnerRuntime& Spawner, float DeltaTime)
{
    Spawner.Individuals.RemoveAll([](const TWeakObjectPtr<AAmbientEntityActor>& Entry) { return !Entry.IsValid(); });

    if (Spawner.ActiveEntityID.IsNone())
    {
        DespawnSpawnerIndividuals(Spawner);
        return;
    }

    const FAmbientEntityDefinition* Def = FindAmbientEntityDefinition(Spawner.ActiveEntityID);
    if (!Def) return;

    const AAmbientEntitySpawner* Manual = Spawner.ManualSpawner.Get();
    const int32 PackSize = FMath::Max(1, Def->PackSize);
    const int32 Desired = Manual && Manual->MaxIndividuals > 0 ? FMath::Min(PackSize, Manual->MaxIndividuals) : PackSize;

    while (Spawner.Individuals.Num() > Desired)
    {
        if (AAmbientEntityActor* Extra = Spawner.Individuals.Pop().Get())
        {
            Extra->Destroy();
        }
    }
    if (Spawner.Individuals.Num() >= Desired) return;

    // По одной особи с интервалом: стайка собирается, а не возникает разом.
    Spawner.SpawnCooldownSeconds -= DeltaTime;
    if (Spawner.SpawnCooldownSeconds > 0.0f) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    Spawner.SpawnCooldownSeconds = Settings ? Settings->AmbientSpawnIntervalSeconds : 4.0f;

    UWorld* World = GetWorld();
    if (!World) return;

    // Своя случайность, не WorldRNG: где именно встала особь -- презентация,
    // она не должна сдвигать исходы симуляции (тот же довод, что у джиттера
    // актора в SyncManifestedEntityActor).
    FRandomStream Rng(20260920 + GetTypeHash(Spawner.CenterCell) + GetTypeHash(Spawner.ActiveEntityID) + Spawner.Individuals.Num());
    const FVector Center = GetCellWorldPosition(Spawner.CenterCell.X, Spawner.CenterCell.Y);
    const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
    const float Distance = Spawner.RadiusCm * FMath::Sqrt(Rng.FRand());
    const FVector SpawnPos = Center + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);

    TSubclassOf<AHerbalistEntityActor> ClassToSpawn = AAmbientEntityActor::StaticClass();
    if (Def->ActorClass)
    {
        ClassToSpawn = Def->ActorClass;
    }
    AAmbientEntityActor* Actor = World->SpawnActor<AAmbientEntityActor>(ClassToSpawn, SpawnPos, FRotator::ZeroRotator);
    if (!Actor) return;

    Actor->Init(Spawner.ActiveEntityID, Spawner.CenterCell, this);
    Actor->SetWanderZone(Center, Spawner.RadiusCm, Rng.GetCurrentSeed());
    Spawner.Individuals.Add(Actor);
}

void AGridWorldManager::DespawnAllAmbientSpawners()
{
    for (auto& Pair : AmbientSpawners)
    {
        DespawnSpawnerIndividuals(Pair.Value);
    }
    AmbientSpawners.Reset();
}

void AGridWorldManager::UpdateAmbientSpawners(float DeltaTime)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    if (!Settings || !Settings->bUseAmbientSpawners)
    {
        // Флаг выключили на ходу -- за собой убираем, иначе особи остались бы
        // бродить без хозяина.
        DespawnAllAmbientSpawners();
        return;
    }

    const float AutoRadiusCm = FMath::Max(1.0f, Settings->AmbientSpawnerRadiusMeters) * 100.0f;
    const float SpacingCm = FMath::Max(1.0f, Settings->AmbientSpawnerSpacingMeters) * 100.0f;
    const int32 BlockCells = FMath::Max(1, FMath::RoundToInt(SpacingCm / FMath::Max(1.0f, CellSize)));

    TSet<FIntPoint> Alive;

    // 1. Ручные -- они старше автоматических и их центр задан игроком-автором
    // уровня, а не сидом.
    ManualAmbientSpawners.RemoveAll([](const TWeakObjectPtr<AAmbientEntitySpawner>& Entry) { return !Entry.IsValid(); });
    for (const TWeakObjectPtr<AAmbientEntitySpawner>& Entry : ManualAmbientSpawners)
    {
        AAmbientEntitySpawner* Manual = Entry.Get();
        int32 CellX = 0, CellY = 0;
        if (!Manual || !WorldPositionToCell(Manual->GetActorLocation(), CellX, CellY)) continue;

        const FIntPoint Key(CellX, CellY);
        FAmbientSpawnerRuntime& Runtime = AmbientSpawners.FindOrAdd(Key);
        Runtime.CenterCell = Key;
        Runtime.RadiusCm = FMath::Max(1.0f, Manual->GetRadiusMeters()) * 100.0f;
        Runtime.ManualSpawner = Manual;
        Alive.Add(Key);
    }

    // 2. Автоматические -- по одному кандидату на квадрат, только там, где
    // клетка реально живёт (активный чанк и материализованная земля).
    ForEachActiveCell([&](FGridCell& Cell)
    {
        const FIntPoint Block(HerbalistCore::FloorDivCoord(Cell.X, BlockCells), HerbalistCore::FloorDivCoord(Cell.Y, BlockCells));
        if (GetAutoSpawnerCellForBlock(Block, BlockCells) != FIntPoint(Cell.X, Cell.Y)) return;
        if (!IsCellMaterialized(Cell)) return;

        // Ручной спавнер с флагом отменяет автоматические в своём радиусе:
        // поставленный рукой -- замена процедурному заселению, не добавка.
        const FVector CellPos = GetCellWorldPositionFlat(Cell.X, Cell.Y);
        for (const TWeakObjectPtr<AAmbientEntitySpawner>& Entry : ManualAmbientSpawners)
        {
            const AAmbientEntitySpawner* Manual = Entry.Get();
            if (!Manual || !Manual->bOverridesAuto) continue;
            const FVector ManualPos = Manual->GetActorLocation();
            const float RadiusCm = Manual->GetRadiusMeters() * 100.0f;
            if (FVector2D(CellPos - ManualPos).SizeSquared() <= FMath::Square(RadiusCm)) return;
        }

        const FIntPoint Key(Cell.X, Cell.Y);
        // Ручной спавнер на той же клетке (кандидат мог попасть ровно на него,
        // когда тот не отменяет автоматические) -- его состояние не трогаем,
        // иначе потерялись бы его список видов и потолок (ревью 2026-09-20).
        if (const FAmbientSpawnerRuntime* Existing = AmbientSpawners.Find(Key))
        {
            if (Existing->ManualSpawner.IsValid()) return;
        }
        FAmbientSpawnerRuntime& Runtime = AmbientSpawners.FindOrAdd(Key);
        Runtime.CenterCell = Key;
        Runtime.RadiusCm = AutoRadiusCm;
        Runtime.ManualSpawner = nullptr;
        Alive.Add(Key);
    });

    // 3. Спавнеры, выпавшие из активной области (или отменённые ручным),
    // забирают своих особей с собой.
    for (auto It = AmbientSpawners.CreateIterator(); It; ++It)
    {
        if (!Alive.Contains(It.Key()))
        {
            DespawnSpawnerIndividuals(It.Value());
            It.RemoveCurrent();
        }
    }

    // 4. Вид и особи.
    for (auto& Pair : AmbientSpawners)
    {
        FAmbientSpawnerRuntime& Runtime = Pair.Value;
        const FName Species = ChooseAmbientSpeciesForSpawner(Runtime);
        if (Species != Runtime.ActiveEntityID)
        {
            DespawnSpawnerIndividuals(Runtime);
            Runtime.ActiveEntityID = Species;
            Runtime.SpawnCooldownSeconds = 0.0f;
        }
        UpdateSpawnerIndividuals(Runtime, DeltaTime);
    }
}
