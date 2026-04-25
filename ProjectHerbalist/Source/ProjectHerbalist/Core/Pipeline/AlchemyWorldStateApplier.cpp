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

void FAlchemyWorldStateApplier::ApplyDistortionDelta(FMemoryState& Memory, float Delta, float CurrentTime)
{
    if (FMath::IsNearlyZero(Delta)) return;

    const float CurrentDistortion = Memory.AccumulatedDistortion;
    const float Saturation = GetDistortionSaturation(CurrentDistortion);
    const float EffectiveDelta = Delta * Saturation;

    Memory.AccumulatedDistortion = FMath::Clamp(CurrentDistortion + EffectiveDelta, 0.0f, 0.95f);

    const float DeltaTime = CurrentTime - Memory.TimeOfLastDistortionChange;
    if (DeltaTime > KINDA_SMALL_NUMBER)
    {
        Memory.DistortionVelocity = EffectiveDelta / DeltaTime;
    }

    Memory.TimeOfLastDistortionChange = CurrentTime;
}

float FAlchemyWorldStateApplier::GetDistortionSaturation(float CurrentDistortion)
{
    if (CurrentDistortion < 0.8f) return 1.0f;
    if (CurrentDistortion < 0.92f)
    {
        const float t = (CurrentDistortion - 0.8f) / 0.12f;
        return FMath::Lerp(1.0f, 0.1f, t);
    }
    return 0.05f;
}
