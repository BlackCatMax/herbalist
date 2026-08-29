// Source/ProjectHerbalistTests/Private/Tests/BiomeGraphIntegrationTest.cpp
//
// Тестовая дыра, названная в ROADMAP.md §6: ни один автотест до этого не
// гонял настоящий AGridWorldManager::Tick() с реально инициализированным
// UBiomeGraphSubsystem (StepSimulation -> ApplyFieldsToGrid ->
// ApplyBiomeInfluences) одновременно. Herbalist.Save.
// BiomeInfluencesWithZeroFieldsStaySparse (SaveSystemTest.cpp) уже проверяет
// ApplyBiomeInfluences напрямую, юнит-стилем, с руками собранными
// MorokFields/ZaryanaFields -- полезно, но не ловит регрессию, которая
// возникла бы только при взаимодействии двух систем через настоящий Tick().
//
// AGridWorldManager::SpawnAndBeginPlay (см. другие тесты) не поднимает
// UBiomeGraphSubsystem -- его инициализирует только
// AProjectHerbalistGameModeBase::BeginPlay(), которого в editor-мире
// автотеста нет. Здесь делаем то же самое вручную: загружаем боевой
// DA_BiomeGraph и инициализируем подсистему им напрямую -- тот же путь,
// что игра проходит на старте уровня, не фейковый ассет.

#include "Core/World/GridWorldManager.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeGraph_RealTickKeepsDirtyCellsSparse,
    "Herbalist.BiomeGraph.RealTickKeepsDirtyCellsSparse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeGraph_RealTickKeepsDirtyCellsSparse::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
    if (!TestNotNull(TEXT("UBiomeGraphSubsystem present"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    // Тот же путь, что AProjectHerbalistGameModeBase::BeginPlay() -- боевой
    // ассет, не заглушка. Если DA_BiomeGraph переименуют/переместят, тест
    // сообщит об этом явно (TestNotNull), а не молча проскочит с
    // неинициализированным графом (что сделало бы весь тест бессмысленным --
    // StepSimulation() при !bInitialized ничего не делает).
    UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
    if (!TestNotNull(TEXT("DA_BiomeGraph asset loads"), Asset))
    {
        Manager->Destroy();
        return false;
    }
    Graph->InitializeFromAsset(Asset);
    if (!TestTrue(TEXT("BiomeGraphSubsystem reports initialized"), Graph->IsInitialized()))
    {
        Manager->Destroy();
        return false;
    }

    // GameClockSeconds по умолчанию 0.0 -- фаза Рассвет (§15.2), которая сама
    // разливает Purity/Stability-нудж по ВСЕЙ сетке легитимно (не баг) и
    // сделала бы ассерт ниже бессмысленным. GameDayMinutes по умолчанию 32
    // (1920 игровых секунд/сутки), День длится с 6-й по 26-ю игровую минуту
    // (секунды 360-1560) -- стартуем на 10-й минуте (600с) и тикаем реальными
    // 90 секундами теста, что на игровых часах тоже 90 секунд (GameClockSeconds
    // растёт 1:1 с DeltaTime) -- весь прогон остаётся внутри окна Дня, не
    // пересекая Рассвет/Закат/Ночь. Та же ловушка, что уже трижды находилась в
    // этой сессии (Гнильники/Winter-Purity/ItemCorrupting тесты).
    Manager->SetGameClockSeconds(600.0f);

    TestEqual(TEXT("Untouched, freshly initialized world has zero dirty cells"),
        Manager->CaptureSaveCells().Num(), 0);

    // Настоящий Tick(), не прямой вызов ApplyBiomeInfluences -- 900 шагов по
    // 0.1с = 90 секунд игрового времени. UBiomeGraphSubsystem::FixedTimeStep
    // (0.2с) и AGridWorldManager::SimulationFixedTimeStep (0.05с) оба меньше
    // шага 0.1с, так что каждый Tick() гарантированно прогоняет минимум по
    // одному внутреннему шагу обеих систем -- граф реально считает поля и
    // реально пишет их в клетки через ApplyBiomeInfluences на каждой
    // итерации, не только в момент старта.
    for (int32 Step = 0; Step < 900; ++Step)
    {
        Manager->Tick(0.1f);
    }

    const int32 GridCellCount = Manager->GridSizeX * Manager->GridSizeY;
    const int32 DirtyCount = Manager->CaptureSaveCells().Num();

    // Регрессия §7.1 (AUDIT_AND_REFACTORING_PLAN.md): до фикса ЛЮБОЙ реальный
    // шаг ApplyFieldsToGrid грязнил все 400/400 клеток безусловно, даже когда
    // Морок/Заряна ещё на нуле. Свежая сетка стартует близко к дефолтам --
    // поля успевают сдвинуться за 90 секунд, но не должны задеть каждую
    // клетку без разбора. Порог -- четверть сетки, не ноль: биомные амбиентные
    // существа (Гнильники и т.п.) и релаксация могут законно тронуть
    // отдельные клетки, тест ловит именно "унеслось до размера сетки", а не
    // "тронуло хоть что-то".
    TestTrue(FString::Printf(TEXT("Dirty cells (%d) stay well below full grid (%d) after 90s of real Tick()"),
        DirtyCount, GridCellCount), DirtyCount < GridCellCount / 4);
    TestTrue(TEXT("Dirty cells did not hit literally every cell (the exact §7.1 symptom)"),
        DirtyCount < GridCellCount);

    // UBiomeGraphSubsystem — WorldSubsystem, переживает этот тест: все
    // автотесты делят один и тот же GEditor->GetEditorWorldContext().World().
    // Без явной деинициализации следующий тест в том же прогоне унаследовал
    // бы уже инициализированный граф с реальными (не нулевыми) MorokField --
    // нашлось так: Herbalist.Legendary.* и Herbalist.Landmark.* тесты после
    // этого стали молча проявлять Легендарных существ поверх ожидаемых
    // Landmark на тех же клетках (общий детерминированный layout сетки).
    // Deinitialize() возвращает подсистему в то же состояние, что и после
    // OnWorldBeginPlay (Nodes/Edges пустые, bInitialized=false) -- тот же
    // сброс, который сделал бы реальный уход с уровня.
    Graph->Deinitialize();

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
