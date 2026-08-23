// Core/World/GridWorldManagerShrine.cpp
//
// Капища v1 (02_GDD/15_Cycles_And_Shrines.md §15.5) — регистрация (там, где
// стоит котёл, см. AAlchemyTableActor::BeginPlay) и спад Restoration при
// небрежении. Рост (подношение варкой) — в RunSimulationStep
// (GridWorldManagerTick.cpp), там уже есть сопоставление команд с результатом,
// которое использует и Травник. Эффект 1 (модуляция релаксации) —
// RegenerateCellParameters (GridWorldManagerCore.cpp). Эффект 2 (надбавка к
// Coherence) — Simulation::PipelineV2::ProcessApplyCommand, через
// FWorldSnapshot::Shrines. Эффект 4 (защита инвентаря) —
// HerbalistInventoryComponent::TickComponent.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

void AGridWorldManager::RegisterShrine(const FIntPoint& Cell, EShrineType Type)
{
    if (FShrine* Existing = FindShrineAt(Cell))
    {
        Existing->Type = Type;
        return;
    }

    FShrine NewShrine;
    NewShrine.Cell = Cell;
    NewShrine.Type = Type;
    Shrines.Add(NewShrine);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Shrine] Registered at (%d,%d), type=%d"), Cell.X, Cell.Y, (int32)Type);
}

FShrine* AGridWorldManager::FindShrineAt(const FIntPoint& Cell)
{
    for (FShrine& S : Shrines)
    {
        if (S.Cell == Cell) return &S;
    }
    return nullptr;
}

void AGridWorldManager::UpdateShrines(float DeltaTime)
{
    if (Shrines.Num() == 0) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DecayDays = Settings ? Settings->ShrineNeglectDecayDays : 28.0f;
    const float DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f;
    const float DecayPerSecond = 1.0f / FMath::Max(DecayDays * DaySeconds, KINDA_SMALL_NUMBER);

    for (FShrine& S : Shrines)
    {
        // Спад всегда к нулю, с обеих сторон — "простое небрежение... даёт
        // Restoration угасать до 0" (§15.5), включая осквернённые (отрицательные)
        // капища: неглект не усугубляет осквернение, только гасит его же.
        if (S.Restoration > 0.0f)
        {
            S.Restoration = FMath::Max(S.Restoration - DecayPerSecond * DeltaTime, 0.0f);
        }
        else if (S.Restoration < 0.0f)
        {
            S.Restoration = FMath::Min(S.Restoration + DecayPerSecond * DeltaTime, 0.0f);
        }
    }
}
