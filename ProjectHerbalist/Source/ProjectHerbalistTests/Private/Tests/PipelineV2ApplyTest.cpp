// Source/ProjectHerbalistTests/Private/Tests/PipelineV2ApplyTest.cpp
//
// Варка/применение зелья (ProcessApplyCommand/ComputeApplyResult, PipelineV2.cpp)
// — до этого файла не имела ни одного автотеста (находка финального аудита
// 2026-08-30, вероятно центральный игровой глагол Травника). ComputeApplyResult
// и ComputeIntentCoherence -- static внутри своей единицы трансляции, поэтому
// тестируем их черноящично через Simulation::ExecutePipeline, тем же приёмом,
// что уже применяет PipelineV2Test.cpp к Harvest.
//
// Где числа считаются точно, а где сравнительно: Ash/BoiledWater — вырожденные
// ветки, возвращаются ДО Morok/Zaryana, все константы точные. Разбавление водой
// (WaterFraction) считается по Count, не по Magnitude -- дальше по пайплайну
// Magnitude больше никто не трогает, поэтому формула проверяется точно. Bifurcation
// снэпает на фиксированные константы независимо от входных данных -- тоже точно,
// нужно только гарантированно попасть в нужную ветку (см. трюк со Stability=1.0/
// near-0 ниже). Обычная варка (Morok/Zaryana/Coherence вместе) — числа зависят от
// нелинейного Zaryana axis-mix (tanh, ненадёжно для ручного счёта без магических
// констант) — там проверяется сравнительно (при прочих равных X растёт/падает) и
// структурно (счётчики операций/ID/диапазоны), не точным float-равенством.

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "PipelineV2.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    FInventoryItem MakeIngredient(FName ID, float Magnitude, float Distortion, float Stability,
        float Purity, float Potency = 0.f, float Resonance = 0.f, float Corruption = 0.f,
        float Body = 1.f, float Mind = 0.f, float Spirit = 0.f, float Nature = 0.f)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.Count = 1;
        Item.bIsWater = false;
        Item.State.Magnitude = Magnitude;
        Item.State.Direction.Body = Body;
        Item.State.Direction.Mind = Mind;
        Item.State.Direction.Spirit = Spirit;
        Item.State.Direction.Nature = Nature;
        Item.State.Meta.Distortion = Distortion;
        Item.State.Meta.Stability = Stability;
        Item.State.Meta.Purity = Purity;
        Item.State.Meta.Potency = Potency;
        Item.State.Meta.Resonance = Resonance;
        Item.State.Meta.Corruption = Corruption;
        return Item;
    }

    FInventoryItem MakeWater(float Magnitude, float Purity, int32 Count = 1)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("Water"));
        Item.Count = Count;
        Item.bIsWater = true;
        Item.State.Magnitude = Magnitude;
        Item.State.Direction.Body = 0.25f;
        Item.State.Direction.Mind = 0.25f;
        Item.State.Direction.Spirit = 0.25f;
        Item.State.Direction.Nature = 0.25f;
        Item.State.Meta.Purity = Purity;
        return Item;
    }

    // Клетка-мишень по умолчанию: биом без узла в BiomeSnap.Contexts (пусто) --
    // BiomeCtx=nullptr гарантированно, EffectiveMorok=EffectiveZaryana=0, что и
    // нужно большинству тестов ниже (изолировать эффект без шума биом-графа).
    FGridCell MakeTargetCell(int32 X, int32 Y)
    {
        FGridCell Cell;
        Cell.X = X;
        Cell.Y = Y;
        Cell.Biome = EBiomeType::MixedForest;
        Cell.bIsWater = false;
        Cell.HarvestStress = 0.1f;
        return Cell;
    }

    FStateDelta RunApply(const TArray<FInventoryItem>& Ingredients, bool bIsCrafting,
        const FIntPoint& TargetCell, FRandomStream& Rng,
        const FWorldSnapshot& WorldSnap, const FBiomeSnapshot& BiomeSnap, float CallerCoherence = 0.f)
    {
        FInventorySnapshot InvSnap;
        FCommandBatch CmdBatch;
        FCommandEntry Entry;
        Entry.Primitive = ECommandPrimitive::Apply;
        Entry.Apply.TargetCell = TargetCell;
        Entry.Apply.Ingredients = Ingredients;
        Entry.Apply.bIsCrafting = bIsCrafting;
        Entry.Apply.Intent.Coherence = CallerCoherence;
        CmdBatch.AddCommand(Entry);
        return Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, CmdBatch, Rng);
    }
}

