// Source/ProjectHerbalistTests/Private/Tests/WorldLayoutSolverTest.cpp
//
// Разметка мира, этап 1 (2026-09-12) -- чистый решатель FWorldLayoutSolver,
// без мира. Числа -- из DESIGN_World_Layout.md §4 для L_TestDev: квад 1 м,
// компонент 126 квадов, ячейка стриминга 126 м, дальность 252 м, ландшафт
// ±1008 м, начало сетки разбиения в нуле.

#include "Misc/AutomationTest.h"
#include "Core/World/WorldLayout.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Самый дальнобойный локальный механизм сейчас -- разрежение сущностей,
    // 30 м (FAmbientEntityDefinition::MinSpacingMeters). Здесь -- константа
    // теста, реестр читает менеджер.
    constexpr float TestLongestMechanicMeters = 30.0f;
    constexpr float TestSimulationRadiusMeters = 100.0f;

    FHerbalistWorldLayoutSource MakeTestDevSource()
    {
        FHerbalistWorldLayoutSource Source;
        Source.bHasLandscape = true;
        Source.QuadSizeCm = 100.0;
        Source.ComponentSizeQuads = 126;
        Source.LandscapeOrigin = FVector2D(-100800.0, -100800.0);
        Source.LandscapeMin = FVector2D(-100800.0, -100800.0);
        Source.LandscapeMax = FVector2D(100800.0, 100800.0);
        Source.bHasStreamingGrid = true;
        Source.StreamingGridName = FName(TEXT("MainPartition"));
        Source.StreamingCellSizeCm = 12600.0;
        Source.StreamingLoadingRangeCm = 25200.0;
        Source.StreamingGridOrigin = FVector2D::ZeroVector;
        return Source;
    }

    FHerbalistWorldLayout ResolveTestDev(const FHerbalistWorldLayoutSource& Source, TArray<FString>& OutWarnings,
        const FHerbalistWorldLayoutOverrides& Overrides = FHerbalistWorldLayoutOverrides(),
        float SimulationRadiusMeters = TestSimulationRadiusMeters)
    {
        return FWorldLayoutSolver::Resolve(Source, Overrides, SimulationRadiusMeters, TestLongestMechanicMeters, OutWarnings);
    }

    bool AnyWarningContains(const TArray<FString>& Warnings, const TCHAR* Fragment)
    {
        return Warnings.ContainsByPredicate([Fragment](const FString& Warning) { return Warning.Contains(Fragment); });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_CellIsNearestDivisorOfComponent,
    "Herbalist.WorldLayout.Solver.CellIsNearestDivisorOfComponent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_CellIsNearestDivisorOfComponent::RunTest(const FString& Parameters)
{
    // Делители 126: 1, 2, 3, 6, 7, 9, 14, 18, 21, 42, 63, 126.
    TestEqual(TEXT("Желаемая 10 м при квадe 1 м -> 9 квадов"), FWorldLayoutSolver::ChooseCellQuads(126, 100.0, 1000.0), 9);
    TestEqual(TEXT("Желаемая 12 м -> 14 квадов (14 ближе 9)"), FWorldLayoutSolver::ChooseCellQuads(126, 100.0, 1200.0), 14);
    TestEqual(TEXT("Желаемая 11.5 м -- 9 и 14 равноудалены, берётся больший"), FWorldLayoutSolver::ChooseCellQuads(126, 100.0, 1150.0), 14);
    TestEqual(TEXT("Желаемая 0.5 м -> 1 квад"), FWorldLayoutSolver::ChooseCellQuads(126, 100.0, 50.0), 1);
    TestEqual(TEXT("Квад 2 м, желаемая 10 м -> 6 квадов = 12 м (ближе 3 и 7)"), FWorldLayoutSolver::ChooseCellQuads(126, 200.0, 1000.0), 6);
    TestEqual(TEXT("Вырожденный компонент -> 1"), FWorldLayoutSolver::ChooseCellQuads(0, 100.0, 1000.0), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_TestDevResolvesToDesignNumbers,
    "Herbalist.WorldLayout.Solver.TestDevResolvesToDesignNumbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_TestDevResolvesToDesignNumbers::RunTest(const FString& Parameters)
{
    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(MakeTestDevSource(), Warnings);

    TestTrue(TEXT("Разметка выведена"), Layout.bValid);
    TestEqual(TEXT("Клетка 9 м"), Layout.CellSizeCm, 900.0, 0.001);
    TestTrue(TEXT("...выведена автоматически"), Layout.CellSizeOrigin == EWorldLayoutValueOrigin::Auto);
    TestEqual(TEXT("Страница = ячейка стриминга 126 м = 14 клеток"), Layout.PageSizeInCells, 14);
    TestTrue(TEXT("...автоматически"), Layout.PageSizeOrigin == EWorldLayoutValueOrigin::Auto);
    TestEqual(TEXT("Чанк 7 клеток = 63 м"), Layout.ChunkSizeInCells, 7);
    TestEqual(TEXT("Действующий радиус 63 м"), Layout.EffectiveSimulationRadiusMeters, 63.0, 0.001);
    TestTrue(TEXT("Начало отсчёта -- начало сетки разбиения"), Layout.Anchor == FVector2D::ZeroVector);
    TestTrue(TEXT("Первая клетка (-112, -112)"), Layout.MinCell == FIntPoint(-112, -112));
    TestTrue(TEXT("Сетка 224 x 224"), Layout.GridSize == FIntPoint(224, 224));
    TestEqual(TEXT("Окно карты: 2 x (252 + 126) м / 9 м = 84 -> 128"), Layout.WorldStateWindowCells, 128);
    TestEqual(TEXT("На выровненной карте предупреждений нет"), Warnings.Num(), 0);
    for (const FString& Warning : Warnings)
    {
        AddInfo(Warning);
    }
    TestTrue(TEXT("Описание называет происхождение значений"), FWorldLayoutSolver::Describe(Layout).Contains(TEXT("авто")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_StreamingCellDoesNotDriveCellSize,
    "Herbalist.WorldLayout.Solver.StreamingCellDoesNotDriveCellSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_StreamingCellDoesNotDriveCellSize::RunTest(const FString& Parameters)
{
    // Ловушка первой версии документа: клетка из общего делителя компонента
    // (126) и ячейки стриминга (128) вышла бы 2 м.
    FHerbalistWorldLayoutSource Source = MakeTestDevSource();
    Source.StreamingCellSizeCm = 12800.0;

    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(Source, Warnings);

    TestEqual(TEXT("Клетка остаётся 9 м"), Layout.CellSizeCm, 900.0, 0.001);
    TestEqual(TEXT("Страница округлена: 128 / 9 = 14.2 -> 14 клеток"), Layout.PageSizeInCells, 14);
    TestTrue(TEXT("Есть предупреждение о неровной странице"), AnyWarningContains(Warnings, TEXT("не делится на клетку")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_ChunkKeepsLocalMechanicsInsideRadius,
    "Herbalist.WorldLayout.Solver.ChunkKeepsLocalMechanicsInsideRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_ChunkKeepsLocalMechanicsInsideRadius::RunTest(const FString& Parameters)
{
    // Страница 14 клеток по 9 м, механики до 30 м.
    TestEqual(TEXT("R = 100 м: чанк 14 даёт 0 м, чанк 7 -- 63 м"), FWorldLayoutSolver::ChooseChunkCells(14, 900.0, 100.0, 30.0), 7);
    TestEqual(TEXT("R = 50 м: 7 даёт 0 м, 2 (18 м) -- 36 м"), FWorldLayoutSolver::ChooseChunkCells(14, 900.0, 50.0, 30.0), 2);
    TestEqual(TEXT("Стриминг выключен -- чанк равен странице"), FWorldLayoutSolver::ChooseChunkCells(14, 900.0, -1.0, 30.0), 14);
    TestEqual(TEXT("R = 20 м меньше механик -- не подходит ни один, 1"), FWorldLayoutSolver::ChooseChunkCells(14, 900.0, 20.0, 30.0), 1);
    TestEqual(TEXT("Действующий радиус -- формула GetActiveRadiusInChunks"), FWorldLayoutSolver::EffectiveRadiusMeters(7, 900.0, 100.0), 63.0, 0.001);

    TArray<FString> Warnings;
    ResolveTestDev(MakeTestDevSource(), Warnings, FHerbalistWorldLayoutOverrides(), 20.0f);
    TestTrue(TEXT("Радиус меньше механик -- предупреждение"), AnyWarningContains(Warnings, TEXT("дотянутся до спящих")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_SimulationRadiusClampedToLoadingRange,
    "Herbalist.WorldLayout.Solver.SimulationRadiusClampedToLoadingRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_SimulationRadiusClampedToLoadingRange::RunTest(const FString& Parameters)
{
    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(MakeTestDevSource(), Warnings, FHerbalistWorldLayoutOverrides(), 300.0f);

    TestTrue(TEXT("Радиус 300 м больше дальности 252 м -- предупреждение"), AnyWarningContains(Warnings, TEXT("урезан")));
    TestEqual(TEXT("Чанк считается от урезанных 252 м: 14 клеток дают 252 м"), Layout.ChunkSizeInCells, 14);
    TestEqual(TEXT("Действующий радиус не выходит за дальность"), Layout.EffectiveSimulationRadiusMeters, 252.0, 0.001);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_ManualOverridesWinAndWarn,
    "Herbalist.WorldLayout.Solver.ManualOverridesWinAndWarn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_ManualOverridesWinAndWarn::RunTest(const FString& Parameters)
{
    FHerbalistWorldLayoutOverrides Overrides;
    Overrides.bOverrideCellSize = true;
    Overrides.CellSizeCm = 1000.0f;       // 10 квадов, не делит 126
    Overrides.bOverridePageSize = true;
    Overrides.PageSizeInCells = 16;       // 160 м, не ячейка стриминга
    Overrides.bOverrideChunkSize = true;
    Overrides.ChunkSizeInCells = 5;       // не делит 16

    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(MakeTestDevSource(), Warnings, Overrides);

    TestEqual(TEXT("Клетка -- ручная"), Layout.CellSizeCm, 1000.0, 0.001);
    TestTrue(TEXT("...и помечена так"), Layout.CellSizeOrigin == EWorldLayoutValueOrigin::Manual);
    TestEqual(TEXT("Страница -- ручная"), Layout.PageSizeInCells, 16);
    TestEqual(TEXT("Чанк -- ручной"), Layout.ChunkSizeInCells, 5);
    TestTrue(TEXT("Предупреждение о клетке"), AnyWarningContains(Warnings, TEXT("не делит компонент")));
    TestTrue(TEXT("Предупреждение о странице"), AnyWarningContains(Warnings, TEXT("не совпадает с ячейкой стриминга")));
    TestTrue(TEXT("Предупреждение о чанке"), AnyWarningContains(Warnings, TEXT("не делит страницу")));
    TestTrue(TEXT("Начало -112 м: вершины ландшафта не на целых клетках 10 м"), AnyWarningContains(Warnings, TEXT("нецелое число клеток")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_WithoutStreamingGridPageIsComponent,
    "Herbalist.WorldLayout.Solver.WithoutStreamingGridPageIsComponent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_WithoutStreamingGridPageIsComponent::RunTest(const FString& Parameters)
{
    FHerbalistWorldLayoutSource Source = MakeTestDevSource();
    Source.bHasStreamingGrid = false;
    Source.StreamingCellSizeCm = 0.0;
    Source.StreamingLoadingRangeCm = 0.0;

    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(Source, Warnings);

    TestTrue(TEXT("Разметка выведена и без World Partition"), Layout.bValid);
    TestEqual(TEXT("Страница = компонент 126 м = 14 клеток"), Layout.PageSizeInCells, 14);
    TestTrue(TEXT("...запасное правило"), Layout.PageSizeOrigin == EWorldLayoutValueOrigin::Fallback);
    TestTrue(TEXT("Начало отсчёта -- вершина (0, 0) ландшафта"), Layout.Anchor == FVector2D(-100800.0, -100800.0));
    TestTrue(TEXT("Сетка от (0, 0), 224 x 224"), Layout.MinCell == FIntPoint(0, 0));
    TestTrue(TEXT("...224 x 224"), Layout.GridSize == FIntPoint(224, 224));
    TestEqual(TEXT("Окно карты -- вся сетка, степень двойки"), Layout.WorldStateWindowCells, 256);
    TestTrue(TEXT("Предупреждение об отсутствии стриминга"), AnyWarningContains(Warnings, TEXT("Сетки стриминга")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_WithoutLandscapeGridStaysManual,
    "Herbalist.WorldLayout.Solver.WithoutLandscapeGridStaysManual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_WithoutLandscapeGridStaysManual::RunTest(const FString& Parameters)
{
    FHerbalistWorldLayoutSource Source = MakeTestDevSource();
    Source.bHasLandscape = false;

    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(Source, Warnings);

    TestFalse(TEXT("Без ландшафта разметка не выводится"), Layout.bValid);
    TestTrue(TEXT("...и это сказано"), AnyWarningContains(Warnings, TEXT("Ландшафта нет")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_MisalignedOrMixedLandscapeWarns,
    "Herbalist.WorldLayout.Solver.MisalignedOrMixedLandscapeWarns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_MisalignedOrMixedLandscapeWarns::RunTest(const FString& Parameters)
{
    {
        FHerbalistWorldLayoutSource Source = MakeTestDevSource();
        Source.LandscapeOrigin = FVector2D(-100750.0, -100800.0);
        TArray<FString> Warnings;
        ResolveTestDev(Source, Warnings);
        TestTrue(TEXT("Вершины сдвинуты на 50 см -- предупреждение"), AnyWarningContains(Warnings, TEXT("нецелое число клеток")));
    }
    {
        FHerbalistWorldLayoutSource Source = MakeTestDevSource();
        Source.bLandscapesDisagree = true;
        TArray<FString> Warnings;
        const FHerbalistWorldLayout Layout = ResolveTestDev(Source, Warnings);
        TestTrue(TEXT("Разные ландшафты -- предупреждение"), AnyWarningContains(Warnings, TEXT("по первому")));
        TestTrue(TEXT("...и разметка всё равно выведена"), Layout.bValid);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_FingerprintIgnoresBoundsAndLoadingRange,
    "Herbalist.WorldLayout.Solver.FingerprintIgnoresBoundsAndLoadingRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_FingerprintIgnoresBoundsAndLoadingRange::RunTest(const FString& Parameters)
{
    TArray<FString> Warnings;
    const uint32 Base = ResolveTestDev(MakeTestDevSource(), Warnings).Fingerprint;

    {
        // Решение пользователя 14: добавленные плитки сейвы не ломают.
        FHerbalistWorldLayoutSource Source = MakeTestDevSource();
        Source.LandscapeMax = FVector2D(126000.0, 151200.0);
        Source.StreamingLoadingRangeCm = 50400.0;
        TestTrue(TEXT("Расширенный ландшафт и другая дальность -- отпечаток тот же"),
            ResolveTestDev(Source, Warnings).Fingerprint == Base);
    }
    {
        FHerbalistWorldLayoutSource Source = MakeTestDevSource();
        Source.StreamingCellSizeCm = 25200.0;
        TestTrue(TEXT("Другая ячейка стриминга (страница 28) -- отпечаток другой"),
            ResolveTestDev(Source, Warnings).Fingerprint != Base);
    }
    {
        FHerbalistWorldLayoutSource Source = MakeTestDevSource();
        Source.QuadSizeCm = 200.0;
        TestTrue(TEXT("Другой квад (клетка 12 м) -- отпечаток другой"),
            ResolveTestDev(Source, Warnings).Fingerprint != Base);
    }
    {
        FHerbalistWorldLayoutSource Source = MakeTestDevSource();
        Source.StreamingGridOrigin = FVector2D(-6300.0, -6300.0);
        TestTrue(TEXT("Другое начало отсчёта -- отпечаток другой"),
            ResolveTestDev(Source, Warnings).Fingerprint != Base);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_GridCoversLandscapeInWholePages,
    "Herbalist.WorldLayout.Solver.GridCoversLandscapeInWholePages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_GridCoversLandscapeInWholePages::RunTest(const FString& Parameters)
{
    // Несимметричный ландшафт с краями не на страницах. Страница 126 м.
    FHerbalistWorldLayoutSource Source = MakeTestDevSource();
    Source.LandscapeOrigin = FVector2D(-100800.0, -900.0);
    Source.LandscapeMin = FVector2D(-100000.0, -50.0);
    Source.LandscapeMax = FVector2D(50.0, 99000.0);

    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(Source, Warnings);

    // X: -1000 м / 126 = -7.9 -> страница -8 -> клетка -112; 0.5 м -> страница 1.
    // Y: -0.5 м -> страница -1 -> клетка -14; 990 м / 126 = 7.86 -> страница 8.
    TestTrue(TEXT("Первая клетка на границе страниц, накрывающих край"), Layout.MinCell == FIntPoint(-112, -14));
    TestTrue(TEXT("По X 9 страниц, по Y 9 страниц"), Layout.GridSize == FIntPoint(126, 126));
    TestEqual(TEXT("Размер кратен странице"), Layout.GridSize.X % Layout.PageSizeInCells, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_TooManyCellsIsRejected,
    "Herbalist.WorldLayout.Solver.TooManyCellsIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_TooManyCellsIsRejected::RunTest(const FString& Parameters)
{
    // Ручная клетка 1 м на ландшафте 2016 м: 2016 x 2016 = 4 064 256 клеток --
    // больше предела MaxGridCells (4 млн), пока сетка целиком в памяти.
    FHerbalistWorldLayoutOverrides Overrides;
    Overrides.bOverrideCellSize = true;
    Overrides.CellSizeCm = 100.0f;

    TArray<FString> Warnings;
    const FHerbalistWorldLayout Layout = ResolveTestDev(MakeTestDevSource(), Warnings, Overrides);
    TestFalse(TEXT("Разметка отклонена"), Layout.bValid);
    TestTrue(TEXT("...и сказано почему"), AnyWarningContains(Warnings, TEXT("больше предела")));

    // Тот же ландшафт с клеткой 9 м -- 50 176 клеток, в пределе.
    Warnings.Reset();
    TestTrue(TEXT("Клетка 9 м проходит"), ResolveTestDev(MakeTestDevSource(), Warnings).bValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWorldLayout_SourceComparisonToleratesJitter,
    "Herbalist.WorldLayout.Solver.SourceComparisonToleratesJitter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWorldLayout_SourceComparisonToleratesJitter::RunTest(const FString& Parameters)
{
    const FHerbalistWorldLayoutSource Base = MakeTestDevSource();
    FHerbalistWorldLayoutSource Jittered = Base;
    Jittered.LandscapeMax += FVector2D(0.001, -0.002);
    TestTrue(TEXT("Тысячные сантиметра -- та же разметка"), FWorldLayoutSolver::IsSameSource(Base, Jittered));

    FHerbalistWorldLayoutSource Moved = Base;
    Moved.LandscapeMax.X += 12600.0;
    TestFalse(TEXT("Плитка ландшафта -- уже другая"), FWorldLayoutSolver::IsSameSource(Base, Moved));

    FHerbalistWorldLayoutSource Renamed = Base;
    Renamed.StreamingGridName = FName(TEXT("OtherGrid"));
    TestFalse(TEXT("Другое разбиение -- другая"), FWorldLayoutSolver::IsSameSource(Base, Renamed));
    return true;
}

#endif // WITH_AUTOMATION_TESTS
