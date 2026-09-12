// Source/ProjectHerbalist/Core/World/Trample/TrampleWindow.cpp

#include "Core/World/Trample/TrampleWindow.h"

namespace
{
    uint8 TrampleToByte(float Value)
    {
        return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Value, 0.0f, 1.0f) * 255.0f));
    }

    // Начало окна по оси: кратно тайлу, центр окна ближе всего к зрителю.
    int32 SnappedWindowOrigin(int32 ViewerTexel)
    {
        const double Offset = static_cast<double>(ViewerTexel - FTrampleWindow::Size / 2);
        return FMath::RoundToInt(Offset / FTrampleWindow::TileTexels) * FTrampleWindow::TileTexels;
    }
}

int32 FTrampleWindow::WrapIndex(int32 GlobalTexel)
{
    return ((GlobalTexel % Size) + Size) % Size;
}

float FTrampleWindow::VisualFromRaw(float Raw, float Threshold)
{
    if (Threshold >= 1.0f)
    {
        return Raw >= 1.0f ? 1.0f : 0.0f;
    }
    const float X = FMath::Clamp((Raw - Threshold) / (1.0f - Threshold), 0.0f, 1.0f);
    return X * X * (3.0f - 2.0f * X);
}

FTrampleWindow::FRect FTrampleWindow::GetWindowRect() const
{
    FRect Rect;
    Rect.MinGX = Origin.X;
    Rect.MinGY = Origin.Y;
    Rect.Width = Size;
    Rect.Height = Size;
    return Rect;
}

TArray<FTrampleWindow::FRect> FTrampleWindow::Recenter(const FVector2D& ViewerXY)
{
    const int32 ViewerGX = FTrampleField::WorldToTexel(ViewerXY.X);
    const int32 ViewerGY = FTrampleField::WorldToTexel(ViewerXY.Y);
    TArray<FRect> Entered;

    if (!bInitialized)
    {
        Displayed.SetNumZeroed(Size * Size);
        Target.SetNumZeroed(Size * Size);
        TileEasing.Init(false, TilesPerSide * TilesPerSide);
        TileDirty.Init(true, TilesPerSide * TilesPerSide);
        Origin = FIntPoint(SnappedWindowOrigin(ViewerGX), SnappedWindowOrigin(ViewerGY));
        bInitialized = true;
        Entered.Add(GetWindowRect());
        return Entered;
    }

    // Переезд, только когда зритель отошёл от центра на тайл. Строго меньше
    // тайла -- поэтому круг ValidRadiusCm вокруг зрителя всегда внутри окна.
    const int32 DriftX = ViewerGX - (Origin.X + Size / 2);
    const int32 DriftY = ViewerGY - (Origin.Y + Size / 2);
    if (FMath::Abs(DriftX) < TileTexels && FMath::Abs(DriftY) < TileTexels)
    {
        return Entered;
    }

    const FIntPoint NewOrigin(SnappedWindowOrigin(ViewerGX), SnappedWindowOrigin(ViewerGY));
    const int32 DeltaX = NewOrigin.X - Origin.X;
    const int32 DeltaY = NewOrigin.Y - Origin.Y;
    Origin = NewOrigin;

    if (DeltaX == 0 && DeltaY == 0)
    {
        return Entered;
    }
    if (FMath::Abs(DeltaX) >= Size || FMath::Abs(DeltaY) >= Size)
    {
        Entered.Add(GetWindowRect());
        return Entered;
    }

    // Вошедшие столбцы -- на всю высоту окна.
    if (DeltaX != 0)
    {
        FRect Columns;
        Columns.MinGX = (DeltaX > 0) ? NewOrigin.X + Size - DeltaX : NewOrigin.X;
        Columns.MinGY = NewOrigin.Y;
        Columns.Width = FMath::Abs(DeltaX);
        Columns.Height = Size;
        Entered.Add(Columns);
    }

    // Вошедшие строки -- только по столбцам, которые уже были в окне, чтобы
    // угол не пересчитывался дважды.
    if (DeltaY != 0)
    {
        FRect Rows;
        Rows.MinGX = (DeltaX < 0) ? NewOrigin.X - DeltaX : NewOrigin.X;
        Rows.Width = Size - FMath::Abs(DeltaX);
        Rows.MinGY = (DeltaY > 0) ? NewOrigin.Y + Size - DeltaY : NewOrigin.Y;
        Rows.Height = FMath::Abs(DeltaY);
        if (!Rows.IsEmpty())
        {
            Entered.Add(Rows);
        }
    }
    return Entered;
}

