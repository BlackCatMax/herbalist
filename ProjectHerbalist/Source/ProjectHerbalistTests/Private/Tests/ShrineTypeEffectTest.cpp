// Source/ProjectHerbalistTests/Private/Tests/ShrineTypeEffectTest.cpp
//
// Капища, эффект 3 (02_GDD/15_Cycles_And_Shrines.md §15.5 "Типы капищ"),
// реализовано 2026-08-29 вместе с лорной привязкой к богам. Четыре из пяти
// типоспецифичных бонусов проверяются здесь напрямую (Родовое/Лесное/Водное/
// Каменное) -- тот же DispatchBeginPlay-паттерн, что и ShrineTest.cpp/
// BistabilityTest.cpp. Пограничное (правка PropagateWaves в
// BiomeGraphSubsystem.cpp, самая архитектурно новая часть) отдельным
// автотестом не покрыто -- требует полной инициализации боевого
// DA_BiomeGraph поверх RecalculateFieldsFromGrid, которая сама по себе
// перезаписывает MorokField каждый шаг и заслоняет чистый эффект утечки по
// ребру; проверено вручную построчно, тем же принципом, что уже описан у
// эффектов 1/2 капищ ("Интеграция с реальным пайплайном проверена вручную
// построчно, отдельных автотестов на них нет").

