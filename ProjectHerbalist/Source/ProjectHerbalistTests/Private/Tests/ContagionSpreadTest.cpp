// Source/ProjectHerbalistTests/Private/Tests/ContagionSpreadTest.cpp
//
// Заражение соседей (2026-08-30, "думаем над расширением реакции мира на
// действия игрока... разрастание поганых мест") — расширение уже
// существующей бистабильности (BistabilityTest.cpp): пока клетка держится
// в испорченном полюсе (Cell.Memory.bDegrading), она непрерывно толкает
// TargetState четырёх прямых соседей по сетке в ту же сторону, что и её
// собственный полюс — медленно, не мгновенным перекидыванием через порог
// (см. HerbalistSettings.h::ContagionSpreadRate). По прямому решению
// пользователя пересекает границу биома — не ограничено одним биомом.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistContagion_DegradingCellPushesDirectNeighborsTowardItsPole,
    "Herbalist.Contagion.DegradingCellPushesDirectNeighborsTowardItsPole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistContagion_DegradingCellPushesDirectNeighborsTowardItsPole::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Source = Manager->GetCell(5, 5);
    FGridCell* North = Manager->GetCell(5, 4);     // прямой сосед -- должен заразиться
    FGridCell* Diagonal = Manager->GetCell(6, 6);  // диагональный -- НЕ сосед, не должен
    if (!TestNotNull(TEXT("Source cell exists"), Source)
        || !TestNotNull(TEXT("North neighbor exists"), North)
        || !TestNotNull(TEXT("Diagonal cell exists"), Diagonal))
    {
        Manager->Destroy();
        return false;
    }

    // Источник уже в испорченном полюсе -- изолируем эффект заражения от
    // самого перехода (тот уже покрыт BistabilityTest.cpp).
    Source->Biome = EBiomeType::Bog;
    Source->Memory.bDegrading = true;
    Source->TargetState.Meta.Corruption = 1.0f;
    Source->TargetState.Meta.Purity = 0.0f;
    Source->State.Meta.Corruption = 1.0f;   // уже на полюсе, сам не переключится повторно

    // Сосед -- сознательно ДРУГОЙ биом, проверяем, что заражение пересекает
    // границу (прямое решение пользователя, не только "внутри своего биома").
    North->Biome = EBiomeType::Taiga;
    North->bIsWater = false;
    North->TargetState.Meta.Corruption = 0.18f;   // здоровое умолчание Тайги
    North->TargetState.Meta.Purity = 0.75f;
    North->Memory.bDegrading = false;

    Diagonal->Biome = EBiomeType::Taiga;
    Diagonal->bIsWater = false;
    Diagonal->TargetState.Meta.Corruption = 0.18f;
    Diagonal->TargetState.Meta.Purity = 0.75f;
    Diagonal->Memory.bDegrading = false;

    const float CorruptionBefore = North->TargetState.Meta.Corruption;
    const float PurityBefore = North->TargetState.Meta.Purity;
    const float DiagonalCorruptionBefore = Diagonal->TargetState.Meta.Corruption;

    Manager->RegenerateCellParameters(1.0f);   // DeltaTime=1s -- удобная арифметика

    TestTrue(TEXT("Direct neighbor's TargetState.Corruption nudged up"),
        North->TargetState.Meta.Corruption > CorruptionBefore);
    TestTrue(TEXT("Direct neighbor's TargetState.Purity nudged down"),
        North->TargetState.Meta.Purity < PurityBefore);
    TestFalse(TEXT("One tick of a small push does not itself flip the neighbor's own pole"),
        North->Memory.bDegrading);

    TestEqual(TEXT("Diagonal cell is not a direct neighbor -- untouched"),
        Diagonal->TargetState.Meta.Corruption, DiagonalCorruptionBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistContagion_NonDegradingCellDoesNotSpread,
    "Herbalist.Contagion.NonDegradingCellDoesNotSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistContagion_NonDegradingCellDoesNotSpread::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Source = Manager->GetCell(5, 5);
    FGridCell* North = Manager->GetCell(5, 4);
    if (!TestNotNull(TEXT("Source cell exists"), Source) || !TestNotNull(TEXT("North neighbor exists"), North))
    {
        Manager->Destroy();
        return false;
    }

    // Источник высоко по Corruption, но НЕ в испорченном полюсе (bDegrading
    // остаётся false -- например, только что вошёл, но ещё не пересчитан
    // этим же вызовом). Заражение не должно сработать до фиксации полюса.
    Source->Biome = EBiomeType::Bog;
    Source->Memory.bDegrading = false;
    Source->State.Meta.Corruption = 0.5f;   // ниже порога входа (0.85), полюс не фиксируется
    Source->TargetState.Meta.Corruption = 0.70f;   // здоровое умолчание Болота

    North->Biome = EBiomeType::Taiga;
    North->bIsWater = false;
    North->TargetState.Meta.Corruption = 0.18f;
    North->Memory.bDegrading = false;

    const float CorruptionBefore = North->TargetState.Meta.Corruption;
    Manager->RegenerateCellParameters(1.0f);

    TestEqual(TEXT("Non-degrading neighbor does not spread contagion"),
        North->TargetState.Meta.Corruption, CorruptionBefore);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
