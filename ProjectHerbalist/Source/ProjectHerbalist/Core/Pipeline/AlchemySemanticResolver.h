// AlchemySemanticResolver.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "AlchemySemantics.h"
#include "AlchemyTypes.h"

struct FAlchemySemanticResult
{
    EAlchemyOutcome Outcome = EAlchemyOutcome::Ash;
    TArray<FAlchemyAtom> WaterAtoms;
    TArray<FAlchemyAtom> IngredientAtoms;
    float Coherence = 0.5f;  // вычисляется при Resolve
};

class FAlchemySemanticResolver
{
public:
    static FAlchemySemanticResult Resolve(const TArray<FAlchemyAtom>& Atoms);
};