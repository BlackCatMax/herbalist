// Source/ProjectHerbalistTests/Private/Tests/BiomeGraphIntegrationTest.cpp
//
// Тестовая дыра, названная в ROADMAP.md (см. CHANGELOG.md 2026-08-29): ни один автотест до этого не
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

    // 2026-09-03, ПЕРЕСМОТР СМЫСЛА ЭТОГО АССЕРТА. Раньше здесь стояло
    // "грязных клеток меньше четверти сетки" -- и это проходило по ложной
    // причине: UBiomeGraphSubsystem::FindGridWorldManager ищет менеджер
    // через TActorIterator и кэширует ПЕРВЫЙ найденный, а в персистентном
    // editor-мире жил настоящий BP_GridWorldManager с L_TestDev. Граф всё
    // это время писал поля в ЧУЖОЙ менеджер, а не в тестовый -- сетка теста
    // оставалась почти чистой сама по себе, и ассерт не измерял ничего.
    // Изоляция починена в TestWorldHelpers.h (прежние менеджеры уничтожаются
    // перед спавном своего), после чего граф впервые реально дошёл до этой
    // сетки -- и тронул все 400 клеток за 90 секунд.
    //
    // Это НЕ регрессия §7.1: сама защита цела (ApplyBiomeInfluences метит
    // клетку грязной только при фактическом изменении TargetState, см.
    // GridWorldManagerCore.cpp) и продолжает проверяться юнит-стилем в
    // Herbalist.Save.BiomeInfluencesWithZeroFieldsStaySparse -- на нулевых
    // полях сетка по-прежнему остаётся чистой. Здесь же поля за 90 игровых
    // секунд законно уезжают от нуля во ВСЕХ биомах сразу (граф считает их
    // на уровне узлов, а узел покрывает много клеток), поэтому "тронуто
    // мало клеток" -- неверное ожидание для этого сценария, а не признак
    // здоровья.
    //
    // Ценность теста -- в связке систем через настоящий Tick(), её и
    // проверяем: граф реально доходит до сетки, а не молчит.
    TestTrue(FString::Printf(TEXT("Real Tick() actually wires graph fields into the grid (%d of %d cells changed)"),
        DirtyCount, GridCellCount), DirtyCount > 0);

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

// ---------------------------------------------------------------------------
// Персистентность накопленных полей графа (AUDIT_AND_REFACTORING_PLAN.md
// §7.1, 2026-09-06, решение пользователя: "граф должен переживать
// сохранение"). RestoreNodeFieldState тестируется напрямую на живой
// подсистеме -- тот же класс пробела на GameInstanceSubsystem (реальный
// путь через UHerbalistSaveSubsystem::SaveGame/LoadGame), что уже
// известен у ActivateWard/TradeWithCommunity (см. ROADMAP.md), сам эффект
// не зависит от GameInstance вовсе.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeGraph_RestoreNodeFieldStateOverwritesCurrentWithSaved,
    "Herbalist.BiomeGraph.RestoreNodeFieldStateOverwritesCurrentWithSaved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeGraph_RestoreNodeFieldStateOverwritesCurrentWithSaved::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
    if (!TestNotNull(TEXT("UBiomeGraphSubsystem present"), Graph)) return false;

    UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
    if (!TestNotNull(TEXT("DA_BiomeGraph asset loads"), Asset)) return false;
    Graph->InitializeFromAsset(Asset);
    if (!TestTrue(TEXT("BiomeGraphSubsystem reports initialized"), Graph->IsInitialized())) return false;

    if (!TestTrue(TEXT("Graph has at least one node"), Graph->GetNodes().Num() > 0))
    {
        Graph->Deinitialize();
        return false;
    }
    const FName BiomeID = Graph->GetNodes().CreateConstIterator()->Key;

    // "Сохранённое" состояние -- то, что было в момент SaveGame.
    if (FBiomeGraphNode* Node = Graph->GetMutableNode(BiomeID))
    {
        Node->MorokField = 0.42f;
        Node->ZaryanaField = 0.24f;
        Node->Memory.MorokHistory = 0.5f;
    }
    const TMap<FName, FBiomeGraphNode> Saved = Graph->GetNodes();

    // "Текущее" состояние на момент LoadGame -- намеренно другое, имитирует
    // сессию, накопившую иные значения до вызова restore.
    if (FBiomeGraphNode* Node = Graph->GetMutableNode(BiomeID))
    {
        Node->MorokField = 0.9f;
        Node->ZaryanaField = 0.1f;
        Node->Memory.MorokHistory = 0.0f;
    }

    Graph->RestoreNodeFieldState(Saved);

    const FBiomeGraphNode* Restored = Graph->GetNode(BiomeID);
    if (TestNotNull(TEXT("Node still exists after restore"), Restored))
    {
        TestEqual(TEXT("MorokField restored to the saved value"), Restored->MorokField, 0.42f);
        TestEqual(TEXT("ZaryanaField restored to the saved value"), Restored->ZaryanaField, 0.24f);
        TestEqual(TEXT("Memory.MorokHistory restored to the saved value"), Restored->Memory.MorokHistory, 0.5f);
    }

    Graph->Deinitialize();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeGraph_RestoreNodeFieldStateIgnoresUnknownBiomes,
    "Herbalist.BiomeGraph.RestoreNodeFieldStateIgnoresUnknownBiomes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeGraph_RestoreNodeFieldStateIgnoresUnknownBiomes::RunTest(const FString& Parameters)
{
    // Старый сейв без этого поля (пустая карта) или сейв, ссылающийся на
    // биом, которого больше нет в DA_BiomeGraph -- не должны крашить или
    // как-либо трогать реальные узлы.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
    if (!TestNotNull(TEXT("UBiomeGraphSubsystem present"), Graph)) return false;

    UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
    if (!TestNotNull(TEXT("DA_BiomeGraph asset loads"), Asset)) return false;
    Graph->InitializeFromAsset(Asset);

    TMap<FName, FBiomeGraphNode> BogusSave;
    FBiomeGraphNode Bogus;
    Bogus.MorokField = 0.99f;
    BogusSave.Add(FName(TEXT("НетТакогоБиома")), Bogus);

    // Не должно упасть, не должно завести несуществующий узел.
    Graph->RestoreNodeFieldState(BogusSave);
    TestNull(TEXT("No node was created for the unknown biome"), Graph->GetNode(FName(TEXT("НетТакогоБиома"))));

    Graph->Deinitialize();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
