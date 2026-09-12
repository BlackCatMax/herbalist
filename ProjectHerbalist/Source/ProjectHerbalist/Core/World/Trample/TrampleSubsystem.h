// Source/ProjectHerbalist/Core/World/Trample/TrampleSubsystem.h
//
// Тропы в мире (2026-09-12): поле вытоптанности (FTrampleField) и его показ
// через одну мировую текстуру вокруг игрока (FTrampleWindow). Мировая
// подсистема, а не актор: существует в любом мире без расстановки -- на
// L_PlaytestPaint менеджера сетки нет вовсе.
//
// Показ: RT_TrampleMap (1024x1024, 25 см, адресация по кругу) плюс рамка в
// MPC_WorldStateFields -- TrampleMapFrame (размер окна, начало и конец
// затухания) и TramplePlayerPosition. Материал ландшафта/травы читает
// frac(WorldPos / размер) и гасит тропу к краю по расстоянию до игрока.
// Сначала был показ через Runtime Virtual Texture -- снят в тот же день
// (глюки на тайлах, выигрыша не было, см. CHANGELOG.md).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/World/Trample/TrampleWindow.h"
#include "TrampleSubsystem.generated.h"

class AGridWorldManager;
class UMaterialParameterCollection;
class UTextureRenderTarget2D;
struct FSavedTrampleChunk;

UCLASS()
class PROJECTHERBALIST_API UTrampleSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // Editor -- ради автотестов (они идут в editor-мире). Тикает подсистема
    // всё равно только в игровых мирах: FTickableGameObject по умолчанию не
    // тикает в редакторе.
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;

    // Перемещение ходока -> штрих. Публичный: тестовый шов и точка входа для
    // будущих источников (НПС, звери). Не на земле (прыжок, падение) --
    // не топчет и сбрасывает отсчёт, чтобы приземление не прочертило линию
    // по воздуху.
    void FeedWalker(const FVector& Location, float RadiusCm, bool bOnGround);
    void AddStroke(const FVector2D& From, const FVector2D& To, float RadiusCm);

    // Показ за кадр: окно вокруг зрителя, цели после штрихов и распада, догон
    // картинки, выгрузка изменившихся тайлов, рамка в MPC.
    void UpdateDisplay(const FVector& ViewerLocation, float DeltaSeconds);

    float GetNowSeconds() const;
    float GetFullClearSeconds(const FIntPoint& ChunkCoord) const;
    float GetPassDeposit() const;
    float GetValueAt(const FVector2D& WorldXY) const;
    float GetDisplayedAt(const FVector2D& WorldXY) const;

    // Пересчёт распада -- раз в ступень RGBA8 при данном периоде зарастания.
    static float GetDecayRefreshIntervalSeconds(float FullClearSeconds);
    // Такт догона -- время, за которое показ сдвигается на ступень RGBA8.
    static float GetEaseStepSeconds();

    float GetFadeStartCm() const;
    static float GetFadeEndCm() { return FTrampleWindow::ValidRadiusCm; }

    const FTrampleField& GetField() const { return Field; }
    const FTrampleWindow& GetWindow() const { return Window; }

    TArray<FSavedTrampleChunk> CaptureSaveChunks();
    void RestoreSaveChunks(const TArray<FSavedTrampleChunk>& InChunks);

    // Часы для автотестов: в editor-мире теста нет ни менеджера с игровыми
    // часами, ни идущего времени мира.
    void SetClockOverride(float Seconds) { ClockOverride = Seconds; }
    void ClearClockOverride() { ClockOverride.Reset(); }
    void ResetTrample();

    // Шаг штриха: не мельче двух текселей. Штрих по хорде аддитивен, так что
    // более частые штрихи результат не меняют -- только нагружают CPU.
    // Совпадает с MinStampDistance прототипа BP_PaintTest (50 см).
    static constexpr float MinStrokeDistanceCm = 2.0f * FTrampleField::TexelSizeCm;

    // Скорость проявления картинки -- та же, что у карты состояния мира
    // (AGridWorldManager::WorldStateMapVisualRatePerSecond): вся картинка мира
    // меняется с одной скоростью. Порог 1/7 до 0.5 (четыре прохода) проступает
    // за 50 с -- не рывком за спиной, но и не за игровые сутки.
    static constexpr float VisualRatePerSecond = 0.01f;

private:
    AGridWorldManager* FindManager() const;
    void RefreshRect(const FTrampleWindow::FRect& Rect, float NowSeconds, bool bSnap);
    void UploadDirtyTiles();
    void PushFrameToCollection(const FVector& ViewerLocation);

    FTrampleField Field;
    FTrampleWindow Window;
    TArray<FTrampleWindow::FRect> PendingStrokeRects;

    UPROPERTY()
    TObjectPtr<UTextureRenderTarget2D> MapTarget = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialParameterCollection> FrameCollection = nullptr;

    TOptional<FVector2D> LastWalkerXY;
    TOptional<float> ClockOverride;
    float LastDecayRefreshSeconds = -1.0f;
    float EaseAccumulatorSeconds = 0.0f;

    mutable TWeakObjectPtr<AGridWorldManager> CachedManager;
};
