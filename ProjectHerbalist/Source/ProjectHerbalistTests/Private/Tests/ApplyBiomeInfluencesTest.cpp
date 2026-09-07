// Source/ProjectHerbalistTests/Private/Tests/ApplyBiomeInfluencesTest.cpp
//
// "Дырявое ведро" вместо непрерывного сложения (2026-09-07, прямой выбор
// пользователя из двух вариантов: A/Лерп к уровню vs B/накопление с
// затуханием -- выбран B). Найдено вживую пользователем на L_Playtest:
// ApplyBiomeInfluences (GridWorldManagerCore.cpp) складывал MorokField*0.1
// в Distortion КАЖДЫЙ шаг симуляции без единого вычитания -- растило
// Distortion к потолку 1.0 без единой внешней причины (ни контагиона, ни
// варки/сбора игрока), вопреки канону "Distortion -- устойчивый уровень
// биома" (02_GDD/12_Biome_Change.md §12.10). Существующий
// Herbalist.Save.BiomeInfluencesWithZeroFieldsStaySparse и
// Herbalist.ShrineType.StoneDampensMorokInfluence уже проверяют смежные
// свойства (нулевые поля не метят грязным / капище глушит PUSH) -- здесь
// три новых теста ровно на саму механику "дырявого ведра": равновесие
// ниже потолка, декей к нулю при отсутствии Морока, и остановка для
// клеток в испорченном полюсе бистабильности. Сохранение ХАРАКТЕРА биома
// (Болото держится у своих честных 0.70) -- НЕ забота этой функции в
// изоляции, это свойство всей связки с RecalculateFieldsFromGrid на
// уровне многошаговой симуляции (первая версия правки этого файла
// пыталась сделать это здесь через "BiomeDefault + MorokField" -- оказалось
// двойным счётом и снова открыло неограниченный рост, см. правку в
// GridWorldManagerCore.cpp; настоящая проверка сохранения характера биома
// теперь в BiomeGraphIntegrationTest.cpp).

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Core/Save/HerbalistSaveSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphTypes.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

// Равновесие "дырявого ведра" -- одного шага достаточно для точной
// аналитической проверки формулы, не нужно гонять до сходимости: результат
// после ОДНОГО шага полностью детерминирован по Push/Decay из
// HerbalistSettings (дефолты 0.01/0.01, если проект не переопределил их
// в конфиге).
//
// Система отсчёта (важно, здесь один раз уже ошиблись). Ведро работает на
// ОТКЛОНЕНИИ от дефолта биома: Dev' = Dev + (Morok*Push - Decay*Dev)*dt,
// откуда равновесие Dev* = Morok*(Push/Decay), то есть равновесный
// Distortion = ДЕФОЛТ БИОМА + Morok*(Push/Decay). Прежняя редакция этого
// комментария утверждала обратное ("цель -- сам MorokField, не BiomeDefault
// + MorokField") -- и была верна ровно для той, АБСОЛЮТНОЙ формулировки,
// которую заменили вторым заходом 2026-09-07: тогда MorokField сам сходился
// к среднему АБСОЛЮТНОМУ Distortion биома, и прибавлять дефолт сверху
// действительно означало считать одну величину дважды (этот баг и ловили
// по PIE-логу пользователя). Теперь MorokField -- среднее ОТКЛОНЕНИЕ, в
// покое ноль, двойного счёта нет, и дефолт обязан входить в ожидание.
//
// Тест ставит клетку РОВНО на дефолт её биома (нулевое отклонение), чтобы
// один шаг показывал вклад одного только Морока, не смешанный с возвратом
// клетки к природе своего биома.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_MorokPushesDistortionTowardEquilibriumNotCeiling,
    "Herbalist.ApplyBiomeInfluences.MorokPushesDistortionTowardEquilibriumNotCeiling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_MorokPushesDistortionTowardEquilibriumNotCeiling::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->Biome = EBiomeType::MixedForest;
    Cell->Memory.bDegrading = false;

    // Старт ровно на природе биома -- отклонение ноль. Дефолт берём тем же
    // выбором land/water, что делает и сама ApplyBiomeInfluences: клетка
    // (0,0) в этом мире может оказаться водной, и зашитое число одного из
    // двух вариантов сделало бы тест зависимым от случайности генерации.
    const float BiomeDefaultDistortion = BiomeDefaultStateForCell(*Cell).Meta.Distortion;
    Cell->TargetState.Meta.Distortion = BiomeDefaultDistortion;

    TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::MixedForest), 0.3f } };
    TMap<FName, float> ZaryanaFields;

    const float DeltaTime = 1.0f;
    Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, DeltaTime);

    // Аналитическое предсказание при дефолтных PushRate=DecayRate=0.01
    // (HerbalistSettings.h): Dev' = Dev + (Morok*Push - Decay*Dev)*dt.
    // При Dev=0 стартово: Dev' = Morok*Push*dt = 0.3*0.01*1.0 = 0.003,
    // то есть итог = дефолт биома + 0.003.
    const float ExpectedDistortion = BiomeDefaultDistortion + 0.3f * 0.01f * DeltaTime;
    TestTrue(FString::Printf(TEXT("Distortion moved toward the equilibrium MorokField sets ABOVE its biome default, not toward 1.0 (got %.5f, expected %.5f, biome default %.5f)"),
        Cell->TargetState.Meta.Distortion, ExpectedDistortion, BiomeDefaultDistortion),
        FMath::IsNearlyEqual(Cell->TargetState.Meta.Distortion, ExpectedDistortion, 0.0001f));
    TestTrue(TEXT("A single step with moderate MorokField does not snap Distortion anywhere near the 1.0 ceiling"),
        Cell->TargetState.Meta.Distortion < 0.5f);

    Manager->Destroy();
    return true;
}

