// Source/ProjectHerbalistTests/Private/Tests/PeregnoyTest.cpp
//
// Гниение как терминальное состояние + внесение перегноя (2026-09-04,
// прямой запрос пользователя: "с биологической точки зрения гнилая трава
// не зола... либо выкидывать, либо придумать применение" -> "применение
// перегноя сразу делаем"). До этой правки ApplyDecayToItem гнал предмет к
// предельно испорченному состоянию и молча останавливался там навсегда --
// слот никогда не освобождался, "куча вечной травы" была технически
// возможна, просто гнилая, не свежая. Теперь: Purity/Distortion пересекают
// оба порога одновременно (HerbalistSettings.h) -> предмет становится
// Перегноем (bSubjectToDecay=false, дальше decay не трогает), который можно
// внести в клетку (AGridWorldManager::ApplyFertilizerToCell), поднимая её
// Environment.Fertility.

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

// ---------------------------------------------------------------------------
// ShouldConvertToPeregnoy -- чистая функция от FMeta, без обращения к
// реестрам/GameInstance (см. довод в HerbalistInventoryComponent.h) --
// тестируется напрямую, оба порога обязаны сойтись одновременно (И).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPeregnoy_ShouldConvertRequiresBothThresholds,
    "Herbalist.Peregnoy.ShouldConvertRequiresBothThresholds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPeregnoy_ShouldConvertRequiresBothThresholds::RunTest(const FString& Parameters)
{
    const float PurityThreshold = 0.05f;
    const float DistortionThreshold = 0.95f;

    FMeta Fresh;
    Fresh.Purity = 0.8f;
    Fresh.Distortion = 0.1f;
    TestFalse(TEXT("Fresh herb (high Purity, low Distortion) does not convert"),
        UHerbalistInventoryComponent::ShouldConvertToPeregnoy(Fresh, PurityThreshold, DistortionThreshold));

    FMeta OnlyPurityGone;
    OnlyPurityGone.Purity = 0.02f;
    OnlyPurityGone.Distortion = 0.5f;   // не пересекает свой порог
    TestFalse(TEXT("Low Purity alone (Distortion still moderate) does not convert"),
        UHerbalistInventoryComponent::ShouldConvertToPeregnoy(OnlyPurityGone, PurityThreshold, DistortionThreshold));

    FMeta OnlyDistortionMaxed;
    OnlyDistortionMaxed.Purity = 0.5f;   // не пересекает свой порог
    OnlyDistortionMaxed.Distortion = 0.99f;
    TestFalse(TEXT("High Distortion alone (Purity still moderate) does not convert"),
        UHerbalistInventoryComponent::ShouldConvertToPeregnoy(OnlyDistortionMaxed, PurityThreshold, DistortionThreshold));

    FMeta FullyRotten;
    FullyRotten.Purity = 0.02f;
    FullyRotten.Distortion = 0.99f;
    TestTrue(TEXT("Both thresholds crossed together -> converts"),
        UHerbalistInventoryComponent::ShouldConvertToPeregnoy(FullyRotten, PurityThreshold, DistortionThreshold));

    return true;
}

// ---------------------------------------------------------------------------
// Полный путь через TickComponent: предмет, уже гнилой на входе, после
// одного тика decay честно пересекает оба порога и превращается в
// Перегной -- терминально (bSubjectToDecay=false), слот не "застревает"
// навечно с максимально испорченными числами.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPeregnoy_FullyRottenItemConvertsOnTick,
    "Herbalist.Peregnoy.FullyRottenItemConvertsOnTick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPeregnoy_FullyRottenItemConvertsOnTick::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("TestHerb"));
    Item.Count = 1;
    Item.bSubjectToDecay = true;
    // Уже за порогом ДО тика -- decay внутри тика только толкает дальше в ту
    // же сторону (см. ApplyDecayToItem), проверка идёт ПОСЛЕ применения decay.
    Item.State.Meta.Purity = 0.02f;
    Item.State.Meta.Distortion = 0.98f;
    Item.State.Meta.Stability = 0.0f;   // максимальная Instability
    Inventory->AddItem(Item, 1);

    FActorComponentTickFunction DummyTick;
    Inventory->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);

    const FInventoryItem* Slot = Inventory->GetSlot(0);
    if (!TestNotNull(TEXT("Slot still present (converted in place, not removed)"), Slot))
    {
        Owner->Destroy();
        return false;
    }

    TestEqual(TEXT("IngredientID becomes Peregnoy"), Slot->IngredientID, UHerbalistInventoryComponent::PeregnoyIngredientID);
    TestFalse(TEXT("Peregnoy is terminal -- no longer subject to decay"), Slot->bSubjectToDecay);

    Owner->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPeregnoy_FreshItemDoesNotConvertOnTick,
    "Herbalist.Peregnoy.FreshItemDoesNotConvertOnTick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPeregnoy_FreshItemDoesNotConvertOnTick::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AActor* Owner = World->SpawnActor<AActor>();
    UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
    Inventory->RegisterComponent();

    FInventoryItem Item;
    Item.IngredientID = FName(TEXT("TestHerb"));
    Item.Count = 1;
    Item.bSubjectToDecay = true;
    Item.State.Meta.Purity = 0.7f;
    Item.State.Meta.Distortion = 0.1f;
    Item.State.Meta.Stability = 0.9f;   // низкая Instability -- decay почти не двигается за один тик
    Inventory->AddItem(Item, 1);

    FActorComponentTickFunction DummyTick;
    Inventory->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);

    const FInventoryItem* Slot = Inventory->GetSlot(0);
    if (!TestNotNull(TEXT("Slot present"), Slot))
    {
        Owner->Destroy();
        return false;
    }

    TestEqual(TEXT("Fresh herb keeps its own IngredientID, does not convert after one tick"), Slot->IngredientID, FName(TEXT("TestHerb")));
    TestTrue(TEXT("Still subject to decay -- ordinary aging continues"), Slot->bSubjectToDecay);

    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// ApplyFertilizerToCell -- не трогает GameInstance (см. довод в
// GridWorldManager.h), напрямую тестируема, тот же приём, что и
// PlantSeedInCell в GardenPlantingTest.cpp.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPeregnoy_ApplyFertilizerRaisesFertilityAndClamps,
    "Herbalist.Peregnoy.ApplyFertilizerRaisesFertilityAndClamps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPeregnoy_ApplyFertilizerRaisesFertilityAndClamps::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(7, 7);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    Cell->Environment.Fertility = 0.5f;
    TestTrue(TEXT("First application succeeds"), Manager->ApplyFertilizerToCell(FIntPoint(7, 7)));
    TestTrue(TEXT("Fertility rose after one application"), Cell->Environment.Fertility > 0.5f);

    // Насыщение у верхней границы -- несколько применений подряд не должны
    // унести Fertility выше 1.0.
    for (int32 i = 0; i < 20; ++i)
    {
        Manager->ApplyFertilizerToCell(FIntPoint(7, 7));
    }
    TestEqual(TEXT("Fertility clamps at 1.0, does not overflow"), Cell->Environment.Fertility, 1.0f);

    TestFalse(TEXT("No cell at an out-of-range coordinate -> refused"),
        Manager->ApplyFertilizerToCell(FIntPoint(999999, 999999)));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
