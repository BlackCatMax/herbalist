// Source/ProjectHerbalistTests/Private/Tests/LegendaryEntityTest.cpp
//
// Легендарный ранг (§16.4, LegendaryEntityTypes.h, 2026-08-29). Тесты
// инициализируют UBiomeGraphSubsystem боевым DA_BiomeGraph (тот же путь,
// что BiomeGraphIntegrationTest.cpp) и мутируют MorokField узла напрямую
// через GetMutableNode -- не гоняют полный StepSimulation/Tick(), чтобы
// PropagateWaves/RecalculateFieldsFromGrid не перезаписали контролируемое
// тестом значение. Manager->Deinitialize() графа в конце каждого теста --
// та же дисциплина, что уже подтвердила себя нужной в
// BiomeGraphIntegrationTest.cpp (иначе следующий тест унаследует граф).

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/BiomeGraph/BiomeGraphAsset.h"
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

    UBiomeGraphSubsystem* InitGraph(UWorld* World)
    {
        UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>();
        if (!Graph) return nullptr;
        UBiomeGraphAsset* Asset = LoadObject<UBiomeGraphAsset>(nullptr, TEXT("/Game/Data/DA_BiomeGraph"));
        if (!Asset) return nullptr;
        Graph->InitializeFromAsset(Asset);
        return Graph;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLegendary_MalignPoleTriggersOnMorokSpike,
    "Herbalist.Legendary.MalignPoleTriggersOnMorokSpike",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLegendary_MalignPoleTriggersOnMorokSpike::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День -- не хотим коллизии с разлитым по всей сетке нуджом Рассвета/Ночи

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized from DA_BiomeGraph"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Болотный царь has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    const FName BogID = FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog);
    FBiomeGraphNode* Node = Graph->GetMutableNode(BogID);
    if (!TestNotNull(TEXT("Bog node exists in the graph"), Node))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    Node->MorokField = 0.1f;   // низкий -- не должен триггерить
    Manager->UpdateEntityManifestations(1.0f);
    FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y);
    TestNotEqual(TEXT("Low MorokField does not manifest Болотный царь"),
        Cell->ManifestedEntityID, FName(TEXT("Болотный царь")));

    Node->MorokField = 0.9f;   // спайк -- должен триггерить
    const float CorruptionBefore = Cell->TargetState.Meta.Corruption;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Morok spike manifests Болотный царь"),
        Cell->ManifestedEntityID, FName(TEXT("Болотный царь")));
    TestTrue(TEXT("Болотный царь nudges Corruption up"),
        Cell->TargetState.Meta.Corruption > CorruptionBefore);

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLegendary_BenignPoleTriggersOnLowMorokOrShrine,
    "Herbalist.Legendary.BenignPoleTriggersOnLowMorokOrShrine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLegendary_BenignPoleTriggersOnLowMorokOrShrine::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День -- не хотим коллизии с разлитым по всей сетке нуджом Рассвета/Ночи

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized from DA_BiomeGraph"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    const FIntPoint* Anchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Дуб-старец")));
    if (!TestNotNull(TEXT("Дуб-старец has a seeded anchor cell"), Anchor))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    const FName ForestID = FBiomeDefaults::BiomeTypeToName(EBiomeType::BroadleafForest);
    FBiomeGraphNode* Node = Graph->GetMutableNode(ForestID);
    if (!TestNotNull(TEXT("Broadleaf forest node exists"), Node))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }

    // Высокий MorokField, нет капища рядом -- НЕ должен проявиться.
    Node->MorokField = 0.8f;
    Manager->UpdateEntityManifestations(1.0f);
    FGridCell* Cell = Manager->GetCell(Anchor->X, Anchor->Y);
    TestNotEqual(TEXT("High MorokField, no shrine -- Дуб-старец does not manifest"),
        Cell->ManifestedEntityID, FName(TEXT("Дуб-старец")));

    // Первый путь: MorokField падает достаточно низко.
    Node->MorokField = 0.05f;
    const float StabilityBefore = Cell->TargetState.Meta.Stability;
    Manager->UpdateEntityManifestations(1.0f);
    TestEqual(TEXT("Low MorokField manifests Дуб-старец"),
        Cell->ManifestedEntityID, FName(TEXT("Дуб-старец")));
    TestTrue(TEXT("Дуб-старец nudges Stability up"),
        Cell->TargetState.Meta.Stability > StabilityBefore);

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistLegendary_OnlyOneAnchorCellManifestsNotWholeBiome,
    "Herbalist.Legendary.OnlyOneAnchorCellManifestsNotWholeBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistLegendary_OnlyOneAnchorCellManifestsNotWholeBiome::RunTest(const FString& Parameters)
{
    // Регрессия найдена до коммита (BiomeGraphIntegrationTest поймал 243/400
    // грязных клеток на первой версии, где Легендарный жил в общем цикле по
    // Cells, не по одной клетке-якорю): триггер на уровне биом-графа общий
    // на весь биом, но эффект должен применяться ровно к ОДНОЙ клетке.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(10.0f * 60.0f);   // День -- не хотим коллизии с разлитым по всей сетке нуджом Рассвета/Ночи

    UBiomeGraphSubsystem* Graph = InitGraph(World);
    if (!TestNotNull(TEXT("Graph initialized from DA_BiomeGraph"), Graph))
    {
        Manager->Destroy();
        return false;
    }

    const FName BogID = FBiomeDefaults::BiomeTypeToName(EBiomeType::Bog);
    FBiomeGraphNode* Node = Graph->GetMutableNode(BogID);
    if (!TestNotNull(TEXT("Bog node exists"), Node))
    {
        Graph->Deinitialize();
        Manager->Destroy();
        return false;
    }
    Node->MorokField = 0.9f;   // достаточно для всех Malign существ Болота

    Manager->UpdateEntityManifestations(1.0f);

    // Считаем только клетки, занятые ИМЕННО этими двумя легендарными Болота
    // (не Гнильники и т.п. -- те тоже законно проявляются на Болоте по
    // умолчанию, DT_BiomeDefaults даёт Corruption Болота ~0.70, выше порога
    // Гнильников; это ожидаемый фон, не то, что здесь проверяется).
    const FName BolotnyTsar(TEXT("Болотный царь"));
    const FName LikhoOdnoglazoe(TEXT("Лихо Одноглазое"));
    int32 LegendaryManifestedCount = 0;
    Manager->ForEachCell([&LegendaryManifestedCount, &BolotnyTsar, &LikhoOdnoglazoe](const FGridCell& Cell)
    {
        if (Cell.ManifestedEntityID == BolotnyTsar || Cell.ManifestedEntityID == LikhoOdnoglazoe)
        {
            ++LegendaryManifestedCount;
        }
    });

    // Болотный царь + Лихо Одноглазое оба на Болоте -- максимум 2 клетки
    // легендарно заняты (по одной на каждого), не все клетки биома Болото.
    TestTrue(FString::Printf(TEXT("Only anchor cells manifest (%d), not every Bog cell"), LegendaryManifestedCount),
        LegendaryManifestedCount <= 2);
    TestTrue(TEXT("At least one legendary anchor did manifest"), LegendaryManifestedCount >= 1);

    Graph->Deinitialize();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
