// Core/World/GridWorldManagerWards.cpp
//
// Обереги (кристаллы Пещеры, DESIGN_Community_And_Homestead.md §2.4,
// 2026-09-04) — "работа оберегов слабая, но постоянная, есть временные
// лимиты, не постоянное действие" (прямая формулировка запроса). Тот же
// GameClockSeconds-экспаери, что уже InvisibilityCap/YouthApple/Alkonost
// (GridWorldManagerArtifactEffects.cpp/GridWorldManagerProphetFeathers.cpp)
// — выбор таймера обоснован у EWardEffectType (HerbalistCoreTypes.h).
//
// Владение (есть ли нужный кристалл в инвентаре игрока) НЕ проверяется
// здесь — тот же принцип разделения обязанностей, что уже
// OfferToCommunity/RegisterGardenPlot: инвентарные поиски делает
// AHerbalistPlayerController::ActivateWard (резолвит FIngredientTableRow
// через IngredientRegistrySubsystem, дальше зовёт одну из функций ниже),
// GridWorldManager хранит только мировое состояние уже активированного
// эффекта. Это тот же класс границы, что уже отделяет GenerateHarvestResult
// от Landmark/Respect (ROADMAP.md) — и та же причина, по которой резолв
// через IngredientRegistrySubsystem не покрыт автотестом напрямую
// (GameInstanceSubsystem недоступен в Editor-мире автотестов, тот же
// пробел, что уже у TradeWithCommunity).

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

bool AGridWorldManager::ActivateWardBrewBoost()
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->WardDurationSeconds : 600.0f;
    WardBrewBoostExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] BrewBoost active until %.1f"), WardBrewBoostExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsWardBrewBoostActive() const
{
    return GameClockSeconds < WardBrewBoostExpiryGameSeconds;
}

bool AGridWorldManager::ActivateWardConcealment(const FIntPoint& Center)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->WardDurationSeconds : 600.0f;
    WardConcealmentCenter = Center;
    WardConcealmentExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] Concealment active at (%d,%d) until %.1f"),
        Center.X, Center.Y, WardConcealmentExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsWardConcealmentActive() const
{
    return GameClockSeconds < WardConcealmentExpiryGameSeconds;
}

bool AGridWorldManager::IsWardConcealmentActive(const FIntPoint& Cell) const
{
    if (!IsWardConcealmentActive()) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->WardConcealmentRadius : 1;
    const int32 Dist = FMath::Max(FMath::Abs(Cell.X - WardConcealmentCenter.X), FMath::Abs(Cell.Y - WardConcealmentCenter.Y));
    return Dist <= Radius;
}

// MorokReduction (Куриный бог, второй заход 2026-09-04) — тот же
// Center+Radius API, что и EntityConceal выше (активация фиксирует клетку
// игрока в момент вызова). Читается только из ComputePerceptionDistortion
// (GridWorldManagerEntities.cpp), которая сама решает применять ли эффект
// только ночью — здесь, на уровне таймера/геометрии, никакого дневного/
// ночного различия нет, ровно как ActivateWardConcealment не знает о
// сущностях, которые он в итоге гасит.
bool AGridWorldManager::ActivateWardMorokReduction(const FIntPoint& Center)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->WardDurationSeconds : 600.0f;
    WardMorokReductionCenter = Center;
    WardMorokReductionExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] MorokReduction active at (%d,%d) until %.1f"),
        Center.X, Center.Y, WardMorokReductionExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsWardMorokReductionActive() const
{
    return GameClockSeconds < WardMorokReductionExpiryGameSeconds;
}

bool AGridWorldManager::IsWardMorokReductionActive(const FIntPoint& Cell) const
{
    if (!IsWardMorokReductionActive()) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->WardMorokReductionRadius : 1;
    const int32 Dist = FMath::Max(FMath::Abs(Cell.X - WardMorokReductionCenter.X), FMath::Abs(Cell.Y - WardMorokReductionCenter.Y));
    return Dist <= Radius;
}

// Аудит 2026-09-05: см. подробный довод у объявления в GridWorldManager.h.
// Шесть GameClockSeconds-таймеров "короткого окна" (Ward*, InvisibilityCap,
// YouthApple, Alkonost) осознанно не персистятся, но без явного сброса
// здесь откат GameClockSeconds назад при LoadGame мог оставить их
// формально "ещё не истёкшими" относительно нового, более раннего
// времени — оберег читался бы активным ещё раз, вопреки уже принятому
// решению "не переживает загрузку".
void AGridWorldManager::ResetSessionOnlyWardTimers()
{
    AlkonostSuppressionExpiryGameSeconds = 0.0f;
    YouthAppleClarityBoostExpiryGameSeconds = 0.0f;
    InvisibilityCapExpiryGameSeconds = 0.0f;
    InvisibilityCapCenter = FIntPoint(-1, -1);
    WardBrewBoostExpiryGameSeconds = 0.0f;
    WardConcealmentExpiryGameSeconds = 0.0f;
    WardConcealmentCenter = FIntPoint(-1, -1);
    WardMorokReductionExpiryGameSeconds = 0.0f;
    WardMorokReductionCenter = FIntPoint(-1, -1);
}

