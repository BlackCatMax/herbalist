// Source/ProjectHerbalistTests/Private/Tests/DiegeticHandTest.cpp
//
// Диегетический интерфейс, этап 1 (DESIGN_Diegetic_Interface.md,
// 2026-09-21): рука, осмотр со строкой ощущения, подсветка того, на что
// смотришь. Проверяется логика -- что держится, что сказано, что записано,
// что подсвечено; «удобно ли в руке» -- дело PIE.

#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistSensation.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Community/OrderCacheActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Player/HeldItemComponent.h"
#include "Player/HeldItemActor.h"
#include "Player/LookHighlightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FRealState MakeSensationState(float Body, float Mind, float Spirit, float Nature)
    {
        FRealState State;
        State.Magnitude = 0.5f;
        State.Direction.Body = Body;
        State.Direction.Mind = Mind;
        State.Direction.Spirit = Spirit;
        State.Direction.Nature = Nature;
        State.Meta.Purity = 0.5f;
        State.Meta.Corruption = 0.1f;
        State.Meta.Stability = 0.5f;
        State.Meta.Distortion = 0.2f;
        State.Meta.Potency = 0.5f;
        State.Meta.Resonance = 0.5f;
        return State;
    }

    bool ContainsDigit(const FString& Text)
    {
        for (TCHAR Ch : Text)
        {
            if (FChar::IsDigit(Ch)) return true;
        }
        return false;
    }

    FInventoryItem MakeHandTestHerb(float CreationTime)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("bol_01"));
        Item.Count = 1;
        Item.CreationTime = CreationTime;
        Item.State = MakeSensationState(0.1f, 0.1f, 0.1f, 0.7f);
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_SensationSpeaksWithoutNumbers,
    "Herbalist.Diegetic.SensationSpeaksWithoutNumbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_SensationSpeaksWithoutNumbers::RunTest(const FString& Parameters)
{
    // Куда тянет -- ведущая ось.
    const FString Earthy = HerbalistSensation::Describe(MakeSensationState(0.1f, 0.1f, 0.1f, 0.7f));
    TestTrue(FString::Printf(TEXT("Природа -- пахнет землёй: %s"), *Earthy), Earthy.Contains(TEXT("землёй")));

    const FString Warm = HerbalistSensation::Describe(MakeSensationState(0.7f, 0.1f, 0.1f, 0.1f));
    TestTrue(FString::Printf(TEXT("Тело -- тёплый: %s"), *Warm), Warm.Contains(TEXT("ёплый")));

    // Ровное направление -- ни к чему не тянет.
    const FString Flat = HerbalistSensation::Describe(MakeSensationState(0.25f, 0.25f, 0.25f, 0.25f));
    TestTrue(FString::Printf(TEXT("Ровный: %s"), *Flat), Flat.Contains(TEXT("ровный")) || Flat.Contains(TEXT("Ровный")));

    // Порча заметнее всего остального и идёт первой из свойств.
    FRealState Rotten = MakeSensationState(0.1f, 0.1f, 0.1f, 0.7f);
    Rotten.Meta.Corruption = 0.8f;
    Rotten.Meta.Purity = 0.9f;
    const FString RottenLine = HerbalistSensation::Describe(Rotten);
    TestTrue(FString::Printf(TEXT("Порча слышна: %s"), *RottenLine), RottenLine.Contains(TEXT("гнилью")));
    TestTrue(TEXT("Порча раньше чистоты"),
        RottenLine.Find(TEXT("гнилью")) < RottenLine.Find(TEXT("чистый")) || !RottenLine.Contains(TEXT("чистый")));

    // Чисел нет никогда (07_UX §7.1) -- ни в одной из строк.
    for (const FString& Line : { Earthy, Warm, Flat, RottenLine })
    {
        TestFalse(FString::Printf(TEXT("Без цифр: %s"), *Line), ContainsDigit(Line));
    }
    // Одна фраза: с заглавной и с точкой.
    TestTrue(TEXT("Заканчивается точкой"), Earthy.EndsWith(TEXT(".")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_HandHoldsInspectsAndRecordsInJournal,
    "Herbalist.Diegetic.HandHoldsInspectsAndRecordsInJournal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_HandHoldsInspectsAndRecordsInJournal::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UHeldItemComponent* Hand = PC->HeldItemComponent;
    if (!TestNotNull(TEXT("У контроллера есть рука"), Hand)) { PC->Destroy(); Manager->Destroy(); return false; }

    PC->InventoryComponent->AddItem(MakeHandTestHerb(42.0f));
    const int32 HerbIndex = PC->InventoryComponent->GetItems().IndexOfByPredicate([](const FInventoryItem& Item) { return Item.CreationTime == 42.0f; });
    if (!TestTrue(TEXT("Трава в котомке"), HerbIndex != INDEX_NONE)) { PC->Destroy(); Manager->Destroy(); return false; }

    TestFalse(TEXT("Нет ячейки -- взять нечего"), Hand->TakeFromInventory(999));
    TestFalse(TEXT("Рука пуста"), Hand->IsHolding());

    const int32 BagSizeBefore = PC->InventoryComponent->GetItems().Num();
    TestTrue(TEXT("Взяли траву"), Hand->TakeFromInventory(HerbIndex));
    TestTrue(TEXT("Рука держит"), Hand->IsHolding());
    TestEqual(TEXT("Держит именно её"), Hand->GetHeldItem().IngredientID, FName(TEXT("bol_01")));
    TestEqual(TEXT("Котомка не опустела: предмет в руке не вынут из неё"), PC->InventoryComponent->GetItems().Num(), BagSizeBefore);

    const int32 JournalBefore = PC->JournalComponent->GetEntries().Num();
    Hand->ToggleInspect();
    TestTrue(TEXT("Осматривает"), Hand->IsInspecting());
    TestTrue(FString::Printf(TEXT("Строка ощущения про землю: %s"), *Hand->GetSensationLine()), Hand->GetSensationLine().Contains(TEXT("землёй")));
    TestEqual(TEXT("Осмотр записан в Травник"), PC->JournalComponent->GetEntries().Num(), JournalBefore + 1);
    if (PC->JournalComponent->GetEntries().Num() > JournalBefore)
    {
        const FJournalEntry& Entry = PC->JournalComponent->GetEntries().Last();
        TestTrue(TEXT("Запись -- осмотр"), Entry.Type == EJournalEntryType::Inspection);
        TestEqual(TEXT("В записи та же строка"), Entry.FragmentText.ToString(), Hand->GetSensationLine());
    }

    // Предмет ушёл из котомки (потратили, отдали) -- рука пустеет сама.
    Hand->ToggleInspect();
    PC->InventoryComponent->RemoveItem(Hand->ResolveHeldIndex(), 1);
    Hand->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Предмета нет -- рука пуста"), Hand->IsHolding());

    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_HighlightMarksOnlyInteractableTargets,
    "Herbalist.Diegetic.HighlightMarksOnlyInteractableTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_HighlightMarksOnlyInteractableTargets::RunTest(const FString& Parameters)
{
    // Прицела нет -- вместо него проступает то, с чем можно что-то сделать
    // (решение пользователя 2026-09-21). Подсветка -- Custom Depth со своим
    // трафаретом; как именно проступает, решает материал в редакторе.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    ULookHighlightComponent* Look = PC->LookHighlightComponent;
    if (!TestNotNull(TEXT("У контроллера есть подсветка"), Look)) { PC->Destroy(); Manager->Destroy(); return false; }

    // Тайник -- то, с чем можно взаимодействовать.
    AOrderCacheActor* Cache = World->SpawnActor<AOrderCacheActor>(AOrderCacheActor::StaticClass(), FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    // Держатель предмета -- просто актор, взаимодействовать с ним нельзя.
    AHeldItemActor* Plain = World->SpawnActor<AHeldItemActor>(AHeldItemActor::StaticClass(), FVector(400.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Тайник создан"), Cache) || !TestNotNull(TEXT("Простой актор создан"), Plain))
    {
        PC->Destroy(); Manager->Destroy(); return false;
    }

    auto IsLit = [](AActor* Actor)
    {
        TArray<UPrimitiveComponent*> Primitives;
        Actor->GetComponents<UPrimitiveComponent>(Primitives);
        for (const UPrimitiveComponent* Primitive : Primitives)
        {
            if (Primitive->bRenderCustomDepth && Primitive->CustomDepthStencilValue == ULookHighlightComponent::HighlightStencilValue)
            {
                return true;
            }
        }
        return false;
    };

    Look->SetFocusedActor(Cache);
    TestEqual(TEXT("Тайник в фокусе"), Look->GetFocusedActor(), static_cast<AActor*>(Cache));
    TestTrue(TEXT("Тайник подсвечен"), IsLit(Cache));

    Look->SetFocusedActor(Plain);
    TestTrue(TEXT("Простой актор в фокус не берётся"), Look->GetFocusedActor() == nullptr);
    TestFalse(TEXT("Простой актор не подсвечен"), IsLit(Plain));
    TestFalse(TEXT("С тайника подсветка снята"), IsLit(Cache));

    Cache->Destroy();
    Plain->Destroy();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistDiegetic_HandKeepsAnAgingItem,
    "Herbalist.Diegetic.HandKeepsAnAgingItem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistDiegetic_HandKeepsAnAgingItem::RunTest(const FString& Parameters)
{
    // Ревью 2026-09-21: распад котомки двигает состояние предмета каждую
    // секунду, а рука сравнивала его точно -- и роняла траву прямо при
    // осмотре. Слияние стопок так же сдвигает время создания. Рука должна
    // держать тот же предмет, пока он в котомке, как бы он ни старел.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    AHerbalistPlayerController* PC = SpawnControllerAndBeginPlay(World, Manager);
    if (!TestNotNull(TEXT("Controller spawned"), PC)) { Manager->Destroy(); return false; }
    UHeldItemComponent* Hand = PC->HeldItemComponent;

    PC->InventoryComponent->AddItem(MakeHandTestHerb(77.0f));
    const int32 Index = PC->InventoryComponent->FindItemIndex(MakeHandTestHerb(77.0f));
    if (!TestTrue(TEXT("Трава в котомке"), Index != INDEX_NONE)) { PC->Destroy(); Manager->Destroy(); return false; }
    TestTrue(TEXT("Взяли"), Hand->TakeFromInventory(Index));
    Hand->ToggleInspect();

    // Распад: состояние в котомке поползло -- как делает ApplyDecayToItem.
    FInventoryItem Aged = PC->InventoryComponent->GetItems()[Index];
    Aged.State.Meta.Purity -= 0.05f;
    Aged.State.Meta.Corruption += 0.05f;
    Aged.State.Direction.Body += 0.03f;
    PC->InventoryComponent->RemoveItem(Index, Aged.Count);
    PC->InventoryComponent->AddItem(Aged);
    Hand->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Постаревшая трава -- всё ещё в руке"), Hand->IsHolding());
    TestTrue(TEXT("И всё ещё осматривается"), Hand->IsInspecting());
    TestEqual(TEXT("Снимок в руке обновился вместе с котомкой"), Hand->GetHeldItem().State.Meta.Corruption, Aged.State.Meta.Corruption);

    // Слияние стопок сдвинуло время создания -- находится по близкому состоянию.
    FInventoryItem Merged = PC->InventoryComponent->GetItems()[Hand->ResolveHeldIndex()];
    const int32 MergedIndex = Hand->ResolveHeldIndex();
    PC->InventoryComponent->RemoveItem(MergedIndex, Merged.Count);
    Merged.CreationTime += 5.0f;
    PC->InventoryComponent->AddItem(Merged);
    Hand->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("После слияния стопок -- всё ещё в руке"), Hand->IsHolding());

    Hand->PutAway();
    PC->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
