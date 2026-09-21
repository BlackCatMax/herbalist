// Source/ProjectHerbalistTests/Private/Tests/DiegeticCauldronTest.cpp
//
// Диегетический интерфейс, этап 3 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): котёл без окна. Трава и вода кладутся из руки по одной
// порции, пустой рукой котёл мешают; совпали заложенное, вода и час с шагом
// ритуала -- идёт ритуал, иначе обычная варка тем же путём, что было у окна.

#include "Core/World/GridWorldManager.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeCauldronPortion(FName ID, float CreationTime)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = 1;
        Item.CreationTime = CreationTime;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 1.0f;
        Item.State.Meta.Purity = 0.5f;
        Item.State.Meta.Stability = 0.5f;
        Item.State.Meta.Distortion = 0.3f;
        Item.State.Meta.Corruption = 0.1f;
        return Item;
    }

    FInventoryItem MakeCauldronPortionWater(FName ID, float CreationTime)
    {
        FInventoryItem Item = MakeCauldronPortion(ID, CreationTime);
        Item.bIsWater = true;
        Item.State.Direction.Body = Item.State.Direction.Mind = Item.State.Direction.Spirit = Item.State.Direction.Nature = 0.25f;
        return Item;
    }

    // Дневной час -- не рассвет, не закат, не ночь: ни один шаг ритуала не
    // подходит (RitualBrewingTest.cpp, те же моменты).
    const double CauldronDayMoment = 700.0;
    const double CauldronDuskMoment = 1300.0;

    struct FCauldronRig
    {
        AGridWorldManager* Manager = nullptr;
        AAlchemyTableActor* Table = nullptr;
        AHerbalistPlayerController* PC = nullptr;

        void Destroy()
        {
            if (PC) PC->Destroy();
            if (Table) Table->Destroy();
            if (Manager)
            {
                Manager->ActiveRituals.Reset();
                Manager->Destroy();
            }
        }
    };

    bool SpawnCauldronRig(FAutomationTestBase& Test, UWorld* World, FCauldronRig& Rig)
    {
        // Чужие столы из других тестов в этом мире не нужны.
        for (TActorIterator<AAlchemyTableActor> It(World); It; ++It)
        {
            It->Destroy();
        }
        Rig.Manager = SpawnAndBeginPlay(World);
        if (!Test.TestNotNull(TEXT("Manager spawned"), Rig.Manager)) return false;
        Rig.Table = World->SpawnActor<AAlchemyTableActor>(AAlchemyTableActor::StaticClass(),
            Rig.Manager->GetCellWorldPosition(4, 4), FRotator::ZeroRotator);
        if (!Test.TestNotNull(TEXT("Table spawned"), Rig.Table)) { Rig.Destroy(); return false; }
        Rig.Table->DispatchBeginPlay();
        Rig.PC = SpawnControllerAndBeginPlay(World, Rig.Manager);
        if (!Test.TestNotNull(TEXT("Controller spawned"), Rig.PC)) { Rig.Destroy(); return false; }
        return true;
    }

    int32 CauldronIndexOf(const AHerbalistPlayerController* PC, const FInventoryItem& Item)
    {
        return PC->InventoryComponent->FindItemIndex(Item);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_CauldronTakesPortionsFromTheHand,
    "Herbalist.Diegetic.CauldronTakesPortionsFromTheHand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_CauldronTakesPortionsFromTheHand::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FCauldronRig Rig;
    if (!SpawnCauldronRig(*this, World, Rig)) return false;
    AHerbalistPlayerController* PC = Rig.PC;
    AAlchemyTableActor* Table = Rig.Table;

    const FInventoryItem Herb = MakeCauldronPortion(FName(TEXT("bol_01")), 41.0f);
    PC->InventoryComponent->AddItem(Herb, 2);

    // Рука держит горсть из двух -- в котёл уходит одна порция, рука не пустеет.
    TestTrue(TEXT("Взял траву"), PC->HeldItemComponent->TakeFromInventory(CauldronIndexOf(PC, Herb)));
    TestTrue(TEXT("Котёл принял"), Table->ReceiveHeldItem(PC, PC->HeldItemComponent->ResolveHeldIndex()));
    TestEqual(TEXT("В котле одна порция"), Table->GetContents().Num(), 1);
    const int32 LeftIndex = CauldronIndexOf(PC, Herb);
    TestTrue(TEXT("В котомке осталась одна"), LeftIndex != INDEX_NONE && PC->InventoryComponent->GetItems()[LeftIndex].Count == 1);
    TestEqual(TEXT("Рука держит ту же горсть"), PC->HeldItemComponent->ResolveHeldIndex(), LeftIndex);

    // Вода -- одна; вторая остаётся в котомке.
    const FInventoryItem Water = MakeCauldronPortionWater(FName(TEXT("BogWater")), 42.0f);
    const FInventoryItem SecondWater = MakeCauldronPortionWater(FName(TEXT("Water")), 43.0f);
    PC->InventoryComponent->AddItem(Water, 1);
    PC->InventoryComponent->AddItem(SecondWater, 1);
    Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Water));
    TestEqual(TEXT("Вода легла"), Table->GetContents().Num(), 2);
    TestTrue(TEXT("Вода помечена водой -- ритуал ищет её по флагу"), Table->GetContents()[1].bIsWater);
    TestTrue(TEXT("Отказ -- тоже ответ цели"), Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, SecondWater)));
    TestEqual(TEXT("Вторая вода не легла"), Table->GetContents().Num(), 2);
    TestTrue(TEXT("Вторая вода в котомке"), CauldronIndexOf(PC, SecondWater) != INDEX_NONE);

    // Трав -- три, как было слотов у окна.
    for (int32 i = 0; i < 3; ++i)
    {
        const FInventoryItem Extra = MakeCauldronPortion(FName(*FString::Printf(TEXT("cauldron_extra_%d"), i)), 50.0f + i);
        PC->InventoryComponent->AddItem(Extra, 1);
        Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Extra));
    }
    TestEqual(TEXT("Одна вода и три травы"), Table->GetContents().Num(), 1 + AAlchemyTableActor::MaxHerbPortions);
    TestEqual(TEXT("Порядок закладки -- порядок жестов"), Table->GetContents()[0].IngredientID, FName(TEXT("bol_01")));

    PC->HeldItemComponent->PutAway();
    Rig.Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_StirringBrewsThroughTheCauldronPath,
    "Herbalist.Diegetic.StirringBrewsThroughTheCauldronPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_StirringBrewsThroughTheCauldronPath::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FCauldronRig Rig;
    if (!SpawnCauldronRig(*this, World, Rig)) return false;
    AHerbalistPlayerController* PC = Rig.PC;
    AAlchemyTableActor* Table = Rig.Table;
    Rig.Manager->SetGameClockSeconds(CauldronDayMoment);

    TestTrue(TEXT("Пустой котёл мешать нечего"), Table->Stir(PC) == ECauldronStirResult::Empty);

    const FInventoryItem Herb = MakeCauldronPortion(FName(TEXT("bol_01")), 61.0f);
    const FInventoryItem Water = MakeCauldronPortionWater(FName(TEXT("Water")), 62.0f);
    PC->InventoryComponent->AddItem(Herb, 1);
    PC->InventoryComponent->AddItem(Water, 1);
    Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Herb));
    Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Water));

    int32 Produced = 0;
    const FDelegateHandle Handle = Rig.Manager->OnBrewCompleted.AddLambda([&Produced](const FInventoryItem&, const FIntPoint&) { ++Produced; });

    // Взаимодействие пустой рукой -- то же «помешать».
    PC->HeldItemComponent->PutAway();
    Table->OnInteract_Implementation(PC);
    TestEqual(TEXT("Котёл опустел"), Table->GetContents().Num(), 0);
    TestTrue(TEXT("Ритуала днём нет"), !Rig.Manager->ActiveRituals.Contains(Table->GetGridCoords()));

    // Варка -- командой в очереди менеджера, как у окна: результат приходит
    // шагом симуляции.
    for (int32 Step = 0; Step < 5 && Produced == 0; ++Step)
    {
        Rig.Manager->Tick(0.1f);
    }
    TestEqual(TEXT("Сварилось одно"), Produced, 1);

    Rig.Manager->OnBrewCompleted.Remove(Handle);
    Rig.Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_StirringAtTheRightHourIsARitualStep,
    "Herbalist.Diegetic.StirringAtTheRightHourIsARitualStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_StirringAtTheRightHourIsARitualStep::RunTest(const FString& Parameters)
{
    // Закатный шаг «Заревой воды»: Багульник и Сон-трава в болотной воде
    // (RitualBrewingTest.cpp). Игрок не выбирает «ритуал» -- он делает
    // правильно, и помешать значит шаг.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FCauldronRig Rig;
    if (!SpawnCauldronRig(*this, World, Rig)) return false;
    AHerbalistPlayerController* PC = Rig.PC;
    AAlchemyTableActor* Table = Rig.Table;
    Rig.Manager->ActiveRituals.Reset();
    Rig.Manager->SetGameClockSeconds(CauldronDuskMoment);

    const FInventoryItem Bagulnik = MakeCauldronPortion(FName(TEXT("bol_01")), 71.0f);
    const FInventoryItem SonTrava = MakeCauldronPortion(FName(TEXT("les_06")), 72.0f);
    const FInventoryItem BogWater = MakeCauldronPortionWater(FName(TEXT("BogWater")), 73.0f);
    for (const FInventoryItem& Item : { Bagulnik, SonTrava, BogWater })
    {
        PC->InventoryComponent->AddItem(Item, 1);
        Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Item));
    }
    TestEqual(TEXT("Заложено трое"), Table->GetContents().Num(), 3);

    TestTrue(TEXT("Шаг ритуала принят"), Table->Stir(PC) == ECauldronStirResult::RitualProgressed);
    TestTrue(TEXT("Ритуал идёт на клетке котла"), Rig.Manager->ActiveRituals.Contains(Table->GetGridCoords()));
    TestEqual(TEXT("Котёл опустел"), Table->GetContents().Num(), 0);

    Rig.Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_FullBagLeavesTheCauldronFull,
    "Herbalist.Diegetic.FullBagLeavesTheCauldronFull",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_FullBagLeavesTheCauldronFull::RunTest(const FString& Parameters)
{
    // Урок окна варки (ревью 2026-09-14): полная котомка -- травы уже в
    // котле, зелье пропало бы. Помешать нельзя, заложенное ждёт.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FCauldronRig Rig;
    if (!SpawnCauldronRig(*this, World, Rig)) return false;
    AHerbalistPlayerController* PC = Rig.PC;
    AAlchemyTableActor* Table = Rig.Table;
    Rig.Manager->SetGameClockSeconds(CauldronDayMoment);

    const FInventoryItem Herb = MakeCauldronPortion(FName(TEXT("bol_01")), 81.0f);
    PC->InventoryComponent->AddItem(Herb, 1);
    Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Herb));

    for (int32 i = 0; PC->InventoryComponent->GetItems().Num() < PC->InventoryComponent->MaxSlots && i < 40; ++i)
    {
        PC->InventoryComponent->AddItem(MakeCauldronPortion(FName(*FString::Printf(TEXT("cauldron_fill_%d"), i)), 100.0f + i), 1);
    }
    TestTrue(TEXT("Котомка полна"), Table->Stir(PC) == ECauldronStirResult::BagFull);
    TestEqual(TEXT("Заложенное в котле"), Table->GetContents().Num(), 1);

    Rig.Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_FullBagDoesNotStealTheRitualHour,
    "Herbalist.Diegetic.FullBagDoesNotStealTheRitualHour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_FullBagDoesNotStealTheRitualHour::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: шагу ритуала место в котомке не нужно, а час у него
    // один. Полная котомка не мешает шагу; мешает только обычной варке.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    FCauldronRig Rig;
    if (!SpawnCauldronRig(*this, World, Rig)) return false;
    AHerbalistPlayerController* PC = Rig.PC;
    AAlchemyTableActor* Table = Rig.Table;
    Rig.Manager->ActiveRituals.Reset();
    Rig.Manager->SetGameClockSeconds(CauldronDuskMoment);

    const FInventoryItem Bagulnik = MakeCauldronPortion(FName(TEXT("bol_01")), 91.0f);
    const FInventoryItem SonTrava = MakeCauldronPortion(FName(TEXT("les_06")), 92.0f);
    const FInventoryItem BogWater = MakeCauldronPortionWater(FName(TEXT("BogWater")), 93.0f);
    for (const FInventoryItem& Item : { Bagulnik, SonTrava, BogWater })
    {
        PC->InventoryComponent->AddItem(Item, 1);
        Table->ReceiveHeldItem(PC, CauldronIndexOf(PC, Item));
    }
    for (int32 i = 0; PC->InventoryComponent->GetItems().Num() < PC->InventoryComponent->MaxSlots && i < 40; ++i)
    {
        PC->InventoryComponent->AddItem(MakeCauldronPortion(FName(*FString::Printf(TEXT("cauldron_ritual_fill_%d"), i)), 200.0f + i), 1);
    }

    TestTrue(TEXT("Шаг принят при полной котомке"), Table->Stir(PC) == ECauldronStirResult::RitualProgressed);
    TestTrue(TEXT("Ритуал идёт"), Rig.Manager->ActiveRituals.Contains(Table->GetGridCoords()));

    Rig.Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
