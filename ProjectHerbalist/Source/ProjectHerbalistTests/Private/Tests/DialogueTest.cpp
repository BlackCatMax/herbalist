// Source/ProjectHerbalistTests/Private/Tests/DialogueTest.cpp
//
// Диалоги (DESIGN_Community_And_Homestead.md §1.1, реализация 2026-08-31) —
// дерево не хранит состояние, читает Respect/Molva и статический реестр
// (Core/Dialogue/HerbalistDialogueTypes.h) — тестируется напрямую, без
// PlayerController/мира, тем же приёмом, что GetLandmarkDefinitions уже
// тестируется опосредованно через LandmarkTest.cpp, но здесь у самого
// движка (фильтрация веток по гейту) прежде не было ни одного теста.

#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

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

#endif // WITH_AUTOMATION_TESTS
