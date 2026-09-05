// PerceptionService.cpp
#include "PerceptionService.h"

namespace Simulation
{
    // При Distortion=0 объект виден почти точно; чем выше Distortion, тем шире
    // диапазон возможного шума. MaxNoise* — амплитуда шума при Distortion=1.
    FRealState FPerceptionService::PerceiveRealState(const FRealState& Real, FRandomStream& Rng, float Clarity)
    {
        const float NoiseScale = FMath::Clamp(Real.Meta.Distortion, 0.f, 1.f) * (1.0f - FMath::Clamp(Clarity, 0.f, 1.f));
        const float MaxNoiseMain = 0.15f;   // Magnitude/Distortion/Purity/Stability
        const float MaxNoiseAxis = 0.08f;   // Direction

        FRealState Perceived = Real;
        Perceived.Magnitude = FMath::Clamp(Real.Magnitude + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);
        Perceived.Meta.Distortion = FMath::Clamp(Real.Meta.Distortion + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);
        Perceived.Meta.Purity = FMath::Clamp(Real.Meta.Purity + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);
        Perceived.Meta.Stability = FMath::Clamp(Real.Meta.Stability + Rng.FRandRange(-MaxNoiseMain, MaxNoiseMain) * NoiseScale, 0.f, 1.f);

        Perceived.Direction.Body = FMath::Max(0.f, Real.Direction.Body + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.Mind = FMath::Max(0.f, Real.Direction.Mind + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.Spirit = FMath::Max(0.f, Real.Direction.Spirit + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.Nature = FMath::Max(0.f, Real.Direction.Nature + Rng.FRandRange(-MaxNoiseAxis, MaxNoiseAxis) * NoiseScale);
        Perceived.Direction.NormalizeSum();

        return Perceived;
    }

    FPerceivedWorld FPerceptionService::ComputePerceivedWorld(const FWorldSnapshot& RealWorld, FRandomStream& Rng, float Clarity)
    {
        FPerceivedWorld Result;
        Result.WorldSeed = RealWorld.WorldSeed;
        for (const auto& Pair : RealWorld.GridState)
        {
            FPerceivedCell P;
            P.Coord = Pair.Key;
            P.bIsVisible = true;
            P.PerceivedState = PerceiveRealState(Pair.Value.State, Rng, Clarity);
            Result.Cells.Add(Pair.Key, P);
        }
        return Result;
    }

    // Сид = чистая функция identity+текущего State предмета (аудит 2026-09-05,
    // "пересчитывается каждый тик из тикового сида — держи тултип открытым и
    // усредни шум до честного значения"; решение пользователя: заменить на
    // identity+состояние, без понятия "радиуса", которого у предмета в
    // инвентаре физически нет). CreationTime — стабильный якорь ЭКЗЕМПЛЯРА
    // (проставляется один раз при харвесте, не меняется, пока предмет не
    // пересоздан/не стакнут заново), IngredientID отличает разные травы,
    // остальные поля State гарантируют, что шум сам сдвинется, когда
    // реальное состояние предмета действительно изменится (порча/сушка/
    // отстой), а не будет молча плавать от опроса к опросу без причины.
    static int32 ComputeInventoryPerceptionSeed(const FInventoryItem& Item)
    {
        uint32 Seed = 20260905u;
        Seed = HashCombine(Seed, GetTypeHash(Item.IngredientID));
        Seed = HashCombine(Seed, GetTypeHash(Item.CreationTime));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Magnitude));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Meta.Distortion));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Meta.Purity));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Meta.Stability));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Meta.Corruption));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Meta.Potency));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Meta.Resonance));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Direction.Body));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Direction.Mind));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Direction.Spirit));
        Seed = HashCombine(Seed, GetTypeHash(Item.State.Direction.Nature));
        return static_cast<int32>(Seed);
    }

    FPerceivedInventory FPerceptionService::ComputePerceivedInventory(const FInventorySnapshot& RealInventory, float Clarity)
    {
        FPerceivedInventory Result;
        for (const auto& Pair : RealInventory.ContainerContents)
        {
            TArray<FInventoryItem> PerceivedItems;
            PerceivedItems.Reserve(Pair.Value.Num());
            for (const FInventoryItem& Item : Pair.Value)
            {
                FInventoryItem Perceived = Item;
                FRandomStream ItemRng(ComputeInventoryPerceptionSeed(Item));
                Perceived.State = PerceiveRealState(Item.State, ItemRng, Clarity);
                PerceivedItems.Add(MoveTemp(Perceived));
            }
            Result.ContainerContents.Add(Pair.Key, MoveTemp(PerceivedItems));
        }
        return Result;
    }
}