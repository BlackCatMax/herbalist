// AlchemyPhysicsPipeline.cpp
#include "AlchemyPhysicsPipeline.h"
#include "Core/Pipeline/HerbalistPipeline.h"
#include "Core/Pipeline/AlchemyTypes.h"

FAlchemyPhysicsResult FAlchemyPhysicsPipeline::Run(
    const TArray<FRealState>& IngredientStates,
    const TArray<FRealState>& WaterStates,
    const FRealState& CellState,
    const FEnvironment& Env,
    const FMemoryState& Memory,
    float Coherence,
    FRngState& Rng,
    float MorokField,
    float ZaryanaField,
    const FVector4& AxisDrift)
{
    FAlchemyPhysicsResult Result;

    // Собираем совместимый с текущим ApplyMorok список
    TArray<FInventoryItem> Items;
    for (const FRealState& S : IngredientStates)
    {
        FInventoryItem Item;
        Item.State = S;
        Items.Add(Item);
    }
    for (const FRealState& S : WaterStates)
    {
        FInventoryItem Item;
        Item.State = S;
        Item.IngredientID = FName(TEXT("Water"));
        Items.Add(Item);
    }

    // Создаём Intent из вычисленной Coherence
    FIntent Intent;
    Intent.Coherence = Coherence;

    Result.State = HerbalistCore::Pipeline::ApplyMorok(
        Items,
        CellState,
        Env,
        Memory,
        Intent,
        Rng,
        MorokField,
        ZaryanaField,
        AxisDrift,
        nullptr // IngredientRegistry не нужен при маркировке Water через IngredientID
    );

    if (Result.State.Meta.Distortion > 0.92f)
    {
        bool bTrigger = ShouldTriggerCatastrophe(Result.State, Memory, Rng);
        if (bTrigger)
        {
            Result.bCatastropheTriggered = true;
            Result.bCollapse = (HerbalistCore::Random01(Rng) < 0.5f);
        }
    }

    return Result;
}

bool FAlchemyPhysicsPipeline::ShouldTriggerCatastrophe(const FRealState& State, const FMemoryState& Memory, FRngState& Rng)
{
    const float Threshold = 0.92f;
    float Excess = (State.Meta.Distortion - Threshold) / (1.0f - Threshold);
    float BaseChance = FMath::Pow(Excess, 1.5f);
    float Instability = (1.0f - State.Meta.Stability) * (1.0f - State.Meta.Purity);
    float Factor = FMath::Lerp(0.5f, 1.0f, Instability);
    float MemoryFactor = 1.0f - Memory.StabilityMemory * 0.7f;
    float Chance = FMath::Clamp(BaseChance * Factor * MemoryFactor, 0.0f, 0.95f);
    return HerbalistCore::Random01(Rng) < Chance;
}