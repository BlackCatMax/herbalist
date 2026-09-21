// HerbalistSensation.cpp
#include "Core/Types/HerbalistSensation.h"
#include "Algo/Sort.h"

namespace
{
    // Направление: разница между первой и второй осью, ниже которой предмет
    // считается «ровным» -- ни к чему особенно не тянет. Та же мысль, что у
    // заказов (ведущая ось наравне с не-ведущей -- мимо), просто без числа
    // на экране.
    constexpr float DirectionLeadMargin = 0.1f;

    struct FNote
    {
        float Salience;   // насколько далеко за порогом -- кто заметнее
        const TCHAR* Text;
    };
}

FString HerbalistSensation::Describe(const FRealState& Perceived)
{
    // --- Куда тянет ---
    const FDirection& D = Perceived.Direction;
    struct FAxis { float Value; const TCHAR* Text; };
    FAxis Axes[4] = {
        { D.Body,   TEXT("тёплый, живой в пальцах") },
        { D.Mind,   TEXT("холодит, проясняет голову") },
        { D.Spirit, TEXT("лёгкий, будто тянет вверх") },
        { D.Nature, TEXT("пахнет землёй и сырой травой") },
    };
    Algo::Sort(Axes, [](const FAxis& A, const FAxis& B) { return A.Value > B.Value; });
    const TCHAR* DirectionText = (Axes[0].Value - Axes[1].Value >= DirectionLeadMargin)
        ? Axes[0].Text
        : TEXT("ровный, ни к чему особенно не тянет");

    // --- Что заметно ---
    const FMeta& M = Perceived.Meta;
    TArray<FNote> Notes;
    if (M.Corruption >= 0.5f)       Notes.Add({ M.Corruption - 0.5f + 1.0f, TEXT("тронут гнилью") });
    else if (M.Corruption >= 0.3f)  Notes.Add({ M.Corruption - 0.3f, TEXT("с душком") });
    if (M.Distortion >= 0.6f)       Notes.Add({ M.Distortion - 0.6f + 0.5f, TEXT("плывёт в глазах") });
    if (M.Purity >= 0.75f)          Notes.Add({ M.Purity - 0.75f, TEXT("чистый дух") });
    if (M.Stability <= 0.3f)        Notes.Add({ 0.3f - M.Stability, TEXT("дрожит, не держит себя") });
    if (M.Potency >= 0.75f)         Notes.Add({ M.Potency - 0.75f, TEXT("бьёт в нос") });
    if (M.Resonance >= 0.75f)       Notes.Add({ M.Resonance - 0.75f, TEXT("отзывается, как струна") });
    if (Perceived.Magnitude <= 0.2f)      Notes.Add({ 0.2f - Perceived.Magnitude, TEXT("слабый, почти пустой") });
    else if (Perceived.Magnitude >= 0.8f) Notes.Add({ Perceived.Magnitude - 0.8f, TEXT("тяжёлый, полный силы") });

    // Порча и Морок -- первыми: травнику важнее, что с предметом не так,
    // чем чем он хорош (веса выше у них заложены прибавкой выше).
    Notes.Sort([](const FNote& A, const FNote& B) { return A.Salience > B.Salience; });

    FString Result = DirectionText;
    if (Notes.Num() > 0)
    {
        Result += TEXT("; ");
        Result += Notes[0].Text;
        if (Notes.Num() > 1)
        {
            Result += TEXT(", ");
            Result += Notes[1].Text;
        }
    }
    Result += TEXT(".");

    // С заглавной -- это фраза, не список.
    if (Result.Len() > 0)
    {
        Result[0] = FChar::ToUpper(Result[0]);
    }
    return Result;
}
