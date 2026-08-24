// Source/ProjectHerbalistTests/Private/Tests/AmbientEntityTest.cpp
//
// Регрессия для AmbientEntityTypes.h + AGridWorldManager::UpdateEntityManifestations
// после рефакторинга 2026-08-24: Гнильники (единственный Низший до этой
// сессии) вынесены из захардкоженного if-блока в таблицу определений,
// добавлены Моховые духи (Тайга) и Степные огни (Степь) — этот файл
// проверяет, что (а) старое поведение Гнильников не изменилось и (б) новые
// определения действительно применяются через тот же обобщённый цикл.
// DispatchBeginPlay-паттерн — тот же, что BistabilityTest.cpp/ShrineTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    AGridWorldManager* SpawnAndBeginPlay(UWorld* World)
    {
        if (!World) return nullptr;
        AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
        if (Manager)
        {
            Manager->DispatchBeginPlay();
        }
        return Manager;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_GnilnikiStillManifestsAfterRefactor,
    "Herbalist.AmbientEntity.GnilnikiStillManifestsAfterRefactor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_GnilnikiStillManifestsAfterRefactor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Bog;
    Cell->bIsWater = false;
    Cell->State.Meta.Corruption = 0.8f;   // выше дефолтного порога 0.6
    Cell->State.Meta.Purity = 0.5f;
    // TargetState по умолчанию FRealState() = всё в нуле (BiomeDataTable не
    // заполнен в голом тестовом мире, см. комментарий в BistabilityTest.cpp) —
    // нудж вниз от 0.0 не отличим от "не сработало" (кламп к 0.0), поэтому
    // задаём правдоподобное ненулевое начальное значение явно.
    Cell->TargetState.Meta.Purity = 0.5f;
    const float PurityBefore = Cell->TargetState.Meta.Purity;
    const float CorruptionBefore = Cell->TargetState.Meta.Corruption;

    Manager->UpdateEntityManifestations(1.0f);   // DeltaTime=1s -- удобная арифметика

    TestEqual(TEXT("Гнильники manifest on the corrupted Bog cell"), Cell->ManifestedEntityID, FName(TEXT("Гнильники")));
    TestTrue(TEXT("TargetState.Corruption nudged up"), Cell->TargetState.Meta.Corruption > CorruptionBefore);
    TestTrue(TEXT("TargetState.Purity nudged down"), Cell->TargetState.Meta.Purity < PurityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_MokhovyeDukhiHealHighPurityTaiga,
    "Herbalist.AmbientEntity.MokhovyeDukhiHealHighPurityTaiga",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_MokhovyeDukhiHealHighPurityTaiga::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Taiga;
    Cell->bIsWater = false;
    Cell->State.Meta.Purity = 0.9f;   // выше порога 0.75
    const float PurityBefore = Cell->TargetState.Meta.Purity;
    const float StabilityBefore = Cell->TargetState.Meta.Stability;

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Моховые духи manifest on the pristine Taiga cell"), Cell->ManifestedEntityID, FName(TEXT("Моховые духи")));
    TestTrue(TEXT("TargetState.Purity nudged up (единственный 'улучшающий' низший, §16.2)"), Cell->TargetState.Meta.Purity > PurityBefore);
    TestTrue(TEXT("TargetState.Stability nudged up"), Cell->TargetState.Meta.Stability > StabilityBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_StepnyeOgniOnlyManifestAtNight,
    "Herbalist.AmbientEntity.StepnyeOgniOnlyManifestAtNight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_StepnyeOgniOnlyManifestAtNight::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Steppe;
    Cell->bIsWater = false;

    // День (GameClockSeconds по умолчанию 0.0 -- рассвет) -- Степные огни не
    // должны проявляться, у их определения TriggerAxis::None, только ночь.
    Manager->SetGameClockSeconds(0.0f);
    TestFalse(TEXT("Sanity: it's day at t=0"), Manager->IsNight());
    Manager->UpdateEntityManifestations(1.0f);
    TestNotEqual(TEXT("Степные огни don't manifest during the day"), Cell->ManifestedEntityID, FName(TEXT("Степные огни")));

    // Ночь -- последние 6 игровых минут суток (32 по умолчанию), см. IsNight().
    Manager->SetGameClockSeconds(31.0f * 60.0f);
    TestTrue(TEXT("Sanity: it's night near the end of the day"), Manager->IsNight());
    const float DistortionBefore = Cell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Степные огни manifest at night"), Cell->ManifestedEntityID, FName(TEXT("Степные огни")));
    TestTrue(TEXT("TargetState.Distortion nudged up (дезориентация)"), Cell->TargetState.Meta.Distortion > DistortionBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_NightHorrorAffectsEveryBiomeWithoutClaimingTheCell,
    "Herbalist.AmbientEntity.NightHorrorAffectsEveryBiomeWithoutClaimingTheCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_NightHorrorAffectsEveryBiomeWithoutClaimingTheCell::RunTest(const FString& Parameters)
{
    // §16.5: "сквозная ночная фаза" -- Вурдалаки/Навьи/... не привязаны к
    // биому (biome: Повсеместно) и не "владеют" клеткой как Гнильники
    // болотом; проверяем на биоме, где сегодня нет ни одного другого
    // определения (Тундра), чтобы эффект был виден изолированно.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(0, 0);
    if (!TestNotNull(TEXT("Cell (0,0) exists"), Cell))
    {
        Manager->Destroy();
        return false;
    }
    Cell->Biome = EBiomeType::Tundra;
    Cell->bIsWater = false;

    Manager->SetGameClockSeconds(0.0f);   // день
    const float DistortionBeforeDay = Cell->TargetState.Meta.Distortion;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("No change during the day on a biome with no other definition"),
        Cell->TargetState.Meta.Distortion, DistortionBeforeDay);

    Manager->SetGameClockSeconds(31.0f * 60.0f);   // ночь
    const float DistortionBeforeNight = Cell->TargetState.Meta.Distortion;
    const float CorruptionBeforeNight = Cell->TargetState.Meta.Corruption;
    Manager->UpdateEntityManifestations(1.0f);

    TestTrue(TEXT("TargetState.Distortion nudged up at night"), Cell->TargetState.Meta.Distortion > DistortionBeforeNight);
    TestTrue(TEXT("TargetState.Corruption nudged up at night"), Cell->TargetState.Meta.Corruption > CorruptionBeforeNight);
    TestEqual(TEXT("Night horror does not claim ManifestedEntityID -- it's atmosphere, not a 'owner'"),
        Cell->ManifestedEntityID, FName(NAME_None));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