// ============================================================================
// ТИРАЖНЫЕ ОБЕРЕГИ (награда ритуалов перехода ярусов биомов, 2026-09-04)
// ============================================================================
//
// В отличие от трёх оберегов выше -- НЕТ таймера (прямой запрос: "как
// активировал/надел, так и работает"), НЕТ Center/Radius, запомненных на
// активации: "дом" оберега определяется биомом клетки, для которой
// спрашивают (GetCellConst), сверенным со списком WardHomeBiomes с
// карточки конкретного кристалла (IngredientTableRow.h::bIsTieredWard).
// Владение (есть ли кристалл в инвентаре) по-прежнему проверяет
// AHerbalistPlayerController::ActivateWard, тот же принцип разделения
// обязанностей, что и у остальных Activate*-функций этого файла.

// Аудит 2026-09-05, решение пользователя (а): в отличие от
// ResetSessionOnlyWardTimers выше (шесть таймеров "короткого окна",
// которые ПРОДОЛЖАЮТ намеренно не персистится), тиражные обереги —
// постоянная награда за завершённый ритуал, без таймера, и обязаны
// пережить перезагрузку так же, как AcquiredArtifacts/AcquiredFeathers.
FSavedTieredWards AGridWorldManager::CaptureTieredWards() const
{
    FSavedTieredWards Saved;
    Saved.bConcealmentActive = bTieredConcealmentActive;
    Saved.ConcealmentHomeBiomes = TieredConcealmentHomeBiomes;
    Saved.bMorokReductionActive = bTieredMorokReductionActive;
    Saved.MorokReductionHomeBiomes = TieredMorokReductionHomeBiomes;
    Saved.bBrewBoostActive = bTieredBrewBoostActive;
    Saved.BrewBoostHomeBiomes = TieredBrewBoostHomeBiomes;
    return Saved;
}

void AGridWorldManager::RestoreTieredWards(const FSavedTieredWards& InWards)
{
    bTieredConcealmentActive = InWards.bConcealmentActive;
    TieredConcealmentHomeBiomes = InWards.ConcealmentHomeBiomes;
    bTieredMorokReductionActive = InWards.bMorokReductionActive;
    TieredMorokReductionHomeBiomes = InWards.MorokReductionHomeBiomes;
    bTieredBrewBoostActive = InWards.bBrewBoostActive;
    TieredBrewBoostHomeBiomes = InWards.BrewBoostHomeBiomes;
}

void AGridWorldManager::ActivateTieredWard(EWardEffectType Type, const TArray<EBiomeType>& HomeBiomes)
{
    switch (Type)
    {
    case EWardEffectType::EntityConceal:
        bTieredConcealmentActive = true;
        TieredConcealmentHomeBiomes = HomeBiomes;
        break;
    case EWardEffectType::MorokReduction:
        bTieredMorokReductionActive = true;
        TieredMorokReductionHomeBiomes = HomeBiomes;
        break;
    case EWardEffectType::BrewBoost:
        bTieredBrewBoostActive = true;
        TieredBrewBoostHomeBiomes = HomeBiomes;
        break;
    default:
        UE_LOG(LogHerbalistWorld, Warning, TEXT("[Ward] ActivateTieredWard: unhandled WardEffectType"));
        break;
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Ward] Tiered ward activated (Type=%d, %d home biomes), no expiry"),
        static_cast<int32>(Type), HomeBiomes.Num());
}

// EntityConceal (тираж) -- та же геометрия, что читает манифестация сущностей
// (GridWorldManagerEntities.cpp, IsWardConcealmentActive рядом), но по биому
// самой клетки, а не по расстоянию от запомненного центра (см. довод у
// объявления в GridWorldManager.h).
bool AGridWorldManager::IsTieredConcealmentActive(const FIntPoint& PlayerCell) const
{
    if (!bTieredConcealmentActive) return false;

    const FGridCell* Cell = GetCellConst(PlayerCell.X, PlayerCell.Y);
    return Cell && TieredConcealmentHomeBiomes.Contains(Cell->Biome);
}

// MorokReduction (тираж) -- полная сила в домашнем биоме, ослабленная
// (TieredWardOutOfBiomeStrength) вне них, ноль если тиражный MorokReduction
// не активирован вовсе -- вызывающая сторона (ComputePerceptionDistortion)
// просто вычитает результат без отдельной проверки активности.
float AGridWorldManager::GetTieredMorokReductionAmount(const FIntPoint& PlayerCell) const
{
    if (!bTieredMorokReductionActive) return 0.0f;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float FullAmount = Settings ? Settings->WardMorokReductionAmount : 0.1f;
    const float OutOfBiomeStrength = Settings ? Settings->TieredWardOutOfBiomeStrength : 0.5f;

    const FGridCell* Cell = GetCellConst(PlayerCell.X, PlayerCell.Y);
    const bool bHome = Cell && TieredMorokReductionHomeBiomes.Contains(Cell->Biome);
    return bHome ? FullAmount : FullAmount * OutOfBiomeStrength;
}
