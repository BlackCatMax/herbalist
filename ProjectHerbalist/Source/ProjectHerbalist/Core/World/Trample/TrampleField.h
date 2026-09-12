// Source/ProjectHerbalist/Core/World/Trample/TrampleField.h
//
// Поле вытоптанности (2026-09-12): тропы, которые игрок протаптывает
// повторной ходьбой и которые зарастают, если по ним не ходить. Запрос:
// "как в Death Stranding ... очень плавно и очень постепенно, условно, за
// неделю игрового времени". Решено: только тропы (не мгновенные следы),
// данные на CPU, пока только визуал.
//
// Этот класс -- чистые данные и математика, без мира и рендера, ради прямой
// проверки автотестами. Показ (текстуры, плоскости-писатели RVT) -- в
// UTrampleSubsystem.
//
// Почему CPU, а не ping-pong на GPU, как в прототипе BP_PaintTest:
// недельный период распада при шаге 0.05 с даёт множитель 1 - 2.6e-6 за
// шаг, ниже разрешения RGBA16F -- распад застревает. Здесь распад
// считается формулой по прошедшему времени, draw call на него не нужен
// вовсе, а поле сохраняется и проверяется как обычные данные.
//
// Модель -- та же, что у HarvestStress клетки (линейный спад до нуля,
// кламп в 1), чтобы земля заживала одинаково от сбора и от ходьбы:
//
//   * Штрих (отрезок пути, пройденный с радиусом тела R) добавляет тексeлю
//     PassDeposit x |отрезок ∩ круг(тексель, R)| / (2R). Для текселя на
//     оси пути полный проход даёт ровно PassDeposit; сбоку -- по хорде,
//     профиль sqrt(1 - (d/R)^2). Хорды последовательных отрезков
//     складываются точно, поэтому результат не зависит ни от частоты
//     кадров, ни от того, как путь нарезан на штрихи.
//   * Распад линейный: v -= dt / FullClearSeconds. Один шаг на dt и N шагов
//     по dt/N с клампом в ноль дают одно и то же, поэтому чанк можно
//     догонять лениво, в момент обращения.

#pragma once

#include "CoreMinimal.h"

class PROJECTHERBALIST_API FTrampleField
{
public:
    // 25 см на тексель -- разрешение прототипа BP_PaintTest (1024 текселя на
    // 252 м = 24.6 см), которое уже проверено глазами. Тропа шириной в метр
    // занимает 4 текселя; мельче нужно для отпечатков ног, которые решено
    // не делать.
    static constexpr float TexelSizeCm = 25.0f;

    // Степень двойки -- ради мипов и привычных размеров текстуры; 128
    // текселей по 25 см = 32 м на чанк.
    static constexpr int32 ChunkTexels = 128;
    static constexpr float ChunkSizeCm = ChunkTexels * TexelSizeCm;

    struct FChunk
    {
        // ChunkTexels x ChunkTexels, построчно: индекс J * ChunkTexels + I,
        // столбец I идёт вдоль мировой X, строка J -- вдоль мировой Y. Ровно
        // так разложены UV движковой плоскости /Engine/BasicShapes/Plane
        // (U вдоль локальной X, V вдоль Y), см. TramplePlaneLayoutTest.cpp.
        TArray<float> Values;

        // Игровое время, до которого распад уже применён к Values.
        float LastAdvanceSeconds = 0.0f;

        // Значения менялись не распадом (штрих, восстановление из сейва) --
        // показ обязан выгрузить чанк, не дожидаясь своего такта.
        bool bDirty = true;
    };

    // Сколько секунд чанку нужно, чтобы значение 1.0 спало до нуля.
    using FClearSecondsFn = TFunctionRef<float(const FIntPoint& ChunkCoord)>;

    static FIntPoint WorldToChunk(const FVector2D& WorldXY);
    static FVector2D GetChunkOrigin(const FIntPoint& ChunkCoord);
    static FVector2D GetChunkCenter(const FIntPoint& ChunkCoord);
    static FVector2D GetTexelCenter(const FIntPoint& ChunkCoord, int32 I, int32 J);

    // Длина части отрезка AB, лежащей внутри круга. Публичная ради прямой
    // проверки геометрии.
    static float ChordLengthInDisk(const FVector2D& A, const FVector2D& B, const FVector2D& Center, float Radius);

    // Отрезок пути From -> To с радиусом тела RadiusCm. Затронутые чанки
    // сначала догоняются до NowSeconds, потом получают вклад.
    void AddStroke(const FVector2D& From, const FVector2D& To, float RadiusCm, float PassDeposit,
        float NowSeconds, FClearSecondsFn ClearSeconds);

    // Применяет распад к одному чанку до NowSeconds. true -- чанк пуст.
    bool AdvanceChunk(const FIntPoint& ChunkCoord, float NowSeconds, float FullClearSeconds);

    // Догоняет все чанки и удаляет опустевшие. Возвращает удалённые координаты.
    TArray<FIntPoint> AdvanceAll(float NowSeconds, FClearSecondsFn ClearSeconds);

    // Значение в точке на момент NowSeconds, без изменения поля.
    float GetValueAt(const FVector2D& WorldXY, float NowSeconds, FClearSecondsFn ClearSeconds) const;

    const TMap<FIntPoint, FChunk>& GetChunks() const { return Chunks; }
    const FChunk* FindChunk(const FIntPoint& ChunkCoord) const { return Chunks.Find(ChunkCoord); }
    void ClearDirty(const FIntPoint& ChunkCoord);
    void RemoveChunk(const FIntPoint& ChunkCoord) { Chunks.Remove(ChunkCoord); }
    void Reset() { Chunks.Reset(); }

    // Восстановление (сейв): значения считаются уже применёнными на NowSeconds.
    bool SetChunkValues(const FIntPoint& ChunkCoord, TArray<float>&& InValues, float NowSeconds);

    // Серый RGBA8 для текстуры чанка: R = G = B = значение, A = 255.
    void BuildChunkPixels(const FIntPoint& ChunkCoord, TArray<FColor>& OutPixels) const;

    // uint16, а не uint8: восьмибитный шаг 1/255 равен ~53 с распада при
    // недельном периоде, 1/65535 -- ~0.2 с, погрешность сейва неразличима.
    static void QuantizeValues(const TArray<float>& InValues, TArray<uint16>& OutValues);
    static void DequantizeValues(const TArray<uint16>& InValues, TArray<float>& OutValues);

private:
    TMap<FIntPoint, FChunk> Chunks;
};
