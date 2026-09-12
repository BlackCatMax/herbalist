// Source/ProjectHerbalistTests/Private/Tests/CellRadiusMetersTest.cpp
//
// Разметка мира, этап 3 (2026-09-12) -- радиусы в метрах (решение пользователя
// 11, DESIGN_World_Layout.md §5). Радиусы капищ, Шапки, оберегов, приманки,
// Соловья и Росы заданы в метрах и переводятся в клетки на размере клетки
// сетки. Отдельно -- фронт порчи: решение 12 (скорость заражения в метрах)
// проверкой не подтвердилось, фронт идёт со скоростью релаксации, а не ставки
// заражения (открытый вопрос в ROADMAP.md).

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
        Manager->SetGoryuchKamenSite(FIntPoint(-1, -1));
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
        for (double Elapsed = StepSeconds; Elapsed <= 60000.0; Elapsed += StepSeconds)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCellRadiusMeters_ContagionFrontIsLimitedByRelaxation,
    "Herbalist.WorldLayout.Meters.ContagionFrontIsLimitedByRelaxation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCellRadiusMeters_ContagionFrontIsLimitedByRelaxation::RunTest(const FString& Parameters)
{
    // DESIGN_World_Layout.md §5 предполагал, что фронт порчи идёт со
    // скоростью ставки заражения, и её пересчёт по размеру клетки удержит
    // фронт в метрах. Проверка (2026-09-12) показала другое: ставка 0.01
    // поднимает TargetState соседа в двадцать раз быстрее, чем State следует за
    // ним (AGridWorldManager::StateRelaxationPerSecond, 0.0005 в секунду), и
    // сосед перекидывается, когда до порога входа дойдёт именно State. Время
    // на клетку -- (порог входа - здоровое значение) / 0.0005 на любом размере
    // клетки, скорость в метрах пропорциональна клетке. Тест фиксирует это
    // поведение, пока не решено, как держать фронт в метрах (ROADMAP.md).
    // 60 м по оси X -- 3 клетки по 20 м, 6 по 10 м, 12 по 5 м.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const double EnterThreshold = Settings->BiomeDegradeCenterCorruption + Settings->BiomeDegradeMargin;
    const double HealthyCorruption = FBiomeDefaults::GetDefaultState(EBiomeType::Taiga).Meta.Corruption;
    const double RelaxationPerSecond = AGridWorldManager::StateRelaxationPerSecond;
    const double ExpectedSecondsPerCell = (EnterThreshold - HealthyCorruption) / RelaxationPerSecond;

    struct FCase { float CellSizeCm; int32 Column; };
    const FCase Cases[] = { { 2000.0f, 3 }, { 1000.0f, 6 }, { 500.0f, 12 } };
    for (const FCase& Case : Cases)
    {
        const double Seconds = MeasureContagionFrontSecondsForLayout(Manager, Case.CellSizeCm, Case.Column);
        if (!TestTrue(FString::Printf(TEXT("Клетка %.0f м: фронт дошёл до 60 м"), Case.CellSizeCm / 100.0f), Seconds > 0.0))
        {
            Manager->Destroy();
            return false;
        }
        const double SecondsPerCell = Seconds / Case.Column;
        AddInfo(FString::Printf(TEXT("Фронт порчи: клетка %.0f м, 60 м за %.0f с (%.0f с на клетку, %.5f м/с)"),
            Case.CellSizeCm / 100.0f, Seconds, SecondsPerCell, 60.0 / Seconds));
        // Допуск 5%: шаг симуляции 10 с добавляет к каждой клетке один шаг
        // обнаружения перехода (~10 с из ~1350).
        TestEqual(FString::Printf(TEXT("Клетка %.0f м: время на клетку задаёт релаксация"), Case.CellSizeCm / 100.0f),
            SecondsPerCell, ExpectedSecondsPerCell, ExpectedSecondsPerCell * 0.05);
    }

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
