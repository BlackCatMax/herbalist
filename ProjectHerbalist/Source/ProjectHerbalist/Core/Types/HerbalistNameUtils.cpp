#include "HerbalistNameUtils.h"

FText GeneratePotionName(const FRealState& State)
{
    struct FAxisInfo { FString Name; float Value; };
    FAxisInfo Axes[] = {
        {TEXT("Телесное"), State.Direction.Body},
        {TEXT("Разумное"), State.Direction.Mind},
        {TEXT("Духовное"), State.Direction.Spirit},
        {TEXT("Природное"), State.Direction.Nature}
    };
    int Best = 0;
    for (int i = 1; i < 4; ++i)
        if (Axes[i].Value > Axes[Best].Value) Best = i;

    FString MainAxis = Axes[Best].Name;
    float Distortion = State.Meta.Distortion;
    float Purity = State.Meta.Purity;

    FString Quality;
    if (Distortion < 0.3f)
        Quality = TEXT("Чистое");
    else if (Distortion < 0.6f)
        Quality = TEXT("Мутное");
    else
        Quality = TEXT("Искажённое");

    if (Distortion < 0.5f && Purity > 0.5f && State.Meta.Stability > 0.5f)
        Quality = TEXT("Очищенное");

    return FText::FromString(FString::Printf(TEXT("%s %s зелье"), *Quality, *MainAxis));
}