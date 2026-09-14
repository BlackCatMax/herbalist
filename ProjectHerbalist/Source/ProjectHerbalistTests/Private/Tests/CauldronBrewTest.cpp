// Source/ProjectHerbalistTests/Private/Tests/CauldronBrewTest.cpp
//
// Варка у котла (2026-09-14, PIE-лог пользователя: «зелья не падают обратно в
// инвентарь», «UI не адаптируется к ширине текста»). Три дефекта:
// - Pipeline второй раз списывал ингредиенты, уже изъятые котлом при
//   переносе в слоты;
// - витрина результата искала зелье в сумке по времени создания и
//   промахивалась, а сопоставление сбора и варки с добытым шло одним
//   индексом;
// - варка у стола не получала фазу луны, BrewBoost, межбиомность и тиражный
//   оберег -- их резолвил только ApplyAlchemyResult.
// Плюс SizeBox окон из макета WBP: фиксированный размер становится минимальным.

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "UI/HerbalistWidgetSizing.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Имена с префиксом Cauldron: unity-сборка склеивает Tests/*.cpp, а
    // MakeIngredient/MakeWater уже есть в PipelineV2ApplyTest.cpp.
    FInventoryItem MakeCauldronHerb(FName ID, EBiomeType SourceBiome)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = 1;
        Item.bIsWater = false;
        Item.SourceBiome = SourceBiome;
        Item.State.Magnitude = 0.6f;
        Item.State.Direction.Body = 1.0f;
        Item.State.Meta.Distortion = 0.3f;
        Item.State.Meta.Stability = 0.5f;
        Item.State.Meta.Purity = 0.6f;
        return Item;
    }

    FInventoryItem MakeCauldronWater(EBiomeType SourceBiome)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("Water"));
        Item.Count = 1;
        Item.bIsWater = true;
        Item.SourceBiome = SourceBiome;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 0.25f;
        Item.State.Direction.Mind = 0.25f;
        Item.State.Direction.Spirit = 0.25f;
        Item.State.Direction.Nature = 0.25f;
        Item.State.Meta.Purity = 0.8f;
        return Item;
    }

    int32 CountCauldronOps(const FStateDelta& Delta, EInventoryOpType Type)
    {
        return Delta.InventoryOps.FilterByPredicate([Type](const FInventoryOperation& Op) { return Op.OpType == Type; }).Num();
    }

    FInventoryOperation MakeCauldronAddOp(FName ID, int32 ContainerID = 0)
    {
        FInventoryOperation Op;
        Op.ContainerID = ContainerID;
        Op.OpType = EInventoryOpType::Add;
        Op.Ingredient.IngredientID = ID;
        Op.Amount = 1;
        return Op;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCauldron_WithdrawnIngredientsAreNotRemovedAgain,
    "Herbalist.Alchemy.Cauldron.WithdrawnIngredientsAreNotRemovedAgain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCauldron_WithdrawnIngredientsAreNotRemovedAgain::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    FGridCell TableCell;
    TableCell.X = 5;
    TableCell.Y = 5;
    TableCell.Biome = EBiomeType::MixedForest;
    WorldSnap.GridState.Add(FIntPoint(5, 5), TableCell);
    FBiomeSnapshot BiomeSnap;
    FInventorySnapshot InvSnap;

    const TArray<FInventoryItem> Ingredients = {
        MakeCauldronHerb(TEXT("Ромашка"), EBiomeType::MixedForest),
        MakeCauldronWater(EBiomeType::MixedForest)
    };

    auto Brew = [&](bool bWithdrawn)
    {
        FCommandEntry Entry;
        Entry.Primitive = ECommandPrimitive::Apply;
        Entry.Apply.TargetCell = FIntPoint(5, 5);
        Entry.Apply.Ingredients = Ingredients;
        Entry.Apply.bIsCrafting = true;
        Entry.Apply.bIngredientsAlreadyWithdrawn = bWithdrawn;
        FCommandBatch Batch;
        Batch.AddCommand(Entry);
        FRandomStream Rng(7);
        return Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, Rng);
    };

    const FStateDelta FromBag = Brew(false);
    const FStateDelta FromCauldron = Brew(true);

    TestEqual(TEXT("Варка из сумки списывает по операции на ингредиент"),
        CountCauldronOps(FromBag, EInventoryOpType::Remove), Ingredients.Num());
    TestEqual(TEXT("Варка у котла не списывает ничего: котёл изъял ингредиенты при переносе"),
        CountCauldronOps(FromCauldron, EInventoryOpType::Remove), 0);
    TestEqual(TEXT("Результат варки у котла кладётся в сумку"),
        CountCauldronOps(FromCauldron, EInventoryOpType::Add), 1);

    const FInventoryOperation* BagAdd = FromBag.InventoryOps.FindByPredicate([](const FInventoryOperation& Op) { return Op.OpType == EInventoryOpType::Add; });
    const FInventoryOperation* CauldronAdd = FromCauldron.InventoryOps.FindByPredicate([](const FInventoryOperation& Op) { return Op.OpType == EInventoryOpType::Add; });
    if (TestNotNull(TEXT("Есть результат из сумки"), BagAdd) && TestNotNull(TEXT("Есть результат у котла"), CauldronAdd))
    {
        TestEqual(TEXT("Флаг меняет только списание: тот же результат"),
            CauldronAdd->Ingredient.IngredientID, BagAdd->Ingredient.IngredientID);
        TestEqual(TEXT("Флаг меняет только списание: та же сила"),
            CauldronAdd->Ingredient.State.Magnitude, BagAdd->Ingredient.State.Magnitude);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCauldron_BrewCommandResolvesBrewModifiers,
    "Herbalist.Alchemy.Cauldron.BrewCommandResolvesBrewModifiers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCauldron_BrewCommandResolvesBrewModifiers::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    // Середина лунного цикла (фаза 2 из 4) -- не значение команды по
    // умолчанию, иначе равенство ничего бы не доказывало.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f;
    const float PhaseDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    Manager->SetGameClockSeconds(DaySeconds * PhaseDays * 2.5f);
    TestNotEqual(TEXT("Sanity: фаза луны отличается от значения по умолчанию"),
        static_cast<int32>(Manager->GetMoonPhase()), static_cast<int32>(EMoonPhase::NewMoon));

    FAcquiredArtifact Stone;
    Stone.ArtifactID = FName(TEXT("Камень-оберег"));
    Manager->SetAcquiredArtifacts({ Stone });
    Manager->ActivateTieredWard(EWardEffectType::BrewBoost, { EBiomeType::Bog });
    // Без активации оберег выключен, и равенство с IsWardBrewBoostActive()
    // прошло бы и при забытом поле (ревью 2026-09-14).
    TestTrue(TEXT("Sanity: оберег BrewBoost включается"), Manager->ActivateWardBrewBoost());

    // Два биома среди трав; вода из третьего межбиомность не добавляет.
    const TArray<FInventoryItem> AtHome = {
        MakeCauldronHerb(TEXT("Ромашка"), EBiomeType::MixedForest),
        MakeCauldronHerb(TEXT("Багульник"), EBiomeType::Bog),
        MakeCauldronWater(EBiomeType::Steppe)
    };
    const FCommandEntry Cmd = Manager->BuildCauldronBrewCommand(FIntPoint(3, 4), AtHome);

    TestTrue(TEXT("Команда -- Apply"), Cmd.Primitive == ECommandPrimitive::Apply);
    TestEqual(TEXT("Клетка котла"), Cmd.Apply.TargetCell, FIntPoint(3, 4));
    TestTrue(TEXT("Крафт в сумку"), Cmd.Apply.bIsCrafting);
    TestTrue(TEXT("Ингредиенты помечены изъятыми"), Cmd.Apply.bIngredientsAlreadyWithdrawn);
    TestTrue(TEXT("Заряд Камня-оберега"), Cmd.Apply.bBifurcationCharmActive);
    TestEqual(TEXT("Фаза луны менеджера"), static_cast<int32>(Cmd.Apply.MoonPhase), static_cast<int32>(Manager->GetMoonPhase()));
    TestTrue(TEXT("Оберег BrewBoost"), Cmd.Apply.bWardBrewBoostActive);
    TestEqual(TEXT("Межбиомность: два биома трав, вода не в счёт"), Cmd.Apply.DistinctIngredientBiomeCount, 2);
    TestEqual(TEXT("Тиражный оберег в полную силу: одна трава из его биома"), Cmd.Apply.TieredBrewBoostStrength, 1.0f);

    const TArray<FInventoryItem> AwayFromHome = {
        MakeCauldronHerb(TEXT("Ромашка"), EBiomeType::MixedForest),
        MakeCauldronWater(EBiomeType::Bog)
    };
    const FCommandEntry AwayCmd = Manager->BuildCauldronBrewCommand(FIntPoint(3, 4), AwayFromHome);
    const float OutOfBiomeStrength = Settings ? Settings->TieredWardOutOfBiomeStrength : 0.5f;
    TestEqual(TEXT("Тиражный оберег ослаблен: трав из его биома нет, вода не в счёт"),
        AwayCmd.Apply.TieredBrewBoostStrength, OutOfBiomeStrength);
    TestEqual(TEXT("Одна трава -- один биом"), AwayCmd.Apply.DistinctIngredientBiomeCount, 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCauldron_ProducedItemMatchesItsOwnCommand,
    "Herbalist.Alchemy.Cauldron.ProducedItemMatchesItsOwnCommand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCauldron_ProducedItemMatchesItsOwnCommand::RunTest(const FString& Parameters)
{
    FCommandEntry Harvest;
    Harvest.Primitive = ECommandPrimitive::Harvest;
    FCommandEntry CellApply;
    CellApply.Primitive = ECommandPrimitive::Apply;
    FCommandEntry Brew;
    Brew.Primitive = ECommandPrimitive::Apply;
    Brew.Apply.bIsCrafting = true;

    struct FVisit { ECommandPrimitive Primitive; bool bIsCrafting; FName ProducedID; };
    auto Collect = [](const TArray<FCommandEntry>& Commands, const FStateDelta& Delta)
    {
        TArray<FVisit> Visits;
        AGridWorldManager::ForEachProducedItem(Commands, Delta,
            [&Visits](const FCommandEntry& Cmd, const FInventoryOperation& AddOp)
            {
                Visits.Add({ Cmd.Primitive, Cmd.Apply.bIsCrafting, AddOp.Ingredient.IngredientID });
            });
        return Visits;
    };

    // Сбор без добычи впереди варки: раньше общий индекс отдавал зелье сбору.
    {
        FStateDelta Delta;
        FInventoryOperation RemoveOp;
        RemoveOp.OpType = EInventoryOpType::Remove;
        RemoveOp.Ingredient.IngredientID = FName(TEXT("Ромашка"));
        Delta.InventoryOps.Add(RemoveOp);
        Delta.InventoryOps.Add(MakeCauldronAddOp(TEXT("Potion")));

        const TArray<FVisit> Visits = Collect({ Harvest, CellApply, Brew }, Delta);
        if (TestEqual(TEXT("Одно сопоставление"), Visits.Num(), 1))
        {
            TestTrue(TEXT("Зелье -- варке, не сбору"), Visits[0].Primitive == ECommandPrimitive::Apply && Visits[0].bIsCrafting);
            TestEqual(TEXT("Сопоставлено зелье"), Visits[0].ProducedID, FName(TEXT("Potion")));
        }
    }

    // Добыча и зола в обратном порядке команд, предмет в хранилище не в счёт.
    {
        FStateDelta Delta;
        Delta.InventoryOps.Add(MakeCauldronAddOp(TEXT("Ash"), /*ContainerID*/ 5));
        Delta.InventoryOps.Add(MakeCauldronAddOp(TEXT("Ромашка")));
        Delta.InventoryOps.Add(MakeCauldronAddOp(TEXT("Ash")));

        const TArray<FVisit> Visits = Collect({ Brew, Harvest }, Delta);
        if (TestEqual(TEXT("Два сопоставления"), Visits.Num(), 2))
        {
            TestTrue(TEXT("Первой -- варка"), Visits[0].bIsCrafting);
            TestEqual(TEXT("Варке -- зола из сумки"), Visits[0].ProducedID, FName(TEXT("Ash")));
            TestTrue(TEXT("Второй -- сбор"), Visits[1].Primitive == ECommandPrimitive::Harvest);
            TestEqual(TEXT("Сбору -- добыча"), Visits[1].ProducedID, FName(TEXT("Ромашка")));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWidgetSizing_FixedSizeBecomesMinimum,
    "Herbalist.UI.WidgetSizing.FixedSizeBecomesMinimum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWidgetSizing_FixedSizeBecomesMinimum::RunTest(const FString& Parameters)
{
    UWidgetTree* Tree = NewObject<UWidgetTree>(GetTransientPackage());
    UVerticalBox* Root = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Tree->RootWidget = Root;

    USizeBox* Fixed = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Fixed->SetWidthOverride(300.0f);
    Fixed->SetHeightOverride(120.0f);
    Root->AddChild(Fixed);

    USizeBox* MinOnly = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    MinOnly->SetMinDesiredWidth(50.0f);
    Root->AddChild(MinOnly);

    USizeBox* LargerMin = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    LargerMin->SetWidthOverride(200.0f);
    LargerMin->SetMinDesiredWidth(250.0f);
    Root->AddChild(LargerMin);

    USizeBox* MaxBelowFixed = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    MaxBelowFixed->SetWidthOverride(300.0f);
    MaxBelowFixed->SetMaxDesiredWidth(200.0f);
    Root->AddChild(MaxBelowFixed);

    USizeBox* MaxAboveFixed = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    MaxAboveFixed->SetWidthOverride(300.0f);
    MaxAboveFixed->SetMaxDesiredWidth(500.0f);
    Root->AddChild(MaxAboveFixed);

    TestEqual(TEXT("Изменены четыре SizeBox с фиксированным размером"), HerbalistUI::LetSizeBoxesGrowWithContent(Tree), 4);

    TestFalse(TEXT("Ширина больше не фиксирована"), Fixed->IsWidthOverride());
    TestFalse(TEXT("Высота больше не фиксирована"), Fixed->IsHeightOverride());
    TestTrue(TEXT("Прежняя ширина стала минимальной"), Fixed->IsMinDesiredWidthOverride());
    TestEqual(TEXT("Минимальная ширина -- из макета"), Fixed->GetMinDesiredWidth(), 300.0f);
    TestEqual(TEXT("Минимальная высота -- из макета"), Fixed->GetMinDesiredHeight(), 120.0f);

    TestEqual(TEXT("SizeBox без фиксированного размера не тронут"), MinOnly->GetMinDesiredWidth(), 50.0f);
    TestFalse(TEXT("SizeBox без фиксированного размера не получил высоту"), MinOnly->IsMinDesiredHeightOverride());

    TestEqual(TEXT("Больший минимум из макета сохранён"), LargerMin->GetMinDesiredWidth(), 250.0f);

    TestFalse(TEXT("Максимум меньше макета снят -- не сжимает окно ниже минимума"), MaxBelowFixed->IsMaxDesiredWidthOverride());
    TestEqual(TEXT("Минимум при снятом максимуме -- из макета"), MaxBelowFixed->GetMinDesiredWidth(), 300.0f);
    TestTrue(TEXT("Максимум не меньше макета остался"), MaxAboveFixed->IsMaxDesiredWidthOverride());
    TestEqual(TEXT("Предел роста не тронут"), MaxAboveFixed->GetMaxDesiredWidth(), 500.0f);

    TestEqual(TEXT("Повторный вызов ничего не меняет"), HerbalistUI::LetSizeBoxesGrowWithContent(Tree), 0);
    TestEqual(TEXT("Пустое дерево -- ноль"), HerbalistUI::LetSizeBoxesGrowWithContent(nullptr), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistWidgetSizing_GameWindowsGrowWithText,
    "Herbalist.UI.WidgetSizing.GameWindowsGrowWithText",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistWidgetSizing_GameWindowsGrowWithText::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    // TakeWidget строит Slate-дерево и зовёт NativeConstruct -- там и
    // вызывается LetSizeBoxesGrowWithContent. Окно котла ставит фокус через
    // FSlateApplication.
    if (!TestTrue(TEXT("Slate application initialized"), FSlateApplication::IsInitialized())) return false;

    // Все пять макетов с фиксированным SizeBox (2026-09-14).
    const TArray<FString> ClassPaths = {
        TEXT("/Game/Widgets/UI/WBP_AlchemyTransferWidget.WBP_AlchemyTransferWidget_C"),
        TEXT("/Game/Widgets/UI/WBP_AlchemySlotWidget.WBP_AlchemySlotWidget_C"),
        TEXT("/Game/Widgets/UI/WBP_InventoryWidget.WBP_InventoryWidget_C"),
        TEXT("/Game/Widgets/UI/WBP_InventorySlotWidget.WBP_InventorySlotWidget_C"),
        TEXT("/Game/Widgets/UI/WBP_ItemTooltip.WBP_ItemTooltip_C"),
    };

    for (const FString& ClassPath : ClassPaths)
    {
        UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UUserWidget>(nullptr, *ClassPath));
        if (!TestNotNull(*FString::Printf(TEXT("Класс загружен: %s"), *ClassPath), WidgetClass)) continue;

        // Макет из ассета -- эталон размеров; на экземпляре их меняет виджет.
        TArray<USizeBox*> FixedInLayout;
        if (UWidgetTree* Archetype = WidgetClass->GetWidgetTreeArchetype())
        {
            Archetype->ForEachWidget([&FixedInLayout](UWidget* Widget)
            {
                USizeBox* SizeBox = Cast<USizeBox>(Widget);
                if (SizeBox && (SizeBox->IsWidthOverride() || SizeBox->IsHeightOverride()))
                {
                    FixedInLayout.Add(SizeBox);
                }
            });
        }
        TestTrue(*FString::Printf(TEXT("В макете есть SizeBox с фиксированным размером: %s"), *ClassPath), FixedInLayout.Num() > 0);

        UUserWidget* Widget = CreateWidget<UUserWidget>(World, WidgetClass);
        if (!TestNotNull(*FString::Printf(TEXT("Виджет создан: %s"), *ClassPath), Widget)) continue;
        Widget->TakeWidget();

        for (const USizeBox* LayoutBox : FixedInLayout)
        {
            const USizeBox* Box = Widget->WidgetTree ? Widget->WidgetTree->FindWidget<USizeBox>(LayoutBox->GetFName()) : nullptr;
            const FString Where = FString::Printf(TEXT("%s / %s"), *ClassPath, *LayoutBox->GetName());
            if (!TestNotNull(*FString::Printf(TEXT("SizeBox есть на экземпляре: %s"), *Where), Box)) continue;

            if (LayoutBox->IsWidthOverride())
            {
                TestFalse(*FString::Printf(TEXT("Ширина не фиксирована: %s"), *Where), Box->IsWidthOverride());
                TestTrue(*FString::Printf(TEXT("Минимальная ширина не меньше макета: %s"), *Where),
                    Box->IsMinDesiredWidthOverride() && Box->GetMinDesiredWidth() >= LayoutBox->GetWidthOverride());
            }
            if (LayoutBox->IsHeightOverride())
            {
                TestFalse(*FString::Printf(TEXT("Высота не фиксирована: %s"), *Where), Box->IsHeightOverride());
                TestTrue(*FString::Printf(TEXT("Минимальная высота не меньше макета: %s"), *Where),
                    Box->IsMinDesiredHeightOverride() && Box->GetMinDesiredHeight() >= LayoutBox->GetHeightOverride());
            }
        }
        Widget->RemoveFromParent();
    }
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