// Ноль в MorokField тянет Distortion к ДЕФОЛТУ СВОЕГО БИОМА, а не к нулю.
// Тест назывался ...TowardZero и считал ожидание от нуля -- это описывало
// прежнюю, АБСОЛЮТНУЮ формулировку ведра. После перевода ветки Морока на
// отклонения (2026-09-07) затухание поля означает возврат к природе биома:
// Болото при пустом поле обязано осесть на своих 0.70 (0.75 в воде), а не
// сползти в стерильный ноль. Это ровно тот инвариант, который зафиксирован
// прямым решением пользователя -- биом не может уйти ниже собственного
// порога без вмешательства извне.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_ZeroMorokDecaysDistortionTowardTheBiomeDefault,
    "Herbalist.ApplyBiomeInfluences.ZeroMorokDecaysDistortionTowardTheBiomeDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_ZeroMorokDecaysDistortionTowardTheBiomeDefault::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->Biome = EBiomeType::Bog;
    Cell->Memory.bDegrading = false;
    Cell->TargetState.Meta.Distortion = 0.9f;

    const float BiomeDefaultDistortion = BiomeDefaultStateForCell(*Cell).Meta.Distortion;

    TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog), 0.0f } };
    TMap<FName, float> ZaryanaFields;

    Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 1.0f);

    // Dev = 0.9 - дефолт; Dev' = Dev - Decay*Dev*dt; итог = дефолт + Dev'.
    const float StartDeviation = 0.9f - BiomeDefaultDistortion;
    const float ExpectedDistortion = BiomeDefaultDistortion + StartDeviation - 0.01f * StartDeviation * 1.0f;
    TestTrue(FString::Printf(TEXT("Distortion decayed back toward its OWN biome default (got %.4f, expected %.4f, biome default %.4f)"),
        Cell->TargetState.Meta.Distortion, ExpectedDistortion, BiomeDefaultDistortion),
        FMath::IsNearlyEqual(Cell->TargetState.Meta.Distortion, ExpectedDistortion, 0.0001f));
    TestTrue(FString::Printf(TEXT("Пустое поле Морока не утаскивает Болото НИЖЕ его собственной природы (получено %.4f, дефолт биома %.4f)"),
        Cell->TargetState.Meta.Distortion, BiomeDefaultDistortion),
        Cell->TargetState.Meta.Distortion >= BiomeDefaultDistortion);

    Manager->Destroy();
    return true;
}

