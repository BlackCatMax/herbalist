// Source/ProjectHerbalist/Core/World/Trample/TrampleField.cpp

#include "Core/World/Trample/TrampleField.h"

namespace
{
    constexpr int32 TexelCount = FTrampleField::ChunkTexels * FTrampleField::ChunkTexels;

    // Линейный распад со шагом Step и клампом в ноль. true -- все значения
    // нулевые после шага.
    bool DecayValues(TArray<float>& Values, float Step)
    {
        bool bAllZero = true;
        for (float& Value : Values)
        {
            if (Value > 0.0f)
            {
                Value = FMath::Max(Value - Step, 0.0f);
            }
            bAllZero &= (Value <= 0.0f);
        }
        return bAllZero;
    }
}

FIntPoint FTrampleField::WorldToChunk(const FVector2D& WorldXY)
{
    return FIntPoint(
        FMath::FloorToInt(WorldXY.X / ChunkSizeCm),
        FMath::FloorToInt(WorldXY.Y / ChunkSizeCm));
}

FVector2D FTrampleField::GetChunkOrigin(const FIntPoint& ChunkCoord)
{
    return FVector2D(ChunkCoord.X * ChunkSizeCm, ChunkCoord.Y * ChunkSizeCm);
}

FVector2D FTrampleField::GetChunkCenter(const FIntPoint& ChunkCoord)
{
    return GetChunkOrigin(ChunkCoord) + FVector2D(ChunkSizeCm * 0.5f, ChunkSizeCm * 0.5f);
}

FVector2D FTrampleField::GetTexelCenter(const FIntPoint& ChunkCoord, int32 I, int32 J)
{
    return GetChunkOrigin(ChunkCoord) + FVector2D((I + 0.5f) * TexelSizeCm, (J + 0.5f) * TexelSizeCm);
}

float FTrampleField::ChordLengthInDisk(const FVector2D& A, const FVector2D& B, const FVector2D& Center, float Radius)
{
    if (Radius <= 0.0f)
    {
        return 0.0f;
    }

    // |A + t*D - C|^2 = R^2, t в [0, 1].
    const FVector2D D = B - A;
    const FVector2D F = A - Center;
    const double QuadA = D.Dot(D);
    if (QuadA <= UE_DOUBLE_SMALL_NUMBER)
    {
        return 0.0f;
    }
    const double QuadB = 2.0 * F.Dot(D);
    const double QuadC = F.Dot(F) - static_cast<double>(Radius) * Radius;
    const double Discriminant = QuadB * QuadB - 4.0 * QuadA * QuadC;
    if (Discriminant <= 0.0)
    {
        return 0.0f;
    }

    const double Root = FMath::Sqrt(Discriminant);
    const double T0 = FMath::Clamp((-QuadB - Root) / (2.0 * QuadA), 0.0, 1.0);
    const double T1 = FMath::Clamp((-QuadB + Root) / (2.0 * QuadA), 0.0, 1.0);
    return static_cast<float>(FMath::Max(T1 - T0, 0.0) * FMath::Sqrt(QuadA));
}

bool FTrampleField::AdvanceChunk(const FIntPoint& ChunkCoord, float NowSeconds, float FullClearSeconds)
{
    FChunk* Chunk = Chunks.Find(ChunkCoord);
    if (!Chunk)
    {
        return true;
    }

    // Часы откатились (загрузка более раннего сейва) -- распада "назад" не
    // бывает, просто переносим отметку.
    const float Elapsed = NowSeconds - Chunk->LastAdvanceSeconds;
    Chunk->LastAdvanceSeconds = NowSeconds;
    if (Elapsed <= 0.0f)
    {
        return Chunk->Values.ContainsByPredicate([](float Value) { return Value > 0.0f; }) == false;
    }

    const float Step = (FullClearSeconds > KINDA_SMALL_NUMBER) ? Elapsed / FullClearSeconds : 1.0f;
    return DecayValues(Chunk->Values, Step);
}

TArray<FIntPoint> FTrampleField::AdvanceAll(float NowSeconds, FClearSecondsFn ClearSeconds)
{
    TArray<FIntPoint> Removed;
    for (TPair<FIntPoint, FChunk>& Pair : Chunks)
    {
        if (AdvanceChunk(Pair.Key, NowSeconds, ClearSeconds(Pair.Key)))
        {
            Removed.Add(Pair.Key);
        }
    }
    for (const FIntPoint& Coord : Removed)
    {
        Chunks.Remove(Coord);
    }
    return Removed;
}

