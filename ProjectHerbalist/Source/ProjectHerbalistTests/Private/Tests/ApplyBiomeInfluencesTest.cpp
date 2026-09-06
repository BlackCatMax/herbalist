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

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
