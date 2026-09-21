// Source/ProjectHerbalistTests/Private/Tests/DiegeticSpeechTest.cpp
//
// Диегетический интерфейс, этап 5а (DESIGN_Diegetic_Interface.md,
// 2026-09-21): речь хозяев. Реплика -- субтитром и записью в Травник, ответ
// -- строкой выбора: колесо двигает, взаимодействие выбирает (решения
// пользователя). Заговорить -- взглядом на видимый силуэт хозяина.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/LandmarkEntityActor.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "Player/HerbalistPlayerController.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    int32 CountSpeechEntries(const AHerbalistPlayerController* PC)
    {
        int32 Count = 0;
        for (const FJournalEntry& Entry : PC->JournalComponent->GetEntries())
        {
            Count += Entry.Type == EJournalEntryType::HostSpeech ? 1 : 0;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_ChoiceLineMovesWrapsAndConfirms,
    "Herbalist.Diegetic.ChoiceLineMovesWrapsAndConfirms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_ChoiceLineMovesWrapsAndConfirms::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    int32 Chosen = INDEX_NONE;
    PC->BeginChoice(TEXT("Кто идёт?"), { TEXT("Свой"), TEXT("Чужой"), TEXT("Молчать") }, [&Chosen](int32 Index) { Chosen = Index; });
    TestTrue(TEXT("Идёт выбор"), PC->IsChoosing());
    TestEqual(TEXT("Сначала первый"), PC->GetChoiceSelected(), 0);

    PC->MoveChoice(1);
    TestEqual(TEXT("Колесо вниз -- второй"), PC->GetChoiceSelected(), 1);
    PC->MoveChoice(-2);
    TestEqual(TEXT("Вверх с первого -- на последний"), PC->GetChoiceSelected(), 2);

    // Взаимодействие выбирает (Interact при идущем выборе зовёт ConfirmChoice).
    PC->ConfirmChoice();
    TestEqual(TEXT("Выбран последний"), Chosen, 2);
    TestFalse(TEXT("Выбор закрыт"), PC->IsChoosing());

    // Клавиша котомки -- уйти, ничего не выбрав.
    Chosen = INDEX_NONE;
    PC->BeginChoice(TEXT("Ещё?"), { TEXT("Да") }, [&Chosen](int32 Index) { Chosen = Index; });
    PC->Inventory();
    TestFalse(TEXT("Ушёл"), PC->IsChoosing());
    TestEqual(TEXT("Ничего не выбрано"), Chosen, (int32)INDEX_NONE);

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_HostSpeaksInSubtitlesAndTheHerbal,
    "Herbalist.Diegetic.HostSpeaksInSubtitlesAndTheHerbal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_HostSpeaksInSubtitlesAndTheHerbal::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const FIntPoint Cell(3, 3);
    Manager->RegisterDomovoi(Cell);
    FEntityLandmark* Landmark = Manager->FindLandmarkAt(Cell);
    if (!TestNotNull(TEXT("Домовой на месте"), Landmark)) { PC->Destroy(); Manager->Destroy(); return false; }
    const float RespectBefore = Landmark->Respect;

    // Силуэт виден -- взгляд и взаимодействие начинают разговор.
    ALandmarkEntityActor* Silhouette = World->SpawnActor<ALandmarkEntityActor>(ALandmarkEntityActor::StaticClass(),
        Manager->GetCellWorldPosition(Cell.X, Cell.Y), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Силуэт создан"), Silhouette)) { PC->Destroy(); Manager->Destroy(); return false; }
    Silhouette->Init(Landmark->EntityID, Cell, Manager);

    const int32 SpeechBefore = CountSpeechEntries(PC);
    Silhouette->OnInteract_Implementation(PC);
    TestTrue(TEXT("Разговор начат -- строка выбора"), PC->IsChoosing());
    TestTrue(FString::Printf(TEXT("Субтитр называет хозяина: %s"), *PC->GetChoicePrompt()), PC->GetChoicePrompt().Contains(Landmark->EntityID.ToString()));
    TestEqual(TEXT("Реплика записана в Травник"), CountSpeechEntries(PC), SpeechBefore + 1);

    // Первая ветка Домового -- блюдце молока, символическое подношение:
    // выбор строкой идёт тем же ChooseDialogueBranch.
    PC->ConfirmChoice();
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float ExpectedGain = Settings ? Settings->SymbolicOfferingRespectGain : 0.03f;
    TestTrue(TEXT("Выбор строкой -- тот же путь, что команда"), FMath::IsNearlyEqual(Landmark->Respect, RespectBefore + ExpectedGain, 0.0005f));

    // Уйти в любой момент -- клавишей котомки.
    if (PC->IsChoosing())
    {
        PC->Inventory();
    }
    TestFalse(TEXT("Разговор окончен"), PC->IsChoosing());

    // Силуэт скрыт -- заговорить нельзя.
    Silhouette->SetActorHiddenInGame(true);
    Silhouette->OnInteract_Implementation(PC);
    TestFalse(TEXT("Невидимому не отвечают"), PC->IsChoosing());

    // Ревью 2026-09-21: и погашенному мешу тоже.
    Silhouette->SetActorHiddenInGame(false);
    if (UStaticMeshComponent* Mesh = Silhouette->FindComponentByClass<UStaticMeshComponent>())
    {
        Mesh->SetVisibility(false);
    }
    Silhouette->OnInteract_Implementation(PC);
    TestFalse(TEXT("Погашенному силуэту не отвечают"), PC->IsChoosing());

    // Большой шаг колеса не уводит выделение в минус.
    PC->BeginChoice(TEXT("?"), { TEXT("a"), TEXT("b"), TEXT("c") }, [](int32) {});
    PC->MoveChoice(-7);
    TestTrue(TEXT("Выделение в пределах"), PC->GetChoiceSelected() >= 0 && PC->GetChoiceSelected() < 3);
    PC->CancelChoice();

    Silhouette->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
