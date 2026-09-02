// Core/World/GridWorldManagerArtifactEffects.cpp
//
// Эффекты артефактов Легендарных (02_GDD/21_Journey_And_Artifacts.md §21.3,
// 2026-09-01, ревизии "Ending and artifacts"/"Update docs"/"Update artifacts")
// — каждый сформулирован как глагол/действие игрока, не модификатор клетки
// (§21.3 "Принцип баланса"). Дубинка (Дубыня) сюда не входит — изъята из
// дизайна, см. GridWorldManagerBases.cpp. Камень-оберег живёт не здесь, а в
// PipelineV2.cpp/GridWorldManagerTick.cpp — этот файл только хранит
// HasUnspentBifurcationCharm(), сама Bifurcation-логика в детерминированном
// пайплайне. Прогрев (Warmth, вариант C) накапливается в
// GridWorldManagerTick.cpp::RunSimulationStep (на удачной варке), не здесь
// — IsArtifactWarmed() ниже только читает результат.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

bool AGridWorldManager::UseHornOnCell(const FIntPoint& Cell, FText& OutDiagnosis) const
{
    const bool bHasHorn = AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Рог")); });
    if (!bHasHorn)
    {
        OutDiagnosis = FText::GetEmpty();
        return false;
    }

    const FGridCell* Target = GetCellConst(Cell.X, Cell.Y);
    if (!Target || !Target->bIsWater)
    {
        OutDiagnosis = FText::GetEmpty();
        return false;
    }

    // §21.3: "по резонансу понимаешь, испорчен источник или чист, до того
    // как соберёшь оттуда воду" — читает РЕАЛЬНОЕ состояние напрямую, не
    // через PerceiveRealState. Сознательно точнее тултипа — весь смысл
    // предмета в честности показания, не в очередном зашумлённом канале.
    const bool bCorrupted = Target->State.Meta.Corruption >= 0.5f;
    OutDiagnosis = bCorrupted
        ? FText::FromString(TEXT("Рог глухо гудит -- источник испорчен."))
        : FText::FromString(TEXT("Рог поёт ясно и чисто -- источник цел."));
    return true;
}

bool AGridWorldManager::UseCombOnCell(const FIntPoint& Cell)
{
    const int32 CombIndex = AcquiredArtifacts.IndexOfByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Гребень")); });
    if (CombIndex == INDEX_NONE) return false;

    FGridCell* Target = GetCell(Cell.X, Cell.Y);
    if (!Target) return false;

    // §21.3: "мгновенно выращивает заросль/преграду позади -- способ выйти
    // из опасной зоны". В проекте нет механики непроходимости клеток
    // (тот же пробел, что уже был у Дубинки) — вертикальный срез: снимает
    // проявленную сущность немедленно, реального препятствия не строит
    // (согласовано с пользователем, не заводим новую систему коллизии
    // ради одного расходуемого предмета).
    const bool bHadEntity = !Target->ManifestedEntityID.IsNone();
    if (bHadEntity)
    {
        Target->ManifestedEntityID = NAME_None;
        SyncManifestedEntityActor(*Target, nullptr, nullptr);
    }

    AcquiredArtifacts.RemoveAt(CombIndex);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Гребень spent at (%d,%d), entity cleared: %s"),
        Cell.X, Cell.Y, bHadEntity ? TEXT("true") : TEXT("false"));
    return true;
}

bool AGridWorldManager::UseYouthApple()
{
    const int32 AppleIndex = AcquiredArtifacts.IndexOfByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Молодильное яблоко")); });
    if (AppleIndex == INDEX_NONE) return false;

    // §21.3: "на время убирает шум с её росы -- расходуемая, короткая
    // версия того, что GlobalPerceptionClarity даёт навсегда". Не трогает
    // саму Clarity (это обесценило бы прогрессию) — GetZaryanaPerceivedState
    // читает это окно отдельно.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float WindowSeconds = Settings ? Settings->YouthAppleWindowSeconds : 180.0f;
    YouthAppleClarityBoostExpiryGameSeconds = GameClockSeconds + WindowSeconds;

    AcquiredArtifacts.RemoveAt(AppleIndex);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Молодильное яблоко spent, Rosa clarity window until %.1f"),
        YouthAppleClarityBoostExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::UseInvisibilityCap(const FIntPoint& Cell)
{
    const bool bHasCap = AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Шапка-невидимка")); });
    if (!bHasCap) return false;

    // §21.3: "временно выводит клетку/зону из-под срабатывания амбиентных
    // проявлений и угроз Легендарного уровня" — НЕ расходуется (в отличие
    // от Гребня/Яблока), тот же предмет можно активировать снова после
    // истечения окна (переактивация переносит зону на новую клетку).
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->InvisibilityCapDurationSeconds : 300.0f;
    InvisibilityCapCenter = Cell;
    InvisibilityCapExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Шапка-невидимка active at (%d,%d) until %.1f"),
        Cell.X, Cell.Y, InvisibilityCapExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsInvisibilityCapActive() const
{
    return GameClockSeconds < InvisibilityCapExpiryGameSeconds;
}

bool AGridWorldManager::IsInvisibilityCapActive(const FIntPoint& Cell) const
{
    if (!IsInvisibilityCapActive()) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->InvisibilityCapRadius : 3;
    const int32 Dist = FMath::Max(FMath::Abs(Cell.X - InvisibilityCapCenter.X), FMath::Abs(Cell.Y - InvisibilityCapCenter.Y));
    return Dist <= Radius;
}

bool AGridWorldManager::HasUnspentBifurcationCharm() const
{
    return AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Камень-оберег")) && !A.bBifurcationChargeSpent; });
}

bool AGridWorldManager::UseLanternDisclosureOnCell(const FIntPoint& Cell, FText& OutDisclosure) const
{
    OutDisclosure = FText::GetEmpty();

    const bool bHasLantern = AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Фонарь")); });
    if (!bHasLantern || !IsArtifactWarmed(FName(TEXT("Фонарь")))) return false;

    const FGridCell* Target = GetCellConst(Cell.X, Cell.Y);
    if (!Target) return false;

    // §21.3: "на миг показывать настоящее состояние клетки без искажения
    // S_Perceived, противовес ночной надбавке Морочников" — читает
    // Meta НАПРЯМУЮ, тот же принцип честности, что уже UseHornOnCell (не
    // через PerceiveRealState/ComputePerceptionDistortion).
    OutDisclosure = FText::FromString(FString::Printf(TEXT(
        "Фонарь на миг горит ровно и без тени -- клетка (%d,%d): Purity=%.2f, Corruption=%.2f, Distortion=%.2f."),
        Cell.X, Cell.Y, Target->State.Meta.Purity, Target->State.Meta.Corruption, Target->State.Meta.Distortion));
    return true;
}
