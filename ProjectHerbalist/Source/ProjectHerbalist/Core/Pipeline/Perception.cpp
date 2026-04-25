// Perception.cpp
#include "Perception.h"
#include "ProjectHerbalist.h"
float Perception::PerceiveValue(float RealValue, float GlobalDistortion, FRandomStream& Random)
{
    const float M = 2.0f;
    const float BaseDistortion = 0.3f;
    const float K = FMath::Max(1.0f, 1.0f + (GlobalDistortion - BaseDistortion) * M);

    float T = Random.FRand();
    T = FMath::InterpEaseInOut(0.0f, 1.0f, T, 2.0f);

    const float Noise = FMath::Lerp(1.0f / K, K, T);
    return FMath::Clamp(RealValue * Noise, 0.0f, 1.0f);
}

EIngredientClass Perception::PerceiveClass(EIngredientClass RealClass, float GlobalDistortion, FRandomStream& Random)
{
    if (GlobalDistortion < 0.5f)
    {
        return RealClass;
    }

    const float P = FMath::Clamp((GlobalDistortion - 0.5f) * 1.5f, 0.0f, 0.5f);
    if (Random.FRand() < P)
    {
        TArray<EIngredientClass> All = {
            EIngredientClass::Water, EIngredientClass::Plant, EIngredientClass::Mineral,
            EIngredientClass::Fungus, EIngredientClass::Catalyst, EIngredientClass::Essence
        };
        All.Remove(RealClass);
        return All[Random.RandRange(0, All.Num() - 1)];
        UE_LOG(LogHerbalist, Verbose, TEXT("Perception: class changed from %d to %d (Distortion=%.2f)"), (int32)RealClass, (int32)All[Random.RandRange(0, All.Num() - 1)], GlobalDistortion);
    }
    return RealClass;
}
