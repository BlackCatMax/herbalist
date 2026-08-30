// Source/ProjectHerbalistTests/Private/Tests/RitualBrewingTest.cpp
//
// Ритуальная (пошаговая, осмысленная) варка -- AGridWorldManager::
// TryAdvanceRitual, Core/Alchemy/RitualTypes.h (2026-08-30, прямой запрос:
// "не просто закидыванием всего подряд, а осмысленно -- сперва 2
// ингредиента в болотной воде на закате, потом третий на рассвете").
//
// Тесты гоняют настоящий AGridWorldManager (SpawnAndBeginPlay, как
// SystemInteractionTest.cpp/BiomeGraphIntegrationTest.cpp) с реально
// выставленным GameClockSeconds -- IsDawn()/IsDusk() читаются по-настоящему,
// не подделываются.

#include "Core/World/GridWorldManager.h"
#include "Core/Alchemy/RitualTypes.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Private/PipelineV2.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeRitualIngredient(FName ID, float Distortion = 0.3f, float Stability = 0.5f, float Corruption = 0.1f)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = 1;
        Item.bIsWater = false;
        Item.State.Magnitude = 0.5f;
        Item.State.Direction.Body = 1.f;
        Item.State.Meta.Distortion = Distortion;
        Item.State.Meta.Stability = Stability;
        Item.State.Meta.Purity = 0.5f;
        Item.State.Meta.Corruption = Corruption;
        return Item;
    }

    FInventoryItem MakeBogWater()
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

    FInventoryItem MakePlainWater()
    {
        FInventoryItem Item = MakeBogWater();
        Item.IngredientID = FName(TEXT("Water"));   // не BogWater -- для теста "не тот тип воды"
        return Item;
    }

    // Дневное время -- НЕ рассвет/закат/ночь (для теста "не в тот час").
    const float DayMoment = 700.0f;   // ~11.7 мин -- внутри "Дня" (после Рассвета, до Заката)
    const float DuskMoment = 1300.0f; // внутри "Заката" ([1200,1560) сек при 32-мин сутках)
    const float DawnMoment = 100.0f;  // внутри "Рассвета" ([0,360) сек)
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRitual_WrongWaterTypeDoesNotAdvance,
    "Herbalist.Ritual.WrongWaterTypeDoesNotAdvance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRitual_WrongWaterTypeDoesNotAdvance::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(DuskMoment);

    TArray<FInventoryItem> Ingredients = { MakeRitualIngredient(TEXT("A")), MakeRitualIngredient(TEXT("B")), MakePlainWater() };
    FRandomStream Rng(1);
    FInventoryItem Potion;
    const ERitualStepResult Result = Manager->TryAdvanceRitual(FIntPoint(5, 5), Ingredients, Rng, Potion);

    TestTrue(TEXT("Right count and time, but plain Water instead of BogWater -- does not advance"),
        Result == ERitualStepResult::NoMatch);
    TestEqual(TEXT("No partial ritual state left behind"), Manager->ActiveRituals.Num(), 0);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRitual_WrongTimeDoesNotAdvance,
    "Herbalist.Ritual.WrongTimeDoesNotAdvance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRitual_WrongTimeDoesNotAdvance::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(DayMoment);   // не Закат

    TArray<FInventoryItem> Ingredients = { MakeRitualIngredient(TEXT("A")), MakeRitualIngredient(TEXT("B")), MakeBogWater() };
    FRandomStream Rng(1);
    FInventoryItem Potion;
    const ERitualStepResult Result = Manager->TryAdvanceRitual(FIntPoint(5, 5), Ingredients, Rng, Potion);

    TestTrue(TEXT("Right count and water, but wrong time of day -- does not advance"),
        Result == ERitualStepResult::NoMatch);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRitual_WrongIngredientCountDoesNotAdvance,
    "Herbalist.Ritual.WrongIngredientCountDoesNotAdvance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRitual_WrongIngredientCountDoesNotAdvance::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;
    Manager->SetGameClockSeconds(DuskMoment);

    // Первый шаг рецепта "ZarevayaVoda" требует ровно 2 НЕ-водных -- пробуем
    // 1 и 3 (вода в счёт не идёт, см. IngredientCount в RitualTypes.h).
    TArray<FInventoryItem> OneIngredient = { MakeRitualIngredient(TEXT("A")), MakeBogWater() };
    TArray<FInventoryItem> ThreeIngredients = { MakeRitualIngredient(TEXT("A")), MakeRitualIngredient(TEXT("B")), MakeRitualIngredient(TEXT("C")), MakeBogWater() };

    FRandomStream Rng1(1);
    FInventoryItem Potion1;
    TestTrue(TEXT("1 non-water ingredient instead of 2 -- does not advance"),
        Manager->TryAdvanceRitual(FIntPoint(5, 5), OneIngredient, Rng1, Potion1) == ERitualStepResult::NoMatch);

    FRandomStream Rng2(1);
    FInventoryItem Potion2;
    TestTrue(TEXT("3 non-water ingredients instead of 2 -- does not advance"),
        Manager->TryAdvanceRitual(FIntPoint(5, 5), ThreeIngredients, Rng2, Potion2) == ERitualStepResult::NoMatch);

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Полный положительный путь: закатный шаг (2 ингредиента + болотная вода) ->
// (проходит игровое время) -> рассветный шаг (1 ингредиент) -> сварено.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRitual_CompletesAcrossTwoStepsWithRealTimeBetween,
    "Herbalist.Ritual.CompletesAcrossTwoStepsWithRealTimeBetween",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRitual_CompletesAcrossTwoStepsWithRealTimeBetween::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FIntPoint Cauldron(5, 5);

    // --- Шаг 1: закат, 2 не-водных ингредиента + болотная вода ---
    Manager->SetGameClockSeconds(DuskMoment);
    TArray<FInventoryItem> Step1 = { MakeRitualIngredient(TEXT("A")), MakeRitualIngredient(TEXT("B")), MakeBogWater() };
    FRandomStream Rng1(1);
    FInventoryItem PotionAfterStep1;
    const ERitualStepResult Result1 = Manager->TryAdvanceRitual(Cauldron, Step1, Rng1, PotionAfterStep1);
    TestTrue(TEXT("Step 1 (dusk, 2 ingredients, bog water) advances the ritual"), Result1 == ERitualStepResult::Progressed);
    TestEqual(TEXT("One active ritual tracked at the cauldron cell"), Manager->ActiveRituals.Num(), 1);

    const FActiveRitualState* Progress = Manager->ActiveRituals.Find(Cauldron);
    if (TestNotNull(TEXT("Progress state exists"), Progress))
    {
        TestEqual(TEXT("Recipe is ZarevayaVoda"), Progress->RecipeID, FName(TEXT("ZarevayaVoda")));
        TestEqual(TEXT("One step completed so far"), Progress->CompletedSteps, 1);
        TestEqual(TEXT("Three items accumulated so far (2 herbs + bog water)"), Progress->AccumulatedIngredients.Num(), 3);
    }

    // --- Реальное игровое время проходит: закат -> рассвет следующих суток ---
    Manager->SetGameClockSeconds(DawnMoment);
    TArray<FInventoryItem> Step2 = { MakeRitualIngredient(TEXT("C")) };
    FRandomStream Rng2(2);
    FInventoryItem FinalPotion;
    const ERitualStepResult Result2 = Manager->TryAdvanceRitual(Cauldron, Step2, Rng2, FinalPotion);
    TestTrue(TEXT("Step 2 (dawn, 1 ingredient) completes the ritual"), Result2 == ERitualStepResult::Completed);
    TestEqual(TEXT("Ritual progress cleared after completion"), Manager->ActiveRituals.Num(), 0);
    TestEqual(TEXT("Completed ritual produces a real Potion"), FinalPotion.IngredientID, FName(TEXT("Potion")));

    Manager->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// Ритуал обходит градации опасности по числу ингредиентов -- те же 3
