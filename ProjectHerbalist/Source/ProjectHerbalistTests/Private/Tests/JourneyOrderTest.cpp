// Source/ProjectHerbalistTests/Private/Tests/JourneyOrderTest.cpp
//
// Маршрут путешествия (2026-09-19, 23_Journey_Order.md): вес фрагментов в
// якоре Clarity (девять фрагментов пути + один на Буяне = ровно 1.0, ХЛЕБ-СОЛЬ
// без веса), фрагмент Болота БРОД (ночью, клетка Болота с долгим низким
// искажением), привязка ранних фрагментов к своим биомам (ТИХОЕ МЕСТО --
// Лесостепь, ПОДНОШЕНИЕ -- капище Речной поймы).

#include "Core/World/GridWorldManager.h"
#include "Core/Zaryana/MemoryFragmentDefinitions.h"
#include "Core/Config/HerbalistSettings.h"
#include "Commandlets/MemoryFragmentsCreateCommandlet.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Все фрагменты с триггером по состоянию мира, кроме проверяемого, --
    // собраны: иначе спавн мог бы отдать другой (та же изоляция, что в
    // ZaryanaTest.cpp).
    TSet<FName> AllStateFragmentsExcept(FName Kept)
    {
        TSet<FName> Out;
        for (const TCHAR* ID : { TEXT("TIKHOE_MESTO"), TEXT("PODNOSHENIE"), TEXT("KHLEB_SOL"), TEXT("TISHINA_LESA"),
            TEXT("OJIDANIE_BURI"), TEXT("NE_POKHVALILA"), TEXT("NEUDOBNAYA_PRAVDA"), TEXT("BROD") })
        {
            if (FName(ID) != Kept)
            {
                Out.Add(FName(ID));
            }
        }
        return Out;
    }

    // Часы на долю суток (0 -- рассвет).
    void SetJourneyTestTimeOfDay(AGridWorldManager* Manager, float Fraction)
    {
        const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
        const double DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0;
        Manager->SetGameClockSeconds(DaySeconds * Fraction);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJourneyOrder_FragmentWeightsFillTheAnchorExactly,
    "Herbalist.JourneyOrder.FragmentWeightsFillTheAnchorExactly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJourneyOrder_FragmentWeightsFillTheAnchorExactly::RunTest(const FString& Parameters)
{
    float PathSum = 0.0f;
    int32 PathCount = 0;
    float BuyanGain = -1.0f;
    for (const FMemoryFragmentDefinition& Def : HerbalistCore::Zaryana::GetAllMemoryFragmentDefinitions())
    {
        if (Def.Trigger == EMemoryFragmentTrigger::BuyanPathChosen)
        {
            // Три вариации -- игрок получает одну.
            TestEqual(*FString::Printf(TEXT("%s -- вес 0.1"), *Def.ID.ToString()), Def.ClarityGain, 0.1f);
            BuyanGain = Def.ClarityGain;
            continue;
        }
        if (Def.ID == FName(TEXT("KHLEB_SOL")))
        {
            TestEqual(TEXT("ХЛЕБ-СОЛЬ -- сайдовый, без веса"), Def.ClarityGain, 0.0f);
            continue;
        }
        PathSum += Def.ClarityGain;
        ++PathCount;
    }
    TestEqual(TEXT("Фрагментов пути -- девять (восемь биомов и деревня)"), PathCount, 9);
    TestTrue(TEXT("БРОД есть в реестре"), HerbalistCore::Zaryana::FindMemoryFragmentDefinition(FName(TEXT("BROD"))) != nullptr);
    TestEqual(TEXT("Путь и Буян -- ровно полный якорь"), PathSum + BuyanGain, 1.0f, 1e-4f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJourneyOrder_BrodNeedsANightHeldInTheBog,
    "Herbalist.JourneyOrder.BrodNeedsANightHeldInTheBog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJourneyOrder_BrodNeedsANightHeldInTheBog::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Якорь Болотного царя стоит на клетке Болота.
    const FIntPoint* BogAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Болотный царь")));
    if (!TestNotNull(TEXT("Bog cell available via Болотный царь anchor"), BogAnchor))
    {
        Manager->Destroy();
        return false;
    }
    Manager->SetCollectedFragmentIDs(AllStateFragmentsExcept(FName(TEXT("BROD"))));

    // Прочие клетки Болота по умолчанию тоже без искажения -- кандидатом
    // остаётся только якорь.
    Manager->ForEachCell([BogAnchor](FGridCell& C)
    {
        if (C.Biome == EBiomeType::Bog && FIntPoint(C.X, C.Y) != *BogAnchor)
        {
            C.State.Meta.Distortion = 0.9f;
        }
    });
    FGridCell* Cell = Manager->GetCell(BogAnchor->X, BogAnchor->Y);
    if (!TestNotNull(TEXT("Bog anchor cell"), Cell)) { Manager->Destroy(); return false; }
    Cell->bIsWater = false;
    Cell->State.Meta.Distortion = 0.0f;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const int32 Polls = FMath::CeilToInt(Settings->BrodSustainedSeconds / Settings->MemoryFragmentStateCheckInterval);

    // Днём тихое Болото памяти не отдаёт, сколько ни держи.
    SetJourneyTestTimeOfDay(Manager, 0.5f);
    for (int32 i = 0; i < Polls * 2; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
    }
    TestEqual(TEXT("Днём -- ничего"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    // Ночь почти выдержана -- и рассвет: накопление обнуляется целиком.
    SetJourneyTestTimeOfDay(Manager, 0.9f);
    TestTrue(TEXT("Доля 0.9 -- ночь"), Manager->IsNight());
    for (int32 i = 0; i < Polls - 1; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
    }
    TestTrue(TEXT("Ночью накопление идёт"), Manager->GetBrodHoldMapNum() > 0);

    // Собран -- аккумулятор больше не нужен и очищается.
    Manager->SetCollectedFragmentIDs(AllStateFragmentsExcept(NAME_None));
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("БРОД собран -- накопление очищено"), Manager->GetBrodHoldMapNum(), 0);
    Manager->SetCollectedFragmentIDs(AllStateFragmentsExcept(FName(TEXT("BROD"))));

    for (int32 i = 0; i < Polls - 1; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
    }
    SetJourneyTestTimeOfDay(Manager, 0.05f);
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Рассвет обнулил накопление"), Manager->GetBrodHoldMapNum(), 0);

    // Новая ночь -- только полное окно заново: на последнем опросе.
    SetJourneyTestTimeOfDay(Manager, 0.9f);
    for (int32 i = 0; i < Polls - 1; ++i)
    {
        Manager->TrySpawnStateBasedFragment();
        TestEqual(TEXT("Ночь ещё не выдержана"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Выдержанная ночь на Болоте -- БРОД"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("BROD")));


    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJourneyOrder_PodnoshenieNeedsAFloodplainShrine,
    "Herbalist.JourneyOrder.PodnoshenieNeedsAFloodplainShrine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJourneyOrder_PodnoshenieNeedsAFloodplainShrine::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint* ForestAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Дуб-старец")));
    if (!TestNotNull(TEXT("BroadleafForest anchor"), ForestAnchor))
    {
        Manager->Destroy();
        return false;
    }
    // Клетка Поймы -- явно: якоря Берегини в тестовом мире нет.
    const FIntPoint FloodplainCell(6, 6);
    FGridCell* Floodplain = Manager->GetCell(FloodplainCell.X, FloodplainCell.Y);
    if (!TestNotNull(TEXT("Cell (6,6)"), Floodplain) || FloodplainCell == *ForestAnchor)
    {
        Manager->Destroy();
        return false;
    }
    Floodplain->Biome = EBiomeType::Floodplain;
    const FIntPoint* FloodplainAnchor = &FloodplainCell;
    Manager->SetCollectedFragmentIDs(AllStateFragmentsExcept(FName(TEXT("PODNOSHENIE"))));

    // Капище вне Поймы -- до 2026-09-19 оно отдавало ПОДНОШЕНИЕ.
    Manager->RegisterShrine(*ForestAnchor, EShrineType::Ancestral);
    FShrine* ForestShrine = Manager->FindShrineAt(*ForestAnchor);
    if (!TestNotNull(TEXT("Лесное капище зарегистрировано"), ForestShrine))
    {
        Manager->Destroy();
        return false;
    }
    ForestShrine->Restoration = 0.9f;
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Капище в лесу -- не ПОДНОШЕНИЕ"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    Manager->RegisterShrine(*FloodplainAnchor, EShrineType::Ancestral);
    if (FShrine* Shrine = Manager->FindShrineAt(*FloodplainAnchor))
    {
        Shrine->Restoration = 0.9f;
    }
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Капище Поймы -- ПОДНОШЕНИЕ"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("PODNOSHENIE")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJourneyOrder_TikhoeMestoNeedsTheForestSteppe,
    "Herbalist.JourneyOrder.TikhoeMestoNeedsTheForestSteppe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJourneyOrder_TikhoeMestoNeedsTheForestSteppe::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const FIntPoint* SteppeAnchor = Manager->GetLegendaryAnchors().Find(FName(TEXT("Гамаюн")));
    if (!TestNotNull(TEXT("ForestSteppe anchor via Гамаюн"), SteppeAnchor))
    {
        Manager->Destroy();
        return false;
    }
    Manager->SetCollectedFragmentIDs(AllStateFragmentsExcept(FName(TEXT("TIKHOE_MESTO"))));

    // Вся Лесостепь шумит, прочий мир тих -- тишина вне Лесостепи не в счёт.
    Manager->ForEachCell([](FGridCell& C)
    {
        C.State.Meta.Distortion = C.Biome == EBiomeType::ForestSteppe ? 0.9f : 0.0f;
    });
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Тихо только вне Лесостепи -- ничего"), Manager->GetActiveFragmentDefinitionID(), FName(NAME_None));

    FGridCell* Cell = Manager->GetCell(SteppeAnchor->X, SteppeAnchor->Y);
    if (!TestNotNull(TEXT("ForestSteppe anchor cell"), Cell)) { Manager->Destroy(); return false; }
    Cell->bIsWater = false;
    Cell->State.Meta.Distortion = 0.0f;
    Manager->TrySpawnStateBasedFragment();
    TestEqual(TEXT("Тихая клетка Лесостепи -- ТИХОЕ МЕСТО"), Manager->GetActiveFragmentDefinitionID(), FName(TEXT("TIKHOE_MESTO")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJourneyOrder_SyncAlignsWeightsAndAddsMissingRows,
    "Herbalist.JourneyOrder.SyncAlignsWeightsAndAddsMissingRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJourneyOrder_SyncAlignsWeightsAndAddsMissingRows::RunTest(const FString& Parameters)
{
    // Копия старой таблицы: один ряд со старым весом и текстом, правленным
    // в редакторе.
    UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
    Table->RowStruct = FMemoryFragmentDefinition::StaticStruct();
    FMemoryFragmentDefinition Old;
    Old.ID = FName(TEXT("TIKHOE_MESTO"));
    Old.ClarityGain = 0.05f;
    Old.TrueText = FText::FromString(TEXT("Текст из редактора"));
    Table->AddRow(Old.ID, Old);

    TestTrue(TEXT("Синхронизация что-то изменила"), UMemoryFragmentsCreateCommandlet::SyncExistingTable(Table) > 0);
    const FMemoryFragmentDefinition* Quiet = Table->FindRow<FMemoryFragmentDefinition>(Old.ID, TEXT("Test"));
    if (TestNotNull(TEXT("Ряд на месте"), Quiet))
    {
        TestEqual(TEXT("Вес выровнен до 0.1"), Quiet->ClarityGain, 0.1f);
        TestEqual(TEXT("Текст из редактора не тронут"), Quiet->TrueText.ToString(), FString(TEXT("Текст из редактора")));
    }
    TestNotNull(TEXT("БРОД дописан"), Table->FindRow<FMemoryFragmentDefinition>(FName(TEXT("BROD")), TEXT("Test")));
    TestEqual(TEXT("Повтор -- ничего"), UMemoryFragmentsCreateCommandlet::SyncExistingTable(Table), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistJourneyOrder_LoadedAnchorFollowsCollectedFragments,
    "Herbalist.JourneyOrder.LoadedAnchorFollowsCollectedFragments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistJourneyOrder_LoadedAnchorFollowsCollectedFragments::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Сейв до смены веса: три фрагмента по 0.05, якорь 0.15.
    Manager->SetCollectedFragmentIDs({ FName(TEXT("TIKHOE_MESTO")), FName(TEXT("KHLEB_SOL")), FName(TEXT("BROD")) });
    Manager->SetClarityAnchor(0.15f);
    Manager->RecomputeClarityAnchorFromFragments();
    TestEqual(TEXT("Якорь -- по нынешним весам: 0.1 + 0 + 0.1"), Manager->GetClarityAnchor(), 0.2f, 1e-4f);
    TestTrue(TEXT("Clarity не ниже якоря"), Manager->GetGlobalPerceptionClarity() >= 0.2f - 1e-4f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
