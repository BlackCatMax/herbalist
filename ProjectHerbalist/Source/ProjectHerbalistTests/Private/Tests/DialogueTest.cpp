// Source/ProjectHerbalistTests/Private/Tests/DialogueTest.cpp
//
// Диалоги (DESIGN_Community_And_Homestead.md §1.1, реализация 2026-08-31) —
// дерево не хранит состояние, читает Respect/Molva и статический реестр
// (Core/Dialogue/HerbalistDialogueTypes.h) — тестируется напрямую, без
// PlayerController/мира, тем же приёмом, что GetLandmarkDefinitions уже
// тестируется опосредованно через LandmarkTest.cpp, но здесь у самого
// движка (фильтрация веток по гейту) прежде не было ни одного теста.
//
// Символическое подношение (2026-09-06) — единственное исключение из
// принципа выше: сам эффект (Respect += Gain) живёт в
// AHerbalistPlayerController::ChooseDialogueBranch, не в данных дерева,
// поэтому его регрессия ниже требует полного DispatchBeginPlay-мира (тот
// же приём, что уже ArtifactInventoryTest.cpp/GardenNicheUnlockTest.cpp) —
// файл получил WITH_EDITOR в гварде ради этого одного теста.

#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_DomovoiDefinitionExists,
    "Herbalist.Dialogue.DomovoiDefinitionExists",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_DomovoiDefinitionExists::RunTest(const FString& Parameters)
{
    const FDialogueDefinition* Def = FindDialogueDefinition(FName(TEXT("Домовой")));
    if (!TestNotNull(TEXT("Домовой has a registered dialogue"), Def)) return false;

    TestEqual(TEXT("Starts at the Home node"), Def->StartNodeID, FName(TEXT("Home")));
    const FDialogueNode* Start = FindDialogueNode(*Def, Def->StartNodeID);
    TestNotNull(TEXT("Start node resolves"), Start);

    const FDialogueDefinition* Unknown = FindDialogueDefinition(FName(TEXT("НиктоТакойНеЗарегистрирован")));
    TestNull(TEXT("Unregistered EntityID has no dialogue (honest gap, not a crash)"), Unknown);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_BranchesGatedByRespect,
    "Herbalist.Dialogue.BranchesGatedByRespect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_BranchesGatedByRespect::RunTest(const FString& Parameters)
{
    const FDialogueDefinition* Def = FindDialogueDefinition(FName(TEXT("Домовой")));
    if (!TestNotNull(TEXT("Домовой dialogue found"), Def)) return false;
    const FDialogueNode* Home = FindDialogueNode(*Def, FName(TEXT("Home")));
    if (!TestNotNull(TEXT("Home node found"), Home)) return false;

    // Высокий Respect (0.8, выше порога благословения 0.5, LandmarkTypes.h)
    // -- доступны "всегда" и "хорошая" ветки, "плохая" (гейт до -0.3) нет.
    const TArray<const FDialogueBranch*> HighRespect = GetAvailableBranches(*Home, 0.8f);
    TestEqual(TEXT("High Respect: two branches available (offer + good)"), HighRespect.Num(), 2);

    // Низкий Respect (-0.5, ниже порога порчи -0.3) -- "всегда" и "плохая",
    // "хорошая" (гейт от 0.5) нет.
    const TArray<const FDialogueBranch*> LowRespect = GetAvailableBranches(*Home, -0.5f);
    TestEqual(TEXT("Low Respect: two branches available (offer + bad)"), LowRespect.Num(), 2);

    // Нейтральный Respect (0.1) -- ни один именной порог не пройден, доступна
    // только "всегда" ветка (подношение).
    const TArray<const FDialogueBranch*> NeutralRespect = GetAvailableBranches(*Home, 0.1f);
    TestEqual(TEXT("Neutral Respect: only the always-available branch"), NeutralRespect.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_GoodStandingBranchLeadsToItsOwnNode,
    "Herbalist.Dialogue.GoodStandingBranchLeadsToItsOwnNode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_GoodStandingBranchLeadsToItsOwnNode::RunTest(const FString& Parameters)
{
    const FDialogueDefinition* Def = FindDialogueDefinition(FName(TEXT("Домовой")));
    if (!TestNotNull(TEXT("Домовой dialogue found"), Def)) return false;
    const FDialogueNode* Home = FindDialogueNode(*Def, FName(TEXT("Home")));
    if (!TestNotNull(TEXT("Home node found"), Home)) return false;

    const TArray<const FDialogueBranch*> HighRespect = GetAvailableBranches(*Home, 0.8f);
    const FDialogueBranch* GoodBranch = nullptr;
    for (const FDialogueBranch* Branch : HighRespect)
    {
        if (Branch->NextNodeID == FName(TEXT("GoodStanding"))) GoodBranch = Branch;
    }
    if (!TestNotNull(TEXT("A branch leading to GoodStanding exists at high Respect"), GoodBranch)) return false;

    const FDialogueNode* GoodNode = FindDialogueNode(*Def, GoodBranch->NextNodeID);
    if (TestNotNull(TEXT("GoodStanding node resolves"), GoodNode))
    {
        TestTrue(TEXT("GoodStanding has a non-empty speaker line"), !GoodNode->SpeakerLine.IsEmpty());
        TestEqual(TEXT("GoodStanding is a dead end (no further branches, conversation ends)"), GoodNode->Branches.Num(), 0);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_MilkOfferingBranchIsFlaggedSymbolic,
    "Herbalist.Dialogue.MilkOfferingBranchIsFlaggedSymbolic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_MilkOfferingBranchIsFlaggedSymbolic::RunTest(const FString& Parameters)
{
    // Регрессия на DomovoiMilkOfferingPatchCommandlet (2026-09-06, решение
    // пользователя: бесплатный символический жест) -- "Оставить у печи
    // блюдце молока" обязана нести bIsSymbolicOffering=true в живой
    // DT_Dialogue, иначе AHerbalistPlayerController::ChooseDialogueBranch
    // молча не даст никакого эффекта, как и было до этой находки.
    const FDialogueDefinition* Def = FindDialogueDefinition(FName(TEXT("Домовой")));
    if (!TestNotNull(TEXT("Домовой dialogue found"), Def)) return false;
    const FDialogueNode* Home = FindDialogueNode(*Def, FName(TEXT("Home")));
    if (!TestNotNull(TEXT("Home node found"), Home)) return false;

    const FDialogueBranch* OfferBranch = nullptr;
    for (const FDialogueBranch& Branch : Home->Branches)
    {
        if (Branch.ActionText.ToString() == TEXT("Оставить у печи блюдце молока")) OfferBranch = &Branch;
    }
    if (!TestNotNull(TEXT("Milk-offering branch exists"), OfferBranch)) return false;

    TestTrue(TEXT("Milk-offering branch is flagged as a symbolic offering"), OfferBranch->bIsSymbolicOffering);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_ChoosingSymbolicOfferingBranchRaisesRespect,
    "Herbalist.Dialogue.ChoosingSymbolicOfferingBranchRaisesRespect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_ChoosingSymbolicOfferingBranchRaisesRespect::RunTest(const FString& Parameters)
{
    // Решение пользователя 2026-09-06: бесплатный символический жест
    // ("оставить у печи блюдце молока") сам по себе -- подношение, без
    // предмета. Реальный эффект (Respect += SymbolicOfferingRespectGain)
    // живёт в ChooseDialogueBranch, не в данных дерева -- нужен полный мир.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    Manager->RegisterDomovoi(FIntPoint(3, 3));
    FEntityLandmark* Landmark = Manager->FindLandmarkAt(FIntPoint(3, 3));
    if (!TestNotNull(TEXT("Домовой registered"), Landmark)) { Manager->Destroy(); PC->Destroy(); return false; }
    const float RespectBefore = Landmark->Respect;

    PC->TalkTo(3, 3);
    // "Оставить у печи блюдце молока" -- MinGate=-1/MaxGate=1, всегда
    // первая доступная ветка независимо от текущего Respect.
    PC->ChooseDialogueBranch(0);

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float ExpectedGain = Settings ? Settings->SymbolicOfferingRespectGain : 0.03f;
    TestTrue(TEXT("Respect rose by roughly SymbolicOfferingRespectGain"),
        FMath::IsNearlyEqual(Landmark->Respect, RespectBefore + ExpectedGain, 0.0005f));

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_ZmeyGorynychDefinitionHasFightAndDealBranches,
    "Herbalist.Dialogue.ZmeyGorynychDefinitionHasFightAndDealBranches",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_ZmeyGorynychDefinitionHasFightAndDealBranches::RunTest(const FString& Parameters)
{
    // Регрессия на KalinovMostDialogueAppendCommandlet (§4.4, 2026-09-06) --
    // без этого ряда в живой DT_Dialogue AHerbalistPlayerController::TalkTo
    // молча откажет ("has no dialogue tree yet"), а не упадёт.
    const FDialogueDefinition* Def = FindDialogueDefinition(FName(TEXT("ЗмейГорыныч")));
    if (!TestNotNull(TEXT("ЗмейГорыныч has a registered dialogue"), Def)) return false;

    const FDialogueNode* Start = FindDialogueNode(*Def, Def->StartNodeID);
    if (!TestNotNull(TEXT("Start node resolves"), Start)) return false;

    TestEqual(TEXT("Exactly two branches: Fight and Deal"), Start->Branches.Num(), 2);

    bool bHasFight = false, bHasDeal = false;
    for (const FDialogueBranch& Branch : Start->Branches)
    {
        if (Branch.bIsKalinovMostFight) bHasFight = true;
        if (Branch.bIsKalinovMostDeal) bHasDeal = true;
    }
    TestTrue(TEXT("One branch is flagged as the fight"), bHasFight);
    // Регрессия на KalinovMostDealPatchCommandlet (2026-09-06) -- без него
    // выбор "Сделка" молча ничего не вооружает, тем же классом пробела,
    // что раньше был у "блюдца молока" до DomovoiMilkOfferingPatchCommandlet.
    TestTrue(TEXT("The other branch is flagged as the deal"), bHasDeal);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_ChoosingKalinovMostFightBranchCostsPurityAndStability,
    "Herbalist.Dialogue.ChoosingKalinovMostFightBranchCostsPurityAndStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_ChoosingKalinovMostFightBranchCostsPurityAndStability::RunTest(const FString& Parameters)
{
    // Решение пользователя 2026-09-06: "бой или сделка" через уже
    // существующую диалоговую систему, не через боевую систему, которой в
    // проекте нет. Ветка "Бой" бьёт по Purity/Stability клетки Змея, эффект
    // живёт в ChooseDialogueBranch -- нужен полный мир, тот же приём, что
    // уже FHerbalistDialogue_ChoosingSymbolicOfferingBranchRaisesRespect.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    Manager->RegisterZmeyGorynych(FIntPoint(4, 4));
    FGridCell* Cell = Manager->GetCell(4, 4);
    if (!TestNotNull(TEXT("Cell (4,4) exists"), Cell)) { Manager->Destroy(); PC->Destroy(); return false; }
    Cell->State.Meta.Purity = 0.8f;
    Cell->State.Meta.Stability = 0.8f;

    PC->TalkTo(4, 4);
    // "Вступить в бой со Змеем" -- MinGate=-1/MaxGate=1, первая ветка узла.
    PC->ChooseDialogueBranch(0);

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float ExpectedCost = Settings ? Settings->KalinovMostFightCost : 0.3f;
    TestTrue(TEXT("Purity dropped by roughly KalinovMostFightCost"),
        FMath::IsNearlyEqual(Cell->State.Meta.Purity, 0.8f - ExpectedCost, 0.0005f));
    TestTrue(TEXT("Stability dropped by roughly KalinovMostFightCost"),
        FMath::IsNearlyEqual(Cell->State.Meta.Stability, 0.8f - ExpectedCost, 0.0005f));

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_ChoosingKalinovMostDealArmsItAndTollRemovesArtifact,
    "Herbalist.Dialogue.ChoosingKalinovMostDealArmsItAndTollRemovesArtifact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_ChoosingKalinovMostDealArmsItAndTollRemovesArtifact::RunTest(const FString& Parameters)
{
    // Решение пользователя 2026-09-06 (DESIGN_POI_Art_And_LevelDesign.md):
    // цена Сделки -- один из уже добытых артефактов, по выбору странника.
    // Выбор ветки только вооружает сделку, PayKalinovMostToll завершает её.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    // Артефакт уже во владении -- напрямую, минуя весь путь добычи
    // (TryAcquireArtifact требует проявленную Легендарную сущность,
    // не относящийся к этому тесту путь).
    FAcquiredArtifact Mirror;
    Mirror.ArtifactID = FName(TEXT("Зеркальце"));
    Manager->SetAcquiredArtifacts({ Mirror });

    Manager->RegisterZmeyGorynych(FIntPoint(6, 6));
    TestFalse(TEXT("Sanity: deal not armed yet"), Manager->IsKalinovMostDealPending());

    PC->TalkTo(6, 6);
    // "Откупиться подношением, пройти без боя" -- MinGate=-1/MaxGate=1,
    // вторая ветка узла.
    PC->ChooseDialogueBranch(1);
    TestTrue(TEXT("Choosing the Deal branch arms it"), Manager->IsKalinovMostDealPending());

    // Отказ до вооружения/несуществующим артефактом -- ничего не списывает.
    TestFalse(TEXT("Paying with an unowned artifact fails"), Manager->TryPayKalinovMostToll(FName(TEXT("Клубочек"))));
    TestEqual(TEXT("Still one artifact -- failed payment changed nothing"), Manager->GetAcquiredArtifacts().Num(), 1);

    PC->PayKalinovMostToll(FName(TEXT("Зеркальце")));
    TestEqual(TEXT("Artifact given up as toll"), Manager->GetAcquiredArtifacts().Num(), 0);
    TestFalse(TEXT("Deal no longer pending after payment"), Manager->IsKalinovMostDealPending());

    Manager->Destroy();
    PC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDialogue_PayingKalinovMostTollWithoutArmingFails,
    "Herbalist.Dialogue.PayingKalinovMostTollWithoutArmingFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDialogue_PayingKalinovMostTollWithoutArmingFails::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FAcquiredArtifact Mirror;
    Mirror.ArtifactID = FName(TEXT("Зеркальце"));
    Manager->SetAcquiredArtifacts({ Mirror });

    TestFalse(TEXT("Toll fails -- no deal was ever armed"), Manager->TryPayKalinovMostToll(FName(TEXT("Зеркальце"))));
    TestEqual(TEXT("Artifact untouched"), Manager->GetAcquiredArtifacts().Num(), 1);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
