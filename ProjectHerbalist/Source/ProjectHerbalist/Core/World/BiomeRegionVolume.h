// Core/World/BiomeRegionVolume.h
//
// PCG-биомы (2026-08-31, по прямому запросу пользователя) — заменяет
// блочную 5x5-раскраску сетки (AGridWorldManager::InitializeCells) на
// авторские сплайн-регионы. PCG нужен только как редакторский инструмент
// формирования этих сплайнов — в рантайме/PIE ABiomeRegionVolume это
// обычный плейсед-актор уровня, GridWorldManager находит их через
// TActorIterator при BeginPlay(), никакой зависимости от PCG в игре нет.
//
// Алгоритм адаптирован из UpdateCachedPoints/IsPointInside пользователя
// (T:\IDOL\IDOL\Source\IDOL_VS\...\SplineTriggerVolume.h/.cpp) —
// дискретизация замкнутого сплайна в 2D-точки + ray-cast point-in-polygon.
// Взята только эта математика, не окружение оригинала (Tick/Timer/
// OnCharacterEnter-Exit/трекинг одного персонажа) — нам нужен разовый
// запрос "точка внутри?" на клетку при инициализации сетки, не
// поштучная проверка присутствия актора каждый кадр. Z-ветка оригинала
// (bIgnoreZAxis=false) тоже не портирована — там пустой цикл
// (MinZ/MaxZ считаются и не используются) и "AvgZ" на деле Z первой
// точки сплайна, не среднее; наша сетка плоская, 2D достаточно.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "BiomeRegionVolume.generated.h"

class USplineComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API ABiomeRegionVolume : public AActor
{
    GENERATED_BODY()

public:
    ABiomeRegionVolume();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    EBiomeType Biome = EBiomeType::MixedForest;

    // Дискретизация сплайна — больше точек, точнее форма, дороже проверка.
    // 64 — тот же порядок, что и SplineResolution=100 в оригинале, чуть
    // ниже: наш вызов происходит GridSizeX*GridSizeY раз за BeginPlay
    // (не каждый кадр на одного персонажа, как в оригинале), точность
    // важнее экономии на разовом проходе, но с запасом вниз от 100 —
    // регионы обычно крупнее и проще одиночной триггер-зоны.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "8", ClampMax = "500"))
    int32 SplineResolution = 64;

    // Принудительно пересчитать кэш точек сплайна. GridWorldManager вызывает
    // это явно на каждом найденном регионе перед проверкой клеток — не
    // полагается на то, что BeginPlay() региона уже отработал (UE не
    // гарантирует порядок BeginPlay между акторами уровня).
    UFUNCTION(BlueprintCallable, Category = "Biome")
    void UpdateCachedPoints();

    // Точка внутри региона (2D, X-Y)? Самовосстанавливающийся кэш — если
    // вызвана раньше UpdateCachedPoints()/BeginPlay(), пересчитает точки
    // сама, не вернёт молчаливый false из-за пустого кэша.
    UFUNCTION(BlueprintCallable, Category = "Biome")
    bool IsPointInside(const FVector& Point) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USplineComponent> SplineComponent;

    virtual void BeginPlay() override;

private:
    void EnsureCachedPoints() const;

    mutable TArray<FVector2D> CachedPoints2D;
};
