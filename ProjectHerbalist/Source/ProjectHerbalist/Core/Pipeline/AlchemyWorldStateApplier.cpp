// AlchemyWorldStateApplier.cpp
#include "AlchemyWorldStateApplier.h"
#include "AlchemySemantics.h"

FRealState FAlchemyWorldStateApplier::Apply(
    const FAlchemySemanticResult& Semantic,
    const FAlchemyPhysicsResult& Physics,
    FRngState& Rng)
{
    switch (Semantic.Outcome)
    {
    case EAlchemyOutcome::Ash:
        return HerbalistCore::ApplyAshTransform(FMeta{});

    case EAlchemyOutcome::BoiledWater:
    {
        TArray<FRealState> WaterStates;
        for (const FAlchemyAtom& Atom : Semantic.WaterAtoms)
            WaterStates.Add(Atom.State);
        return HerbalistCore::ApplyBoiledWaterTransform(WaterStates);
    }

    case EAlchemyOutcome::Valid:
    default:
    {
        FRealState NewState = Physics.State;
        if (Physics.bCatastropheTriggered)
        {
            NewState = HerbalistCore::ApplyCatastropheTransform(NewState, Physics.bCollapse, Rng);
        }
        return NewState;
    }
    }
}