// ингредиента (по составу дающие Bifurcation при пониженном пороге на 3
// импровизированных ингредиента) через ритуал НЕ должны сорваться, хотя
// импровизированная (не ритуальная) варка тем же составом -- должна.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistRitual_BypassesIngredientCountRisk,
    "Herbalist.Ritual.BypassesIngredientCountRisk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistRitual_BypassesIngredientCountRisk::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;
    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FIntPoint Cauldron(5, 5);

    // Distortion=0.95 гарантированно за любым порогом (сниженным или
    // исходным), Stability=1.0 -- та самая "раньше гарантированная Purified"
    // ситуация. У ритуала (bIsRitual=true, RiskyCount принудительно 0)
    // `Rng.FRand() < 1.0*1.0` истинно ВСЕГДА, независимо от сида. У той же
    // тройки БЕЗ ритуала (RiskyCount=1 на 3 ингредиента) шанс домножается на
    // PurifyOddsMultiplier=1-0.3*1=0.7 -- уже не гарантия, при части сидов
    // Catastrophe.
    Manager->SetGameClockSeconds(DuskMoment);
    TArray<FInventoryItem> Step1 = { MakeRitualIngredient(TEXT("A"), 0.95f, 1.0f), MakeRitualIngredient(TEXT("B"), 0.95f, 1.0f), MakeBogWater() };
    FRandomStream RngStep1(1);
    FInventoryItem Dummy1;
    Manager->TryAdvanceRitual(Cauldron, Step1, RngStep1, Dummy1);

    Manager->SetGameClockSeconds(DawnMoment);
    TArray<FInventoryItem> Step2 = { MakeRitualIngredient(TEXT("C"), 0.95f, 1.0f) };
    FRandomStream RngStep2(1);
    FInventoryItem RitualPotion;
    const ERitualStepResult Result = Manager->TryAdvanceRitual(Cauldron, Step2, RngStep2, RitualPotion);
    if (!TestTrue(TEXT("Ritual completes"), Result == ERitualStepResult::Completed)) { Manager->Destroy(); return false; }

    TestEqual(TEXT("Ritual-brewed potion: Stability=1.0 still guarantees Purified despite 3 ingredients"),
        RitualPotion.BrewOutcome, EAlchemyOutcome::Purified);

    // Решающая часть: та же тройка (тот же состав, та же Stability=1.0),
    // но БЕЗ ритуала (bIsRitual=false, один обычный Apply на все три разом,
    // откалиброванный сид) -- гарантия должна сломаться. Не просто "у
    // ритуала повезло", а честная демонстрация того, что именно даёт
    // bIsRitual: то же самое "старую гарантию ломает 4й ингредиент"
    // (ApplyFourIngredientsBreaksStabilityGuaranteeTest), но для 3.
    FWorldSnapshot WorldSnap;
    FInventorySnapshot InvSnap;
    FBiomeSnapshot BiomeSnap;
    FCommandBatch Batch;
    FCommandEntry Entry;
    Entry.Primitive = ECommandPrimitive::Apply;
    Entry.Apply.Ingredients = { MakeRitualIngredient(TEXT("A"), 0.95f, 1.0f), MakeRitualIngredient(TEXT("B"), 0.95f, 1.0f), MakeRitualIngredient(TEXT("C"), 0.95f, 1.0f), MakeBogWater() };
    Entry.Apply.bIsCrafting = true;
    Entry.Apply.bIsRitual = false;
    Batch.AddCommand(Entry);
    // Откалибровано реальным прогоном (диапазон сидов 1-40 -- при
    // PurifyOddsMultiplier=0.7 (3 не-водных ингредиента) примерно 30% сидов
    // дают Catastrophe, seed=3 надёжно среди них).
    FRandomStream RngDirect(3);
    FStateDelta DirectDelta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, Batch, RngDirect);
    const FInventoryOperation* DirectOp = nullptr;
    for (const FInventoryOperation& Op : DirectDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) DirectOp = &Op;
    if (TestNotNull(TEXT("Direct (non-ritual) op present"), DirectOp))
    {
        TestEqual(TEXT("Sanity: the SAME three ingredients brewed without a ritual break the Stability=1.0 guarantee (proves bIsRitual is doing real work, not that these inputs are just harmless)"),
            DirectOp->Ingredient.BrewOutcome, EAlchemyOutcome::Catastrophe);
    }

    Manager->Destroy();
    return true;
}

#endif
