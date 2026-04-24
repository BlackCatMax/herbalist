// Perception.h
#pragma once
#include "CoreMinimal.h"
#include "Core/Data/IngredientTableRow.h"

namespace Perception
{
    float PerceiveValue(float RealValue, float GlobalDistortion, FRandomStream& Random);
    EIngredientClass PerceiveClass(EIngredientClass RealClass, float GlobalDistortion, FRandomStream& Random);
}