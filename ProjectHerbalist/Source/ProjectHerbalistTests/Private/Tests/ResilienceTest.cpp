// Source/ProjectHerbalistTests/Private/Tests/ResilienceTest.cpp
//
// Регрессия для GenerateHarvestResult (PipelineV2.cpp, "звено 3" связности
// зависимостей — DESIGN_World_State.md §8/§15): проверяет, что
// IngredientTableRow::Resilience действительно управляет тем, насколько
// сбор подчиняется месту, а не собственной природе ингредиента.
//
// Найдено при аудите 2026-08-24: ни один из 71 ингредиента компендиума не
// задавал Resilience осознанно (extract_ingredients.py даже не читал такое
// поле из frontmatter) — все молчаливо получали дефолт 0.0 (клетка
// полностью подавляет базовое состояние). Добавлены пять диагностических
// карточек (herbalist_docs/.../Дубовая кора.md, Дождевик.md, Плакун-трава.md,
// Белена.md, Аконит.md), каждая целится в конкретную границу формулы —
// этот файл превращает две из них (эталоны 1.0 и 0.0) в автотест, а не
// только в лорный текст.
//
// Тестируем через полный FCommandBatch -> ExecutePipeline, а не напрямую
// статическую GenerateHarvestResult() — она не экспортирована из PipelineV2.cpp
// (тот же паттерн, что PipelineV2Test.cpp).
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
    // Клетка Болота на грани деградации — тот же характер, что дефолты
    // DT_BiomeDefaults (Corruption ~0.70, Purity низкая), чтобы разрыв с
    // "чистым" базовым состоянием ингредиента был однозначным.
    FGridCell MakeBogCell()
    {
        FGridCell Cell;
        Cell.X = 2;
        Cell.Y = 2;
        Cell.Biome = EBiomeType::Bog;
        Cell.bIsWater = false;
        Cell.State.Magnitude = 0.6f;
        Cell.State.Direction.Body   = 0.1f;
        Cell.State.Direction.Mind   = 0.1f;
        Cell.State.Direction.Spirit = 0.1f;
        Cell.State.Direction.Nature = 0.7f;
        Cell.State.Direction.NormalizeSum();
        Cell.State.Meta.Purity     = 0.15f;
        Cell.State.Meta.Corruption = 0.75f;
        Cell.State.Meta.Distortion = 0.65f;
        Cell.State.Meta.Stability  = 0.25f;
        return Cell;
    }

    // Дубовая кора (herbalist_docs/.../Дубовая кора.md): Resilience=1.0,
    // Purity=0.8, Corruption=0.05 — резко чище клетки Болота.
    FRealState MakeOakBarkBase()
    {
        FRealState S;
        S.Magnitude = 0.6f;
        S.Direction.Body = 0.7f; S.Direction.Mind = 0.1f;
        S.Direction.Spirit = 0.4f; S.Direction.Nature = 0.6f;
        S.Direction.NormalizeSum();
        S.Meta.Purity = 0.8f; S.Meta.Corruption = 0.05f;
        S.Meta.Distortion = 0.1f; S.Meta.Stability = 0.9f;
        return S;
    }

    // Дождевик (herbalist_docs/.../Дождевик.md): Resilience=0.0,
    // Purity=0.5, Corruption=0.2 — тоже чище клетки, но её не держит вовсе.
    FRealState MakePuffballBase()
    {
        FRealState S;
        S.Magnitude = 0.2f;
        S.Direction.Body = 0.3f; S.Direction.Mind = 0.3f;
        S.Direction.Spirit = 0.2f; S.Direction.Nature = 0.3f;
        S.Direction.NormalizeSum();
        S.Meta.Purity = 0.5f; S.Meta.Corruption = 0.2f;
        S.Meta.Distortion = 0.3f; S.Meta.Stability = 0.4f;
        return S;
    }

    FGridCell HarvestOnce(const FGridCell& Cell, const FRealState& BaseState, float Resilience, int32 Seed)
    {
        FWorldSnapshot WorldSnap;
        WorldSnap.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
        WorldSnap.WorldSeed = Seed;

        FInventorySnapshot InvSnap;
        FBiomeSnapshot BiomeSnap;

        FCommandBatch CmdBatch;
        FCommandEntry CmdEntry;
        CmdEntry.Primitive = ECommandPrimitive::Harvest;
        CmdEntry.Harvest.TargetCell = FIntPoint(Cell.X, Cell.Y);
        CmdEntry.Harvest.IngredientID = TEXT("ResilienceProbe");
        CmdEntry.Harvest.Amount = 1;
        CmdEntry.Harvest.BaseState = BaseState;
        CmdEntry.Harvest.Resilience = Resilience;
        CmdBatch.AddCommand(CmdEntry);

        FRandomStream Rng(Seed);
        FStateDelta Delta = Simulation::ExecutePipeline(WorldSnap, InvSnap, BiomeSnap, CmdBatch, Rng);

        // Урожай появляется как InventoryOp, не WorldChanges — сама клетка
        // после Harvest тоже меняется (HarvestStress), но нас интересует
        // состояние СОБРАННОГО предмета, не клетки.
        check(Delta.InventoryOps.Num() == 1);
        FGridCell Result = Cell;
        Result.State = Delta.InventoryOps[0].Ingredient.State;
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FResilienceMaxIgnoresPlaceTest,
    "Herbalist.Resilience.MaxResilienceIgnoresPlace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FResilienceMaxIgnoresPlaceTest::RunTest(const FString& Parameters)
{
    // Resilience=1.0 -> K=0 независимо от HarvestBiomeWeight (Settings может
    // быть недоступен в тестовом окружении, см. Bistability тест) — Дубовая
    // кора должна выйти из Болота ровно такой, какой её задал compendium.
    const FGridCell Cell = MakeBogCell();
    const FRealState Base = MakeOakBarkBase();
    const FGridCell Harvested = HarvestOnce(Cell, Base, /*Resilience=*/1.0f, /*Seed=*/1001);

    TestEqual(TEXT("Purity unchanged by Bog"), Harvested.State.Meta.Purity, Base.Meta.Purity, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("Corruption unchanged by Bog"), Harvested.State.Meta.Corruption, Base.Meta.Corruption, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("Stability unchanged by Bog"), Harvested.State.Meta.Stability, Base.Meta.Stability, KINDA_SMALL_NUMBER);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FResilienceZeroYieldsMoreToPlaceTest,
    "Herbalist.Resilience.ZeroResilienceYieldsMoreToPlace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FResilienceZeroYieldsMoreToPlaceTest::RunTest(const FString& Parameters)
{
    // Сравнение, а не точное число: не завязываемся на конкретный
    // HarvestBiomeWeight (может отличаться в тестовом окружении, где
    // UHerbalistSettings не проинициализирован игровым режимом) — только
    // на направление эффекта, который и проверяем.
    const FGridCell Cell = MakeBogCell();
    const FRealState OakBase = MakeOakBarkBase();
    const FRealState PuffballBase = MakePuffballBase();

    const FGridCell OakHarvested      = HarvestOnce(Cell, OakBase,      1.0f, 2001);
    const FGridCell PuffballHarvested = HarvestOnce(Cell, PuffballBase, 0.0f, 2002);

    // Оба базовых состояния чище Болота (Purity 0.8 и 0.5 против 0.15
    // клетки) — при Resilience=0 итог должен просесть к болотной Purity
    // заметно больше, чем при Resilience=1 (который её не просаживает вовсе).
    const float OakDrop      = OakBase.Meta.Purity      - OakHarvested.State.Meta.Purity;
    const float PuffballDrop = PuffballBase.Meta.Purity - PuffballHarvested.State.Meta.Purity;

    TestTrue(TEXT("Resilience=1.0 сохраняет Purity почти без просадки"), OakDrop < 0.01f);
    TestTrue(TEXT("Resilience=0.0 просаживает Purity к болоту заметно сильнее"), PuffballDrop > 0.05f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FResilienceMonotonicTest,
    "Herbalist.Resilience.EffectIsMonotonic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FResilienceMonotonicTest::RunTest(const FString& Parameters)
{
    // Рост Resilience от 0 к 1 должен монотонно приближать Purity собранного
    // предмета к базовому Purity ингредиента (Плакун-трава: Resilience=0.6,
    // между двумя эталонами) — растущая сопротивляемость не должна давать
    // немонотонный (например, U-образный) отклик формулы.
    const FGridCell Cell = MakeBogCell();
    const FRealState Base = MakeOakBarkBase();

    float PrevGap = TNumericLimits<float>::Max();
    bool bMonotonic = true;
    for (float Resilience : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const FGridCell Harvested = HarvestOnce(Cell, Base, Resilience, 3000 + FMath::RoundToInt(Resilience * 100));
        const float Gap = FMath::Abs(Base.Meta.Purity - Harvested.State.Meta.Purity);
        if (Gap > PrevGap + KINDA_SMALL_NUMBER)
        {
            bMonotonic = false;
        }
        PrevGap = Gap;
    }

    TestTrue(TEXT("Разрыв с базовым Purity убывает монотонно по мере роста Resilience"), bMonotonic);
    return true;
}

#endif
