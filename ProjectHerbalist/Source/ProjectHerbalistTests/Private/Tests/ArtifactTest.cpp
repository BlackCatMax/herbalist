// Source/ProjectHerbalistTests/Private/Tests/ArtifactTest.cpp
//
// Артефакты Легендарных сущностей (21_Journey_And_Artifacts.md §21.3-21.4,
// 2026-09-01). Тот же DispatchBeginPlay-паттерн, что уже обкатан в
// LegendaryEntityTest.cpp/ZaryanaTest.cpp/BasesTest.cpp — конкретные имена
// сущностей/якорей полагаются на детерминированный SeedLegendaryAnchors
// дефолтной тестовой сетки (тот же приём, что уже LegendaryEntityTest.cpp
// использует для "Болотный царь").

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/ArtifactTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeOfferedItem(float Purity, float Distortion)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("TestOffering"));
        Item.State.Meta.Purity = Purity;
        Item.State.Meta.Distortion = Distortion;
        Item.Count = 1;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_IsLegendaryManifestedReadsAnchorCell,
    "Herbalist.Artifact.IsLegendaryManifestedReadsAnchorCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_IsLegendaryManifestedReadsAnchorCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FName EntityID(TEXT("Дуб-старец"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(EntityID);
    if (!TestNotNull(TEXT("Дуб-старец has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }

    TestFalse(TEXT("Not manifested before ManifestedEntityID is set"), Manager->IsLegendaryManifested(EntityID));

    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = EntityID;
    }
    TestTrue(TEXT("Manifested once ManifestedEntityID matches at the anchor"), Manager->IsLegendaryManifested(EntityID));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_IsBereginyaManifestedScansAllCells,
    "Herbalist.Artifact.IsBereginyaManifestedScansAllCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_IsBereginyaManifestedScansAllCells::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Берегиня отсутствует в LegendaryEntityTypes.h/LegendaryAnchors --
    // никакой якорь не нужен, IsBereginyaManifested сканирует все клетки.
    TestFalse(TEXT("Not manifested at start"), Manager->IsBereginyaManifested());

    if (FGridCell* Cell = Manager->GetCell(5, 5))
    {
        Cell->ManifestedEntityID = FName(TEXT("Берегиня"));
    }
    TestTrue(TEXT("Manifested once any cell carries her ID"), Manager->IsBereginyaManifested());

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_HonestOfferingAcquiresArtifact,
    "Herbalist.Artifact.HonestOfferingAcquiresArtifact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_HonestOfferingAcquiresArtifact::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FName LegendaryID(TEXT("Индрик-зверь"));
    const FName ArtifactID(TEXT("Рог"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Индрик-зверь has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }

    TArray<FInventoryItem> Offered = { MakeOfferedItem(0.9f, 0.0f) };

    // Не проявлена -- добыть нельзя, даже с честным подношением.
    bool bViaDeception = true;
    TestFalse(TEXT("Cannot acquire before the entity is manifested"),
        Manager->TryAcquireArtifact(ArtifactID, Offered, bViaDeception));

    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    // Distortion=0 -> шум формулы PerceiveRealState гарантированно нулевой
    // (та же гарантия, что уже проверяет PerceptionServiceTest.cpp) --
    // честный путь детерминирован, не зависит от сида.
    const bool bAcquired = Manager->TryAcquireArtifact(ArtifactID, Offered, bViaDeception);
    TestTrue(TEXT("High real Purity acquires the artifact"), bAcquired);
    TestFalse(TEXT("High real Purity is the honest path, not deception"), bViaDeception);
    TestEqual(TEXT("One artifact recorded"), Manager->GetAcquiredArtifacts().Num(), 1);

    // Повторно -- уже добыт.
    TestFalse(TEXT("Cannot acquire the same artifact twice"),
        Manager->TryAcquireArtifact(ArtifactID, Offered, bViaDeception));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_WeakOfferingFailsEitherWay,
    "Herbalist.Artifact.WeakOfferingFailsEitherWay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_WeakOfferingFailsEitherWay::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FName LegendaryID(TEXT("Баба-Яга"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Баба-Яга has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    // Distortion=0 -> и реальный, и воспринятый Purity равны низкому 0.1 --
    // ни честный, ни обманный путь не проходит порог.
    TArray<FInventoryItem> WeakOffering = { MakeOfferedItem(0.1f, 0.0f) };
    bool bViaDeception = false;
    TestFalse(TEXT("A weak offering acquires nothing, honestly or not"),
        Manager->TryAcquireArtifact(FName(TEXT("Шапка-невидимка")), WeakOffering, bViaDeception));
    TestEqual(TEXT("Nothing recorded"), Manager->GetAcquiredArtifacts().Num(), 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_LanternIsAlwaysAcquiredViaDeception,
    "Herbalist.Artifact.LanternIsAlwaysAcquiredViaDeception",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_LanternIsAlwaysAcquiredViaDeception::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FName LegendaryID(TEXT("Болотный царь"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    // Distortion=0, высокий real Purity — то же подношение, которое дало бы
    // ЧЕСТНЫЙ путь любому другому артефакту (см. тест выше). Фонарь —
    // §21.3: "единственный артефакт только через обман" — bDeceptionOnly
    // не смотрит на реальный Purity вовсе, только на воспринятый.
    TArray<FInventoryItem> Offered = { MakeOfferedItem(0.9f, 0.0f) };
    bool bViaDeception = false;
    const bool bAcquired = Manager->TryAcquireArtifact(FName(TEXT("Фонарь")), Offered, bViaDeception);
    TestTrue(TEXT("Lantern acquired"), bAcquired);
    TestTrue(TEXT("Lantern is always recorded as deception, even with an honest-looking offering"), bViaDeception);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_CompanionArtifactsDoNotAddToAcquiredList,
    "Herbalist.Artifact.CompanionArtifactsDoNotAddToAcquiredList",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_CompanionArtifactsDoNotAddToAcquiredList::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FName LegendaryID(TEXT("Гамаюн"));
    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(LegendaryID);
    if (!TestNotNull(TEXT("Гамаюн has a seeded anchor cell"), Anchor))
    {
        Manager->Destroy();
        return false;
    }
    if (FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y))
    {
        Cell->ManifestedEntityID = LegendaryID;
    }

    TArray<FInventoryItem> Offered = { MakeOfferedItem(0.9f, 0.0f) };
    bool bViaDeception = true;
    const bool bAcquired = Manager->TryAcquireArtifact(FName(TEXT("Зеркальце")), Offered, bViaDeception);
    TestTrue(TEXT("Зеркальце is 'acquired' (result true)"), bAcquired);
    TestFalse(TEXT("...honestly"), bViaDeception);
    TestEqual(TEXT("But it does not add an AcquiredArtifacts entry -- it is the shift 5 gift, not a new item"),
        Manager->GetAcquiredArtifacts().Num(), 0);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
