// Source/ProjectHerbalistTests/Private/Tests/AmbientEntityBatch5Test.cpp
//
// Пятая пачка §16.2 (2026-08-29, "закрываем весь бестиарий"): Шептуны/
// Подпольники/Стукачи/Пеньковые (прокси-заглушки, тот же класс, что
// Ржавые духи/Водяные бесы/Злыдни) и Межевые -- первый потребитель
// bRequiresBiomeBorder (реальная, не приближённая проверка соседей клетки)
// и первый Низший с эффектом на Direction (NatureRate), не только Meta.

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_MezhevyeOnlyManifestOnBiomeBorder,
    "Herbalist.AmbientEntity.MezhevyeOnlyManifestOnBiomeBorder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_MezhevyeOnlyManifestOnBiomeBorder::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День

    // Клетка (5,5) окружена тем же биомом со всех сторон -- НЕ граница.
    FGridCell* InteriorCell = Manager->GetCell(5, 5);
    InteriorCell->Biome = EBiomeType::ForestSteppe;
    InteriorCell->bIsWater = false;
    Manager->GetCell(6, 5)->Biome = EBiomeType::ForestSteppe;
    Manager->GetCell(4, 5)->Biome = EBiomeType::ForestSteppe;
    Manager->GetCell(5, 6)->Biome = EBiomeType::ForestSteppe;
    Manager->GetCell(5, 4)->Biome = EBiomeType::ForestSteppe;

    // Клетка (10,5) та же ForestSteppe, но один сосед -- другой биом -- ГРАНИЦА.
    FGridCell* BorderCell = Manager->GetCell(10, 5);
    BorderCell->Biome = EBiomeType::ForestSteppe;
    BorderCell->bIsWater = false;
    Manager->GetCell(11, 5)->Biome = EBiomeType::Steppe;   // сосед другого биома
    Manager->GetCell(9, 5)->Biome  = EBiomeType::ForestSteppe;
    Manager->GetCell(10, 6)->Biome = EBiomeType::ForestSteppe;
    Manager->GetCell(10, 4)->Biome = EBiomeType::ForestSteppe;

    const float InteriorNatureBefore = InteriorCell->TargetState.Direction.Nature;
    const float BorderNatureBefore = BorderCell->TargetState.Direction.Nature;

    Manager->UpdateEntityManifestations(1.0f);

    TestNotEqual(TEXT("Interior cell (same biome all around) does not manifest Межевые"),
        InteriorCell->ManifestedEntityID, FName(TEXT("Межевые")));
    TestEqual(TEXT("Interior cell Direction.Nature unchanged"),
        InteriorCell->TargetState.Direction.Nature, InteriorNatureBefore);

    TestEqual(TEXT("Border cell manifests Межевые"), BorderCell->ManifestedEntityID, FName(TEXT("Межевые")));
    TestTrue(TEXT("Border cell Direction.Nature nudged up"),
        BorderCell->TargetState.Direction.Nature > BorderNatureBefore);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_ProxyStubEntitiesManifestOnTheirConditions,
    "Herbalist.AmbientEntity.ProxyStubEntitiesManifestOnTheirConditions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_ProxyStubEntitiesManifestOnTheirConditions::RunTest(const FString& Parameters)
{
    // Шептуны/Подпольники/Стукачи/Пеньковые -- проверяем проявление по
    // условию и что ни один не даёт реального эффекта (заглушки, тот же
    // класс, что Ржавые духи/Водяные бесы/Злыдни).
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День

    FGridCell* WhisperCell = Manager->GetCell(0, 0);
    WhisperCell->Biome = EBiomeType::Tundra;
    WhisperCell->bIsWater = false;
    WhisperCell->State.Meta.Stability = 0.5f;

    // Подпольники: HarvestStress в окне (0.4, 0.6] -- ниже злыдневского
    // порога (0.6), иначе на том же биоме выиграли бы Злыдни (зарегистрированы раньше).
    FGridCell* CellarCell = Manager->GetCell(1, 0);
    CellarCell->Biome = EBiomeType::BroadleafForest;
    CellarCell->bIsWater = false;
    CellarCell->HarvestStress = 0.5f;

    FGridCell* SpyCell = Manager->GetCell(2, 0);
    SpyCell->Biome = EBiomeType::BroadleafForest;
    SpyCell->bIsWater = false;
    SpyCell->State.Meta.Distortion = 0.7f;

    FGridCell* StumpCell = Manager->GetCell(3, 0);
    StumpCell->Biome = EBiomeType::Taiga;
    StumpCell->bIsWater = false;
    StumpCell->HarvestStress = 0.0f;
    StumpCell->State.Meta.Purity = 0.1f;   // ниже порога Моховых духов (0.75), не коллизия

    const FRealState WhisperBefore = WhisperCell->TargetState;
    const FRealState CellarBefore = CellarCell->TargetState;
    const FRealState SpyBefore = SpyCell->TargetState;
    const FRealState StumpBefore = StumpCell->TargetState;

    Manager->UpdateEntityManifestations(1.0f);

    TestEqual(TEXT("Шептуны manifest in Tundra"), WhisperCell->ManifestedEntityID, FName(TEXT("Шептуны")));
    TestEqual(TEXT("Подпольники manifest in the HarvestStress warning window"), CellarCell->ManifestedEntityID, FName(TEXT("Подпольники")));
    TestEqual(TEXT("Стукачи manifest on high Distortion"), SpyCell->ManifestedEntityID, FName(TEXT("Стукачи")));
    TestEqual(TEXT("Пеньковые manifest on untouched (low HarvestStress) Taiga"), StumpCell->ManifestedEntityID, FName(TEXT("Пеньковые")));

    TestEqual(TEXT("Шептуны cell TargetState unchanged"), WhisperCell->TargetState.Meta.Stability, WhisperBefore.Meta.Stability);
    TestEqual(TEXT("Подпольники cell TargetState unchanged"), CellarCell->TargetState.Meta.Corruption, CellarBefore.Meta.Corruption);
    TestEqual(TEXT("Стукачи cell TargetState unchanged"), SpyCell->TargetState.Meta.Distortion, SpyBefore.Meta.Distortion);
    TestEqual(TEXT("Пеньковые cell TargetState unchanged"), StumpCell->TargetState.Meta.Purity, StumpBefore.Meta.Purity);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