// Испорченный полюс бистабильности (Cell.Memory.bDegrading) управляет
// Distortion сам (02_GDD/12_Biome_Change.md §12.10: "выход только прямым
// действием игрока") -- "дырявое ведро" должно полностью отступить, иначе
// оно тихо подрывало бы именно то свойство, ради которого полюс существует.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_SkipsCellsInTheDegradingBistablePole,
    "Herbalist.ApplyBiomeInfluences.SkipsCellsInTheDegradingBistablePole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_SkipsCellsInTheDegradingBistablePole::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->Biome = EBiomeType::Taiga;
    Cell->Memory.bDegrading = true;
    Cell->TargetState.Meta.Distortion = 1.0f;   // как ставит сама бистабильность на переходе

    // Максимально сильное давление Морока -- если бы guard не сработал,
    // Distortion не изменился бы всё равно (уже на потолке), поэтому
    // проверяем на Purity/Stability (Заряна тянула бы их ВВЕРХ от их
    // текущего значения -- изменение было бы заметно, если бы не guard).
    Cell->TargetState.Meta.Purity = 0.0f;
    Cell->TargetState.Meta.Stability = 0.0f;

    TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga), 1.0f } };
    TMap<FName, float> ZaryanaFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga), 1.0f } };

    Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 1.0f);

    TestEqual(TEXT("Distortion untouched while degrading -- bistability owns it exclusively"),
        Cell->TargetState.Meta.Distortion, 1.0f);
    TestEqual(TEXT("Purity untouched while degrading -- Zaryana push does not passively rescue a corrupted-pole cell"),
        Cell->TargetState.Meta.Purity, 0.0f);
    TestEqual(TEXT("Stability untouched while degrading, same reasoning"),
        Cell->TargetState.Meta.Stability, 0.0f);

    Manager->Destroy();
    return true;
}