#include "Core/World/GridWorldManager.h"
#include "Core/Shrine/ShrineTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineType_AncestralBoostsOnlyStabilityPull,
    "Herbalist.ShrineType.AncestralBoostsOnlyStabilityPull",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineType_AncestralBoostsOnlyStabilityPull::RunTest(const FString& Parameters)
{
    // Родовое (Дажьбог): ×1.5 к пуллингу ТОЛЬКО Stability, не остальных осей
    // (§15.5 "усиливает пуллинг Stability", не "усиливает пуллинг вообще").
    // Сравниваем не с "без капища вообще" (там эффект 1 уже одинаково даёт
    // Modulation ВСЕМ осям через CellDeltaRegen, включая Purity, и разница в
    // Purity была бы ложным срабатыванием не про эффект 3) -- а с ДРУГИМ
    // типом капища при той же Restoration: эффект 1 у обоих одинаковый
    // (зависит только от Restoration/bApproachingS0, не от Type), значит
    // любая разница в Stability -- чисто эффект 3 Родового.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    auto SetupCell = [](AGridWorldManager* Manager) -> FGridCell*
    {
        FGridCell* Cell = Manager->GetCell(0, 0);
        Cell->Biome = EBiomeType::Taiga;
        Cell->bIsWater = false;
        Cell->Memory.bDegrading = false;
        Cell->State.Meta.Corruption = 0.1f;
        Cell->TargetState.Meta.Corruption = 0.1f;
        Cell->State.Meta.Stability = 0.0f;
        Cell->TargetState.Meta.Stability = 1.0f;
        Cell->State.Meta.Purity = 0.0f;
        Cell->TargetState.Meta.Purity = 1.0f;
        return Cell;
    };

    AGridWorldManager* OtherType = SpawnAndBeginPlay(World);
    AGridWorldManager* Ancestral = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Other-type manager spawned"), OtherType) || !TestNotNull(TEXT("Ancestral manager spawned"), Ancestral))
    {
        if (OtherType) OtherType->Destroy();
        if (Ancestral) Ancestral->Destroy();
        return false;
    }

    FGridCell* OtherCell = SetupCell(OtherType);
    FGridCell* AncestralCell = SetupCell(Ancestral);
    OtherType->RegisterShrine(FIntPoint(0, 0), EShrineType::Stone);   // любой не-Родовое тип
    Ancestral->RegisterShrine(FIntPoint(0, 0), EShrineType::Ancestral);
    FShrine* OtherShrine = OtherType->FindShrineAt(FIntPoint(0, 0));
    FShrine* AncestralShrine = Ancestral->FindShrineAt(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Other shrine registered"), OtherShrine) || !TestNotNull(TEXT("Ancestral shrine registered"), AncestralShrine))
    {
        OtherType->Destroy();
        Ancestral->Destroy();
        return false;
    }
    OtherShrine->Restoration = 0.6f;
    AncestralShrine->Restoration = 0.6f;   // одинаковая Restoration -- эффект 1 идентичен у обоих

    OtherType->RegenerateCellParameters(1.0f);
    Ancestral->RegenerateCellParameters(1.0f);

    TestTrue(TEXT("Ancestral shrine pulls Stability further than a same-Restoration non-Ancestral shrine"),
        AncestralCell->State.Meta.Stability > OtherCell->State.Meta.Stability);

    // Другая ось (Purity) должна получить ровно тот же эффект-1-буст у обоих
    // manager'ов -- Родовое бьёт только по Stability, не по всей релаксации.
    TestEqual(TEXT("Purity pull is identical -- Ancestral does not touch it"),
        AncestralCell->State.Meta.Purity, OtherCell->State.Meta.Purity);

    OtherType->Destroy();
    Ancestral->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineType_ForestSpeedsUpOnlyStressDecay,
    "Herbalist.ShrineType.ForestSpeedsUpOnlyStressDecay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineType_ForestSpeedsUpOnlyStressDecay::RunTest(const FString& Parameters)
{
    // Лесное (Велес): ускоряет спад HarvestStress в радиусе, не трогает
    // Meta-релаксацию (та же CellDeltaRegen у обоих manager'ов).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    auto SetupCell = [](AGridWorldManager* Manager) -> FGridCell*
    {
        FGridCell* Cell = Manager->GetCell(0, 0);
        Cell->Biome = EBiomeType::Taiga;
        Cell->bIsWater = false;
        Cell->Memory.bDegrading = false;
        Cell->HarvestStress = 1.0f;
        return Cell;
    };

    AGridWorldManager* Baseline = SpawnAndBeginPlay(World);
    AGridWorldManager* WithShrine = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Baseline manager spawned"), Baseline) || !TestNotNull(TEXT("Shrine manager spawned"), WithShrine))
    {
        if (Baseline) Baseline->Destroy();
        if (WithShrine) WithShrine->Destroy();
        return false;
    }

    FGridCell* BaselineCell = SetupCell(Baseline);
    FGridCell* ShrineCell = SetupCell(WithShrine);
    WithShrine->RegisterShrine(FIntPoint(0, 0), EShrineType::Forest);
    FShrine* Shrine = WithShrine->FindShrineAt(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Shrine registered"), Shrine))
    {
        Baseline->Destroy();
        WithShrine->Destroy();
        return false;
    }
    Shrine->Restoration = 1.0f;   // полное восстановление -- decay ×(1+0.5) = ×1.5

    const float BigDeltaTime = 3600.0f;   // крупный шаг, разница видна сразу
    Baseline->RegenerateCellParameters(BigDeltaTime);
    WithShrine->RegenerateCellParameters(BigDeltaTime);

    TestTrue(TEXT("Forest shrine at full Restoration decays HarvestStress faster"),
        ShrineCell->HarvestStress < BaselineCell->HarvestStress);
    TestTrue(TEXT("Neither cell fully healed in one step (comparison is meaningful, not clamped)"),
        BaselineCell->HarvestStress > 0.0f);

    Baseline->Destroy();
    WithShrine->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineType_WaterPullsPurityOnlyForWaterCellsInRadius,
    "Herbalist.ShrineType.WaterPullsPurityOnlyForWaterCellsInRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineType_WaterPullsPurityOnlyForWaterCellsInRadius::RunTest(const FString& Parameters)
{
    // Водное (Мокошь): подтягивает TargetState.Meta.Purity воды к 1.0 в
    // радиусе -- локально (в радиусе капища), не через глобальный
    // DefaultWaterState биома, и только для клеток с bIsWater=true.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, -1, 1000.0f);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* WaterCell = Manager->GetCell(0, 0);
    WaterCell->Biome = EBiomeType::Bog;
    WaterCell->bIsWater = true;
    WaterCell->Memory.bDegrading = false;
    WaterCell->TargetState.Meta.Purity = 0.2f;

    FGridCell* LandCell = Manager->GetCell(1, 0);
    LandCell->Biome = EBiomeType::Bog;
    LandCell->bIsWater = false;
    LandCell->Memory.bDegrading = false;
    LandCell->TargetState.Meta.Purity = 0.2f;

    Manager->RegisterShrine(FIntPoint(0, 0), EShrineType::Water);
    FShrine* Shrine = Manager->FindShrineAt(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Shrine registered"), Shrine))
    {
        Manager->Destroy();
        return false;
    }
    Shrine->Restoration = 1.0f;

    Manager->RegenerateCellParameters(10.0f);

    TestTrue(TEXT("Water cell's TargetState.Purity was pulled up toward 1.0"),
        WaterCell->TargetState.Meta.Purity > 0.2f);
    TestEqual(TEXT("Land cell's TargetState.Purity is untouched -- Water shrine only affects water"),
        LandCell->TargetState.Meta.Purity, 0.2f);

    // Насыщение: ещё один большой шаг после Purity=1.0 не должен помечать
    // клетку грязной без реального изменения -- тот же §7.1 паттерн
    // (IsNearlyEqual перед записью), что уже применён у ночного/зимнего нуджа.
    Manager->RegenerateCellParameters(1000.0f);
    TestEqual(TEXT("Purity clamps at 1.0, does not overshoot"), WaterCell->TargetState.Meta.Purity, 1.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineType_WaterPullFrameSizedStepsAccumulate,
    "Herbalist.ShrineType.WaterPullFrameSizedStepsAccumulate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineType_WaterPullFrameSizedStepsAccumulate::RunTest(const FString& Parameters)
{
    // Притяжение Purity кадрами (ревью 2026-09-13): RegenerateCellParameters
    // идёт каждый кадр с реальным DeltaTime, и при Restoration 0.2 толчок за
    // кадр 1/60 с (0.02 × 0.2 / 60 ≈ 7e-5) меньше KINDA_SMALL_NUMBER. Сравнение
    // с эпсилоном перед записью такой толчок пропускало. Тест выше шагает по
    // 10 с и этого не видел.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World, {}, -1, 1000.0f);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* WaterCell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0, 0) exists"), WaterCell))
    {
        Manager->Destroy();
        return false;
    }
    WaterCell->Biome = EBiomeType::Bog;
    WaterCell->bIsWater = true;
    WaterCell->Memory.bDegrading = false;
    WaterCell->TargetState.Meta.Purity = 0.2f;

    Manager->RegisterShrine(FIntPoint(0, 0), EShrineType::Water, 0.2f);
    const FShrine* Shrine = Manager->FindShrineAt(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Shrine registered"), Shrine))
    {
        Manager->Destroy();
        return false;
    }

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const double PullPerSecond = Settings->ShrineWaterPurityPullRate * Shrine->Restoration;
    const float FrameSeconds = 1.0f / 60.0f;
    const int32 Frames = 600;
    const float PurityBefore = WaterCell->TargetState.Meta.Purity;
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Manager->RegenerateCellParameters(FrameSeconds);
    }

    const double Expected = PullPerSecond * Frames * FrameSeconds;
    const double Grown = WaterCell->TargetState.Meta.Purity - PurityBefore;
    AddInfo(FString::Printf(TEXT("Толчок за кадр %.2e, за %d кадров Purity выросла на %.6f (ожидалось %.6f)"),
        PullPerSecond * FrameSeconds, Frames, Grown, Expected));
    TestTrue(TEXT("Sanity: толчок за кадр меньше KINDA_SMALL_NUMBER -- тот самый случай"), PullPerSecond * FrameSeconds < KINDA_SMALL_NUMBER);
    TestEqual(TEXT("За 600 кадров по 1/60 с Purity выросла на притяжение × 10 с"), Grown, Expected, Expected * 0.02);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistShrineType_StoneDampensMorokInfluence,
    "Herbalist.ShrineType.StoneDampensMorokInfluence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistShrineType_StoneDampensMorokInfluence::RunTest(const FString& Parameters)
{
    // Каменное (Стрибог): глушит вклад MorokField в локальный Distortion на
    // (1 − 0.4×Restoration) -- тот же прямой вызов ApplyBiomeInfluences, что
    // Herbalist.Save.BiomeInfluencesWithZeroFieldsStaySparse, юнит-стилем.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    auto SetupCell = [](AGridWorldManager* Manager) -> FGridCell*
    {
        FGridCell* Cell = Manager->GetCell(0, 0);
        Cell->Biome = EBiomeType::Steppe;
        Cell->TargetState.Meta.Distortion = 0.0f;
        return Cell;
    };

    AGridWorldManager* Baseline = SpawnAndBeginPlay(World);
    AGridWorldManager* WithShrine = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Baseline manager spawned"), Baseline) || !TestNotNull(TEXT("Shrine manager spawned"), WithShrine))
    {
        if (Baseline) Baseline->Destroy();
        if (WithShrine) WithShrine->Destroy();
        return false;
    }

    FGridCell* BaselineCell = SetupCell(Baseline);
    FGridCell* ShrineCell = SetupCell(WithShrine);
    WithShrine->RegisterShrine(FIntPoint(0, 0), EShrineType::Stone);
    FShrine* Shrine = WithShrine->FindShrineAt(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Shrine registered"), Shrine))
    {
        Baseline->Destroy();
        WithShrine->Destroy();
        return false;
    }
    Shrine->Restoration = 1.0f;   // полное восстановление -- дампинг ×(1-0.4) = ×0.6

    TMap<FName, float> MorokFields, ZaryanaFields;
    MorokFields.Add(FBiomeDefaults::BiomeTypeToName(EBiomeType::Steppe), 0.5f);

    Baseline->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 1.0f);
    WithShrine->ApplyBiomeInfluences(MorokFields, ZaryanaFields, 1.0f, 1.0f);

    TestTrue(TEXT("Stone shrine at full Restoration lets through less Distortion than no shrine"),
        ShrineCell->TargetState.Meta.Distortion < BaselineCell->TargetState.Meta.Distortion);
    TestTrue(TEXT("Baseline cell still gained some Distortion (comparison is meaningful)"),
        BaselineCell->TargetState.Meta.Distortion > 0.0f);

    Baseline->Destroy();
    WithShrine->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
