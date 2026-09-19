// Core/Community/OrderTypes.cpp

#include "Core/Community/OrderTypes.h"

void HerbalistOrders::ComputeCircleWeights(float Molva, float& OutVillagers, float& OutWarriors, float& OutOutlaws)
{
    const float M = FMath::Clamp(Molva, -1.0f, 1.0f);
    const float Villagers = FMath::Max(0.0f, 0.2f + M);
    const float Warriors = 0.5f * FMath::Clamp(M + 0.5f, 0.0f, 1.0f);
    const float Outlaws = FMath::Max(0.0f, 0.2f - M);
    const float Sum = Villagers + Warriors + Outlaws;   // > 0 при любой M
    OutVillagers = Villagers / Sum;
    OutWarriors = Warriors / Sum;
    OutOutlaws = Outlaws / Sum;
}

namespace
{
    float AxisValue(const FDirection& Direction, EOrderAxis Axis)
    {
        switch (Axis)
        {
        case EOrderAxis::Body:   return Direction.Body;
        case EOrderAxis::Mind:   return Direction.Mind;
        case EOrderAxis::Spirit: return Direction.Spirit;
        default:                 return Direction.Nature;
        }
    }

    // Мета по порядку полей FMeta.
    void MetaValues(const FMeta& Meta, float Out[6])
    {
        Out[0] = Meta.Distortion;
        Out[1] = Meta.Stability;
        Out[2] = Meta.Purity;
        Out[3] = Meta.Potency;
        Out[4] = Meta.Resonance;
        Out[5] = Meta.Corruption;
    }
}

bool HerbalistOrders::IsDeliverable(const FInventoryItem& Item)
{
    return Item.IngredientID == FName(TEXT("Potion")) && Item.Count > 0;
}

EOrderMatch HerbalistOrders::EvaluateOrderMatch(const FOrderDefinition& Def, const FRealState& State,
    float EdgeMargin, float MinMagnitude, float& OutDistance)
{
    // Минимальное нарушение ничьей: ведущая ось вровень с другой -- не
    // «преобладает», и отклонение не нулевое.
    constexpr float TieViolation = 0.01f;
    float Violation = 0.0f;
    float Margin = 1.0f;

    if (State.Magnitude < MinMagnitude)
    {
        Violation += MinMagnitude - State.Magnitude;
    }

    // Ведущие оси: каждая не ниже любой не-ведущей. Запас -- разрыв до
    // ближайшей не-ведущей, нарушение -- насколько она выше.
    if (Def.LeadingAxes.Num() > 0)
    {
        FDirection Direction = State.Direction;
        Direction.NormalizeSum();
        float BestOther = 0.0f;
        for (const EOrderAxis Axis : { EOrderAxis::Body, EOrderAxis::Mind, EOrderAxis::Spirit, EOrderAxis::Nature })
        {
            if (!Def.LeadingAxes.Contains(Axis))
            {
                BestOther = FMath::Max(BestOther, AxisValue(Direction, Axis));
            }
        }
        for (const EOrderAxis Axis : Def.LeadingAxes)
        {
            const float Gap = AxisValue(Direction, Axis) - BestOther;
            if (Gap <= 0.0f)
            {
                Violation += FMath::Max(-Gap, TieViolation);
            }
            else
            {
                Margin = FMath::Min(Margin, Gap);
            }
        }
    }

    // Мета: выход за границы -- нарушение; запас считается только у заданных
    // границ (0 и 1 -- «не ограничено»).
    float Values[6];
    float Mins[6];
    float Maxs[6];
    MetaValues(State.Meta, Values);
    MetaValues(Def.MetaMin, Mins);
    MetaValues(Def.MetaMax, Maxs);
    for (int32 Index = 0; Index < 6; ++Index)
    {
        if (Values[Index] < Mins[Index])
        {
            Violation += Mins[Index] - Values[Index];
        }
        else if (Values[Index] > Maxs[Index])
        {
            Violation += Values[Index] - Maxs[Index];
        }
        else
        {
            if (Mins[Index] > 0.0f)
            {
                Margin = FMath::Min(Margin, Values[Index] - Mins[Index]);
            }
            if (Maxs[Index] < 1.0f)
            {
                Margin = FMath::Min(Margin, Maxs[Index] - Values[Index]);
            }
        }
    }

    OutDistance = FMath::Clamp(Violation, 0.0f, 1.0f);
    if (Violation > 0.0f)
    {
        return EOrderMatch::Miss;
    }
    return Margin >= EdgeMargin ? EOrderMatch::Exact : EOrderMatch::Edge;
}

const TArray<FOrderDefinition>& HerbalistOrders::GetAllOrderDefinitions()
{
    static const TArray<FOrderDefinition> Definitions = []()
    {
        check(IsInGameThread());   // LoadObject не потокобезопасен

        TArray<FOrderDefinition> Out;
        UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Orders"));
        if (!Table)
        {
            UE_LOG(LogTemp, Error, TEXT("GetAllOrderDefinitions: не удалось загрузить DT_Orders -- заказов не будет"));
            return Out;
        }
        Table->AddToRoot();
        TArray<FOrderDefinition*> Rows;
        Table->GetAllRows(TEXT("GetAllOrderDefinitions"), Rows);
        for (const FOrderDefinition* Row : Rows)
        {
            if (Row)
            {
                Out.Add(*Row);
            }
        }
        Out.Sort([](const FOrderDefinition& A, const FOrderDefinition& B) { return A.SortOrder < B.SortOrder; });
        return Out;
    }();
    return Definitions;
}

const FOrderDefinition* HerbalistOrders::FindOrderDefinition(FName ID)
{
    for (const FOrderDefinition& Def : GetAllOrderDefinitions())
    {
        if (Def.ID == ID)
        {
            return &Def;
        }
    }
    return nullptr;
}