// Ради чего затевалась правка "а" (2026-09-07, выбор пользователя):
// Purity/Stability обязаны возвращаться к дефолтам СВОЕГО биома, а не к
// f(Distortion), как было до неё (MATH_REFERENCE.md §6.2, замер: Тайга
// теряла Purity 0.70 -> 0.55 за 300с и шла к 0.375).
//
// Тесту нужны НАСТОЯЩИЕ дефолты биомов: на нулевых "отклонение от дефолта"
// численно неотличимо от "абсолютного значения", и проверять было бы нечего.
// Раньше здесь стояла точечная загрузка DT_BiomeDefaults с обязательным
// возвратом глобального static в nullptr — потому что остальные тесты
// писались против нулевых дефолтов и падали от неё в зависимости от порядка
// выполнения. 2026-09-07 причина устранена в корне: FBiomeDefaults грузит
// таблицу лениво сам (Core/Types/BiomeTypes.cpp), настоящие дефолты видит
// ВЕСЬ прогон, и никакой глобальной возни в тесте не нужно. Явная проверка
// "таблица действительно в силе" ниже оставлена — она страхует от молчаливого
// возврата к нулевым константам, если ассет когда-нибудь переедет.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_PurityReturnsToItsOwnBiomeDefault,
    "Herbalist.ApplyBiomeInfluences.PurityReturnsToItsOwnBiomeDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_PurityReturnsToItsOwnBiomeDefault::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    const float DefaultPurity = FBiomeDefaults::GetDefaultState(EBiomeType::Taiga).Meta.Purity;
    if (!TestTrue(TEXT("Real biome table is in effect (Taiga Purity is not the zeroed stub)"), DefaultPurity > 0.5f))
    {
        return false;
    }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }

    // Клетку "испортили" по Purity гораздо ниже её природы -- ровно тот
    // случай, который до правки НЕ восстанавливался (уезжал ещё ниже, к
    // 0.5*(1-Distortion)).
    Cell->Biome = EBiomeType::Taiga;
    Cell->Memory.bDegrading = false;
    Cell->TargetState.Meta.Purity = 0.3f;

    // Поле Заряны в покое -- ноль (биом в своей природе, возмущения нет).
    TMap<FName, float> MorokFields;
    TMap<FName, float> ZaryanaFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga), 0.0f } };

    // 300 симулированных секунд шагами графа (0.2с): при декее 0.01/с
    // отклонение должно ужаться примерно в e^-3 ≈ 20 раз.
    for (int32 Step = 0; Step < 1500; ++Step)
    {
        Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 0.2f);
    }

    // Мёртвая зона сторожа разреженности (замерена этим же тестом,
    // MATH_REFERENCE.md §6.4): запись в TargetState пропускается, пока шаг
    // меньше KINDA_SMALL_NUMBER, а шаг равен |отклонение|·Decay·dt. Значит
    // восстановление останавливается на |отклонение| ≈ 1e-4/(0.01·0.2) =
    // 0.05 -- ровно это и наблюдается (0.3 -> 0.65 при дефолте 0.70).
    // Это свойство защиты §7.1, общее для обеих веток, а не изъян правки.
    const float DeadZone = 0.06f;   // 0.05 замеренных + запас на float
    const float Recovered = Cell->TargetState.Meta.Purity;
    TestTrue(FString::Printf(TEXT("Purity climbed back toward its OWN biome default (0.3 -> %.4f, default %.4f) instead of sinking toward f(Distortion)"),
        Recovered, DefaultPurity),
        Recovered > DefaultPurity - DeadZone);
    TestTrue(FString::Printf(TEXT("Purity did not overshoot past the biome default (got %.4f, default %.4f)"),
        Recovered, DefaultPurity),
        Recovered <= DefaultPurity + KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

// Ветка Морока переведена на отклонения вторым заходом (2026-09-07),
// симметрично Заряне. Так же опирается на настоящие дефолты биомов -- см.
// довод у теста PurityReturnsToItsOwnBiomeDefault выше.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_DistortionReturnsToItsOwnBiomeDefault,
    "Herbalist.ApplyBiomeInfluences.DistortionReturnsToItsOwnBiomeDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_DistortionReturnsToItsOwnBiomeDefault::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    const float DefaultDistortion = FBiomeDefaults::GetDefaultState(EBiomeType::Taiga).Meta.Distortion;
    if (!TestTrue(TEXT("Real biome table is in effect (Taiga Distortion is not the zeroed stub)"), DefaultDistortion > 0.1f))
    {
        return false;
    }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }

    // Клетку "испортили" далеко выше её природы -- до правки она бы тут и
    // осталась (поле тянуло к себе, а не к дефолту биома).
    Cell->Biome = EBiomeType::Taiga;
    Cell->Memory.bDegrading = false;
    Cell->TargetState.Meta.Distortion = 0.9f;

    TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga), 0.0f } };
    TMap<FName, float> ZaryanaFields;

    for (int32 Step = 0; Step < 1500; ++Step)   // 300 симулированных секунд
    {
        Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 0.2f);
    }

    // Мёртвая зона сторожа разреженности -- 0.05 (MATH_REFERENCE.md §6.4).
    const float Recovered = Cell->TargetState.Meta.Distortion;
    TestTrue(FString::Printf(TEXT("Distortion fell back toward its OWN biome default (0.9 -> %.4f, default %.4f)"),
        Recovered, DefaultDistortion),
        Recovered < DefaultDistortion + 0.06f);
    TestTrue(FString::Printf(TEXT("Distortion did not undershoot below the biome default (got %.4f, default %.4f)"),
        Recovered, DefaultDistortion),
        Recovered >= DefaultDistortion - KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

// Разреженность сохранения при НАСТОЯЩИХ дефолтах биомов. Именно этот
// сценарий ронял Herbalist.Save.BiomeInfluencesWithZeroFieldsStaySparse при
// пробном включении реальной таблицы (2026-09-07): абсолютное "ведро" всегда
// тянуло Distortion от дефолта биома к нулю, то есть КАЖДЫЙ шаг менял все
// 400 клеток и обесценивал липкий DirtyCellIndices, вокруг которого
// построена вся система сохранений (AUDIT_AND_REFACTORING_PLAN.md §7.1).
// На отклонениях покой даёт ровно нулевой шаг -- записи нет.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_RestingWorldStaysSparseWithRealBiomeDefaults,
    "Herbalist.ApplyBiomeInfluences.RestingWorldStaysSparseWithRealBiomeDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_RestingWorldStaysSparseWithRealBiomeDefaults::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        return false;
    }

    // Мир только что создан: каждая клетка стоит ровно на дефолте своего
    // биома (InitializeCells), поля графа нулевые -- полный покой.
    TestEqual(TEXT("Freshly initialized world starts with zero dirty cells"), Manager->CaptureSaveCells().Num(), 0);

    TMap<FName, float> MorokFields, ZaryanaFields;
    for (EBiomeType Biome : FBiomeDefaults::GetAllBiomeTypes())
    {
        const FName BiomeID = FBiomeDefaults::BiomeTypeToName(Biome);
        MorokFields.Add(BiomeID, 0.0f);
        ZaryanaFields.Add(BiomeID, 0.0f);
    }

    for (int32 Step = 0; Step < 50; ++Step)
    {
        Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 0.2f);
    }

    TestEqual(TEXT("A world resting at its biome defaults stays sparse -- ambient branch writes nothing"),
        Manager->CaptureSaveCells().Num(), 0);

    Manager->Destroy();
    return true;
}

