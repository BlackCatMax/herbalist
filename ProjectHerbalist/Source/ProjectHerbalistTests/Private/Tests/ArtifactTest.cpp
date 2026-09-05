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
#include "Core/Types/BiomeTypes.h"
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

    // Distortion=0 -> PerceiveRealState гарантированно без шума (та же
    // гарантия, что уже используют MakeOfferedItem-тесты выше) -- нужна для
    // детерминированных тестов "Сцены обмана Болотного царя".
    FRealState MakeLurePotionState(float Purity)
    {
        FRealState State;
        State.Meta.Purity = Purity;
        State.Meta.Distortion = 0.0f;
        return State;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_LegendaryManifestedFallsBackToFullScanWithoutAnchor,
    "Herbalist.Artifact.LegendaryManifestedFallsBackToFullScanWithoutAnchor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_LegendaryManifestedFallsBackToFullScanWithoutAnchor::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Берегиня (bUsesCellHistoryPurity=true, LegendaryEntityTypes.h) не
    // получает якорь в LegendaryAnchors (SeedLegendaryAnchors пропускает
    // per-клеточные карточки) -- 2026-09-02, унификация: раньше отдельный
    // метод IsBereginyaManifested(), теперь fallback внутри самого
    // IsLegendaryManifested (нет якоря -> сканирует все клетки).
    const FName BereginyaID(TEXT("Берегиня"));
    TestFalse(TEXT("Not manifested at start"), Manager->IsLegendaryManifested(BereginyaID));

    if (FGridCell* Cell = Manager->GetCell(5, 5))
    {
        Cell->ManifestedEntityID = BereginyaID;
    }
    TestTrue(TEXT("Manifested once any cell carries her ID"), Manager->IsLegendaryManifested(BereginyaID));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_CompanionArtifactsAddToAcquiredListOnGeneralPath,
    "Herbalist.Artifact.CompanionArtifactsAddToAcquiredListOnGeneralPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_CompanionArtifactsAddToAcquiredListOnGeneralPath::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // §21.2 (ревизия "Update docs", 2026-09-01): "Аграфена их не даёт...
    // оба -- такие же артефакты Легендарных, как остальные шесть" --
    // Зеркальце/Клубочек теперь тоже получают запись в AcquiredArtifacts
    // (нужна для Warmth/прогрева §21.4), не особый случай, как раньше.
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
    TestTrue(TEXT("Зеркальце acquired"), bAcquired);
    TestFalse(TEXT("...honestly"), bViaDeception);
    TestEqual(TEXT("Now adds an AcquiredArtifacts entry, like any other artifact"),
        Manager->GetAcquiredArtifacts().Num(), 1);
    TestFalse(TEXT("Not warmed yet -- Warmth starts at 0"), Manager->IsArtifactWarmed(FName(TEXT("Зеркальце"))));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_BabaYagaHonestPathNeedsNoSpecialCase,
    "Herbalist.Artifact.BabaYagaHonestPathNeedsNoSpecialCase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_BabaYagaHonestPathNeedsNoSpecialCase::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // §21.3: "единственный Легендарный, для которого честный/обманный
    // выбор — не наша механика поверх фольклора, а его собственная
    // фольклорная суть... технически можно завести тем же
    // TryAcquireArtifact, что и остальные семь, без специального случая" —
    // этот тест доказывает именно это: ни один код-путь здесь не
    // special-case на "Баба-Яга"/"Шапка-невидимка", проходит через общий
    // честный/обманный гейт как любой другой артефакт.
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

    // Distortion=0, высокий real Purity -> честный путь, детерминированно
    // (та же гарантия, что уже HonestOfferingAcquiresArtifact).
    TArray<FInventoryItem> Offered = { MakeOfferedItem(0.9f, 0.0f) };
    bool bViaDeception = true;
    const bool bAcquired = Manager->TryAcquireArtifact(FName(TEXT("Шапка-невидимка")), Offered, bViaDeception);
    TestTrue(TEXT("Шапка-невидимка acquired honestly through the general gate"), bAcquired);
    TestFalse(TEXT("Honest path, not deception"), bViaDeception);
    TestEqual(TEXT("Recorded like any other non-companion artifact"), Manager->GetAcquiredArtifacts().Num(), 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_LureRequiresTsarManifestedAndProximity,
    "Herbalist.Artifact.LureRequiresTsarManifestedAndProximity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_LureRequiresTsarManifestedAndProximity::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized"), Graph)) { Manager->Destroy(); return false; }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    bool bGranted = true;
    TestFalse(TEXT("Not manifested -- no attempt possible"),
        Manager->TryLureSwampTsarWithPotion(*Anchor, MakeLurePotionState(1.0f), bGranted));
    TestFalse(TEXT("bOutGranted stays false when the attempt itself fails"), bGranted);

    // Спайк -- Царь проявлен.
    FBiomeGraphNode* Node = Graph->GetMutableNode(FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog));
    if (!TestNotNull(TEXT("Bog node exists"), Node)) { Graph->Deinitialize(); Manager->Destroy(); return false; }
    Node->MorokField = 0.9f;
    Manager->UpdateEntityManifestations(1.0f);
    TestTrue(TEXT("Болотный царь now manifested"), Manager->IsLegendaryManifested(FName(TEXT("Болотный царь"))));

    // Далеко от его якоря (за пределами LurePotionRadius=1) -- всё ещё нет попытки.
    const FIntPoint Far(Anchor->X + 5, Anchor->Y + 5);
    bGranted = true;
    TestFalse(TEXT("Too far from the anchor -- no attempt"),
        Manager->TryLureSwampTsarWithPotion(Far, MakeLurePotionState(1.0f), bGranted));
    TestFalse(TEXT("bOutGranted stays false when the attempt fails"), bGranted);

    // Прямо на его клетке -- попытка теперь состоится.
    bGranted = false;
    TestTrue(TEXT("On the anchor cell -- attempt proceeds"),
        Manager->TryLureSwampTsarWithPotion(*Anchor, MakeLurePotionState(1.0f), bGranted));

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_LureWithMaxPerceivedPurityAlwaysSucceeds,
    "Herbalist.Artifact.LureWithMaxPerceivedPurityAlwaysSucceeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_LureWithMaxPerceivedPurityAlwaysSucceeds::RunTest(const FString& Parameters)
{
    // Purity=1.0, Distortion=0 -> PerceivedPurity==1.0 exactly (no noise) ->
    // Chance=(1.0-0.6)/(1.0-0.6)=1.0 -> WorldRNG.FRand() всегда в [0,1),
    // строго меньше 1.0 -- детерминированный успех без завязки на конкретный сид.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized"), Graph)) { Manager->Destroy(); return false; }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    FBiomeGraphNode* Node = Graph->GetMutableNode(FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog));
    Node->MorokField = 0.9f;
    Manager->UpdateEntityManifestations(1.0f);

    bool bGranted = false;
    const bool bAttempted = Manager->TryLureSwampTsarWithPotion(*Anchor, MakeLurePotionState(1.0f), bGranted);
    TestTrue(TEXT("Attempt proceeds"), bAttempted);
    TestTrue(TEXT("Maximally convincing decoy always steals the Фонарь"), bGranted);
    TestEqual(TEXT("Фонарь recorded as acquired via deception"), Manager->GetAcquiredArtifacts().Num(), 1);
    if (Manager->GetAcquiredArtifacts().Num() == 1)
    {
        TestTrue(TEXT("Recorded via deception"), Manager->GetAcquiredArtifacts()[0].bAcquiredViaDeception);
    }

    // Уже добыт -- вторая попытка не проходит гейт вовсе.
    bGranted = true;
    TestFalse(TEXT("Cannot lure a second time once the Фонарь is already held"),
        Manager->TryLureSwampTsarWithPotion(*Anchor, MakeLurePotionState(1.0f), bGranted));
    TestFalse(TEXT("bOutGranted stays false on the blocked second attempt"), bGranted);

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_LureWithUnconvincingPotionFailsWithoutGranting,
    "Herbalist.Artifact.LureWithUnconvincingPotionFailsWithoutGranting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_LureWithUnconvincingPotionFailsWithoutGranting::RunTest(const FString& Parameters)
{
    // Purity=0.1, Distortion=0 -> PerceivedPurity==0.1 exactly, ниже
    // ArtifactHonestPurityThreshold (0.6) -- Chance=0, никогда не ворует.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized"), Graph)) { Manager->Destroy(); return false; }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    FBiomeGraphNode* Node = Graph->GetMutableNode(FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog));
    Node->MorokField = 0.9f;
    Manager->UpdateEntityManifestations(1.0f);

    bool bGranted = true;
    const bool bAttempted = Manager->TryLureSwampTsarWithPotion(*Anchor, MakeLurePotionState(0.1f), bGranted);
    TestTrue(TEXT("Attempt proceeds (potion is spent) even though it fails"), bAttempted);
    TestFalse(TEXT("Unconvincing decoy never steals the Фонарь"), bGranted);
    TestEqual(TEXT("Nothing recorded"), Manager->GetAcquiredArtifacts().Num(), 0);

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_InvisibilityCapGuaranteesLureRegardlessOfRoll,
    "Herbalist.Artifact.InvisibilityCapGuaranteesLureRegardlessOfRoll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_InvisibilityCapGuaranteesLureRegardlessOfRoll::RunTest(const FString& Parameters)
{
    // §21.3: "если у игрока уже есть Шапка -- отвлечение гарантировано, не
    // вероятностно" -- Purity чуть выше порога (Chance далёк от 1.0 без
    // Шапки) должно всё равно гарантированно сработать с ней.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized"), Graph)) { Manager->Destroy(); return false; }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    FBiomeGraphNode* Node = Graph->GetMutableNode(FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog));
    Node->MorokField = 0.9f;
    Manager->UpdateEntityManifestations(1.0f);

    FAcquiredArtifact Cap;
    Cap.ArtifactID = FName(TEXT("Шапка-невидимка"));
    Manager->SetAcquiredArtifacts({ Cap });

    // Purity=0.61 -- barely above the 0.6 threshold, Chance without the cap
    // would be (0.61-0.6)/(1.0-0.6)=0.025 (2.5%), essentially never
    // succeeding on a real dice roll -- but the cap bypasses the roll.
    bool bGranted = false;
    const bool bAttempted = Manager->TryLureSwampTsarWithPotion(*Anchor, MakeLurePotionState(0.61f), bGranted);
    TestTrue(TEXT("Attempt proceeds"), bAttempted);
    TestTrue(TEXT("Шапка guarantees the theft despite a barely-convincing decoy"), bGranted);

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

// Аудит 2026-09-05: bWarmsCompanionItem нигде в коде не читался --
// HerbalistPlayerController::OfferForArtifact резолвил Зеркальце/Клубочек
// по двум жёстко прописанным именам, полностью игнорируя сам флаг.
// Третий предмет-спутник, заведённый чистой правкой DT_Artifacts, молча
// ничего не получил бы. Теперь флаг СВЕРЯЕТСЯ (см. HerbalistPlayerController.cpp:
// предупреждение в лог при рассинхронизации имени и флага) -- этот тест
// фиксирует инвариант, на который эта сверка опирается: DT_Artifacts
// действительно помечает оба существующих предмета-спутника флагом, а не
// полагается на два голых имени без данных за ними.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistArtifact_CompanionItemsAreFlaggedInDataTable,
    "Herbalist.Artifact.CompanionItemsAreFlaggedInDataTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistArtifact_CompanionItemsAreFlaggedInDataTable::RunTest(const FString& Parameters)
{
    const FArtifactDefinition* Mirror = FindArtifactDefinition(FName(TEXT("Зеркальце")));
    if (TestNotNull(TEXT("Зеркальце found in DT_Artifacts"), Mirror))
    {
        TestTrue(TEXT("Зеркальце is flagged bWarmsCompanionItem"), Mirror->bWarmsCompanionItem);
    }

    const FArtifactDefinition* YarnBall = FindArtifactDefinition(FName(TEXT("Клубочек")));
    if (TestNotNull(TEXT("Клубочек found in DT_Artifacts"), YarnBall))
    {
        TestTrue(TEXT("Клубочек is flagged bWarmsCompanionItem"), YarnBall->bWarmsCompanionItem);
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
