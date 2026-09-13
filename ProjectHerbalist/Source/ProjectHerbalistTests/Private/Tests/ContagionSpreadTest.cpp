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
#include "Core/Config/HerbalistSettings.h"
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

// GetGridCorruptionReport (2026-09-06, найдено по PIE-логу пользователя:
// "после трёх сборов и долгого времени испортилась вся сетка"). Заражение
// само по себе уже проверено выше на одном шаге/одном соседе;
// GetSelectedCellInfo (правый клик) проверяет одну клетку за раз -- этого
// не хватает, чтобы одним логом отследить, докатилась ли волна заражения
// до всей сетки. Отчёт по всей сетке разом -- новый консольный инструмент
// именно для этого случая.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistContagion_GridReportCountsDegradingCellsAcrossWholeGrid,
    "Herbalist.Contagion.GridReportCountsDegradingCellsAcrossWholeGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistContagion_GridReportCountsDegradingCellsAcrossWholeGrid::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Дефолтная сетка -- 20x20=400 (AGridWorldManager::GridSizeX/Y), тот же
    // размер, что и у остальных тестов этого файла через SpawnAndBeginPlay.
    const FString CleanReport = Manager->GetGridCorruptionReport();
    TestTrue(FString::Printf(TEXT("Clean grid reports its real size (got '%s')"), *CleanReport),
        CleanReport.Contains(TEXT("400 cells")));
    TestTrue(FString::Printf(TEXT("Clean grid reports zero degrading cells (got '%s')"), *CleanReport),
        CleanReport.Contains(TEXT("0 degrading")));

    // Заражаем ровно две клетки -- считаем, что отчёт видит именно их, не
    // все 400 и не ноль.
    FGridCell* First = Manager->GetCell(5, 5);
    FGridCell* Second = Manager->GetCell(5, 4);
    if (!TestNotNull(TEXT("First cell exists"), First) || !TestNotNull(TEXT("Second cell exists"), Second))
    {
        Manager->Destroy();
        return false;
    }
    First->Memory.bDegrading = true;
    First->State.Meta.Distortion = 1.0f;
    Second->Memory.bDegrading = true;
    Second->State.Meta.Distortion = 1.0f;

    const FString InfectedReport = Manager->GetGridCorruptionReport();
    TestTrue(FString::Printf(TEXT("Infected grid still reports its real size (got '%s')"), *InfectedReport),
        InfectedReport.Contains(TEXT("400 cells")));
    TestTrue(FString::Printf(TEXT("Infected grid counts exactly two degrading cells (got '%s')"), *InfectedReport),
        InfectedReport.Contains(TEXT("2 degrading")));
    TestTrue(FString::Printf(TEXT("Infected grid reports the max Distortion the two cells were set to (got '%s')"), *InfectedReport),
        InfectedReport.Contains(TEXT("max=1.000")));

    Manager->Destroy();
    return true;
}

// Толчок заражения кадрами (ревью 2026-09-13). RegenerateCellParameters идёт
// каждый кадр с реальным DeltaTime; ставка на клетке 9 м -- 0.0005 в секунду, за
// кадр 1/60 с это ≈ 8e-6, меньше KINDA_SMALL_NUMBER. Сравнение с эпсилоном
// перед записью пропускало такой толчок каждый кадр, и фронт в игре стоял.
// Остальные тесты файла шагают по секунде и этого не видели.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistContagion_FrameSizedStepsAccumulate,
    "Herbalist.Contagion.FrameSizedStepsAccumulate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistContagion_FrameSizedStepsAccumulate::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    // Клетка 9 м, как на L_TestDev.
    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, -1, 900.0f);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    // Без этой проверки тест, уехавший на клетку 1 м, прошёл бы тихо: толчок
    // 7.5e-5 там тоже меньше порога, и ожидание пересчиталось бы (ревью).
    if (!TestEqual(TEXT("Sanity: клетка 9 м"), Manager->CellSize, 900.0f))
    {
        Manager->Destroy();
        return false;
    }

    FGridCell* Source = Manager->GetCell(5, 5);
    FGridCell* North = Manager->GetCell(5, 4);
    if (!TestNotNull(TEXT("Source cell exists"), Source) || !TestNotNull(TEXT("North neighbor exists"), North))
    {
        Manager->Destroy();
        return false;
    }

    Source->Biome = EBiomeType::Bog;
    Source->Memory.bDegrading = true;
    Source->TargetState.Meta.Corruption = 1.0f;
    Source->TargetState.Meta.Purity = 0.0f;
    Source->State.Meta.Corruption = 1.0f;

    North->Biome = EBiomeType::Taiga;
    North->bIsWater = false;
    North->TargetState.Meta.Corruption = 0.18f;
    North->TargetState.Meta.Purity = 0.75f;
    North->State.Meta.Corruption = 0.18f;
    North->Memory.bDegrading = false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const double Rate = Settings->ContagionSpreadRate * UHerbalistSettings::ContagionReferenceCellMeters * 100.0 / Manager->CellSize;
    const float FrameSeconds = 1.0f / 60.0f;
    const int32 Frames = 600;
    const float CorruptionBefore = North->TargetState.Meta.Corruption;
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Manager->RegenerateCellParameters(FrameSeconds);
    }

    const double Expected = Rate * Frames * FrameSeconds;
    const double Grown = North->TargetState.Meta.Corruption - CorruptionBefore;
    AddInfo(FString::Printf(TEXT("Толчок за кадр %.2e, за %d кадров вырос на %.6f (ожидалось %.6f)"),
        Rate * FrameSeconds, Frames, Grown, Expected));
    TestTrue(TEXT("Sanity: толчок за кадр меньше KINDA_SMALL_NUMBER -- тот самый случай"), Rate * FrameSeconds < KINDA_SMALL_NUMBER);
    // Допуск 2%: 600 сложений во float на 0.18 теряют не больше ~5e-6 из 0.005.
    TestEqual(TEXT("За 600 кадров по 1/60 с TargetState соседа вырос на ставку × 10 с"), Grown, Expected, Expected * 0.02);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
