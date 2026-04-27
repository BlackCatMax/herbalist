// AlchemySemantics.cpp
#include "AlchemySemantics.h"
#include "ProjectHerbalist.h"

namespace HerbalistCore
{
    EAlchemyOutcome ClassifyOutcome(const TArray<FInventoryItem>& Inputs)
    {
        if (Inputs.Num() == 0) return EAlchemyOutcome::Ash;

        bool bHasWater = false;
        bool bHasIngredient = false;
        for (const FInventoryItem& Item : Inputs)
        {
            // Вoda определяется по ID, совпадающему с "Water", или по пустому ID (для сырых FRealState)
            if (Item.IngredientID == FName(TEXT("Water")) || Item.IngredientID.IsNone())
                bHasWater = true;
            else if (!Item.IngredientID.IsNone())
                bHasIngredient = true;
        }

        if (!bHasWater) return EAlchemyOutcome::Ash;
        if (!bHasIngredient) return EAlchemyOutcome::BoiledWater;
        return EAlchemyOutcome::Valid;
    }

    FRealState ApplyAshTransform(const FMeta& CoreMeta)
    {
        FRealState Ash;
        Ash.Magnitude = 0.1f;
        Ash.Meta.Distortion = FMath::Clamp(0.9f + CoreMeta.Distortion * 0.1f, 0.8f, 1.0f);
        Ash.Meta.Corruption = 0.9f;
        Ash.Meta.Purity = 0.0f;
        Ash.Meta.Stability = 0.0f;
        Ash.Meta.Potency = 0.0f;
        Ash.Meta.Resonance = 0.0f;
        Ash.Direction.Body = Ash.Direction.Mind = Ash.Direction.Spirit = Ash.Direction.Nature = 0.25f;
        Ash.Direction.NormalizeSum();
        return Ash;
    }

    FRealState ApplyBoiledWaterTransform(const TArray<FRealState>& WaterStates)
    {
        if (WaterStates.Num() == 0) return FRealState();

        FRealState Avg;
        Avg.Magnitude = 0.0f;
        int32 N = WaterStates.Num();
        for (const FRealState& W : WaterStates)
        {
            Avg.Magnitude += W.Magnitude;
            Avg.Direction.Body += W.Direction.Body;
            Avg.Direction.Mind += W.Direction.Mind;
            Avg.Direction.Spirit += W.Direction.Spirit;
            Avg.Direction.Nature += W.Direction.Nature;
            Avg.Meta.Purity += W.Meta.Purity;
            Avg.Meta.Stability += W.Meta.Stability;
            Avg.Meta.Corruption += W.Meta.Corruption;
            Avg.Meta.Distortion += W.Meta.Distortion;
        }
        Avg.Magnitude /= N;
        Avg.Direction.Body /= N; Avg.Direction.Mind /= N; Avg.Direction.Spirit /= N; Avg.Direction.Nature /= N;
        Avg.Meta.Purity /= N; Avg.Meta.Stability /= N; Avg.Meta.Corruption /= N; Avg.Meta.Distortion /= N;

        FRealState Boiled = Avg;
        Boiled.Magnitude = FMath::Clamp(Boiled.Magnitude * 0.8f, 0.0f, 1.0f);
        Boiled.Meta.Purity = FMath::Clamp(Boiled.Meta.Purity + 0.2f, 0.0f, 1.0f);
        Boiled.Meta.Distortion = FMath::Clamp(Boiled.Meta.Distortion * 0.2f, 0.0f, 1.0f);
        Boiled.Meta.Corruption = FMath::Clamp(Boiled.Meta.Corruption * 0.2f, 0.0f, 1.0f);
        
        float Sum = Boiled.Direction.Body + Boiled.Direction.Mind + Boiled.Direction.Spirit + Boiled.Direction.Nature;
        if (Sum > KINDA_SMALL_NUMBER)
            Boiled.Direction.NormalizeSum();
        else
            Boiled.Direction.Body = Boiled.Direction.Mind = Boiled.Direction.Spirit = Boiled.Direction.Nature = 0.25f;
        return Boiled;
    }

    FRealState ApplyCatastropheTransform(FRealState& InState, bool bCollapse, FRngState& Rng)
    {
        if (bCollapse)
        {
            InState.Meta.Distortion = FMath::Clamp(InState.Meta.Distortion * 0.3f, 0.1f, 0.4f);
            InState.Meta.Stability = FMath::Clamp(InState.Meta.Stability + 0.1f, 0.0f, 1.0f);
            UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] COLLAPSE! Distortion=%.2f, Stability+0.1"), InState.Meta.Distortion);
        }
        else
        {
            InState.Meta.Distortion = FMath::Clamp(InState.Meta.Distortion * 0.6f, 0.3f, 0.5f);
            float Boost = 0.3f * (1.0f - InState.Meta.Stability);
            InState.Meta.Stability = FMath::Clamp(InState.Meta.Stability + Boost, 0.0f, 1.0f);
            InState.Meta.Purity = FMath::Clamp(InState.Meta.Purity + Boost * 0.8f, 0.0f, 1.0f);
            UE_LOG(LogHerbalist, Warning, TEXT("[CATASTROPHE] PURIFICATION! Distortion=%.2f"), InState.Meta.Distortion);
        }
        return InState;
    }
}