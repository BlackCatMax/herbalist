// Source/ProjectHerbalistTests/Private/Tests/SystemInteractionTest.cpp
//
// "Смотрим, насколько реально мешают столкновения систем на практике"
// (обсуждение 2026-08-30, вопрос про глобального директора). Не юнит-тест
// одной системы -- намеренно СТАЛКИВАЕТ несколько независимых писателей
// TargetState на одной и соседних клетках одновременно (бистабильность +
// Моховые духи + заражение соседей + влияние биом-графа) и гоняет много
// реальных тиков (RegenerateCellParameters + UpdateEntityManifestations
// вместе, как в Tick()), чтобы увидеть эмпирически, а не по чтению кода:
// сходится ли итог к чему-то осмысленному, не улетает ли в NaN/за пределы
// [0,1], не мигает ли ManifestedEntityID каждый тик.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    UBiomeGraphSubsystem* InitGraphForInteractionTest(UWorld* World)
    {
        UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
        if (!Graph) return nullptr;
        UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
        if (!Asset) return nullptr;
        Graph->InitializeFromAsset(Asset);
        return Graph;
    }

    bool InRange01(float v)
    {
        return FMath::IsFinite(v) && v >= -0.0001f && v <= 1.0001f;
    }

    bool CellAxesInRange(const FGridCell& Cell)
    {
        const FRealState& S = Cell.State;
        const FRealState& T = Cell.TargetState;
        return InRange01(S.Meta.Corruption) && InRange01(S.Meta.Purity) && InRange01(S.Meta.Distortion) && InRange01(S.Meta.Stability)
            && InRange01(S.Meta.Potency) && InRange01(S.Meta.Resonance) && InRange01(S.Magnitude)
            && InRange01(T.Meta.Corruption) && InRange01(T.Meta.Purity) && InRange01(T.Meta.Distortion) && InRange01(T.Meta.Stability)
            && InRange01(T.Meta.Potency) && InRange01(T.Meta.Resonance) && InRange01(T.Magnitude);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_StackedWritersConvergeWithoutChaos,
    "Herbalist.SystemInteraction.StackedWritersConvergeWithoutChaos",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_StackedWritersConvergeWithoutChaos::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День -- не хотим ночного/рассветного нуджа поверх

    // Клетка-мишень: Тайга, ОДНОВРЕМЕННО за порогом входа бистабильности
    // (Corruption > 0.85) и за порогом Моховых духов (Purity > 0.80) --
    // Purity/Corruption независимые оси по дизайну (см. HerbalistCoreMath.h),
    // обе высокие сразу -- не ошибка ввода, а намеренно редкий, но легальный
    // случай, который стоит проверить.
    FGridCell* Target = Manager->GetCell(10, 10);
    // Соседняя заражающая клетка -- уже на полюсе, будет весь прогон толкать Target.
    FGridCell* Contagious = Manager->GetCell(10, 9);
    if (!TestNotNull(TEXT("Target cell exists"), Target) || !TestNotNull(TEXT("Contagious neighbor exists"), Contagious))
    {
        Manager->Destroy();
        return false;
    }

    Target->Biome = EBiomeType::Taiga;
    Target->bIsWater = false;
    Target->State.Meta.Corruption = 0.95f;
    Target->State.Meta.Purity = 0.85f;
    Target->State.Meta.Stability = 0.5f;
    Target->State.Meta.Distortion = 0.2f;
    Target->TargetState = Target->State;
    Target->Memory.bDegrading = false;   // ещё не зафиксирована -- зафиксируется первым же тиком

    Contagious->Biome = EBiomeType::Bog;
    Contagious->bIsWater = false;
    Contagious->Memory.bDegrading = true;
    Contagious->State.Meta.Corruption = 1.0f;
    Contagious->TargetState.Meta.Corruption = 1.0f;
    Contagious->TargetState.Meta.Purity = 0.0f;
    Contagious->TargetState.Meta.Distortion = 1.0f;
    Contagious->TargetState.Meta.Stability = 0.0f;

    // 200 тиков по 1 секунде -- достаточно, чтобы релаксация (StressRecoveryGameDays-
    // масштаба медленная) и гистерезис успели сказать своё слово, не только
    // мгновенный первый кадр.
    bool bSawNaNOrOutOfRange = false;
    int32 ManifestFlips = 0;
    FName PrevManifest = Target->ManifestedEntityID;

    for (int32 i = 0; i < 200; ++i)
    {
        Manager->RegenerateCellParameters(1.0f);
        Manager->UpdateEntityManifestations(1.0f);

        const FRealState& S = Target->State;
        const FRealState& T = Target->TargetState;
        auto InRange = [](float v) { return FMath::IsFinite(v) && v >= -0.0001f && v <= 1.0001f; };
        if (!InRange(S.Meta.Corruption) || !InRange(S.Meta.Purity) || !InRange(S.Meta.Distortion) || !InRange(S.Meta.Stability)
            || !InRange(T.Meta.Corruption) || !InRange(T.Meta.Purity) || !InRange(T.Meta.Distortion) || !InRange(T.Meta.Stability))
        {
            bSawNaNOrOutOfRange = true;
        }

        if (Target->ManifestedEntityID != PrevManifest)
        {
            ++ManifestFlips;
            PrevManifest = Target->ManifestedEntityID;
        }
    }

    TestFalse(TEXT("State/TargetState never leaves [0,1] or produces NaN under stacked writers"), bSawNaNOrOutOfRange);

    // Не "никогда не переключается" (гистерезис допускает единичный переход,
    // когда бистабильность один раз побеждает), а "не дребезжит каждый тик" --
    // если что-то мигает по нескольку раз за 200 тиков, это и есть тот самый
    // практический вред от столкновения систем, который стоило бы разобрать
    // отдельно перед тем, как решать про директора.
    TestTrue(FString::Printf(TEXT("ManifestedEntityID does not flicker uncontrollably (%d flips over 200 ticks)"), ManifestFlips),
        ManifestFlips <= 2);

    // Смысловая проверка исхода: заражение (Corruption+) толкает клетку в ту
    // же сторону, что и её собственная бистабильность -- при таком заведомо
    // перекошенном старте (Corruption 0.95 против Purity 0.85, плюс
    // непрерывный внешний толчок) естественно ожидать, что клетка в итоге
    // зафиксируется на испорченном полюсе, а не останется хозяйством Моховых
    // духов -- сама бистабильность работает по Corruption, заражение усиливает
    // именно эту сторону, ничто не толкает Corruption обратно вниз.
    TestTrue(TEXT("Cell ends up locked onto the corrupt pole (contagion + self bistability both push that way)"),
        Target->Memory.bDegrading);

    // Фикс 2026-08-30 (по итогам обхода всего бестиария, см. AmbientSweep/
    // LandmarkSweep/LegendarySweep ниже): Моховые духи (TriggerAxis=Purity,
    // ортогонален Corruption) больше не должны манифестировать на клетке,
    // которую бистабильность зафиксировала испорченной -- раньше именно это
    // и происходило (см. CHANGELOG.md, находка сессии).
    TestTrue(TEXT("Benign ambient entity does not remain manifested on a bDegrading cell"),
        Target->ManifestedEntityID.IsNone() || Target->ManifestedEntityID != FName(TEXT("Моховые духи")));

    AddInfo(FString::Printf(TEXT("Final state: ManifestedEntityID=%s bDegrading=%d State.Purity=%.3f State.Corruption=%.3f TargetState.Purity=%.3f TargetState.Corruption=%.3f"),
        *Target->ManifestedEntityID.ToString(), Target->Memory.bDegrading,
        Target->State.Meta.Purity, Target->State.Meta.Corruption,
        Target->TargetState.Meta.Purity, Target->TargetState.Meta.Corruption));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_LegendaryBenignVsBistability,
    "Herbalist.SystemInteraction.LegendaryBenignVsBistability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_LegendaryBenignVsBistability::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    UBiomeGraphSubsystem* Graph = InitGraphForInteractionTest(World);
    if (!TestNotNull(TEXT("Graph initialized from DA_BiomeGraph"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    // Индрик-зверь (Тайга, Благой) -- тот же архитектурный класс, что уже
    // дал находку с Моховыми духами (независимый триггер "хороший", тут по
    // MorokField графа, а не по Purity клетки), но другой канал: якорная
    // клетка Легендарного, не любая подходящая по биому.
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Индрик-зверь")));
    if (!TestNotNull(TEXT("Индрик-зверь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    const FName TaigaID = FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga);
    FBiomeGraphNode* Node = Graph->GetMutableNode(TaigaID);
    if (!TestNotNull(TEXT("Taiga node exists in the graph"), Node))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    Node->MorokField = 0.05f;   // ниже MorokThreshold=0.2 Индрик-зверя -- Благой путь через низкий Морок

    FGridCell* Anchored = Manager->GetCell(Anchor->X, Anchor->Y);
    if (!TestNotNull(TEXT("Anchor cell exists on the grid"), Anchored))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    // Та же клетка-якорь одновременно за порогом входа бистабильности --
    // Легендарный триггер (MorokField графа) и бистабильность (Corruption
    // клетки) читают полностью разные величины, ничто не мешает обеим
    // сработать на одной и той же клетке разом.
    Anchored->State.Meta.Corruption = 0.95f;
    Anchored->TargetState = Anchored->State;
    Anchored->Memory.bDegrading = false;

    bool bSawNaNOrOutOfRange = false;
    int32 ManifestFlips = 0;
    FName PrevManifest = Anchored->ManifestedEntityID;

    for (int32 i = 0; i < 200; ++i)
    {
        Manager->RegenerateCellParameters(1.0f);
        Manager->UpdateEntityManifestations(1.0f);

        if (!CellAxesInRange(*Anchored)) bSawNaNOrOutOfRange = true;
        if (Anchored->ManifestedEntityID != PrevManifest)
        {
            ++ManifestFlips;
            PrevManifest = Anchored->ManifestedEntityID;
        }
    }

    TestFalse(TEXT("State/TargetState never leaves [0,1] or produces NaN"), bSawNaNOrOutOfRange);
    TestTrue(FString::Printf(TEXT("ManifestedEntityID does not flicker uncontrollably (%d flips over 200 ticks)"), ManifestFlips),
        ManifestFlips <= 2);

    // Фикс 2026-08-30: Благой полюс (MorokField графа, ортогонален Corruption
    // клетки) больше не манифестирует на bDegrading-клетке.
    TestTrue(TEXT("Benign Legendary does not remain manifested on a bDegrading cell"),
        Anchored->ManifestedEntityID.IsNone() || Anchored->ManifestedEntityID != FName(TEXT("Индрик-зверь")));

    AddInfo(FString::Printf(TEXT("Final state: ManifestedEntityID=%s bDegrading=%d State.Corruption=%.3f State.Potency=%.3f TargetState.Corruption=%.3f TargetState.Potency=%.3f"),
        *Anchored->ManifestedEntityID.ToString(), Anchored->Memory.bDegrading,
        Anchored->State.Meta.Corruption, Anchored->State.Meta.Potency,
        Anchored->TargetState.Meta.Corruption, Anchored->TargetState.Meta.Potency));

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_LandmarkBlessVsContagion,
    "Herbalist.SystemInteraction.LandmarkBlessVsContagion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_LandmarkBlessVsContagion::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);

    // Полевик, давно и щедро одаренный (Respect не меняется от заражения --
    // подношение единственный канал, см. GridWorldManagerTick.cpp), на
    // клетке, которую при этом непрерывно толкает соседнее заражение.
    FGridCell* Landmark = Manager->GetCell(3, 3);
    FGridCell* Contagious = Manager->GetCell(3, 2);
    if (!TestNotNull(TEXT("Landmark cell exists"), Landmark) || !TestNotNull(TEXT("Contagious neighbor exists"), Contagious))
    {
        Manager->Destroy();
        return false;
    }

    Landmark->Biome = EBiomeType::ForestSteppe;
    Landmark->bIsWater = false;
    Landmark->TargetState.Meta.Potency = 0.3f;
    Landmark->TargetState.Meta.Purity = 0.3f;
    Landmark->TargetState.Meta.Stability = 0.5f;

    FEntityLandmark PolevikLandmark;
    PolevikLandmark.EntityID = FName(TEXT("Полевик"));
    PolevikLandmark.Cell = FIntPoint(3, 3);
    PolevikLandmark.Respect = 0.9f;   // далеко за порогом благословения (0.5)
    Manager->SetEntityLandmarks({ PolevikLandmark });

    Contagious->Biome = EBiomeType::Bog;
    Contagious->bIsWater = false;
    Contagious->Memory.bDegrading = true;
    Contagious->State.Meta.Corruption = 1.0f;
    Contagious->TargetState.Meta.Corruption = 1.0f;
    Contagious->TargetState.Meta.Purity = 0.0f;
    Contagious->TargetState.Meta.Distortion = 1.0f;
    Contagious->TargetState.Meta.Stability = 0.0f;

    bool bSawNaNOrOutOfRange = false;
    int32 ManifestFlips = 0;
    FName PrevManifest = Landmark->ManifestedEntityID;

    for (int32 i = 0; i < 200; ++i)
    {
        Manager->RegenerateCellParameters(1.0f);
        Manager->UpdateEntityManifestations(1.0f);

        if (!CellAxesInRange(*Landmark)) bSawNaNOrOutOfRange = true;
        if (Landmark->ManifestedEntityID != PrevManifest)
        {
            ++ManifestFlips;
            PrevManifest = Landmark->ManifestedEntityID;
        }
    }

    TestFalse(TEXT("State/TargetState never leaves [0,1] or produces NaN"), bSawNaNOrOutOfRange);
    TestTrue(FString::Printf(TEXT("ManifestedEntityID does not flicker uncontrollably (%d flips over 200 ticks)"), ManifestFlips),
        ManifestFlips <= 2);

    const FEntityLandmark* FoundLandmark = Manager->FindLandmarkAt(FIntPoint(3, 3));
    TestTrue(TEXT("Landmark.Respect is untouched by contagion -- offering is the only channel"),
        FoundLandmark && FMath::IsNearlyEqual(FoundLandmark->Respect, 0.9f));

    AddInfo(FString::Printf(TEXT("Final state: ManifestedEntityID=%s bDegrading=%d State.Corruption=%.3f State.Potency=%.3f State.Stability=%.3f"),
        *Landmark->ManifestedEntityID.ToString(), Landmark->Memory.bDegrading,
        Landmark->State.Meta.Corruption, Landmark->State.Meta.Potency, Landmark->State.Meta.Stability));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_MaximumStackDoesNotBreak,
    "Herbalist.SystemInteraction.MaximumStackDoesNotBreak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_MaximumStackDoesNotBreak::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Зима -- добавляет разлитый по сетке WinterPurityRate нудж поверх всего
    // остального (§15.4), тот же прокси, что уже использует SeasonTest.cpp.
    const float DayLength = 32.0f * 60.0f;
    const float YearLength = 117.0f * 3.0f * DayLength;
    const float WinterStart = YearLength * 2.0f / 3.0f;
    Manager->SetGameClockSeconds(WinterStart + 10.0f * 60.0f);
    if (!TestEqual(TEXT("Sanity: we are actually in Winter"), Manager->GetSeason(), ESeason::Winter))
    {
        Manager->Destroy();
        return false;
    }

    UBiomeGraphSubsystem* Graph = InitGraphForInteractionTest(World);
    if (!TestNotNull(TEXT("Graph initialized from DA_BiomeGraph"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    // Цель -- та же перекошенная клетка Тайги (бистабильность и Моховые
    // духи легально разом), теперь ещё и под диффузией Морока/Заряны по
    // биом-графу (ApplyBiomeInfluences читается напрямую, не через полный
    // StepSimulation/PropagateWaves -- тот же приём, что уже применяет
    // LegendaryEntityTest.cpp через GetMutableNode, только влияние сразу
    // на клетки, а не только на узел).
    FGridCell* Target = Manager->GetCell(10, 10);
    FGridCell* ContagiousA = Manager->GetCell(10, 9);
    FGridCell* ContagiousB = Manager->GetCell(9, 10);
    if (!TestNotNull(TEXT("Target cell exists"), Target)
        || !TestNotNull(TEXT("Contagious neighbor A exists"), ContagiousA)
        || !TestNotNull(TEXT("Contagious neighbor B exists"), ContagiousB))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    Target->Biome = EBiomeType::Taiga;
    Target->bIsWater = false;
    Target->State.Meta.Corruption = 0.95f;
    Target->State.Meta.Purity = 0.85f;
    Target->State.Meta.Stability = 0.5f;
    Target->State.Meta.Distortion = 0.2f;
    Target->TargetState = Target->State;
    Target->Memory.bDegrading = false;

    for (FGridCell* C : { ContagiousA, ContagiousB })
    {
        C->Biome = EBiomeType::Bog;
        C->bIsWater = false;
        C->Memory.bDegrading = true;
        C->State.Meta.Corruption = 1.0f;
        C->TargetState.Meta.Corruption = 1.0f;
        C->TargetState.Meta.Purity = 0.0f;
        C->TargetState.Meta.Distortion = 1.0f;
        C->TargetState.Meta.Stability = 0.0f;
    }

    const TMap<FName, float> MorokFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga), 0.8f } };
    const TMap<FName, float> ZaryanaFields = { { FBiomeDefaults::BiomeTypeToName(EBiomeType::Taiga), 0.8f } };

    bool bSawNaNOrOutOfRange = false;
    int32 ManifestFlips = 0;
    FName PrevManifest = Target->ManifestedEntityID;

    for (int32 i = 0; i < 300; ++i)
    {
        // Диффузия биом-графа -- та же клетка Тайги, каждый тик, как и
        // остальные три писателя ниже. GlobalScale=1.0, тот же дефолт, что
        // использует сама подсистема без модификаторов Заряны игрока.
        Manager->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f);
        Manager->RegenerateCellParameters(1.0f);
        Manager->UpdateEntityManifestations(1.0f);

        if (!CellAxesInRange(*Target)) bSawNaNOrOutOfRange = true;
        if (Target->ManifestedEntityID != PrevManifest)
        {
            ++ManifestFlips;
            PrevManifest = Target->ManifestedEntityID;
        }
    }

    TestFalse(TEXT("State/TargetState never leaves [0,1] or produces NaN under maximum stacking"), bSawNaNOrOutOfRange);
    TestTrue(FString::Printf(TEXT("ManifestedEntityID does not flicker uncontrollably (%d flips over 300 ticks)"), ManifestFlips),
        ManifestFlips <= 4);

    // Фикс 2026-08-30, тот же гейт, что и в StackedWritersConvergeWithoutChaos
    // -- под максимальной нагрузкой (зима + диффузия биом-графа сверху)
    // держится так же, не только в изолированном случае.
    TestTrue(TEXT("Benign ambient entity does not remain manifested on a bDegrading cell under max stacking"),
        Target->ManifestedEntityID.IsNone() || Target->ManifestedEntityID != FName(TEXT("Моховые духи")));

    AddInfo(FString::Printf(TEXT("Final state: ManifestedEntityID=%s bDegrading=%d State=(Corruption=%.3f Purity=%.3f Distortion=%.3f Stability=%.3f) Target=(Corruption=%.3f Purity=%.3f Distortion=%.3f Stability=%.3f)"),
        *Target->ManifestedEntityID.ToString(), Target->Memory.bDegrading,
        Target->State.Meta.Corruption, Target->State.Meta.Purity, Target->State.Meta.Distortion, Target->State.Meta.Stability,
        Target->TargetState.Meta.Corruption, Target->TargetState.Meta.Purity, Target->TargetState.Meta.Distortion, Target->TargetState.Meta.Stability));

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_LandmarkBlessVsSelfBistability,
    "Herbalist.SystemInteraction.LandmarkBlessVsSelfBistability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_LandmarkBlessVsSelfBistability::RunTest(const FString& Parameters)
{
    // "Хозяева" (Основной ранг), прямой самоконфликт -- не через заражение
    // соседа (уже проверено, чисто), а через то, что Respect (подношение)
    // и Corruption (бистабильность клетки) две полностью независимые
    // величины: ничто не мешает клетке-обиталищу одновременно быть щедро
    // одаренной И собственной Corruption пересечь порог входа. Три хозяина
    // на трёх разных биомах -- механизм у всех 14 идентичен (один и тот же
    // Respect-канал), биом сам по себе логику не меняет, три образца
    // достаточно, чтобы не гонять оставшиеся 11 ради того же самого вывода.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    struct FCase { FName EntityID; EBiomeType Biome; FIntPoint Cell; };
    const FCase Cases[] = {
        { FName(TEXT("Полевик")),          EBiomeType::ForestSteppe, FIntPoint(3, 3) },
        { FName(TEXT("Кикимора болотная")), EBiomeType::Bog,          FIntPoint(4, 4) },
        { FName(TEXT("Бродницы")),          EBiomeType::Floodplain,  FIntPoint(5, 5) },
    };

    for (const FCase& C : Cases)
    {
        AGridWorldManager* Manager = SpawnAndBeginPlay(World);
        if (!TestNotNull(TEXT("Manager spawned"), Manager)) continue;
        Manager->SetGameClockSeconds(10.0f * 60.0f);

        FGridCell* Cell = Manager->GetCell(C.Cell.X, C.Cell.Y);
        if (!TestNotNull(TEXT("Landmark cell exists"), Cell)) { Manager->Destroy(); continue; }

        Cell->Biome = C.Biome;
        Cell->bIsWater = false;
        Cell->State.Meta.Corruption = 0.95f;   // за порогом входа бистабильности САМОЙ клетки
        Cell->TargetState = Cell->State;
        Cell->Memory.bDegrading = false;

        FEntityLandmark Landmark;
        Landmark.EntityID = C.EntityID;
        Landmark.Cell = C.Cell;
        Landmark.Respect = 0.9f;   // щедро одаренный, далеко за порогом благословения (0.5)
        Manager->SetEntityLandmarks({ Landmark });

        bool bSawNaNOrOutOfRange = false;
        for (int32 i = 0; i < 100; ++i)
        {
            Manager->RegenerateCellParameters(1.0f);
            Manager->UpdateEntityManifestations(1.0f);
            if (!CellAxesInRange(*Cell)) bSawNaNOrOutOfRange = true;
        }

        TestFalse(FString::Printf(TEXT("[%s] State/TargetState stays in range"), *C.EntityID.ToString()), bSawNaNOrOutOfRange);

        // Фикс 2026-08-30: благословенный Хозяин (Respect, ортогонален
        // Corruption клетки) больше не остаётся манифестирован на клетке,
        // которую бистабильность зафиксировала испорченной.
        const bool bIncoherent = Cell->Memory.bDegrading && Cell->ManifestedEntityID == C.EntityID;
        TestFalse(FString::Printf(TEXT("[%s/%s] blessed host does not remain manifested on a bDegrading cell"),
            *C.EntityID.ToString(), *UEnum::GetValueAsString(C.Biome)), bIncoherent);
        AddInfo(FString::Printf(TEXT("[%s/%s] ManifestedEntityID=%s bDegrading=%d"),
            *C.EntityID.ToString(), *UEnum::GetValueAsString(C.Biome),
            *Cell->ManifestedEntityID.ToString(), Cell->Memory.bDegrading));

        Manager->Destroy();
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_AmbientSweepAcrossAllBiomes,
    "Herbalist.SystemInteraction.AmbientSweepAcrossAllBiomes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_AmbientSweepAcrossAllBiomes::RunTest(const FString& Parameters)
{
    // Программный обход ВСЕГО реестра Низшего ранга (2026-08-30, "проверим
    // сочетания всех биомов"), не горстка образцов вручную. Каждое
    // определение с осевым триггером (TriggerAxis != None), кроме самой
    // Corruption (та согласована с бистабильностью по построению, не
    // противоречит) и кроме требующих время/сезон/луну/погоду/границу биома
    // (эти пропускаются с явной пометкой в отчёте, не тихо) -- заводится в
    // клетку СВОЕГО биома, ОДНОВременно с Corruption за порогом
    // бистабильности, и проверяется через реальные тики.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    int32 Tested = 0, Skipped = 0, Incoherent = 0;
    for (const FAmbientEntityDefinition& Def : GetAmbientEntityDefinitions())
    {
        if (Def.TriggerAxis == EAmbientTriggerAxis::None || Def.TriggerAxis == EAmbientTriggerAxis::Corruption)
        {
            ++Skipped;
            continue;
        }
        if (Def.bRequiresNight || Def.bRequiresDusk || Def.bRequiresSeason || Def.bRequiresMoonPhase
            || Def.bRequiresWeather || Def.bRequiresLateSummer || Def.bRequiresKupalaNight || Def.bRequiresBiomeBorder)
        {
            AddInfo(FString::Printf(TEXT("[%s] SKIPPED -- needs a time/season/weather/border gate not covered by this sweep"), *Def.EntityID.ToString()));
            ++Skipped;
            continue;
        }

        AGridWorldManager* Manager = SpawnAndBeginPlay(World);
        if (!TestNotNull(TEXT("Manager spawned"), Manager)) continue;
        Manager->SetGameClockSeconds(10.0f * 60.0f);

        FGridCell* Cell = Manager->GetCell(6, 6);
        if (!TestNotNull(TEXT("Sweep cell exists"), Cell)) { Manager->Destroy(); continue; }

        Cell->Biome = Def.Biome;
        Cell->bIsWater = Def.bWaterOnly;
        Cell->State.Meta.Corruption = 0.95f;   // бистабильность клетки, независимо от биома/оси
        Cell->TargetState = Cell->State;
        Cell->Memory.bDegrading = false;

        // Заводим ось триггера с явным запасом за порог гистерезиса, в
        // нужную сторону (bTriggerAbove).
        const float Margin = Def.HysteresisMargin + 0.05f;
        const float AxisValue = FMath::Clamp(Def.bTriggerAbove ? Def.TriggerThreshold + Margin : Def.TriggerThreshold - Margin, 0.0f, 1.0f);
        SetAmbientTriggerAxisValue(*Cell, Def.TriggerAxis, AxisValue);

        bool bSawNaNOrOutOfRange = false;
        for (int32 i = 0; i < 50; ++i)
        {
            Manager->RegenerateCellParameters(1.0f);
            Manager->UpdateEntityManifestations(1.0f);
            if (!CellAxesInRange(*Cell)) bSawNaNOrOutOfRange = true;
        }

        TestFalse(FString::Printf(TEXT("[%s] State/TargetState stays in range"), *Def.EntityID.ToString()), bSawNaNOrOutOfRange);
        ++Tested;

        // Фикс 2026-08-30: любое существо с триггер-осью, ортогональной
        // Corruption, больше не остаётся манифестировано на bDegrading-клетке
        // (Гнильники исключены из этого правила самим гейтом -- их ось и есть
        // Corruption -- поэтому им сюда попасть и не полагалось).
        const bool bIncoherent = Cell->Memory.bDegrading && Cell->ManifestedEntityID == Def.EntityID;
        if (bIncoherent) ++Incoherent;
        TestFalse(FString::Printf(TEXT("[%s/%s] does not remain manifested on a bDegrading cell"),
            *Def.EntityID.ToString(), *UEnum::GetValueAsString(Def.Biome)), bIncoherent);
        AddInfo(FString::Printf(TEXT("[%s/%s] ManifestedEntityID=%s bDegrading=%d"),
            *Def.EntityID.ToString(), *UEnum::GetValueAsString(Def.Biome),
            *Cell->ManifestedEntityID.ToString(), Cell->Memory.bDegrading));

        Manager->Destroy();
    }

    AddInfo(FString::Printf(TEXT("Ambient sweep summary: %d tested, %d skipped (time/season/weather/border gates), %d incoherent"),
        Tested, Skipped, Incoherent));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSystemInteraction_LegendarySweepAcrossAllBiomes,
    "Herbalist.SystemInteraction.LegendarySweepAcrossAllBiomes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSystemInteraction_LegendarySweepAcrossAllBiomes::RunTest(const FString& Parameters)
{
    // Программный обход ВСЕГО реестра Легендарного ранга, тем же принципом,
    // что и Ambient-развёртка выше -- каждое существо на своей якорной
    // клетке, якорь дополнительно продавлен за порог бистабильности,
    // MorokField узла настроен под полюс существа (низкий для Благого,
    // высокий для Злого -- bHasShrinePath не мешает: это OR-условие,
    // одного MorokField достаточно независимо от него).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    int32 Tested = 0, Skipped = 0, Incoherent = 0;
    for (const FLegendaryEntityDefinition& Def : GetLegendaryEntityDefinitions())
    {
        AGridWorldManager* Manager = SpawnAndBeginPlay(World);
        if (!TestNotNull(TEXT("Manager spawned"), Manager)) continue;
        Manager->SetGameClockSeconds(10.0f * 60.0f);

        UBiomeGraphSubsystem* Graph = InitGraphForInteractionTest(World);
        if (!TestNotNull(TEXT("Graph initialized"), Graph)) { Manager->Destroy(); continue; }

        const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(Def.EntityID);
        if (!Anchor)
        {
            // Не у каждого биома в тестовой 20x20 сетке найдётся подходящая
            // клетка для КАЖДОГО существа сразу -- молчаливый no-op в самом
            // коде (см. SeedLegendaryAnchors), здесь просто помечаем пропуск.
            AddInfo(FString::Printf(TEXT("[%s] SKIPPED -- no seeded anchor cell in this grid"), *Def.EntityID.ToString()));
            ++Skipped;
            Graph->Deinitialize();
            Manager->Destroy();
            continue;
        }

        const FName BiomeID = FBiomeDefaults::BiomeTypeToName(Def.Biome);
        FBiomeGraphNode* Node = Graph->GetMutableNode(BiomeID);
        if (!Node)
        {
            AddInfo(FString::Printf(TEXT("[%s] SKIPPED -- biome node missing from DA_BiomeGraph"), *Def.EntityID.ToString()));
            ++Skipped;
            Graph->Deinitialize();
            Manager->Destroy();
            continue;
        }
        Node->MorokField = Def.Pole == ELegendaryPole::Malign ? 0.9f : 0.05f;

        FGridCell* Anchored = Manager->GetCell(Anchor->X, Anchor->Y);
        if (!TestNotNull(TEXT("Anchor cell exists"), Anchored)) { Graph->Deinitialize(); Manager->Destroy(); continue; }

        Anchored->State.Meta.Corruption = 0.95f;
        Anchored->TargetState = Anchored->State;
        Anchored->Memory.bDegrading = false;

        bool bSawNaNOrOutOfRange = false;
        for (int32 i = 0; i < 50; ++i)
        {
            Manager->RegenerateCellParameters(1.0f);
            Manager->UpdateEntityManifestations(1.0f);
            if (!CellAxesInRange(*Anchored)) bSawNaNOrOutOfRange = true;
        }

        TestFalse(FString::Printf(TEXT("[%s] State/TargetState stays in range"), *Def.EntityID.ToString()), bSawNaNOrOutOfRange);
        ++Tested;

        // Фикс 2026-08-30: Благой полюс больше не остаётся манифестирован на
        // bDegrading-клетке; Злой исключён из правила намеренно (высокий
        // MorokField уже согласован с испорченным полюсом тематически).
        const bool bIncoherent = Anchored->Memory.bDegrading && Anchored->ManifestedEntityID == Def.EntityID;
        if (bIncoherent) ++Incoherent;
        if (Def.Pole == ELegendaryPole::Benign)
        {
            TestFalse(FString::Printf(TEXT("[%s/Benign] does not remain manifested on a bDegrading cell"), *Def.EntityID.ToString()), bIncoherent);
        }
        AddInfo(FString::Printf(TEXT("[%s/%s/%s] ManifestedEntityID=%s bDegrading=%d"),
            *Def.EntityID.ToString(), *UEnum::GetValueAsString(Def.Biome),
            Def.Pole == ELegendaryPole::Malign ? TEXT("Malign") : TEXT("Benign"),
            *Anchored->ManifestedEntityID.ToString(), Anchored->Memory.bDegrading));

        Graph->Deinitialize();
        Manager->Destroy();
    }

    AddInfo(FString::Printf(TEXT("Legendary sweep summary: %d tested, %d skipped, %d incoherent"), Tested, Skipped, Incoherent));
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
