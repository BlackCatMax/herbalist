// Source/ProjectHerbalistTests/Private/Tests/SaveAuditFixesTest.cpp
//
// Пакет 1 аудита кода 2026-09-26 (docs/audits/AUDIT_CODE_2026-09-26.md):
// Б1 -- начатый ритуал переживает сейв, Б2 -- сейв клетки не пишет чужие
// ресурсы, Б3 -- запись сейва без игры не выдаётся за успех.
//
// Загрузку с диска (UHerbalistSaveSubsystem::LoadGame) в мире редактора не
// вызвать -- нет UGameInstance (довод в SaveSystemTest.cpp), поэтому ритуал
// проходит через сериализацию сейва в память, как в
// SaveLayoutCompatibilityTest.cpp.

#include "Core/World/GridWorldManager.h"
#include "Core/Alchemy/RitualTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Player/HerbalistPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeSaveAuditHerb(const TCHAR* ID)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(ID);
        Item.Count = 1;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 1.f;
        Item.State.Meta.Distortion = 0.3f;
        Item.State.Meta.Stability = 0.5f;
        Item.State.Meta.Purity = 0.5f;
        Item.State.Meta.Corruption = 0.1f;
        return Item;
    }

    FInventoryItem MakeSaveAuditBogWater()
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("BogWater"));
        Item.Count = 1;
        Item.bIsWater = true;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = Item.State.Direction.Mind = Item.State.Direction.Spirit = Item.State.Direction.Nature = 0.25f;
        Item.State.Meta.Purity = 0.4f;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveAudit_StartedRitualSurvivesTheSave,
    "Herbalist.SaveAudit.StartedRitualSurvivesTheSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveAudit_StartedRitualSurvivesTheSave::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const double Day = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0;
    const FIntPoint Cauldron(5, 5);

    // Заревая вода, шаг заката: Багульник, Сон-трава, болотная вода.
    Manager->SetGameClockSeconds(1300.0);
    TArray<FInventoryItem> DuskStep = { MakeSaveAuditHerb(TEXT("bol_01")), MakeSaveAuditHerb(TEXT("les_06")), MakeSaveAuditBogWater() };
    FRandomStream DuskRng(1);
    FInventoryItem Unused;
    if (!TestTrue(TEXT("Шаг заката принят"), Manager->TryAdvanceRitual(Cauldron, DuskStep, DuskRng, Unused) == ERitualStepResult::Progressed))
    {
        Manager->Destroy();
        return false;
    }

    // Сон на лавке: сейв -- на диск (здесь в память) -- загрузка.
    UHerbalistSaveGame* Save = NewObject<UHerbalistSaveGame>();
    Save->ActiveRituals = Manager->ActiveRituals;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Сейв записан в память"), UGameplayStatics::SaveGameToMemory(Save, Bytes)))
    {
        Manager->Destroy();
        return false;
    }
    const UHerbalistSaveGame* Loaded = Cast<UHerbalistSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Сейв прочитан"), Loaded))
    {
        Manager->Destroy();
        return false;
    }

    // Новая сессия: ритуалов нет, пока загрузка их не вернула.
    Manager->ActiveRituals.Reset();
    Manager->ActiveRituals = Loaded->ActiveRituals;
    const FActiveRitualState* Restored = Manager->ActiveRituals.Find(Cauldron);
    if (TestNotNull(TEXT("Ритуал у котла вернулся"), Restored))
    {
        TestEqual(TEXT("Тот же рецепт"), Restored->RecipeID, FName(TEXT("ZarevayaVoda")));
        TestEqual(TEXT("Один шаг принят"), Restored->CompletedSteps, 1);
        TestEqual(TEXT("Потраченные травы и вода при нём"), Restored->AccumulatedIngredients.Num(), 3);
    }

    // Рассвет следующих суток: второй шаг завершает ритуал.
    Manager->SetGameClockSeconds(Day + 100.0);
    TArray<FInventoryItem> DawnStep = { MakeSaveAuditHerb(TEXT("riv_06")) };
    FRandomStream DawnRng(2);
    FInventoryItem Potion;
    TestTrue(TEXT("После загрузки ритуал доводится до конца"),
        Manager->TryAdvanceRitual(Cauldron, DawnStep, DawnRng, Potion) == ERitualStepResult::Completed);
    TestEqual(TEXT("Результат -- зелье"), Potion.IngredientID, FName(TEXT("Potion")));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveAudit_CellSaveSkipsForeignResources,
    "Herbalist.SaveAudit.CellSaveSkipsForeignResources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveAudit_CellSaveSkipsForeignResources::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* Cell = Manager->GetCell(3, 3);
    if (!TestNotNull(TEXT("Клетка есть"), Cell)) { Manager->Destroy(); return false; }

    int32 GridOwned = 0;
    for (const TWeakObjectPtr<AHerbalistResourceActor>& Resource : Cell->ResourceActors)
    {
        if (Resource.IsValid() && Resource->WasSpawnedByGrid()) ++GridOwned;
    }

    // Ресурс «от PCG-графа»: Init() не звался, на клетке регистрируется сам.
    AHerbalistResourceActor* Foreign = World->SpawnActor<AHerbalistResourceActor>(
        AHerbalistResourceActor::StaticClass(), Manager->GetCellWorldPosition(3, 3), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Чужой ресурс создан"), Foreign)) { Manager->Destroy(); return false; }
    Foreign->SetWorldManager(Manager);
    Foreign->DispatchBeginPlay();
    TestFalse(TEXT("Он не от сетки"), Foreign->WasSpawnedByGrid());
    TestTrue(TEXT("Но стоит на клетке"), Cell->ResourceActors.Contains(Foreign));

    // Клетка тронута -- попадает в сейв.
    Manager->MarkCellDirtyForTests(3, 3);
    const TArray<FSavedCellState> SavedCells = Manager->CaptureSaveCells();
    const FSavedCellState* Saved = SavedCells.FindByPredicate([](const FSavedCellState& Entry) { return Entry.X == 3 && Entry.Y == 3; });
    if (TestNotNull(TEXT("Клетка в сейве"), Saved))
    {
        TestEqual(TEXT("В сейве клетки -- только выращенные сеткой"), Saved->ResourceIngredientIDs.Num(), GridOwned);
        TestEqual(TEXT("Места -- столько же"), Saved->ResourceSlots.Num(), GridOwned);
    }

    Foreign->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistSaveAudit_SaveWithoutGameIsNotASuccess,
    "Herbalist.SaveAudit.SaveWithoutGameIsNotASuccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistSaveAudit_SaveWithoutGameIsNotASuccess::RunTest(const FString& Parameters)
{
    // Мир редактора без UGameInstance: записать некуда -- TrySaveGame обязан
    // сказать «нет», а сон -- состояться (проснуться), не выдав это за сейв.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    TestFalse(TEXT("Без игры сохранение -- не успех"), PC->TrySaveGame());
    TestTrue(TEXT("Сон всё равно состоялся"), PC->SleepUntilDawn());

    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif
