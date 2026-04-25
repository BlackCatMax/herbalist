// AlchemySemanticResolver.cpp
#include "AlchemySemanticResolver.h"
#include "Core/Pipeline/IntentResolver.h"

FAlchemySemanticResult FAlchemySemanticResolver::Resolve(const TArray<FAlchemyAtom>& Atoms, float GlobalDistortion)
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

    // Теперь передаём коэффициенты явно (значения из бывших настроек по умолчанию)
    // Это убирает зависимость от UHerbalistSettings внутри ComputeIntentCoherence
    constexpr float FoldWeightDecay = 0.8f;
    constexpr float CatalystBonus = 0.1f;
    constexpr float UnknownPenalty = 0.15f;
    constexpr float EssenceBonus = 0.05f;
    constexpr float WaterBonusFactor = 0.2f;

    Result.Coherence = HerbalistCore::ComputeIntentCoherence(
        Result.IngredientAtoms,
        Result.WaterAtoms,
        GlobalDistortion,
        FoldWeightDecay,
        CatalystBonus,
        UnknownPenalty,
        EssenceBonus,
        WaterBonusFactor
    );

    return Result;
}
