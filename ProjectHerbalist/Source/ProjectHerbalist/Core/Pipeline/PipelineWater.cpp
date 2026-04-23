// PipelineWater.cpp
#include "HerbalistPipeline.h"
#include "ProjectHerbalist.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Harvest/HarvestService.h"
#include "Core/Types/HerbalistIngredient.h"
#include "Core/Data/IngredientRegistry.h"
#include "Math/UnrealMathUtility.h"

namespace HerbalistCore
{
    void Pipeline::SeparateWaterAndIngredients(
        const TArray<FInventoryItem>& Inputs,
        TArray<FRealState>& OutNonWater,
        TArray<FRealState>& OutWater)
    {
        OutNonWater.Empty();
        OutWater.Empty();
        for (const FInventoryItem& Item : Inputs)
        {
            // Проверяем воду через реестр
            bool bIsWater = false;
            if (Item.IngredientID != FName(TEXT("Potion")))
            {
                bIsWater = FIngredientRegistry::IsWater(Item.IngredientID);
            }

            if (bIsWater)
                OutWater.Add(Item.State);
            else
                OutNonWater.Add(Item.State);
        }
    }

    FRealState Pipeline::ProcessWaterOnly(const TArray<FRealState>& WaterStates)
    {
        FRealState AvgWater;
        AvgWater.Magnitude = 0.0f;
        AvgWater.Direction.Body = 0.0f; AvgWater.Direction.Mind = 0.0f;
        AvgWater.Direction.Spirit = 0.0f; AvgWater.Direction.Nature = 0.0f;
        AvgWater.Meta.Purity = 0.0f; AvgWater.Meta.Stability = 0.0f;
        AvgWater.Meta.Corruption = 0.0f; AvgWater.Meta.Potency = 0.0f;

        for (const FRealState& W : WaterStates)
        {
            AvgWater.Magnitude += W.Magnitude;
            AvgWater.Direction.Body += W.Direction.Body;
            AvgWater.Direction.Mind += W.Direction.Mind;
            AvgWater.Direction.Spirit += W.Direction.Spirit;
            AvgWater.Direction.Nature += W.Direction.Nature;
            AvgWater.Meta.Purity += W.Meta.Purity;
            AvgWater.Meta.Stability += W.Meta.Stability;
            AvgWater.Meta.Corruption += W.Meta.Corruption;
            AvgWater.Meta.Potency += W.Meta.Potency;
        }
        float Count = (float)WaterStates.Num();
        AvgWater.Magnitude /= Count;
        AvgWater.Direction.Body /= Count;
        AvgWater.Direction.Mind /= Count;
        AvgWater.Direction.Spirit /= Count;
        AvgWater.Direction.Nature /= Count;
        AvgWater.Meta.Purity /= Count;
        AvgWater.Meta.Stability /= Count;
        AvgWater.Meta.Corruption /= Count;
        AvgWater.Meta.Potency /= Count;
        AvgWater.Direction.NormalizeSum();

        // Кипячение
        AvgWater.Magnitude = FMath::Clamp(AvgWater.Magnitude * 0.8f, 0.0f, 1.0f);
        AvgWater.Meta.Purity = FMath::Clamp(AvgWater.Meta.Purity + 0.2f, 0.0f, 1.0f);
        AvgWater.Meta.Stability = FMath::Clamp(AvgWater.Meta.Stability + 0.1f, 0.0f, 1.0f);
        AvgWater.Meta.Distortion = FMath::Clamp(AvgWater.Meta.Distortion - 0.2f, 0.0f, 1.0f);
        AvgWater.Meta.Corruption = FMath::Clamp(AvgWater.Meta.Corruption - 0.1f, 0.0f, 1.0f);

        FVector4 DirVec(AvgWater.Direction.Body, AvgWater.Direction.Mind, AvgWater.Direction.Spirit, AvgWater.Direction.Nature);
        FVector4 Center(0.25f, 0.25f, 0.25f, 0.25f);
        DirVec = FMath::Lerp(DirVec, Center, 0.2f);
        float Sum = DirVec.X + DirVec.Y + DirVec.Z + DirVec.W;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            AvgWater.Direction.Body = DirVec.X / Sum;
            AvgWater.Direction.Mind = DirVec.Y / Sum;
            AvgWater.Direction.Spirit = DirVec.Z / Sum;
            AvgWater.Direction.Nature = DirVec.W / Sum;
        }
        else
        {
            AvgWater.Direction.Body = AvgWater.Direction.Mind = AvgWater.Direction.Spirit = AvgWater.Direction.Nature = 0.25f;
        }

        UE_LOG(LogHerbalist, Log, TEXT("Boiled water produced: Mag=%.2f Purity=%.2f Dist=%.2f"), AvgWater.Magnitude, AvgWater.Meta.Purity, AvgWater.Meta.Distortion);
        return AvgWater;
    }

    FRealState Pipeline::ProcessNoWater()
    {
        FRealState Ash;
        Ash.Magnitude = 0.1f;
        Ash.Meta.Distortion = 0.9f;
        Ash.Meta.Stability = 0.0f;
        Ash.Meta.Purity = 0.0f;
        Ash.Meta.Potency = 0.0f;
        Ash.Meta.Resonance = 0.0f;
        Ash.Meta.Corruption = 0.9f;
        Ash.Direction.Body = Ash.Direction.Mind = Ash.Direction.Spirit = Ash.Direction.Nature = 0.25f;
        UE_LOG(LogHerbalist, Warning, TEXT("No water in ingredients! Produced ash."));
        return Ash;
    }

} // namespace HerbalistCore