// Совместимость сохранений при смене СЕМАНТИКИ поля (2026-09-07). Поля
// биом-графа сериализуются (`Save->BiomeGraphNodes = Graph->GetNodes()`), а
// смысл MorokField в этот день сменился с абсолютного уровня на знаковое
// отклонение. Без миграции старый сейв Болота (0.70 абсолютных) прочитался
// бы как "+0.70 сверх природных 0.70" -- мир загрузился бы максимально
// испорченным МОЛЧА, без единой ошибки в логе. Найдено финальной проверкой
// математики, а не тестами: ни один тест не покрывал смену семантики
// сериализуемого поля.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSave_BiomeGraphV1NodesMigrateToDeviations,
    "Herbalist.Save.BiomeGraphV1NodesMigrateToDeviations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSave_BiomeGraphV1NodesMigrateToDeviations::RunTest(const FString& Parameters)
{
    const FName BogID = FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog);
    const float BogDefault = FBiomeDefaults::GetDefaultState(EBiomeType::Bog).Meta.Distortion;
    if (!TestTrue(TEXT("Real biome table is in effect"), BogDefault > 0.1f))
    {
        return false;
    }

    // Узел ровно в том виде, в каком его записал бы сейв v1: MorokField --
    // АБСОЛЮТНЫЙ уровень, равный природе биома (мир в покое), ZaryanaField --
    // старое зеркало Морока (1 - Distortion).
    TMap<FName, FBiomeGraphNode> LegacyNodes;
    FBiomeGraphNode LegacyBog;
    LegacyBog.MorokField = BogDefault;
    LegacyBog.ZaryanaField = 1.0f - BogDefault;
    LegacyNodes.Add(BogID, LegacyBog);

    UHerbalistSaveSubsystem::MigrateBiomeGraphNodesV1ToV2(LegacyNodes);

    const FBiomeGraphNode& Migrated = LegacyNodes[BogID];
    TestTrue(FString::Printf(TEXT("Спокойный мир v1 (Морок = природа биома %.3f) переносится в НУЛЕВОЕ отклонение, а не в удвоение (получено %.4f)"),
        BogDefault, Migrated.MorokField),
        FMath::IsNearlyEqual(Migrated.MorokField, 0.0f, 0.001f));
    TestTrue(FString::Printf(TEXT("Старое зеркало Морока в ZaryanaField обнулено, а не истолковано как отклонение Stability (получено %.4f)"),
        Migrated.ZaryanaField),
        FMath::IsNearlyEqual(Migrated.ZaryanaField, 0.0f, 0.001f));

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