// ---------------------------------------------------------------------------
// Ash: непустой список ингредиентов без единой капли воды -- вырожденный
// результат, все константы фиксированы (05_Systems.md, "обязательность воды").
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyAshTest,
    "ProjectHerbalist.PipelineV2.ApplyAsh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyAshTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("DryHerb"), 0.6f, 0.3f, 0.5f, 0.6f) };
    FRandomStream Rng(1);
    FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    // 1 remove (ингредиент) + 1 add (Ash)
    TestEqual(TEXT("Two inventory ops (remove ingredient + add Ash)"), Delta.InventoryOps.Num(), 2);
    const FInventoryOperation* AddOp = nullptr;
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
    }
    if (!TestNotNull(TEXT("Add op present"), AddOp)) return false;

    TestEqual(TEXT("Ash ingredient ID"), AddOp->Ingredient.IngredientID, FName(TEXT("Ash")));
    TestFalse(TEXT("Ash not subject to decay"), AddOp->Ingredient.bSubjectToDecay);
    TestEqual(TEXT("Ash Magnitude"), AddOp->Ingredient.State.Magnitude, 0.05f);
    TestEqual(TEXT("Ash Distortion"), AddOp->Ingredient.State.Meta.Distortion, 0.9f);
    TestEqual(TEXT("Ash Corruption"), AddOp->Ingredient.State.Meta.Corruption, 0.8f);
    TestEqual(TEXT("Ash Purity"), AddOp->Ingredient.State.Meta.Purity, 0.05f);
    TestEqual(TEXT("Ash Stability"), AddOp->Ingredient.State.Meta.Stability, 0.1f);

    // Крафт -- никакого изменения клетки и следа в биом-графе не должно быть.
    TestEqual(TEXT("No world changes on craft"), Delta.WorldChanges.Num(), 0);
    TestEqual(TEXT("No footprint on craft"), Delta.Footprints.Num(), 0);
    return true;
}

// ---------------------------------------------------------------------------
// BoiledWater: только вода, ни одного обычного ингредиента.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyBoiledWaterTest,
    "ProjectHerbalist.PipelineV2.ApplyBoiledWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyBoiledWaterTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    TArray<FInventoryItem> Ingredients = { MakeWater(0.5f, 0.4f) };
    FRandomStream Rng(1);
    FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FInventoryOperation* AddOp = nullptr;
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
    }
    if (!TestNotNull(TEXT("Add op present"), AddOp)) return false;

    TestEqual(TEXT("BoiledWater ingredient ID"), AddOp->Ingredient.IngredientID, FName(TEXT("BoiledWater")));
    TestTrue(TEXT("Purity +0.3"), FMath::IsNearlyEqual(AddOp->Ingredient.State.Meta.Purity, 0.7f, 0.0005f));
    TestEqual(TEXT("Distortion snapped to 0"), AddOp->Ingredient.State.Meta.Distortion, 0.0f);
    TestEqual(TEXT("Magnitude clamped to <=0.2"), AddOp->Ingredient.State.Magnitude, 0.2f);
    return true;
}

// ---------------------------------------------------------------------------
// Разбавление водой -- WaterFraction считается по Count, Magnitude больше
// никто по пайплайну не трогает, поэтому формула проверяется точным float.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyWaterDilutionLinearTest,
    "ProjectHerbalist.PipelineV2.ApplyWaterDilutionLinear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyWaterDilutionLinearTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    // 1 ингредиент (Count=1) + 1 вода (Count=1) -- WaterFraction=0.5, ниже
    // MaxWaterRatio (0.8 по умолчанию), только линейное разбавление.
    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.1f, 0.5f, 0.5f), MakeWater(0.5f, 0.5f, 1) };
    FRandomStream Rng(1);
    FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FInventoryOperation* AddOp = nullptr;
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
    }
    if (!TestNotNull(TEXT("Add op present"), AddOp)) return false;

    // Magnitude = 0.6 * (1 - 0.5) = 0.3, без штрафа за избыток воды.
    TestTrue(TEXT("Magnitude halved by 50% water dilution"),
        FMath::IsNearlyEqual(AddOp->Ingredient.State.Magnitude, 0.3f, 0.0005f));
    return true;
}

