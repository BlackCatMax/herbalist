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
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

// Равновесие "дырявого ведра" -- одного шага достаточно для точной
// аналитической проверки формулы, не нужно гонять до сходимости: результат
// после ОДНОГО шага полностью детерминирован по Push/Decay из
// HerbalistSettings (дефолты 0.01/0.01, если проект не переопределил их
// в конфиге). Цель — САМ MorokField, не "BiomeDefault + MorokField"
// (первая версия этого теста и самой правки, 2026-09-07, была на битой
// формуле — см. правку в GridWorldManagerCore.cpp и комментарий у
// MorokDistortionPushRate в HerbalistSettings.h: MorokField уже сам
// сходится к среднему Distortion клеток биома, отдельно прикладывать
// дефолт биома здесь означало бы удвоенный счёт одной и той же величины).
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
    Cell->TargetState.Meta.Distortion = 0.0f;

    TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::MixedForest), 0.3f } };
    TMap<FName, float> ZaryanaFields;

    const float DeltaTime = 1.0f;
    Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, DeltaTime);

    // Аналитическое предсказание при дефолтных PushRate=DecayRate=0.01
    // (HerbalistSettings.h): D' = D + (Morok*Push - Decay*D)*dt.
    // При D=0 стартово: D' = Morok*Push*dt = 0.3*0.01*1.0 = 0.003.
    const float ExpectedDistortion = 0.3f * 0.01f * DeltaTime;
    TestTrue(FString::Printf(TEXT("Distortion moved toward equilibrium set by MorokField, not toward 1.0 (got %.5f, expected %.5f)"),
        Cell->TargetState.Meta.Distortion, ExpectedDistortion),
        FMath::IsNearlyEqual(Cell->TargetState.Meta.Distortion, ExpectedDistortion, 0.0001f));
    TestTrue(TEXT("A single step with moderate MorokField does not snap Distortion anywhere near the 1.0 ceiling"),
        Cell->TargetState.Meta.Distortion < 0.5f);

    Manager->Destroy();
    return true;
}

// Ноль в MorokField честно тянет Distortion к нулю (не к дефолту биома) --
// сам по себе ApplyBiomeInfluences больше НЕ отвечает за сохранение
// характера биома, это делает связка с RecalculateFieldsFromGrid на
// уровне всей симуляции (см. новый интеграционный тест ниже в
// BiomeGraphIntegrationTest.cpp -- он проверяет именно то, что не
// проверяет этот юнит-тест: что биом реально держится у своего дефолта
// в живом, многошаговом прогоне).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_ZeroMorokDecaysDistortionTowardZero,
    "Herbalist.ApplyBiomeInfluences.ZeroMorokDecaysDistortionTowardZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_ZeroMorokDecaysDistortionTowardZero::RunTest(const FString& Parameters)
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

    TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog), 0.0f } };
    TMap<FName, float> ZaryanaFields;

    Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 1.0f);

    const float ExpectedDistortion = 0.9f - 0.01f * 0.9f * 1.0f;   // D - Decay*D*dt
    TestTrue(FString::Printf(TEXT("Distortion decayed toward zero as MorokField dictates (got %.4f, expected %.4f)"),
        Cell->TargetState.Meta.Distortion, ExpectedDistortion),
        FMath::IsNearlyEqual(Cell->TargetState.Meta.Distortion, ExpectedDistortion, 0.0001f));

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
// Тест ОБЯЗАН поднять настоящую DT_BiomeDefaults: в обычном тестовом
// окружении её никто не грузит (это делает только GameMode), дефолты биомов
// нулевые, и тогда "отклонение от дефолта" численно неотличимо от
// "абсолютного значения" -- проверять было бы нечего. Таблица ставится в
// глобальный static (FBiomeDefaults::SetBiomeTable), поэтому в конце
// ОБЯЗАТЕЛЬНО возвращается обратно в nullptr: иначе остальные 9 тестов,
// написанные против нулевых дефолтов, начнут падать в зависимости от
// порядка выполнения (проверено -- падают ровно так).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistApplyBiomeInfluences_PurityReturnsToItsOwnBiomeDefault,
    "Herbalist.ApplyBiomeInfluences.PurityReturnsToItsOwnBiomeDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistApplyBiomeInfluences_PurityReturnsToItsOwnBiomeDefault::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    UDataTable* BiomeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_BiomeDefaults"));
    if (!TestNotNull(TEXT("DT_BiomeDefaults loads"), BiomeTable)) return false;
    FBiomeDefaults::SetBiomeTable(BiomeTable);

    const float DefaultPurity = FBiomeDefaults::GetDefaultState(EBiomeType::Taiga).Meta.Purity;
    if (!TestTrue(TEXT("Real biome table is in effect (Taiga Purity is not the zeroed stub)"), DefaultPurity > 0.5f))
    {
        FBiomeDefaults::SetBiomeTable(nullptr);
        return false;
    }

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager))
    {
        FBiomeDefaults::SetBiomeTable(nullptr);
        return false;
    }

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell exists"), Cell))
    {
        Manager->Destroy();
        FBiomeDefaults::SetBiomeTable(nullptr);
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
    // Обязательный возврат глобального состояния -- см. довод у заголовка.
    FBiomeDefaults::SetBiomeTable(nullptr);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
