// IntentResolver.cpp
#include "IntentResolver.h"
#include "Core/HerbalistSettings.h"

namespace HerbalistCore
{
    float ComputeIntentCoherence(const TArray<FAlchemyAtom>& OrderedNonWaterAtoms,
                                 const TArray<FAlchemyAtom>& WaterAtoms,
                                 float GlobalDistortion)
    {
        const int32 N = OrderedNonWaterAtoms.Num();
        if (N == 0) return 0.5f; // нет ингредиентов — нейтральное намерение

        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float WeightDecay = Settings ? Settings->FoldWeightDecay : 0.8f;

        // Шаг 1: Веса ингредиентов
        TArray<float> Weights;
        float TotalWeight = 0.0f;
        float W = 1.0f;
        for (int32 i = 0; i < N; ++i)
        {
            Weights.Add(W);
            TotalWeight += W;
            W *= WeightDecay;
        }

        // Шаг 2: Доминантные оси и взвешенное согласие
        TMap<FString, float> AxisWeightMap;

        auto GetDominantAxisName = [](const FRealState& State) -> FString
        {
            const FDirection& Dir = State.Direction;
            if (Dir.Body >= Dir.Mind && Dir.Body >= Dir.Spirit && Dir.Body >= Dir.Nature)
                return TEXT("Body");
            if (Dir.Mind >= Dir.Spirit && Dir.Mind >= Dir.Nature)
                return TEXT("Mind");
            if (Dir.Spirit >= Dir.Nature)
                return TEXT("Spirit");
            return TEXT("Nature");
        };

        for (int32 i = 0; i < N; ++i)
        {
            const FString AxisName = GetDominantAxisName(OrderedNonWaterAtoms[i].State);
            AxisWeightMap.FindOrAdd(AxisName) += Weights[i];
        }

        float MaxAxisWeight = 0.0f;
        for (auto& Pair : AxisWeightMap)
        {
            if (Pair.Value > MaxAxisWeight)
                MaxAxisWeight = Pair.Value;
        }

        float AxisAgreement = (TotalWeight > KINDA_SMALL_NUMBER) ? (MaxAxisWeight / TotalWeight) : 0.0f;

        // Шаг 3: Взвешенные средние Purity и Stability не-водных ингредиентов
        float WeightedPurity = 0.0f, WeightedStability = 0.0f;
        for (int32 i = 0; i < N; ++i)
        {
            const FRealState& State = OrderedNonWaterAtoms[i].State;
            WeightedPurity += State.Meta.Purity * Weights[i];
            WeightedStability += State.Meta.Stability * Weights[i];
        }
        WeightedPurity /= TotalWeight;
        WeightedStability /= TotalWeight;
        const float IngredientQuality = (WeightedPurity + WeightedStability) * 0.5f;

        // Шаг 3.5: Модификаторы классов ингредиентов
        float ClassModifier = 1.0f;
        int32 CatalystCount = 0, UnknownCount = 0, EssenceCount = 0;
        for (int32 i = 0; i < N; ++i)
        {
            switch (OrderedNonWaterAtoms[i].Class)
            {
            case EIngredientClass::Catalyst: CatalystCount++; break;
            case EIngredientClass::Unknown:  UnknownCount++;  break;
            case EIngredientClass::Essence:  EssenceCount++;  break;
            default: break;
            }
        }
        if (CatalystCount > 0) ClassModifier += CatalystCount * 0.1f;
        if (UnknownCount > 0)  ClassModifier -= UnknownCount * 0.15f;
        if (EssenceCount > 0)  ClassModifier += EssenceCount * 0.05f;
        ClassModifier = FMath::Clamp(ClassModifier, 0.5f, 1.5f);

        // Шаг 4: Бонус воды
        float WaterBonus = 0.0f;
        if (WaterAtoms.Num() > 0)
        {
            float AvgWaterPurity = 0.0f;
            for (const FAlchemyAtom& Atom : WaterAtoms)
                AvgWaterPurity += Atom.State.Meta.Purity;
            AvgWaterPurity /= WaterAtoms.Num();
            WaterBonus = AvgWaterPurity * 0.2f;
        }

        // Шаг 5: Сборка с классовым модификатором
        float Coherence = FMath::Lerp(AxisAgreement, IngredientQuality, 0.5f) * ClassModifier + WaterBonus;

        // Шаг 6: Штраф глобального Distortion
        float DistortionPenalty = 1.0f - GlobalDistortion * 0.5f;
        Coherence *= DistortionPenalty;
		
		// IntentResolver.cpp — добавить перед return

        return FMath::Clamp(Coherence, 0.0f, 1.0f);
    }
}