// ---------------------------------------------------------------------------
// Регрессия сессии 2026-08-30 (найдена PlaySessionIntegrationTest.cpp):
// применение уже сваренного зелья на клетку (UsePotion -> ApplyPotionToCell)
// заворачивает готовый предмет в тот же список Ingredients, что и сырые
// материалы при варке -- единственный небольшой предмет без воды безусловно
// попадал бы под правило "обязательность воды" (шаг 4a) и становился золой
// ЗАНОВО, стирая реально сваренное качество. ComputeApplyResult теперь
// распознаёт уже готовый результат по IngredientID (Potion/Ash/BoiledWater)
// и применяет его State напрямую, без повторного Fold/Morok/Zaryana/Bifurcation.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyAlreadyBrewedPotionKeepsItsOwnStateTest,
    "ProjectHerbalist.PipelineV2.ApplyAlreadyBrewedPotionKeepsItsOwnState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyAlreadyBrewedPotionKeepsItsOwnStateTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    // Отчётливое, узнаваемое состояние -- не пересекается ни с одной
    // константой Ash (0.05/0.9/0.8/0.05/0.1) или дефолтом.
    FInventoryItem Potion;
    Potion.IngredientID = FName(TEXT("Potion"));
    Potion.Count = 1;
    Potion.bSubjectToDecay = false;
    Potion.State.Magnitude = 0.55f;
    Potion.State.Meta.Distortion = 0.33f;
    Potion.State.Meta.Purity = 0.77f;
    Potion.State.Meta.Stability = 0.44f;
    Potion.State.Meta.Corruption = 0.22f;

    FRandomStream Rng(1);
    FStateDelta Delta = RunApply({ Potion }, /*bIsCrafting*/ false, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FGridCell* Modified = Delta.WorldChanges.Find(FIntPoint(5, 5));
    if (!TestNotNull(TEXT("Cell modified"), Modified)) return false;

    TestEqual(TEXT("Cell Magnitude matches the potion's own state exactly"), Modified->State.Magnitude, 0.55f);
    TestEqual(TEXT("Cell Distortion matches the potion's own state exactly"), Modified->State.Meta.Distortion, 0.33f);
    TestEqual(TEXT("Cell Purity matches the potion's own state exactly"), Modified->State.Meta.Purity, 0.77f);
    TestEqual(TEXT("Cell Stability matches the potion's own state exactly"), Modified->State.Meta.Stability, 0.44f);
    TestEqual(TEXT("Cell Corruption matches the potion's own state exactly"), Modified->State.Meta.Corruption, 0.22f);

    // Тот же принцип должен работать и для золы/варёной воды, если их
    // когда-нибудь тоже станет можно "применить" (сейчас UsePotion фильтрует
    // только Potion, но правило в ComputeApplyResult общее для всех трёх).
    FInventoryItem BoiledWater;
    BoiledWater.IngredientID = FName(TEXT("BoiledWater"));
    BoiledWater.Count = 1;
    BoiledWater.State.Meta.Purity = 0.66f;
    FRandomStream Rng2(2);
    FStateDelta Delta2 = RunApply({ BoiledWater }, /*bIsCrafting*/ false, FIntPoint(5, 5), Rng2, WorldSnap, BiomeSnap);
    const FGridCell* Modified2 = Delta2.WorldChanges.Find(FIntPoint(5, 5));
    if (TestNotNull(TEXT("Cell modified (BoiledWater)"), Modified2))
    {
        TestEqual(TEXT("BoiledWater applied with its own Purity, not re-processed"), Modified2->State.Meta.Purity, 0.66f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyWaterDilutionExcessPenaltyTest,
    "ProjectHerbalist.PipelineV2.ApplyWaterDilutionExcessPenalty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyWaterDilutionExcessPenaltyTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    // 1 ингредиент (Count=1) + вода (Count=9) -- WaterFraction=0.9, выше
    // MaxWaterRatio (0.8) -- срабатывает дополнительный штраф "водянистого" зелья.
    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.1f, 0.5f, 0.5f), MakeWater(0.5f, 0.5f, 9) };
    FRandomStream Rng(1);
    FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FInventoryOperation* AddOp = nullptr;
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
    }
    if (!TestNotNull(TEXT("Add op present"), AddOp)) return false;

    // Линейно: 0.6*(1-0.9)=0.06. Excess=(0.9-0.8)/(1-0.8)=0.5.
    // Штраф: 0.06*(1-0.5*0.2)=0.06*0.9=0.054.
    TestTrue(TEXT("Magnitude matches excess-water penalty formula"),
        FMath::IsNearlyEqual(AddOp->Ingredient.State.Magnitude, 0.054f, 0.0005f));
    return true;
}

