// Source/ProjectHerbalistTests/Private/Tests/CellRadiusMetersTest.cpp
//
// Разметка мира, этап 3 (2026-09-12) -- радиусы в метрах (решение пользователя
// 11, DESIGN_World_Layout.md §5). Радиусы капищ, Шапки, оберегов, приманки,
// Соловья и Росы заданы в метрах и переводятся в клетки на размере клетки
// сетки. Фронт порчи тоже в метрах (решение 12, 2026-09-13): ставка заражения
// задана для опорной клетки 10 м и пересчитывается на клетку сетки.

#include "Core/World/GridWorldManager.h"
#include "Core/World/WorldLayout.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellRadiusMeters_MetersToCellsFollowsDesignTable,
    "Herbalist.WorldLayout.Meters.MetersToCellsFollowsDesignTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellRadiusMeters_MetersToCellsFollowsDesignTable::RunTest(const FString& Parameters)
{
    // Таблица DESIGN_World_Layout.md §5: при клетке 10 м радиусы прежние (3 и
    // 1 клетка), при 9 м -- 3 и 1 клетка (27 и 9 м), при 7 и 14 м «30 м» --
    // 4 и 2 клетки (28 м).
    TestEqual(TEXT("30 м на клетке 10 м -- 3 клетки, как было"), FWorldLayoutSolver::MetersToCellRadius(30.0, 1000.0), 3);
    TestEqual(TEXT("10 м на клетке 10 м -- 1 клетка, как было"), FWorldLayoutSolver::MetersToCellRadius(10.0, 1000.0), 1);
    TestEqual(TEXT("30 м на клетке 9 м -- 3 клетки (27 м)"), FWorldLayoutSolver::MetersToCellRadius(30.0, 900.0), 3);
    TestEqual(TEXT("10 м на клетке 9 м -- 1 клетка (9 м)"), FWorldLayoutSolver::MetersToCellRadius(10.0, 900.0), 1);
    TestEqual(TEXT("30 м на клетке 7 м -- 4 клетки (28 м)"), FWorldLayoutSolver::MetersToCellRadius(30.0, 700.0), 4);
    TestEqual(TEXT("30 м на клетке 14 м -- 2 клетки (28 м)"), FWorldLayoutSolver::MetersToCellRadius(30.0, 1400.0), 2);
    TestEqual(TEXT("Ноль метров -- ноль клеток"), FWorldLayoutSolver::MetersToCellRadius(0.0, 900.0), 0);
    TestEqual(TEXT("Отрицательные метры -- ноль"), FWorldLayoutSolver::MetersToCellRadius(-5.0, 900.0), 0);
    TestEqual(TEXT("Радиус меньше половины клетки не исчезает -- 1"), FWorldLayoutSolver::MetersToCellRadius(2.0, 900.0), 1);
    TestEqual(TEXT("Вырожденная клетка не делит на ноль"), FWorldLayoutSolver::MetersToCellRadius(30.0, 0.0), 3000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellRadiusMeters_ManagerConvertsOnItsOwnCell,
    "Herbalist.WorldLayout.Meters.ManagerConvertsOnItsOwnCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellRadiusMeters_ManagerConvertsOnItsOwnCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Менеджер автотеста -- клетка 1 м.
    TestEqual(TEXT("Клетка тестового мира 1 м -- 30 м это 30 клеток"), Manager->GetCellRadius(30.0f), 30);
    Manager->CellSize = 1000.0f;
    TestEqual(TEXT("На клетке 10 м -- 3 клетки"), Manager->GetCellRadius(30.0f), 3);

    Manager->Destroy();
    return true;
}

namespace
{
    // Секунды, за которые фронт порчи от испорченного столбца x=0 перекидывает
    // клетку столбца TargetColumn. Вся сетка -- одна Тайга без воды, капищ и
    // Горюч-камня: остаются только заражение соседей и релаксация.
    double MeasureContagionFrontSecondsForLayout(AGridWorldManager* Manager, float CellSizeCm, int32 TargetColumn)
    {
        Manager->CellSize = CellSizeCm;
        Manager->SetShrines({});
        Manager->SetGoryuchKamenSite(HerbalistCore::InvalidCell());
        for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
        {
            for (int32 X = 0; X < Manager->GridSizeX; ++X)
            {
                FGridCell* Cell = Manager->GetCell(X, Y);
                if (!Cell)
                {
                    continue;
                }
                Cell->Biome = EBiomeType::Taiga;
                Cell->bIsWater = false;
                Cell->bEternallyPure = false;
                Cell->HarvestStress = 0.0f;
                const FRealState Healthy = BiomeDefaultStateForCell(*Cell);
                Cell->State = Healthy;
                Cell->TargetState = Healthy;
                Cell->Memory.bDegrading = false;
                if (X == 0)
                {
                    Cell->State.Meta.Corruption = 1.0f;
                    Cell->TargetState.Meta.Corruption = 1.0f;
                    Cell->TargetState.Meta.Purity = 0.0f;
                    Cell->TargetState.Meta.Distortion = 1.0f;
                    Cell->TargetState.Meta.Stability = 0.0f;
                    Cell->Memory.bDegrading = true;
                }
            }
        }

        const int32 Row = Manager->GridSizeY / 2;
        const double StepSeconds = 10.0;
        for (double Elapsed = StepSeconds; Elapsed <= 120000.0; Elapsed += StepSeconds)
        {
            Manager->RegenerateCellParameters(StepSeconds);
            const FGridCell* Probe = Manager->GetCell(TargetColumn, Row);
            if (Probe && Probe->Memory.bDegrading)
            {
                return Elapsed;
            }
        }
        return -1.0;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellRadiusMeters_ContagionFrontSpeedIsSameInMeters,
    "Herbalist.WorldLayout.Meters.ContagionFrontSpeedIsSameInMeters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellRadiusMeters_ContagionFrontSpeedIsSameInMeters::RunTest(const FString& Parameters)
{
    // DESIGN_World_Layout.md §5: скорость фронта порчи в метрах за игровое время
    // одинакова на разных клетках. Сосед перекидывается, когда до порога входа
    // дойдёт его State; пока действующая ставка заражения не быстрее релаксации
    // State (0.0005 в секунду), State идёт за TargetState, и время на клетку --
    // (порог входа - здоровое значение) / действующая ставка. Действующая ставка
    // обратно пропорциональна клетке, так что 90 м проходятся за одно и то же
    // время: 90 м × (порог - здоровое) / (ставка × 10 м).
    // 90 м по оси X -- 3 клетки по 30 м, 5 по 18 м, 10 по 9 м.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const double EnterThreshold = Settings->BiomeDegradeCenterCorruption + Settings->BiomeDegradeMargin;
    const double HealthyCorruption = FBiomeDefaults::GetDefaultState(EBiomeType::Taiga).Meta.Corruption;
    const double RelaxationPerSecond = AGridWorldManager::StateRelaxationPerSecond;
    const double DistanceMeters = 90.0;
    const double ExpectedSeconds = DistanceMeters * (EnterThreshold - HealthyCorruption)
        / (Settings->ContagionSpreadRate * UHerbalistSettings::ContagionReferenceCellMeters);

    struct FCase { float CellSizeCm; int32 Column; };
    const FCase Cases[] = { { 3000.0f, 3 }, { 1800.0f, 5 }, { 900.0f, 10 } };
    for (const FCase& Case : Cases)
    {
        const double Seconds = MeasureContagionFrontSecondsForLayout(Manager, Case.CellSizeCm, Case.Column);
        if (!TestTrue(FString::Printf(TEXT("Клетка %.0f м: фронт дошёл до 90 м"), Case.CellSizeCm / 100.0f), Seconds > 0.0))
        {
            Manager->Destroy();
            return false;
        }
        AddInfo(FString::Printf(TEXT("Фронт порчи: клетка %.0f м, 90 м за %.0f с (%.0f с на клетку, %.5f м/с)"),
            Case.CellSizeCm / 100.0f, Seconds, Seconds / Case.Column, DistanceMeters / Seconds));
        // Допуск 5%: шаг симуляции 10 с добавляет к каждой клетке один шаг
        // обнаружения перехода (до 10 шагов из ~13 500 с).
        TestEqual(FString::Printf(TEXT("Клетка %.0f м: 90 м за то же время, что на любой клетке"), Case.CellSizeCm / 100.0f),
            Seconds, ExpectedSeconds, ExpectedSeconds * 0.05);
    }

    // Клетка мельче 9 м: действующая ставка обгоняет релаксацию, время на клетку
    // задаёт релаксация, и фронт в метрах медленнее -- предел, записанный у
    // ContagionSpreadRate. 90 м -- 18 клеток по 5 м.
    const double SmallCellSeconds = MeasureContagionFrontSecondsForLayout(Manager, 500.0f, 18);
    const double RelaxationLimitedSeconds = 18.0 * (EnterThreshold - HealthyCorruption) / RelaxationPerSecond;
    if (TestTrue(TEXT("Клетка 5 м: фронт дошёл до 90 м"), SmallCellSeconds > 0.0))
    {
        TestEqual(TEXT("Клетка 5 м: время на клетку задаёт релаксация"),
            SmallCellSeconds, RelaxationLimitedSeconds, RelaxationLimitedSeconds * 0.05);
        TestTrue(TEXT("...и фронт в метрах медленнее, чем на клетках от 9 м"), SmallCellSeconds > ExpectedSeconds * 1.05);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
