// Source/ProjectHerbalist/Core/World/Trample/TrampleSubsystem.h
//
// Тропы в мире (2026-09-12): поле вытоптанности (FTrampleField) плюс его
// показ через Runtime Virtual Texture. Мировая подсистема, а не актор:
// существует в любом мире без расстановки -- на L_PlaytestPaint, где лежит
// прототип, менеджера сетки нет вовсе.
//
// Показ: на каждый чанк поля рядом с игроком -- своя текстура 128x128 и
// плоскость-писатель /Engine/BasicShapes/Plane с материалом-писателем
// (M_RVTWriter, параметр CurrentRT), которая рисует в RVT_Trample и не
// рисуется в основном проходе. Ландшафт читает RVT. После выгрузки --
// Invalidate на компоненте объёма RVT (он живёт на
// ARuntimeVirtualTextureVolume, не на плоскости).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/World/Trample/TrampleField.h"
#include "TrampleSubsystem.generated.h"

class AGridWorldManager;
class AStaticMeshActor;
class UMaterialInstanceDynamic;
class URuntimeVirtualTextureComponent;
class UTextureRenderTarget2D;
struct FSavedTrampleChunk;

USTRUCT()
struct FTrampleChunkDisplay
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<AStaticMeshActor> WriterPlane = nullptr;

    UPROPERTY()
    TObjectPtr<UTextureRenderTarget2D> Texture = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Material = nullptr;

    // Игровое время последней выгрузки; < 0 -- ещё не выгружался.
    float LastUploadSeconds = -1.0f;
};

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

    // Догнать распад, завести/убрать показ чанков вокруг зрителя, выгрузить
    // изменившиеся текстуры и инвалидировать RVT.
    void RefreshDisplays(const FVector2D& ViewerXY);

    float GetNowSeconds() const;
    float GetFullClearSeconds(const FIntPoint& ChunkCoord) const;
    float GetPassDeposit() const;
    static float GetUploadIntervalSeconds(float FullClearSeconds);
    float GetValueAt(const FVector2D& WorldXY) const;

    const FTrampleField& GetField() const { return Field; }
    const FTrampleChunkDisplay* FindDisplay(const FIntPoint& ChunkCoord) const { return Displays.Find(ChunkCoord); }
    int32 GetDisplayCount() const { return Displays.Num(); }

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

private:
    AGridWorldManager* FindManager() const;
    URuntimeVirtualTextureComponent* FindVolume() const;
    float GetDisplayRadiusCm() const;
    void EnsureDisplay(const FIntPoint& ChunkCoord);
    void DestroyDisplay(const FIntPoint& ChunkCoord);
    void UploadChunk(const FIntPoint& ChunkCoord, FTrampleChunkDisplay& Display);

    FTrampleField Field;

    UPROPERTY()
    TMap<FIntPoint, FTrampleChunkDisplay> Displays;

    TOptional<FVector2D> LastWalkerXY;
    TOptional<float> ClockOverride;
    float LastFullAdvanceSeconds = -1.0f;
    bool bWarnedMissingAssets = false;

    mutable TWeakObjectPtr<AGridWorldManager> CachedManager;
    mutable TWeakObjectPtr<URuntimeVirtualTextureComponent> CachedVolume;
};
