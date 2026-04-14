// HerbalistCoreTypes.cpp
#include "HerbalistCoreTypes.h"
#include "Math/UnrealMathUtility.h"

const FRealState FAlatyr::S0 = []()
    {
        FRealState S;
        S.Magnitude = 0.5f;
        S.Direction.Body = 0.5f;
        S.Direction.Mind = 0.5f;
        S.Direction.Spirit = 0.5f;
        S.Direction.Nature = 0.5f;

        // Нормализация направления (L2)
        float Len = FMath::Sqrt(0.5f * 0.5f + 0.5f * 0.5f + 0.5f * 0.5f + 0.5f * 0.5f);
        if (Len > KINDA_SMALL_NUMBER)
        {
            S.Direction.Body /= Len;
            S.Direction.Mind /= Len;
            S.Direction.Spirit /= Len;
            S.Direction.Nature /= Len;
        }

        S.Meta.Distortion = 0.0f;
        S.Meta.Stability = 1.0f;
        S.Meta.Purity = 1.0f;

        return S;
    }();