void FTrampleField::AddStroke(const FVector2D& From, const FVector2D& To, float RadiusCm, float PassDeposit,
    float NowSeconds, FClearSecondsFn ClearSeconds)
{
    if (RadiusCm <= 0.0f || PassDeposit <= 0.0f || From.Equals(To, UE_DOUBLE_SMALL_NUMBER))
    {
        return;
    }

    const FVector2D Min(FMath::Min(From.X, To.X) - RadiusCm, FMath::Min(From.Y, To.Y) - RadiusCm);
    const FVector2D Max(FMath::Max(From.X, To.X) + RadiusCm, FMath::Max(From.Y, To.Y) + RadiusCm);
    const FIntPoint MinChunk = WorldToChunk(Min);
    const FIntPoint MaxChunk = WorldToChunk(Max);
    const float DepositPerCm = PassDeposit / (2.0f * RadiusCm);

    for (int32 ChunkY = MinChunk.Y; ChunkY <= MaxChunk.Y; ++ChunkY)
    {
        for (int32 ChunkX = MinChunk.X; ChunkX <= MaxChunk.X; ++ChunkX)
        {
            const FIntPoint Coord(ChunkX, ChunkY);
            const bool bExisted = Chunks.Contains(Coord);
            if (bExisted)
            {
                AdvanceChunk(Coord, NowSeconds, ClearSeconds(Coord));
            }

            FChunk& Chunk = Chunks.FindOrAdd(Coord);
            if (!bExisted)
            {
                Chunk.Values.SetNumZeroed(TexelCount);
                Chunk.LastAdvanceSeconds = NowSeconds;
            }

            // Диапазон текселей, чьи центры могут попасть в радиус.
            const FVector2D Origin = GetChunkOrigin(Coord);
            const int32 MinI = FMath::Clamp(FMath::FloorToInt((Min.X - Origin.X) / TexelSizeCm), 0, ChunkTexels - 1);
            const int32 MaxI = FMath::Clamp(FMath::FloorToInt((Max.X - Origin.X) / TexelSizeCm), 0, ChunkTexels - 1);
            const int32 MinJ = FMath::Clamp(FMath::FloorToInt((Min.Y - Origin.Y) / TexelSizeCm), 0, ChunkTexels - 1);
            const int32 MaxJ = FMath::Clamp(FMath::FloorToInt((Max.Y - Origin.Y) / TexelSizeCm), 0, ChunkTexels - 1);

            bool bTouched = false;
            for (int32 J = MinJ; J <= MaxJ; ++J)
            {
                for (int32 I = MinI; I <= MaxI; ++I)
                {
                    const float Chord = ChordLengthInDisk(From, To, GetTexelCenter(Coord, I, J), RadiusCm);
                    if (Chord <= 0.0f)
                    {
                        continue;
                    }
                    float& Value = Chunk.Values[J * ChunkTexels + I];
                    Value = FMath::Min(Value + DepositPerCm * Chord, 1.0f);
                    bTouched = true;
                }
            }

            if (bTouched)
            {
                Chunk.bDirty = true;
            }
            else if (!bExisted)
            {
                // Рамка задела чанк, но ни один центр текселя не попал в
                // радиус -- пустой чанк не заводим, поле остаётся разреженным.
                Chunks.Remove(Coord);
            }
        }
    }
}

float FTrampleField::GetValueAt(const FVector2D& WorldXY, float NowSeconds, FClearSecondsFn ClearSeconds) const
{
    const FIntPoint Coord = WorldToChunk(WorldXY);
    const FChunk* Chunk = Chunks.Find(Coord);
    if (!Chunk)
    {
        return 0.0f;
    }

    const FVector2D Local = WorldXY - GetChunkOrigin(Coord);
    const int32 I = FMath::Clamp(FMath::FloorToInt(Local.X / TexelSizeCm), 0, ChunkTexels - 1);
    const int32 J = FMath::Clamp(FMath::FloorToInt(Local.Y / TexelSizeCm), 0, ChunkTexels - 1);

    const float FullClear = ClearSeconds(Coord);
    const float Elapsed = FMath::Max(NowSeconds - Chunk->LastAdvanceSeconds, 0.0f);
    const float Step = (FullClear > KINDA_SMALL_NUMBER) ? Elapsed / FullClear : 1.0f;
    return FMath::Max(Chunk->Values[J * ChunkTexels + I] - Step, 0.0f);
}

void FTrampleField::ClearDirty(const FIntPoint& ChunkCoord)
{
    if (FChunk* Chunk = Chunks.Find(ChunkCoord))
    {
        Chunk->bDirty = false;
    }
}

bool FTrampleField::SetChunkValues(const FIntPoint& ChunkCoord, TArray<float>&& InValues, float NowSeconds)
{
    if (InValues.Num() != TexelCount)
    {
        return false;
    }
    FChunk& Chunk = Chunks.FindOrAdd(ChunkCoord);
    Chunk.Values = MoveTemp(InValues);
    Chunk.LastAdvanceSeconds = NowSeconds;
    Chunk.bDirty = true;
    return true;
}

void FTrampleField::BuildChunkPixels(const FIntPoint& ChunkCoord, TArray<FColor>& OutPixels) const
{
    OutPixels.SetNumZeroed(TexelCount);
    const FChunk* Chunk = Chunks.Find(ChunkCoord);
    for (int32 Index = 0; Index < TexelCount; ++Index)
    {
        const uint8 Level = Chunk ? static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Chunk->Values[Index], 0.0f, 1.0f) * 255.0f)) : 0;
        OutPixels[Index] = FColor(Level, Level, Level, 255);
    }
}

void FTrampleField::QuantizeValues(const TArray<float>& InValues, TArray<uint16>& OutValues)
{
    OutValues.SetNum(InValues.Num());
    for (int32 Index = 0; Index < InValues.Num(); ++Index)
    {
        OutValues[Index] = static_cast<uint16>(FMath::RoundToInt(FMath::Clamp(InValues[Index], 0.0f, 1.0f) * 65535.0f));
    }
}

void FTrampleField::DequantizeValues(const TArray<uint16>& InValues, TArray<float>& OutValues)
{
    OutValues.SetNum(InValues.Num());
    for (int32 Index = 0; Index < InValues.Num(); ++Index)
    {
        OutValues[Index] = InValues[Index] / 65535.0f;
    }
}