// ---------------------------------------------------------------------------
// Согласие/конфликт трав (2026-08-30, "докручиваем варку, учитывая реальные
// сильные стороны трав и их сочетаний" -- по прямому запросу). Два и более
// не-водных ингредиента теперь собираются ПОСЛЕДОВАТЕЛЬНО (первый -- затравка,
// каждый следующий реагирует на уже накопленный результат), не одним
// симметричным взвешенным средним -- эта ветка формулы раньше не имела ни
// одного теста вовсе (все существующие тесты выше используют ровно один
// не-водный ингредиент, при котором цикл реакции не выполняется ни разу).
//
// Magnitude -- единственная ось, которую Zaryana/Morok не трогают вообще
// (см. заметку в шапке файла), поэтому её можно проверить точным числом
// через весь пайплайн целиком; остальные Meta-оси проверяются сравнительно.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyMagnitudeExactlyReflectsHarmonyTest,
    "ProjectHerbalist.PipelineV2.ApplyMagnitudeExactlyReflectsHarmony",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyMagnitudeExactlyReflectsHarmonyTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    // Два ингредиента с ОДИНАКОВЫМИ Meta-осями (все 0.9, > 0.5) и одной и той
    // же доминирующей осью Direction -- максимальное согласие по всем 7
    // сигналам голосования (Harmony=1.0 ровно, не приближённо), плюс 1 вода
    // (Count=1). Magnitude первого (затравка) = 0.5, второго намеренно другой
    // (0.7) -- собственная Magnitude второго ингредиента формулу не трогает,
    // только его Meta/Direction (согласие/конфликт), это тоже часть проверки.
    FInventoryItem Item1 = MakeIngredient(TEXT("A"), 0.5f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, /*Body*/ 1.f);
    FInventoryItem Item2 = MakeIngredient(TEXT("B"), 0.7f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, /*Body*/ 1.f);
    FInventoryItem Water = MakeWater(0.5f, 0.5f, 1);

    FRandomStream Rng(1);
    FStateDelta Delta = RunApply({ Item1, Item2, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FInventoryOperation* AddOp = nullptr;
    for (const FInventoryOperation& Op : Delta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
    if (!TestNotNull(TEXT("Add op present"), AddOp)) return false;

    // Harmony=1.0 (полное согласие) -> NonWaterAgg.Magnitude = Lerp(0.5, 1.0,
    // PowerGrowthRate(0.5) * 1.0 * Weight(FoldWeightDecay^1=0.8)) = Lerp(0.5,1.0,0.4) = 0.7.
    // Разбавление водой: WaterFraction = 1/3 (2 не-водных + 1 вода) -> 0.7*(1-1/3) = 0.46667.
    TestTrue(TEXT("Magnitude matches the exact harmony+dilution formula for full agreement"),
        FMath::IsNearlyEqual(AddOp->Ingredient.State.Magnitude, 0.7f * (1.f - 1.f / 3.f), 0.0015f));
    return true;
}

// ---------------------------------------------------------------------------
// Согласные ингредиенты дают итог УБЕДИТЕЛЬНЕЕ каждого из них по отдельности
// (шумное "ИЛИ", не среднее и не Lerp-к-полюсу — тот всегда даёт результат
// МЕЖДУ входом и полюсом, то есть не выше самого сильного входа, что при
// разработке проверено и отвергнуто как недостаточное "усиление").
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyAgreeingIngredientsOutdoBothInputsTest,
    "ProjectHerbalist.PipelineV2.ApplyAgreeingIngredientsOutdoBothInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyAgreeingIngredientsOutdoBothInputsTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    // Два ингредиента, оба с высокой Corruption (0.8 и 0.9), прочие оси
    // нейтральны, но НЕ ровно 0.5 (0.51/0.49 попеременно скучные -- избегаем
    // граничного случая "ровно 0.5 не считается ни высоким, ни низким",
    // который иначе тянул бы общий Harmony вниз без содержательной причины).
    FInventoryItem Strong1 = MakeIngredient(TEXT("A"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.8f, 1.f);
    FInventoryItem Strong2 = MakeIngredient(TEXT("B"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.9f, 1.f);
    FInventoryItem Water = MakeWater(0.5f, 0.5f, 1);

    FRandomStream RngPair(7);
    FStateDelta PairDelta = RunApply({ Strong1, Strong2, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), RngPair, WorldSnap, BiomeSnap);
    FRandomStream RngSolo(7);
    FStateDelta SoloDelta = RunApply({ Strong2, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), RngSolo, WorldSnap, BiomeSnap);

    const FInventoryOperation* PairOp = nullptr;
    for (const FInventoryOperation& Op : PairDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) PairOp = &Op;
    const FInventoryOperation* SoloOp = nullptr;
    for (const FInventoryOperation& Op : SoloDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) SoloOp = &Op;
    if (!TestNotNull(TEXT("Pair op present"), PairOp) || !TestNotNull(TEXT("Solo op present"), SoloOp)) return false;

    TestTrue(TEXT("Two agreeing high-Corruption ingredients outdo the stronger one taken alone"),
        PairOp->Ingredient.State.Meta.Corruption > SoloOp->Ingredient.State.Meta.Corruption + KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// Конфликтующие ингредиенты дают более слабое/мутное зелье (ниже Magnitude),
// чем те же по силе, но согласные -- при прочих равных.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyConflictIsWeakerThanAgreementTest,
    "ProjectHerbalist.PipelineV2.ApplyConflictIsWeakerThanAgreement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyConflictIsWeakerThanAgreementTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    FInventoryItem Seed = MakeIngredient(TEXT("A"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.8f, 1.f);
    FInventoryItem Agreeing  = MakeIngredient(TEXT("B"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.8f, 1.f);
    FInventoryItem Conflicting = MakeIngredient(TEXT("C"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.1f, 1.f);
    FInventoryItem Water = MakeWater(0.5f, 0.5f, 1);

    FRandomStream RngAgree(3);
    FStateDelta AgreeDelta = RunApply({ Seed, Agreeing, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), RngAgree, WorldSnap, BiomeSnap);
    FRandomStream RngConflict(3);
    FStateDelta ConflictDelta = RunApply({ Seed, Conflicting, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), RngConflict, WorldSnap, BiomeSnap);

    const FInventoryOperation* AgreeOp = nullptr;
    for (const FInventoryOperation& Op : AgreeDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) AgreeOp = &Op;
    const FInventoryOperation* ConflictOp = nullptr;
    for (const FInventoryOperation& Op : ConflictDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) ConflictOp = &Op;
    if (!TestNotNull(TEXT("Agree op present"), AgreeOp) || !TestNotNull(TEXT("Conflict op present"), ConflictOp)) return false;

    TestTrue(TEXT("Agreeing pair yields higher Magnitude than a conflicting pair, same seed"),
        AgreeOp->Ingredient.State.Magnitude > ConflictOp->Ingredient.State.Magnitude + KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// Порядок теперь и правда важен: та же тройка ингредиентов в другом порядке
// добавления даёт другой результат (последовательная реакция, не симметричный
// пул) -- ровно то отличие, ради которого затевалась вся правка ("добавление
// 3-го предмета -- не то же самое, что и 3 предмета сразу").
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyIngredientOrderAffectsResultTest,
    "ProjectHerbalist.PipelineV2.ApplyIngredientOrderAffectsResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyIngredientOrderAffectsResultTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    FInventoryItem A = MakeIngredient(TEXT("A"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.8f, 1.f);
    FInventoryItem B = MakeIngredient(TEXT("B"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.8f, 1.f);
    FInventoryItem C = MakeIngredient(TEXT("C"), 0.5f, 0.51f, 0.51f, 0.51f, 0.51f, 0.51f, /*Corruption*/ 0.1f, 1.f);
    FInventoryItem Water = MakeWater(0.5f, 0.5f, 1);

    // A+B согласны (оба Corruption=0.8) и должны успеть усилиться ДО того,
    // как в реакцию вступит конфликтующий C -- если C идёт первым/вторым,
    // накопленный результат к моменту его прихода другой, и его конфликт
    // проявляется иначе.
    FRandomStream RngABC(11);
    FStateDelta DeltaABC = RunApply({ A, B, C, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), RngABC, WorldSnap, BiomeSnap);
    FRandomStream RngCAB(11);
    FStateDelta DeltaCAB = RunApply({ C, A, B, Water }, /*bIsCrafting*/ true, FIntPoint(5, 5), RngCAB, WorldSnap, BiomeSnap);

    const FInventoryOperation* OpABC = nullptr;
    for (const FInventoryOperation& Op : DeltaABC.InventoryOps) if (Op.OpType == EInventoryOpType::Add) OpABC = &Op;
    const FInventoryOperation* OpCAB = nullptr;
    for (const FInventoryOperation& Op : DeltaCAB.InventoryOps) if (Op.OpType == EInventoryOpType::Add) OpCAB = &Op;
    if (!TestNotNull(TEXT("ABC op present"), OpABC) || !TestNotNull(TEXT("CAB op present"), OpCAB)) return false;

    TestFalse(TEXT("Same three ingredients in a different order give a different Magnitude"),
        FMath::IsNearlyEqual(OpABC->Ingredient.State.Magnitude, OpCAB->Ingredient.State.Magnitude, 0.0005f));
    return true;
}

// ---------------------------------------------------------------------------
// Bifurcation: Purified -- Stability=1.0 у единственного ингредиента выживает
// без изменений через Fold/Zaryana (Lerp(1,1,x)=1 при любом x), поэтому
// `Rng.FRand() < 1.0` истинно ГАРАНТИРОВАННО, независимо от сида (FRand() в
// [0,1), никогда не возвращает ровно 1.0). Не зависит от PRNG -- прогнано на
// нескольких разных сидах, чтобы явно это подтвердить.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyBifurcationPurifiedTest,
    "ProjectHerbalist.PipelineV2.ApplyBifurcationPurified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyBifurcationPurifiedTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap; // Contexts пуст -> BiomeCtx=nullptr -> EffectiveMorok=0

    for (int32 Seed : { 1, 2, 3, 100, 9999 })
    {
        // Distortion=0.98 -- после Zaryana push (Pow с показателем чуть >1)
        // остаётся выше CollapseThreshold(0.85) при любом разумном ZaryanaStrength.
        TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.98f, /*Stability*/ 1.0f, 0.5f), MakeWater(0.5f, 0.5f) };
        FRandomStream Rng(Seed);
        FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

        const FInventoryOperation* AddOp = nullptr;
        for (const FInventoryOperation& Op : Delta.InventoryOps)
        {
            if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
        }
        if (!TestNotNull(FString::Printf(TEXT("[seed=%d] Add op present"), Seed), AddOp)) continue;

        TestEqual(FString::Printf(TEXT("[seed=%d] Distortion snapped to 0.4 (Purified)"), Seed),
            AddOp->Ingredient.State.Meta.Distortion, 0.4f);
        TestTrue(FString::Printf(TEXT("[seed=%d] Stability stays clamped at 1.0 (+0.2 clamps)"), Seed),
            FMath::IsNearlyEqual(AddOp->Ingredient.State.Meta.Stability, 1.0f, 0.001f));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Bifurcation: Catastrophe -- одиночный ингредиент с Purity=Stability=0
// прижимает Coherence к минимуму, достижимому с одним ингредиентом (0.5,
// AxisAgreement всегда =1 для одного ингредиента), что даёт Stability=0.075 на
// входе в Bifurcation (Lerp(0,1.0,0.5*0.15)). Не строго 0 -- зависит от сида
// (~92.5% сидов дают Catastrophe), сид ниже откалиброван реальным прогоном.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyBifurcationCatastropheTest,
    "ProjectHerbalist.PipelineV2.ApplyBifurcationCatastrophe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyBifurcationCatastropheTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("BadHerb"), 0.6f, 0.98f, /*Stability*/ 0.0f, /*Purity*/ 0.0f), MakeWater(0.5f, 0.0f) };
    FRandomStream Rng(7); // откалибровано реальным прогоном -- см. комментарий выше класса
    FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FInventoryOperation* AddOp = nullptr;
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
    }
    if (!TestNotNull(TEXT("Add op present"), AddOp)) return false;

    TestEqual(TEXT("Distortion snapped to 0.2 (Catastrophe)"), AddOp->Ingredient.State.Meta.Distortion, 0.2f);
    return true;
}

// ---------------------------------------------------------------------------
// Применение на клетку (не крафт): клетка меняется, HarvestStress растёт на
// фиксированную 0.2, зелье НЕ попадает в инвентарь как отдельный предмет.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyToCellModifiesWorldStateTest,
    "ProjectHerbalist.PipelineV2.ApplyToCellModifiesWorldState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyToCellModifiesWorldStateTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    FGridCell Cell = MakeTargetCell(5, 5);
    Cell.HarvestStress = 0.3f;
    WorldSnap.GridState.Add(FIntPoint(5, 5), Cell);
    FBiomeSnapshot BiomeSnap;

    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.1f, 0.5f, 0.5f), MakeWater(0.5f, 0.5f) };
    FRandomStream Rng(1);
    FStateDelta Delta = RunApply(Ingredients, /*bIsCrafting*/ false, FIntPoint(5, 5), Rng, WorldSnap, BiomeSnap);

    const FGridCell* Modified = Delta.WorldChanges.Find(FIntPoint(5, 5));
    if (!TestNotNull(TEXT("WorldChanges contains target cell"), Modified)) return false;

    TestTrue(TEXT("HarvestStress increased by 0.2"), FMath::IsNearlyEqual(Modified->HarvestStress, 0.5f, 0.0005f));
    TestTrue(TEXT("Cell state within [0,1] range"),
        Modified->State.Magnitude >= 0.f && Modified->State.Magnitude <= 1.f &&
        Modified->State.Meta.Distortion >= 0.f && Modified->State.Meta.Distortion <= 1.f);

    // Не крафт -- никакого "Potion" в инвентаре не должно появиться.
    for (const FInventoryOperation& Op : Delta.InventoryOps)
    {
        TestTrue(TEXT("No Potion add-op when applying to a cell"),
            !(Op.OpType == EInventoryOpType::Add && Op.Ingredient.IngredientID == FName(TEXT("Potion"))));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Передозировка (HerbalistCoreMath::ApplyOverdosePenalty) -- завязана только
// на ветку "применение на клетку", крафт её не должен видеть вовсе (сама
// функция уже юнит-тестирована в OverdoseTest.cpp -- здесь проверяем именно
// то, что ProcessApplyCommand подключает её только к одной из двух веток).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyOverdoseOnlyOnCellTest,
    "ProjectHerbalist.PipelineV2.ApplyOverdoseOnlyOnCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyOverdoseOnlyOnCellTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    // Potency=0.95 -- далеко за порогом передозировки (0.75 по умолчанию).
    // Distortion/Stability низкие, чтобы не задеть Bifurcation отдельно.
    TArray<FInventoryItem> HighPotency = { MakeIngredient(TEXT("Strong"), 0.6f, 0.1f, 0.6f, 0.5f, /*Potency*/ 0.95f), MakeWater(0.5f, 0.5f) };

    FRandomStream RngCell(5);
    FStateDelta CellDelta = RunApply(HighPotency, /*bIsCrafting*/ false, FIntPoint(5, 5), RngCell, WorldSnap, BiomeSnap);
    const FGridCell* Modified = CellDelta.WorldChanges.Find(FIntPoint(5, 5));
    if (!TestNotNull(TEXT("Cell modified"), Modified)) return false;

    FRandomStream RngCraft(5);
    FStateDelta CraftDelta = RunApply(HighPotency, /*bIsCrafting*/ true, FIntPoint(5, 5), RngCraft, WorldSnap, BiomeSnap);
    const FInventoryOperation* PotionOp = nullptr;
    for (const FInventoryOperation& Op : CraftDelta.InventoryOps)
    {
        if (Op.OpType == EInventoryOpType::Add) PotionOp = &Op;
    }
    if (!TestNotNull(TEXT("Craft potion op present"), PotionOp)) return false;

    // Тот же сид и те же ингредиенты -- PotionState до передозировки идентичен
    // в обеих ветках (Bifurcation не должен сработать здесь, Distortion низкий).
    // Применённая на клетку версия обязана быть ХУЖЕ (пенализирована), крафт --
    // нет: Stability ниже, Distortion выше на клетке, чем в зелье-предмете.
    TestTrue(TEXT("Applied-to-cell Stability is penalized below the unpenalized craft value"),
        Modified->State.Meta.Stability < PotionOp->Ingredient.State.Meta.Stability - KINDA_SMALL_NUMBER);
    TestTrue(TEXT("Applied-to-cell Distortion is penalized above the unpenalized craft value"),
        Modified->State.Meta.Distortion > PotionOp->Ingredient.State.Meta.Distortion + KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// Капище, эффект 2 (§15.5/§11.7): Restoration в радиусе поднимает Coherence,
// что при прочих равных должно поднять итоговую Stability зелья (Zaryana push
// монотонен по ZaryanaStrength, который монотонен по Coherence).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyShrineCoherenceBonusTest,
    "ProjectHerbalist.PipelineV2.ApplyShrineCoherenceBonus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyShrineCoherenceBonusTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;

    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.3f, 0.4f, 0.4f), MakeWater(0.5f, 0.4f) };

    FRandomStream RngNoShrine(3);
    FStateDelta NoShrineDelta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), RngNoShrine, WorldSnap, BiomeSnap);

    FWorldSnapshot WorldSnapWithShrine = WorldSnap;
    FShrine S;
    S.Cell = FIntPoint(5, 5);
    S.Type = EShrineType::Ancestral;
    S.Restoration = 0.9f;
    WorldSnapWithShrine.Shrines.Add(S);

    FRandomStream RngWithShrine(3);
    FStateDelta ShrineDelta = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), RngWithShrine, WorldSnapWithShrine, BiomeSnap);

    const FInventoryOperation* NoShrinePotion = nullptr;
    for (const FInventoryOperation& Op : NoShrineDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) NoShrinePotion = &Op;
    const FInventoryOperation* ShrinePotion = nullptr;
    for (const FInventoryOperation& Op : ShrineDelta.InventoryOps) if (Op.OpType == EInventoryOpType::Add) ShrinePotion = &Op;
    if (!TestNotNull(TEXT("No-shrine potion op"), NoShrinePotion) || !TestNotNull(TEXT("Shrine potion op"), ShrinePotion))
        return false;

    TestTrue(TEXT("Shrine-boosted Coherence raises Stability at the same seed"),
        ShrinePotion->Ingredient.State.Meta.Stability > NoShrinePotion->Ingredient.State.Meta.Stability + KINDA_SMALL_NUMBER);
    TestTrue(TEXT("Recorded Coherence itself is higher with the shrine bonus"),
        ShrinePotion->Coherence > NoShrinePotion->Coherence + KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// Biome Context Injection: высокий MorokField/Affinity узла биом-графа должен
// поднимать итоговый Distortion зелья относительно того же прогона без контекста.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyBiomeContextRaisesDistortionTest,
    "ProjectHerbalist.PipelineV2.ApplyBiomeContextRaisesDistortion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyBiomeContextRaisesDistortionTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));

    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.3f, 0.4f, 0.4f), MakeWater(0.5f, 0.4f) };

    // Biome Context Injection применяется только при варке НА клетку в мире
    // (bIsCrafting=false) -- при крафте TargetCell всегда nullptr по построению
    // (см. ProcessApplyCommand: "При крафте... контекста нет"), поэтому здесь
    // обязательно применение на клетку, не крафт.
    FBiomeSnapshot EmptyBiomeSnap;
    FRandomStream RngNoBiome(11);
    FStateDelta NoBiomeDelta = RunApply(Ingredients, /*bIsCrafting*/ false, FIntPoint(5, 5), RngNoBiome, WorldSnap, EmptyBiomeSnap);

    FBiomeSnapshot MorokBiomeSnap;
    FBiomeFieldContext Ctx;
    Ctx.MorokField = 0.9f;
    Ctx.MorokAffinity = 1.0f;
    MorokBiomeSnap.Contexts.Add(FBiomeDefaults::BiomeTypeToName(EBiomeType::MixedForest), Ctx);
    FRandomStream RngWithMorok(11);
    FStateDelta MorokDelta = RunApply(Ingredients, /*bIsCrafting*/ false, FIntPoint(5, 5), RngWithMorok, WorldSnap, MorokBiomeSnap);

    const FGridCell* NoBiomeCell = NoBiomeDelta.WorldChanges.Find(FIntPoint(5, 5));
    const FGridCell* MorokCell = MorokDelta.WorldChanges.Find(FIntPoint(5, 5));
    if (!TestNotNull(TEXT("No-biome cell modified"), NoBiomeCell) || !TestNotNull(TEXT("Morok cell modified"), MorokCell))
        return false;

    TestTrue(TEXT("High MorokField biome context raises Distortion at the same seed"),
        MorokCell->State.Meta.Distortion > NoBiomeCell->State.Meta.Distortion + KINDA_SMALL_NUMBER);

    // Footprint пишется при применении на любую привязанную к биому клетку --
    // не зависит от того, нашёлся ли реальный контекст в BiomeSnap.Contexts
    // (TargetBiomeID резолвится из Cell.Biome напрямую).
    TestEqual(TEXT("Footprint recorded even without a matching biome context"), NoBiomeDelta.Footprints.Num(), 1);
    TestEqual(TEXT("Footprint recorded with a matching biome context"), MorokDelta.Footprints.Num(), 1);
    if (MorokDelta.Footprints.Num() == 1)
    {
        TestEqual(TEXT("Footprint biome ID matches target cell's biome"),
            MorokDelta.Footprints[0].BiomeID, FBiomeDefaults::BiomeTypeToName(EBiomeType::MixedForest));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Детерминизм: тот же сид + те же входные данные -> побитово идентичный
// результат. Тот же архитектурный принцип, что и у TraceReplay для полного
// тика, здесь -- узко для одной команды Apply.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyIsDeterministicTest,
    "ProjectHerbalist.PipelineV2.ApplyIsDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyIsDeterministicTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;
    FBiomeFieldContext Ctx;
    Ctx.MorokField = 0.6f;
    Ctx.ZaryanaField = 0.4f;
    BiomeSnap.Contexts.Add(FBiomeDefaults::BiomeTypeToName(EBiomeType::MixedForest), Ctx);

    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.4f, 0.5f, 0.4f), MakeWater(0.5f, 0.4f) };

    FRandomStream RngA(777);
    FStateDelta DeltaA = RunApply(Ingredients, /*bIsCrafting*/ false, FIntPoint(5, 5), RngA, WorldSnap, BiomeSnap);

    FRandomStream RngB(777);
    FStateDelta DeltaB = RunApply(Ingredients, /*bIsCrafting*/ false, FIntPoint(5, 5), RngB, WorldSnap, BiomeSnap);

    const FGridCell* CellA = DeltaA.WorldChanges.Find(FIntPoint(5, 5));
    const FGridCell* CellB = DeltaB.WorldChanges.Find(FIntPoint(5, 5));
    if (!TestNotNull(TEXT("Cell A modified"), CellA) || !TestNotNull(TEXT("Cell B modified"), CellB)) return false;

    TestEqual(TEXT("Magnitude identical"), CellA->State.Magnitude, CellB->State.Magnitude);
    TestEqual(TEXT("Distortion identical"), CellA->State.Meta.Distortion, CellB->State.Meta.Distortion);
    TestEqual(TEXT("Stability identical"), CellA->State.Meta.Stability, CellB->State.Meta.Stability);
    TestEqual(TEXT("Purity identical"), CellA->State.Meta.Purity, CellB->State.Meta.Purity);
    TestEqual(TEXT("Corruption identical"), CellA->State.Meta.Corruption, CellB->State.Meta.Corruption);
    TestEqual(TEXT("Direction.Body identical"), CellA->State.Direction.Body, CellB->State.Direction.Body);
    return true;
}

