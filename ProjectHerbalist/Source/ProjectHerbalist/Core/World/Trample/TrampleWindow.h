// Source/ProjectHerbalist/Core/World/Trample/TrampleWindow.h
//
// Окно показа троп (2026-09-12): одна мировая текстура вокруг игрока вместо
// Runtime Virtual Texture. RVT на ландшафте давала глюки на тайлах, а
// выигрыша от неё не было: данные и так на CPU, показываются только рядом
// с игроком, и 1024 текселя по 25 см (256 м) покрывают этот радиус целиком
// с тем же разрешением.
//
// Чистые данные, без мира и рендера -- ради автотестов. Выгрузку и рамку
// в MPC делает UTrampleSubsystem.
//
// АДРЕСАЦИЯ ПО КРУГУ. Тексель текстуры для глобального текселя G --
// G mod 1024. Материал считает то же самое как frac(WorldPos / 256 м), с
// сэмплером Wrap. Поэтому при ходьбе ничего не сдвигается и не
// перезаливается целиком: переписываются только полосы, вошедшие в окно.
//
// ПЕРЕЦЕНТРОВКА ПО ЦЕЛЫМ ТАЙЛАМ. Начало окна кратно тайлу (64 текселя = 16 м),
// окно переезжает, только когда зритель отошёл от центра на тайл. Отсюда
// гарантия: данные верны в круге радиусом 512 - 64 = 448 текселей (112 м)
// вокруг зрителя, и материал обязан погасить тропу к этому радиусу -- дальше
// текстура повторяется чужими данными.
//
// ЧТО ВИДНО ГЛАЗУ ("однократный проход совсем не виден -- проявление
// начинается после нескольких проходов", как в Death Stranding, где первый
// проход только запоминается, а видимая тропа проступает при повторных):
//
//   * Порог. Цель показа -- smoothstep(Threshold, 1, значение), Threshold =
//     вклад одного прохода (1/7). Один проход -- ноль; второй -- 7%, третий
//     -- 26%, четвёртый -- половина. Нулевой наклон smoothstep в начале даёт
//     появление без края.
//   * Мягкость. Показ догоняет цель не быстрее заданной скорости -- тот же
//     принцип, что у карты состояния мира: картинка не прыгает за игроком.

#pragma once

#include "CoreMinimal.h"
#include "Core/World/Trample/TrampleField.h"

class PROJECTHERBALIST_API FTrampleWindow
{
public:
    static constexpr int32 Size = 1024;
    static constexpr int32 TileTexels = 64;
    static constexpr int32 TilesPerSide = Size / TileTexels;
    static constexpr float WorldSizeCm = Size * FTrampleField::TexelSizeCm;

    // Радиус вокруг зрителя, в котором данные окна гарантированно верны.
    static constexpr float ValidRadiusCm = (Size / 2 - TileTexels) * FTrampleField::TexelSizeCm;

    // Прямоугольник глобальных текселей.
    struct FRect
    {
        int32 MinGX = 0;
        int32 MinGY = 0;
        int32 Width = 0;
        int32 Height = 0;

        bool IsEmpty() const { return Width <= 0 || Height <= 0; }
        int64 Area() const { return IsEmpty() ? 0 : static_cast<int64>(Width) * Height; }

        // Встроено в заголовок, а не в .cpp: PROJECTHERBALIST_API внешнего
        // класса не экспортирует методы вложенной структуры, и тестовый
        // модуль не находил символ при линковке (LNK2019).
        static FRect Intersect(const FRect& A, const FRect& B)
        {
            FRect Result;
            Result.MinGX = FMath::Max(A.MinGX, B.MinGX);
            Result.MinGY = FMath::Max(A.MinGY, B.MinGY);
            Result.Width = FMath::Max(FMath::Min(A.MinGX + A.Width, B.MinGX + B.Width) - Result.MinGX, 0);
            Result.Height = FMath::Max(FMath::Min(A.MinGY + A.Height, B.MinGY + B.Height) - Result.MinGY, 0);
            return Result;
        }
    };

    // Тексель текстуры для глобального индекса -- совпадает с
    // floor(frac(мир / WorldSizeCm) * Size) в материале.
    static int32 WrapIndex(int32 GlobalTexel);

    // Цель показа для сырого значения поля.
    static float VisualFromRaw(float Raw, float Threshold);

    bool IsInitialized() const { return bInitialized; }
    const FIntPoint& GetOrigin() const { return Origin; }
    FRect GetWindowRect() const;

    // Держит окно вокруг зрителя. Возвращает прямоугольники, вошедшие в окно:
    // им нужны значения заново. Первый вызов -- всё окно.
    TArray<FRect> Recenter(const FVector2D& ViewerXY);

    // Новые цели для прямоугольника внутри окна. RawValues -- построчно, как
    // у FTrampleField::SampleTexelRect. bSnap -- показ сразу равен цели
    // (вход в окно, загрузка); иначе догоняет через Ease.
    void SetTargets(const FRect& Rect, const TArray<float>& RawValues, float Threshold, bool bSnap);

    // Показ идёт к цели не больше чем на MaxDelta. Линейно, а не
    // экспонентой: экспонента рвёт с места, линейное идёт ровно и приходит
    // точно. MaxDelta <= 0 -- сразу к цели.
    void Ease(float MaxDelta);
    bool HasEasing() const;

    // Тайлы, где изменился хоть один байт показа; флаги сбрасываются.
    void CollectDirtyTiles(TArray<FIntPoint>& OutTiles);
    void MarkAllDirty();

    // Серый RGBA8 тайла: R = G = B = показ, A = 255.
    void BuildTilePixels(const FIntPoint& Tile, TArray<FColor>& OutPixels) const;

    float GetDisplayedAtTexel(int32 GX, int32 GY) const;
    float GetTargetAtTexel(int32 GX, int32 GY) const;

    void Reset();

private:
    static int32 IndexOf(int32 GX, int32 GY) { return WrapIndex(GY) * Size + WrapIndex(GX); }
    static int32 TileIndexOf(int32 GX, int32 GY) { return (WrapIndex(GY) / TileTexels) * TilesPerSide + WrapIndex(GX) / TileTexels; }

    // Индексы -- по тексельной сетке текстуры, не мира.
    TArray<float> Displayed;
    TArray<float> Target;
    TArray<bool> TileEasing;
    TArray<bool> TileDirty;

    FIntPoint Origin = FIntPoint::ZeroValue;
    bool bInitialized = false;
};
