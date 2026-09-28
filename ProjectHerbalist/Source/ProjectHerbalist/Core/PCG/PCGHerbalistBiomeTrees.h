// PCGHerbalistBiomeTrees.h
//
// «Herbalist Biome Trees» (2026-09-28, решение пользователя: деревья --
// отдельным графом на том же BP_BiomeVolume и сплайне, запекаются в
// редакторе, по нашей схеме коммандлетами). Узел решает, какое дерево и
// стоит ли оно вообще: биом -- у объёма-владельца исполняющего компонента
// (ABiomeRegionVolume::Biome), набор деревьев биома -- строки DT_BiomeTrees.
//
// На входе -- точки выборки с плотностью SamplerTreesPer100SquareMeters (та
// же, что у сэмплера графа). Точка остаётся с долей
//   сумма TreesPer100SquareMeters строк биома / SamplerTreesPer100SquareMeters,
// умноженной на затухание к краю региона (DensityFalloffStrength объёма, как у
// ресурсов). Вид -- из строк биома по их плотности. Узел ставит атрибут меша
// (MeshAttribute, для Mesh Selector By Attribute спавнера), масштаб строки,
// случайный поворот вокруг вертикали и границы точки радиусом SpacingMeters/2
// -- по ним Self Pruning разводит стволы.
//
// Биом берётся у исходного компонента (GetOriginalComponent): с разбиением
// граф исполняется на актерах разделов, их владелец -- не объём.
#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "PCGHerbalistBiomeTrees.generated.h"

class UStaticMesh;
class AWaterRegionVolume;

// Строка DT_BiomeTrees: один вид дерева в одном биоме.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FHerbalistBiomeTreeRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trees")
    EBiomeType Biome = EBiomeType::MixedForest;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trees")
    TSoftObjectPtr<UStaticMesh> Mesh;

    // Сколько таких деревьев на 100 м² в середине региона.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trees", meta = (ClampMin = "0.0"))
    float TreesPer100SquareMeters = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trees", meta = (ClampMin = "0.01"))
    float ScaleMin = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trees", meta = (ClampMin = "0.01"))
    float ScaleMax = 1.2f;

    // Ближе этого к другому дереву не встанет (Self Pruning по границам точки).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trees", meta = (ClampMin = "0.0"))
    float SpacingMeters = 4.0f;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECTHERBALIST_API UPCGHerbalistBiomeTreesSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("HerbalistBiomeTrees")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGHerbalistBiomeTrees", "NodeTitle", "Herbalist Biome Trees"); }
    virtual FText GetNodeTooltipText() const override
    {
        return NSLOCTEXT("PCGHerbalistBiomeTrees", "NodeTooltip",
            "Деревья биома объёма-владельца из DT_BiomeTrees: прореживание по плотности и к краю региона, меш в атрибут, масштаб, поворот, границы по SpacingMeters.");
    }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
    TSoftObjectPtr<UDataTable> TreeTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_BiomeTrees.DT_BiomeTrees")));

    /** Плотность точек сэмплера графа на 100 м² -- от неё считается доля оставшихся. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.001", PCG_Overridable))
    float SamplerTreesPer100SquareMeters = 4.0f;

    /** Атрибут с мешем дерева для Mesh Selector By Attribute. */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FName MeshAttribute = TEXT("TreeMesh");

    // Чистые правила узла -- открыты для тестов.
    // Доля оставшихся точек: плотность биома к плотности сэмплера, с
    // затуханием к краю (Falloff -- множитель 0..1). Больше 1 не бывает.
    static float KeepFraction(float BiomeTreesPer100SquareMeters, float SamplerTreesPer100SquareMeters, float Falloff);
    // Строка по доле Pick (0..1) пропорционально TreesPer100SquareMeters;
    // строк нет или плотность нулевая -- INDEX_NONE.
    static int32 PickRow(const TArray<const FHerbalistBiomeTreeRow*>& Rows, float Pick);
    // Над водой деревья не растут (2026-09-28, по PIE): точка внутри объёма
    // воды по X-Y. По высоте не сравнивается -- сплайн пруда и сплайн региона
    // на разной высоте, вычитание поверхности в графе их не пересекало.
    static bool IsOverWater(const TArray<const AWaterRegionVolume*>& Waters, const FVector& Location);

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};

class PROJECTHERBALIST_API FPCGHerbalistBiomeTreesElement : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    // Читает актор-владелец и таблицу -- только игровой поток; результат
    // зависит не только от входа -- не кэшировать.
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
