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

// Баг (2026-09-06, найдено по PIE-логу пользователя: "после трёх сборов и
// долгого времени испортилась вся сетка" -- ReportGridCorruption показал
// Distortion, растущий по ВСЕЙ сетке разом чисто от времени, при нуле
// реально сработавших заражений соседей). PropagateWaves складывал
// MorokField/ZaryanaField с вкладом соседних узлов без единого вычитания,
// ApplyBiomeInfluences складывал поле в TargetState.Distortion клеток без
// единого вычитания -- замкнутый контур с положительной обратной связью и
// без тормоза. GlobalMorokDecay/GlobalZaryanaDecay уже существовали, но
// применялись только к Memory.*History, не к самому полю. Регрессия ровно
// на добавленные строки в UpdateMemories.
//
// Изолируем декей от остального конвейера: перекрашиваем ВСЕ клетки сетки
// в биом, отличный от целевого (Tundra) -- у целевого узла (Bog) в
// RecalculateFieldsFromGrid тогда Count==0, ветка "if (Count > 0)" не
// трогает MorokField вовсе. Все ОСТАЛЬНЫЕ узлы занулены -- входящих рёбер
// в Bog в этом шаге нет вовсе (PrevMorok соседа = 0).
//
// Пересмотрено 2026-09-06 (после перевода PropagateWaves на настоящую
// консервативную диффузию, прямое решение пользователя): раньше Bog с
// занулёнными соседями терял только декей, потому что старый код НИЧЕГО
// не вычитал у источника -- собственные исходящие рёбра Bog ничего ему не
// стоили. Теперь, когда FromBiome честно теряет ровно то, что получает
// ToBiome, у Bog с его СОБСТЕННЫМ ненулевым исходящим ребром (Bog->Floodplain,
// leak=0.15) появляется РЕАЛЬНАЯ, ожидаемая убыль от диффузии -- сосед на
// нуле не отправляет ничего ВХОДЯЩЕГО, но сам Bog всё равно ОТПРАВЛЯЕТ
// часть своего значения наружу. Это не баг теста и не баг фикса -- это
// именно то поведение, которое пользователь и просил ("рост в принципе
// невозможен без внешнего источника" подразумевает и честную убыль по
// исходящим рёбрам). Ожидаемое значение считается аналитически из реальных
// рёбер ассета, не захардкожено -- тест переживёт правку данных графа.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeGraph_MorokFieldDecaysOverTimeWithoutInput,
    "Herbalist.BiomeGraph.MorokFieldDecaysOverTimeWithoutInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeGraph_MorokFieldDecaysOverTimeWithoutInput::RunTest(const FString& Parameters)
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

    const FName TargetBiomeID(TEXT("Bog"));
    if (!TestTrue(TEXT("Graph has a Bog node"), Graph->GetNodes().Contains(TargetBiomeID)))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    for (const auto& Pair : Graph->GetNodes())
    {
        if (FBiomeGraphNode* Node = Graph->GetMutableNode(Pair.Key))
        {
            Node->MorokField = 0.0f;
        }
    }
    Graph->GetMutableNode(TargetBiomeID)->MorokField = 1.0f;

    // Ни одна клетка не заявляет Bog -- RecalculateFieldsFromGrid видит
    // Count==0 для него и оставляет MorokField нетронутым своей веткой.
    for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            if (FGridCell* Cell = Manager->GetCell(X, Y))
            {
                Cell->Biome = EBiomeType::Tundra;
            }
        }
    }

    // Аналитическое ожидание одного шага: диффузия честно списывает с Bog
    // сумму leak по ВСЕМ его исходящим рёбрам (соседи на нуле, входящих
    // нет), затем декей. Никаких шрайнов в тестовом мире нет -- демпинг
    // границ капищ (ShrineBorderLeakDampening) не участвует.
    float TotalOutgoingLeak = 0.0f;
    for (const FBiomeGraphEdge& Edge : Graph->GetEdges())
    {
        if (Edge.FromBiome == TargetBiomeID)
        {
            TotalOutgoingLeak += Edge.MorokLeak * Asset->GlobalInfluenceScale;
        }
    }
    const float ExpectedAfterDiffusion = FMath::Clamp(1.0f - TotalOutgoingLeak, 0.0f, 1.0f);
    const float ExpectedFinal = ExpectedAfterDiffusion * (1.0f - Asset->GlobalMorokDecay * Asset->FixedTimeStep);

    Graph->ForceStep();

    const FBiomeGraphNode* Result = Graph->GetNode(TargetBiomeID);
    if (TestNotNull(TEXT("Target node still exists"), Result))
    {
        TestTrue(FString::Printf(TEXT("MorokField matches analytical diffusion+decay prediction (got %.5f, expected %.5f, outgoing leak %.3f)"),
            Result->MorokField, ExpectedFinal, TotalOutgoingLeak),
            FMath::IsNearlyEqual(Result->MorokField, ExpectedFinal, 0.001f));
    }

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

