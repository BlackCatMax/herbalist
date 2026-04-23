// AlchemySemanticResolver.cpp
#include "AlchemySemanticResolver.h"
#include "Core/Pipeline/IntentResolver.h"

FAlchemySemanticResult FAlchemySemanticResolver::Resolve(const TArray<FAlchemyAtom>& Atoms)
{
    FAlchemySemanticResult Result;

    bool bHasWater = false;
    bool bHasIngredient = false;
    for (const FAlchemyAtom& Atom : Atoms)
    {
        if (Atom.bIsWater)
        {
            bHasWater = true;
            Result.WaterAtoms.Add(Atom);
        }
        else if (!Atom.SourceID.IsNone())
        {
            bHasIngredient = true;
            Result.IngredientAtoms.Add(Atom);
        }
    }

    if (!bHasWater) Result.Outcome = EAlchemyOutcome::Ash;
    else if (!bHasIngredient) Result.Outcome = EAlchemyOutcome::BoiledWater;
    else Result.Outcome = EAlchemyOutcome::Valid;

    // Вычисляем Coherence на основе порядка и качества ингредиентов
    Result.Coherence = HerbalistCore::ComputeIntentCoherence(
        Result.IngredientAtoms,
        Result.WaterAtoms);

    return Result;
}