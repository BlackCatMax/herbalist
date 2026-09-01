// Core/World/GridWorldManagerArtifactEffects.cpp
//
// Семь эффектов артефактов Легендарных (02_GDD/21_Journey_And_Artifacts.md
// §21.3, 2026-09-01, ревизия "Ending and artifacts") — каждый сформулирован
// как глагол/действие игрока, не модификатор клетки (§21.3 "Принцип
// баланса"). Дубинка (Дубыня) сюда не входит — изъята из дизайна, см.
// GridWorldManagerBases.cpp. Фонарь не имеет здесь функции вовсе — §21.3
// прямо откладывает силу разоблачения, эффект остаётся "просто источник
// света" (контент/актор, не код). Камень-оберег живёт не здесь, а в
// PipelineV2.cpp/GridWorldManagerTick.cpp — этот файл только хранит
// HasUnspentBifurcationCharm(), сама Bifurcation-логика в детерминированном
// пайплайне.

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

bool AGridWorldManager::UseInvisibilityCap()
{
    const bool bHasCap = AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Шапка-невидимка")); });
    if (!bHasCap) return false;

    // §21.3: "временно выводит клетку/зону из-под срабатывания амбиентных
    // проявлений и угроз Легендарного уровня" — НЕ расходуется (в отличие
    // от Гребня/Яблока), тот же предмет можно активировать снова после
    // истечения окна.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->InvisibilityCapDurationSeconds : 300.0f;
    InvisibilityCapExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Шапка-невидимка active until %.1f"), InvisibilityCapExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsInvisibilityCapActive() const
{
    return GameClockSeconds < InvisibilityCapExpiryGameSeconds;
}

bool AGridWorldManager::HasUnspentBifurcationCharm() const
{
    return AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == FName(TEXT("Камень-оберег")) && !A.bBifurcationChargeSpent; });
}