void FTrampleWindow::SetTargets(const FRect& Rect, const TArray<float>& RawValues, float Threshold, bool bSnap)
{
    if (!bInitialized || Rect.IsEmpty() || RawValues.Num() != Rect.Area())
    {
        return;
    }

    for (int32 Row = 0; Row < Rect.Height; ++Row)
    {
        const int32 GY = Rect.MinGY + Row;
        for (int32 Col = 0; Col < Rect.Width; ++Col)
        {
            const int32 GX = Rect.MinGX + Col;
            const int32 Index = IndexOf(GX, GY);
            const float Visual = VisualFromRaw(RawValues[Row * Rect.Width + Col], Threshold);
            Target[Index] = Visual;

            float& Shown = Displayed[Index];
            if (Shown == Visual)
            {
                continue;
            }
            if (bSnap)
            {
                if (TrampleToByte(Shown) != TrampleToByte(Visual))
                {
                    TileDirty[TileIndexOf(GX, GY)] = true;
                }
                Shown = Visual;
            }
            else
            {
                TileEasing[TileIndexOf(GX, GY)] = true;
            }
        }
    }
}

void FTrampleWindow::Ease(float MaxDelta)
{
    if (!bInitialized)
    {
        return;
    }

    for (int32 Tile = 0; Tile < TileEasing.Num(); ++Tile)
    {
        if (!TileEasing[Tile])
        {
            continue;
        }

        const int32 TileX = Tile % TilesPerSide;
        const int32 TileY = Tile / TilesPerSide;
        bool bStillEasing = false;

        for (int32 J = 0; J < TileTexels; ++J)
        {
            const int32 RowStart = (TileY * TileTexels + J) * Size + TileX * TileTexels;
            for (int32 I = 0; I < TileTexels; ++I)
            {
                const int32 Index = RowStart + I;
                float& Shown = Displayed[Index];
                const float Goal = Target[Index];
                if (Shown == Goal)
                {
                    continue;
                }

                const float Next = (MaxDelta <= 0.0f) ? Goal : Shown + FMath::Clamp(Goal - Shown, -MaxDelta, MaxDelta);
                if (TrampleToByte(Next) != TrampleToByte(Shown))
                {
                    TileDirty[Tile] = true;
                }
                Shown = Next;
                bStillEasing |= (Shown != Goal);
            }
        }
        TileEasing[Tile] = bStillEasing;
    }
}

bool FTrampleWindow::HasEasing() const
{
    return TileEasing.Contains(true);
}

void FTrampleWindow::CollectDirtyTiles(TArray<FIntPoint>& OutTiles)
{
    OutTiles.Reset();
    for (int32 Tile = 0; Tile < TileDirty.Num(); ++Tile)
    {
        if (TileDirty[Tile])
        {
            OutTiles.Add(FIntPoint(Tile % TilesPerSide, Tile / TilesPerSide));
            TileDirty[Tile] = false;
        }
    }
}

void FTrampleWindow::MarkAllDirty()
{
    for (bool& bDirty : TileDirty)
    {
        bDirty = true;
    }
}

void FTrampleWindow::BuildTilePixels(const FIntPoint& Tile, TArray<FColor>& OutPixels) const
{
    OutPixels.SetNumZeroed(TileTexels * TileTexels);
    if (!bInitialized)
    {
        return;
    }
    for (int32 J = 0; J < TileTexels; ++J)
    {
        const int32 RowStart = (Tile.Y * TileTexels + J) * Size + Tile.X * TileTexels;
        for (int32 I = 0; I < TileTexels; ++I)
        {
            const uint8 Level = TrampleToByte(Displayed[RowStart + I]);
            OutPixels[J * TileTexels + I] = FColor(Level, Level, Level, 255);
        }
    }
}

float FTrampleWindow::GetDisplayedAtTexel(int32 GX, int32 GY) const
{
    return bInitialized ? Displayed[IndexOf(GX, GY)] : 0.0f;
}

float FTrampleWindow::GetTargetAtTexel(int32 GX, int32 GY) const
{
    return bInitialized ? Target[IndexOf(GX, GY)] : 0.0f;
}

void FTrampleWindow::Reset()
{
    Displayed.Reset();
    Target.Reset();
    TileEasing.Reset();
    TileDirty.Reset();
    Origin = FIntPoint::ZeroValue;
    bInitialized = false;
}