// ---------------------------------------------------------------------------
// Cmd.Intent.Coherence, заданный вызывающей стороной, обязан игнорироваться --
// "Intent_system не может быть решением игрока/UI, только функцией процесса"
// (комментарий у ProcessApplyCommand). Один и тот же набор ингредиентов даёт
// один и тот же посчитанный Coherence независимо от того, что подсунул вызывающий.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPipelineV2ApplyIgnoresCallerCoherenceTest,
    "ProjectHerbalist.PipelineV2.ApplyIgnoresCallerCoherence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPipelineV2ApplyIgnoresCallerCoherenceTest::RunTest(const FString& Parameters)
{
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(5, 5), MakeTargetCell(5, 5));
    FBiomeSnapshot BiomeSnap;
    TArray<FInventoryItem> Ingredients = { MakeIngredient(TEXT("Herb"), 0.6f, 0.3f, 0.4f, 0.4f), MakeWater(0.5f, 0.4f) };

    FRandomStream RngSentinelLow(9);
    FStateDelta DeltaLow = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), RngSentinelLow, WorldSnap, BiomeSnap, /*CallerCoherence*/ 0.0f);

    FRandomStream RngSentinelHigh(9);
    FStateDelta DeltaHigh = RunApply(Ingredients, /*bIsCrafting*/ true, FIntPoint(5, 5), RngSentinelHigh, WorldSnap, BiomeSnap, /*CallerCoherence*/ 0.999f);

    const FInventoryOperation* PotionLow = nullptr;
    for (const FInventoryOperation& Op : DeltaLow.InventoryOps) if (Op.OpType == EInventoryOpType::Add) PotionLow = &Op;
    const FInventoryOperation* PotionHigh = nullptr;
    for (const FInventoryOperation& Op : DeltaHigh.InventoryOps) if (Op.OpType == EInventoryOpType::Add) PotionHigh = &Op;
    if (!TestNotNull(TEXT("Low-sentinel potion op"), PotionLow) || !TestNotNull(TEXT("High-sentinel potion op"), PotionHigh))
        return false;

    TestEqual(TEXT("Recorded Coherence identical regardless of caller-supplied sentinel"),
        PotionLow->Coherence, PotionHigh->Coherence);
    TestTrue(TEXT("Recorded Coherence is neither caller sentinel verbatim"),
        !FMath::IsNearlyEqual(PotionLow->Coherence, 0.0f, 0.001f) && !FMath::IsNearlyEqual(PotionLow->Coherence, 0.999f, 0.001f));
    return true;
}

#endif
