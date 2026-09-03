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
#include "Math/RandomStream.h"
#include "BiomeRegionVolume.generated.h"

class USplineComponent;

// Результат RollPlacementTransform (2026-09-03) — обычный C++-агрегат, не
// USTRUCT: живёт только между двумя C++-вызовами внутри одного кадра
// (GridWorldManagerCore.cpp), рефлексия/Blueprint-доступ не нужны.
struct FRandomPlacementTransform
{
    FVector PositionOffset = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float UniformScale = 1.0f;
};

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

    // ---- Плотность контента этого КОНКРЕТНОГО региона (2026-09-02, прямой
    // запрос пользователя: "настройки под все дела в этих волюмах") -- до
    // этого числа плотности были одним значением на весь мир
    // (AGridWorldManager::SpawnResourcesInCell/StartRegeneration/
    // InitializeCells), одинаковым для двух разных, по-своему нарисованных
    // регионов ОДНОГО типа биома. Дефолты ниже -- ровно те же числа, что
    // раньше были хардкодом/глобальной настройкой, так что поведение не
    // меняется, пока их не подвинуть руками на конкретном волюме.

    // Кто заселяет этот регион ресурсами (2026-09-03, переход на
    // PCG-расстановку). true (по умолчанию) — прежний клеточный путь:
    // AGridWorldManager::SpawnResourcesInCell сама сыплет 1-3 ресурса в
    // каждую клетку региона при инициализации. false — регион заселяет
    // PCG-граф, а C++ в него не лезет вовсе; заспавненные графом акторы
    // сами регистрируются на своих клетках (AHerbalistResourceActor::
    // BeginPlay) и живут в симуляции полноценно. Флаг на РЕГИОНЕ, а не
    // глобальный: пока переведены не все регионы, соседние могут жить
    // по-разному, без промежуточного состояния "всё сломано".
    //
    // Отрастание собранного (StartRegeneration) флаг НЕ отключает: клетка
    // помнит, что на ней росло, и восстанавливает это независимо от того,
    // кто посадил исходное растение.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Density")
    bool bSpawnResourcesFromGrid = true;

    // Было: WorldRNG.RandRange(1, 3), один диапазон на всю сетку.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Density", meta = (ClampMin = "0"))
    int32 MinResourcesPerCell = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Density", meta = (ClampMin = "0"))
    int32 MaxResourcesPerCell = 3;

    // Было: AGridWorldManager::ResourceRegrowthTime, одно число на менеджере
    // для всего мира сразу.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Density", meta = (ClampMin = "0.1"))
    float ResourceRegrowthTimeSeconds = 10.0f;

    // Было: TargetWaterCount = TotalCells / 5 в InitializeCells -- 20% воды
    // без учёта биома вообще (степь и болото заливались одинаково).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Density", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WaterDensity = 0.2f;

    // ---- Случайная трансформация ресурсов этого региона (2026-09-03,
    // прямой запрос: "как у PCG в ноде Transform") ----
    // До этой правки каждый ресурсный актор ставился с FRotator::ZeroRotator
    // и скейлом 1,1,1 без исключений -- ряды одинаково повёрнутых, одинаково
    // крупных кустов читались как искусственный узор поверх и без того
    // заметного тайлинга джиттера (см. GetResourceJitterRadius). Все поля
    // ниже -- диапазоны Min/Max, из которых на каждый актор берётся своё
    // случайное число.
    //
    // Дефолты Yaw/Tilt -- ровно те числа, что запрошены прямо (0..360 / ±5°),
    // применяются сразу на всех регионах, не требуют ручной настройки.
    // Дефолт скейла (0.85..1.15) -- конкретное число не называлось, скромный
    // разброс ±15% выбран как безопасная отправная точка, не потерявшая
    // читаемость силуэта; смещение/затухание, наоборот, дефолтятся
    // выключенными (0) -- запрошены как "возможность", не как всегда
    // применяемый эффект, включаются вручную на конкретном регионе.

    // Скейл ОДНИМ случайным числом на все три оси (не по X/Y/Z независимо)
    // -- независимый скейл визуально "плющит" меш, растения так не растут.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "0.01"))
    float MinUniformScale = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "0.01"))
    float MaxUniformScale = 1.15f;

    // Поворот вокруг вертикали (Yaw) -- у растения нет "правильной" стороны,
    // полный круг по умолчанию.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
    float MinYawDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
    float MaxYawDegrees = 360.0f;

    // Лёгкий "завал" по горизонтали (Pitch/Roll, независимо друг от друга)
    // -- пара градусов делает посадку органичнее, не полный поворот набок.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
    float MinTiltDegrees = -5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
    float MaxTiltDegrees = 5.0f;

    // Дополнительное смещение позиции поверх джиттера внутри клетки -- та
    // же идея, что Offset у PCG Transform-ноды, диапазон по каждой оси
    // отдельно.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement")
    FVector MinPositionOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement")
    FVector MaxPositionOffset = FVector::ZeroVector;

    // Затухание плотности/скейла от центра региона к границе (0 -- выключено,
    // поведение не меняется; 1 -- у самой границы плотность/скейл падают
    // до нуля). Единый множитель на весь регион, не отдельная кривая --
    // одна ручка проще в редакторе, чем произвольная кривая для v1.
    //
    // "Центр" региона -- не точное расстояние до ближайшего края
    // произвольного многоугольника (отдельная, более дорогая геометрическая
    // задача), а дешёвое приближение: центроид вершин сплайна + расстояние
    // до самой дальней из них (см. GetNormalizedDistanceFromCenter). Для
    // выпуклых/почти выпуклых регионов (обычный случай авторства сплайнов
    // биомов) даёт разумный радиальный градиент; для сильно вытянутых форм
    // градиент у краёв не идеально точен -- приемлемая цена за то, что не
    // нужен отдельный геометрический солвер.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DensityFalloffStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ScaleFalloffStrength = 0.0f;

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

    // 0 в центроиде региона, 1 у самой дальней вершины сплайна (приближение
    // "границы", см. довод у DensityFalloffStrength выше). Публична ради
    // GetClaimingRegion-based вызовов из AGridWorldManager и прямого
    // юнит-теста геометрии без завязки на весь спавн.
    UFUNCTION(BlueprintCallable, Category = "Biome")
    float GetNormalizedDistanceFromCenter(const FVector& Point) const;

    // Бросает случайную трансформацию для одного актора этого региона --
    // скейл/поворот/доп.смещение из диапазонов выше, со скейлом, домноженным
    // на затухание к границе, если оно включено. BasePosition -- уже
    // найденная (свободная, внутри формы биома) точка посадки; функция
    // только решает "как повернуть/масштабировать/сместить", не ищет место
    // заново.
    FRandomPlacementTransform RollPlacementTransform(const FVector& BasePosition, FRandomStream& Rng) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USplineComponent> SplineComponent;

    virtual void BeginPlay() override;

private:
    void EnsureCachedPoints() const;

    mutable TArray<FVector2D> CachedPoints2D;

    // Центроид вершин + расстояние до самой дальней из них -- пересчитаны
    // вместе с CachedPoints2D в UpdateCachedPoints() (не на каждый вызов
    // GetNormalizedDistanceFromCenter, который зовётся по разу на клетку/
    // актора при заселении региона).
    mutable FVector2D CachedCentroid = FVector2D::ZeroVector;
    mutable float CachedMaxRadiusFromCentroid = 0.0f;
};
