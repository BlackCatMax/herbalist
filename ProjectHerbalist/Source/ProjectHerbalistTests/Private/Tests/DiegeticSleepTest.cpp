// Source/ProjectHerbalistTests/Private/Tests/DiegeticSleepTest.cpp
//
// Диегетический интерфейс, этап 6а (DESIGN_Diegetic_Interface.md,
// 2026-09-21): сон вместо меню. Лавка у дома -- проспать до рассвета и
// сохраниться; заложенное в котле переживает сон и загрузку (решения
// пользователя). Автозагрузка при старте работает только в игровом мире --
// в автотестах её нет по построению, проверяется в PIE.

#include "Core/World/GridWorldManager.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Storage/SleepBenchActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Config/HerbalistSettings.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Data/IngredientTableRow.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_SleepingOnTheBenchWakesAtDawn,
    "Herbalist.Diegetic.SleepingOnTheBenchWakesAtDawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_SleepingOnTheBenchWakesAtDawn::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    ASleepBenchActor* Bench = World->SpawnActor<ASleepBenchActor>(ASleepBenchActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Лавка создана"), Bench)) { PC->Destroy(); Manager->Destroy(); return false; }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const double Day = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0;

    // Закат первых суток -- проснуться на рассвете вторых.
    Manager->SetGameClockSeconds(Day * 3.0 + 1300.0);
    Bench->OnInteract_Implementation(PC);
    TestTrue(TEXT("Проснулся на рассвете"), FMath::IsNearlyEqual(Manager->GetGameClockSeconds(), Day * 4.0, 0.01));
    TestTrue(TEXT("Рассвет"), Manager->IsDawn());

    // Лёг ровно на рассвете -- спит до следующего: сон не бывает нулевым.
    TestTrue(TEXT("Сон ещё раз"), PC->SleepUntilDawn());
    TestTrue(TEXT("До следующего рассвета"), FMath::IsNearlyEqual(Manager->GetGameClockSeconds(), Day * 5.0, 0.01));

    Bench->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_CauldronContentsSurviveTheSave,
    "Herbalist.Diegetic.CauldronContentsSurviveTheSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_CauldronContentsSurviveTheSave::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    for (TActorIterator<AAlchemyTableActor> It(World); It; ++It)
    {
        It->Destroy();
    }
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AAlchemyTableActor* Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(),
        Manager->GetCellWorldPosition(4, 4), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Table spawned"), Table)) { Manager->Destroy(); return false; }
    Table->DispatchBeginPlay();
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Table->Destroy(); Manager->Destroy(); return false; }

    FInventoryItem Herb;
    Herb.IngredientID = FName(TEXT("bol_01"));
    Herb.Count = 1;
    Herb.CreationTime = 601.0f;
    Herb.State.Meta.Purity = 0.7f;
    PC->InventoryComponent->AddItem(Herb, 1);
    Table->ReceiveHeldItem(PC, PC->InventoryComponent->FindItemIndex(Herb));

    TArray<FInventoryItem> Contents;
    bool bReady = false;
    FInventoryItem Ready;
    Table->CaptureSaved(Contents, bReady, Ready);
    TestEqual(TEXT("В сохранении -- заложенное"), Contents.Num(), 1);

    // «Загрузка»: котёл пуст, потом восстановлен по сохранённому.
    Table->RestoreSaved({}, false, FInventoryItem());
    TestEqual(TEXT("Пуст"), Table->GetContents().Num(), 0);
    FInventoryItem Reward;
    Reward.IngredientID = FName(TEXT("Зорин-камень"));
    Reward.Count = 1;
    Table->RestoreSaved(Contents, true, Reward);
    TestEqual(TEXT("Заложенное вернулось"), Table->GetContents().Num(), 1);
    TestEqual(TEXT("То самое"), Table->GetContents()[0].IngredientID, Herb.IngredientID);
    TestTrue(TEXT("Результат ритуала ждёт"), Table->HasReadyResult());

    PC->Destroy();
    Table->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_BrushwoodMakesACampAnywhere,
    "Herbalist.Diegetic.BrushwoodMakesACampAnywhere",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_BrushwoodMakesACampAnywhere::RunTest(const FString& Parameters)
{
    // Лагерь (этап 6б, решения пользователя): вязанка хвороста из руки на
    // землю где угодно -- костёр, сон до рассвета, вязанка сгорает.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const double Day = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0;
    Manager->SetGameClockSeconds(Day * 2.0 + 1500.0);

    FInventoryItem Brushwood;
    Brushwood.IngredientID = AHerbalistPlayerController::BrushwoodID();
    Brushwood.Count = 2;
    Brushwood.bSubjectToDecay = false;
    PC->InventoryComponent->AddItem(Brushwood, 2);
    FHitResult Hit;
    Hit.Location = Manager->GetCellWorldPosition(6, 6);
    Hit.ImpactPoint = Hit.Location;

    TestTrue(TEXT("Лагерь разбит"), PC->ApplyHeldItemToGround(PC->InventoryComponent->FindItemIndex(Brushwood), Hit));
    TestEqual(TEXT("Одна вязанка сгорела"), CountItemsWithID(PC->InventoryComponent, Brushwood.IngredientID), 1);
    TestTrue(TEXT("Проснулся на рассвете"), FMath::IsNearlyEqual(Manager->GetGameClockSeconds(), Day * 3.0, 0.01));

    // Ревью 2026-09-21: у логова Легендарной вязанка -- тоже лагерь, не дар.
    if (const FIntPoint* Lair = Manager->GetLegendaryAnchors().Find(FName(TEXT("Индрик-зверь"))))
    {
        FHitResult LairHit;
        LairHit.Location = Manager->GetCellWorldPosition(Lair->X, Lair->Y);
        LairHit.ImpactPoint = LairHit.Location;
        TestTrue(TEXT("У логова -- лагерь"), PC->ApplyHeldItemToGround(PC->InventoryComponent->FindItemIndex(Brushwood), LairHit));
        TestEqual(TEXT("Вторая вязанка сгорела"), CountItemsWithID(PC->InventoryComponent, Brushwood.IngredientID), 0);
        TestTrue(TEXT("И снова рассвет"), FMath::IsNearlyEqual(Manager->GetGameClockSeconds(), Day * 4.0, 0.01));
    }

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_BrushwoodGrowsInTheForest,
    "Herbalist.Diegetic.BrushwoodGrowsInTheForest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_BrushwoodGrowsInTheForest::RunTest(const FString& Parameters)
{
    // Ряд живой таблицы -- тот, что в карточке (BrushwoodAppendCommandlet):
    // лесные биомы, не портится, редкость как у трав.
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_IngredientClass"));
    if (!TestNotNull(TEXT("DT_IngredientClass loads"), Table)) return false;
    const FIngredientTableRow* Row = Table->FindRow<FIngredientTableRow>(AHerbalistPlayerController::BrushwoodID(), TEXT("BrushwoodTest"));
    if (!TestNotNull(TEXT("Ряд «Хворост» есть"), Row)) return false;
    TestTrue(TEXT("Растёт в смешанном лесу"), Row->AllowedBiomes.Contains(EBiomeType::MixedForest));
    TestTrue(TEXT("В широколиственном"), Row->AllowedBiomes.Contains(EBiomeType::BroadleafForest));
    TestTrue(TEXT("В тайге"), Row->AllowedBiomes.Contains(EBiomeType::Taiga));
    TestEqual(TEXT("Не портится"), Row->DecayRate, 0.0f);
    TestEqual(TEXT("Редкость как у трав"), Row->RarityWeight, 1);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