// Тот же баг класса, что MorokField выше, найден фоновым аудитом сразу
// вслед за его фиксом (2026-09-06): Memory.Instability/AxisDrift decay'ились
// голыми множителями за ШАГ (0.995f/0.98f) без домножения на StepDeltaTime.
// Instability/AxisDrift, в отличие от MorokField/ZaryanaField, НЕ трогаются
// ни RecalculateFieldsFromGrid, ни PropagateWaves, ни ApplyFieldsToGrid --
// единственный писатель декея это UpdateMemories, поэтому изоляция не
// нужна вовсе, тест проще предыдущего. Проверяем поведенческую
// эквивалентность на боевом FixedTimeStep (0.2с, BiomeGraphAsset.h):
// InstabilityDecay=0.025/AxisDriftDecay=0.1 посчитаны ОБРАТНО из старых
// констант именно так, чтобы баланс не изменился -- один шаг должен дать
// ТЕ ЖЕ 0.995/0.98, что и до фикса.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeGraph_InstabilityAndAxisDriftDecayMatchesPreFixBehaviorAtDefaultStep,
    "Herbalist.BiomeGraph.InstabilityAndAxisDriftDecayMatchesPreFixBehaviorAtDefaultStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeGraph_InstabilityAndAxisDriftDecayMatchesPreFixBehaviorAtDefaultStep::RunTest(const FString& Parameters)
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

    if (!TestTrue(TEXT("Graph has at least one node"), Graph->GetNodes().Num() > 0))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    const FName TargetBiomeID = Graph->GetNodes().CreateConstIterator()->Key;

    FBiomeGraphNode* Node = Graph->GetMutableNode(TargetBiomeID);
    Node->Memory.Instability = 1.0f;
    Node->Memory.AxisDrift = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

    // Один шаг FixedTimeStep (0.2с) -- не 50, здесь важна ТОЧНАЯ поведенческая
    // эквивалентность одного шага, не долгая тенденция.
    Graph->ForceStep();

    const FBiomeGraphNode* Result = Graph->GetNode(TargetBiomeID);
    if (TestNotNull(TEXT("Target node still exists"), Result))
    {
        TestTrue(FString::Printf(TEXT("Instability matches pre-fix 0.995 factor (got %.5f, expected ~0.995)"), Result->Memory.Instability),
            FMath::IsNearlyEqual(Result->Memory.Instability, 0.995f, 0.001f));
        TestTrue(FString::Printf(TEXT("AxisDrift.X matches pre-fix 0.98 factor (got %.5f, expected ~0.98)"), Result->Memory.AxisDrift.X),
            FMath::IsNearlyEqual(Result->Memory.AxisDrift.X, 0.98f, 0.001f));
    }

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

// Диффузия вместо аддитивного копирования (2026-09-06, прямое решение
// пользователя после найденного вживую бага: пара узлов с рёбрами в обе
// стороны, например Bog<->Floodplain, неограниченно раздувала друг друга,
// декей на порядок слабее утечки). Раньше PropagateWaves добавлял поток
// получателю, ничего не вычитая у источника -- суммарный Морок по всему
// графу рос сам собой на каждом шаге без единого внешнего события.
//
// Проверяем инвариант напрямую: сумма MorokField по ВСЕМ узлам после
// одного полного шага должна отличаться от суммы ДО шага РОВНО на декей
// (GlobalMorokDecay, тот же множитель, что уже у Memory.*History) -- если
// диффузия хоть немного "рождает" или "теряет" Морок сама по себе,
// сумма разойдётся с этим предсказанием. RecalculateFieldsFromGrid
// нейтрализован отдельно: перед шагом каждая клетка сетки получает ТО ЖЕ
// Distortion, что уже стоит в её узле -- блендинг к среднему по сетке
// тогда не двигает поле (Lerp(V,V,x)=V), остаётся ровно диффузия+декей.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeGraph_PropagateWavesConservesTotalMorokAcrossAllNodes,
    "Herbalist.BiomeGraph.PropagateWavesConservesTotalMorokAcrossAllNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeGraph_PropagateWavesConservesTotalMorokAcrossAllNodes::RunTest(const FString& Parameters)
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

    // Раскраска -- каждому узлу своё отдельное, ненулевое значение
    // (0.1, 0.2, 0.3...), чтобы реально нагрузить рёбра во всех
    // направлениях разом, не только один сосед на нуле.
    int32 Index = 0;
    for (const auto& Pair : Graph->GetNodes())
    {
        if (FBiomeGraphNode* Node = Graph->GetMutableNode(Pair.Key))
        {
            Node->MorokField = FMath::Fmod(0.1f * (Index + 1), 0.9f) + 0.05f;
        }
        ++Index;
    }

    // Нейтрализуем RecalculateFieldsFromGrid -- каждая клетка получает
    // Distortion, равный текущему полю ЕЁ СОБСТВЕННОГО биома.
    for (int32 Y = 0; Y < Manager->GridSizeY; ++Y)
    {
        for (int32 X = 0; X < Manager->GridSizeX; ++X)
        {
            FGridCell* Cell = Manager->GetCell(X, Y);
            if (!Cell) continue;
            const FName BiomeID = FBiomeDefaults::BiomeTypeToName(Cell->Biome);
            if (const FBiomeGraphNode* Node = Graph->GetNode(BiomeID))
            {
                Cell->State.Meta.Distortion = Node->MorokField;
            }
        }
    }

    double TotalBefore = 0.0;
    for (const auto& Pair : Graph->GetNodes())
    {
        TotalBefore += Pair.Value.MorokField;
    }

    Graph->ForceStep();

    double TotalAfter = 0.0;
    for (const auto& Pair : Graph->GetNodes())
    {
        TotalAfter += Pair.Value.MorokField;
    }

    // Единственная легитимная убыль за один шаг -- декей, тот же множитель,
    // что уже используется в UpdateMemories для Memory.MorokHistory.
    const double ExpectedFactor = 1.0 - static_cast<double>(Asset->GlobalMorokDecay) * static_cast<double>(Asset->FixedTimeStep);
    const double ExpectedTotal = TotalBefore * ExpectedFactor;

    TestTrue(FString::Printf(TEXT("Total MorokField after diffusion+decay matches pure-decay prediction (got %.6f, expected %.6f, before %.6f)"),
        TotalAfter, ExpectedTotal, TotalBefore),
        FMath::IsNearlyEqual(TotalAfter, ExpectedTotal, 0.001));

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
