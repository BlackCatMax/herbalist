// Core/Pipeline/AlchemyPipelineFacade.cpp
#include "AlchemyPipelineFacade.h"
#include "AlchemyTypes.h"
#include "AlchemySemantics.h"
#include "AlchemySemanticResolver.h"
#include "AlchemyPhysicsPipeline.h"
#include "AlchemyWorldStateApplier.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"

FAlchemyFacadeResult FAlchemyPipelineFacade::Execute(
    const TArray<FInventoryItem>& Items,
    const FRealState& CellState,
    const FEnvironment& Env,
    const FMemoryState& Memory,
    float GlobalDistortion,
    UIngredientRegistrySubsystem* IngredientReg,
    float BiomeMorokField,
    float BiomeZaryanaField,
    const FVector4& BiomeAxisDrift,
    FRngState& Rng)
{
    FAlchemyFacadeResult Result;

    // 1. Конвертация в атомы
    TArray<FAlchemyAtom> Atoms;
    for (const FInventoryItem& Item : Items)
    {
        bool bIsWater = false;
        EIngredientClass Class = EIngredientClass::Unknown;

        if (IngredientReg)
        {
            bIsWater = IngredientReg->IsWater(Item.IngredientID);
            Class = IngredientReg->Classify(Item.IngredientID);
        }

        Atoms.Add(FAlchemyAtom(
            Item.IngredientID,
            bIsWater,
            Item.State,
            Class,
            EAtomOrigin::Harvest,
            GlobalDistortion,
            0.0f
        ));
    }

    // 2. Семантическое разрешение
    FAlchemySemanticResult Semantic = FAlchemySemanticResolver::Resolve(Atoms, GlobalDistortion);

    // 3. Физический расчёт
    if (Semantic.Outcome == EAlchemyOutcome::Valid)
    {
        TArray<FRealState> IngredientStates, WaterStates;
        for (const auto& A : Semantic.IngredientAtoms) IngredientStates.Add(A.State);
        for (const auto& A : Semantic.WaterAtoms) WaterStates.Add(A.State);

        FAlchemyPhysicsResult Physics = FAlchemyPhysicsPipeline::Run(
            IngredientStates, WaterStates,
            CellState, Env, Memory,
            Semantic.Coherence,
            Rng, BiomeMorokField, BiomeZaryanaField, BiomeAxisDrift);

        FRealState State = Physics.State;
        if (Physics.bCatastropheTriggered)
        {
            State = HerbalistCore::ApplyCatastropheTransform(State, Physics.bCollapse, Rng);
            Result.Outcome = EAlchemyOutcome::Catastrophe;
        }
        else
        {
            Result.Outcome = EAlchemyOutcome::Valid;
        }
        Result.FinalState = State;
    }
    else if (Semantic.Outcome == EAlchemyOutcome::BoiledWater)
    {
        TArray<FRealState> WaterStates;
        for (const auto& A : Semantic.WaterAtoms) WaterStates.Add(A.State);
        Result.FinalState = HerbalistCore::ApplyBoiledWaterTransform(WaterStates);
        Result.Outcome = EAlchemyOutcome::BoiledWater;
    }
    else
    {
        Result.FinalState = HerbalistCore::ApplyAshTransform(FMeta{});
        Result.Outcome = EAlchemyOutcome::Ash;
    }

    return Result;
}