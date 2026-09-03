// PCGHerbalistGridData.cpp
#include "Core/PCG/PCGHerbalistGridData.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Helpers/PCGHelpers.h"

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Shrine/ShrineTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "HerbalistLogChannels.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "PCGHerbalistGrid"

namespace PCGHerbalistGridAttributes
{
    // Имена атрибутов -- контракт с графом: переименование сломает уже
    // собранные пользователем ноды Attribute Filter, поэтому меняем их
    // только осознанно.
    const FName Distortion(TEXT("Distortion"));
    const FName Corruption(TEXT("Corruption"));
    const FName Purity(TEXT("Purity"));
    const FName Stability(TEXT("Stability"));
    const FName HarvestStress(TEXT("HarvestStress"));
    const FName ShrineRestoration(TEXT("ShrineRestoration"));
    const FName Biome(TEXT("Biome"));
    const FName IsWater(TEXT("bIsWater"));
    const FName ManifestedEntity(TEXT("ManifestedEntity"));
}

FPCGElementPtr UPCGHerbalistGridSettings::CreateElement() const
{
    return MakeShared<FPCGHerbalistGridElement>();
}

bool FPCGHerbalistGridElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPCGHerbalistGridElement::Execute);
    check(Context);

    const UPCGHerbalistGridSettings* Settings = Context->GetInputSettings<UPCGHerbalistGridSettings>();
    check(Settings);

    UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
    if (!World)
    {
        PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoWorld", "Нет мира для чтения сетки."));
        return true;
    }

    AGridWorldManager* Manager = nullptr;
    for (TActorIterator<AGridWorldManager> It(World); It; ++It)
    {
        Manager = *It;
        break;
    }

    if (!Manager)
    {
        PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoManager", "AGridWorldManager не найден на уровне -- сетка не отдана."));
        return true;
    }

    // Сначала собираем список клеток, которые пойдут в вывод: число точек
    // надо знать до аллокации. Идём через ForEachCell -- сам массив клеток
    // менеджер наружу не отдаёт, и правильно делает.
    TArray<const FGridCell*> Selected;
    int32 TotalCells = 0;
    Manager->ForEachCell([&](const FGridCell& Cell)
    {
        ++TotalCells;
        if (Settings->bExcludeWaterCells && Cell.bIsWater) return;
        if (Settings->bOnlyActiveCells && !Manager->IsCellActive(Cell)) return;
        if (Settings->bOnlyCellsClaimedByBiomeRegions && !Manager->IsCellClaimedByBiomeRegion(Cell)) return;
        Selected.Add(&Cell);
    });

    if (TotalCells == 0)
    {
        // Ровно тот случай, о котором предупреждает шапка заголовка: в
        // редакторе до BeginPlay клеток не существует. Говорим об этом
        // прямо, иначе пустой вывод выглядел бы как поломка графа.
        PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoCells",
            "Сетка ещё не инициализирована (клетки появляются в BeginPlay). "
            "Для работы по живому состоянию мира включи у PCG-компонента генерацию в рантайме."));
        return true;
    }

    const UHerbalistSettings* HerbalistSettings = GetHerbalistSettings();
    const int32 ShrineRadius = HerbalistSettings ? HerbalistSettings->ShrineInfluenceRadius : 3;
    const TArray<FShrine>& Shrines = Manager->GetShrines();

    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
    FPCGTaggedData& Output = Outputs.Emplace_GetRef();

    UPCGBasePointData* PointData = FPCGContext::NewPointData_AnyThread(Context);
    check(PointData && PointData->Metadata);
    Output.Data = PointData;

    FPCGMetadataAttribute<float>* AttrDistortion = PointData->Metadata->CreateAttribute<float>(PCGHerbalistGridAttributes::Distortion, 0.0f, true, false);
    FPCGMetadataAttribute<float>* AttrCorruption = PointData->Metadata->CreateAttribute<float>(PCGHerbalistGridAttributes::Corruption, 0.0f, true, false);
    FPCGMetadataAttribute<float>* AttrPurity = PointData->Metadata->CreateAttribute<float>(PCGHerbalistGridAttributes::Purity, 0.0f, true, false);
    FPCGMetadataAttribute<float>* AttrStability = PointData->Metadata->CreateAttribute<float>(PCGHerbalistGridAttributes::Stability, 0.0f, true, false);
    FPCGMetadataAttribute<float>* AttrStress = PointData->Metadata->CreateAttribute<float>(PCGHerbalistGridAttributes::HarvestStress, 0.0f, true, false);
    FPCGMetadataAttribute<float>* AttrShrine = PointData->Metadata->CreateAttribute<float>(PCGHerbalistGridAttributes::ShrineRestoration, 0.0f, true, false);
    FPCGMetadataAttribute<FString>* AttrBiome = PointData->Metadata->CreateAttribute<FString>(PCGHerbalistGridAttributes::Biome, FString(), false, false);
    FPCGMetadataAttribute<bool>* AttrIsWater = PointData->Metadata->CreateAttribute<bool>(PCGHerbalistGridAttributes::IsWater, false, false, false);
    FPCGMetadataAttribute<FString>* AttrEntity = PointData->Metadata->CreateAttribute<FString>(PCGHerbalistGridAttributes::ManifestedEntity, FString(), false, false);

    PointData->SetNumPoints(Selected.Num(), /*bInitializeValues=*/false);
    PointData->AllocateProperties(EPCGPointNativeProperties::All);

    FPCGPointValueRanges Ranges(PointData, /*bAllocate=*/false);

    const float CellSize = Manager->CellSize;
    for (int32 Index = 0; Index < Selected.Num(); ++Index)
    {
        const FGridCell& Cell = *Selected[Index];

        const FVector Location = Manager->GetCellWorldPosition(Cell.X, Cell.Y);
        Ranges.TransformRange[Index] = FTransform(Location);
        // Габарит точки -- ровно клетка: так Surface Sampler и Difference в
        // графе работают с ней как с настоящей площадкой, а не с безразмерной
        // точкой в её центре.
        Ranges.BoundsMinRange[Index] = FVector(-CellSize * 0.5f, -CellSize * 0.5f, -CellSize * 0.5f);
        Ranges.BoundsMaxRange[Index] = FVector(CellSize * 0.5f, CellSize * 0.5f, CellSize * 0.5f);
        Ranges.DensityRange[Index] = 1.0f;
        Ranges.SteepnessRange[Index] = 1.0f;
        Ranges.ColorRange[Index] = FVector4::One();
        Ranges.SeedRange[Index] = PCGHelpers::ComputeSeedFromPosition(Location);

        const PCGMetadataEntryKey Entry = PointData->Metadata->AddEntry();
        Ranges.MetadataEntryRange[Index] = Entry;

        AttrDistortion->SetValue(Entry, Cell.State.Meta.Distortion);
        AttrCorruption->SetValue(Entry, Cell.State.Meta.Corruption);
        AttrPurity->SetValue(Entry, Cell.State.Meta.Purity);
        AttrStability->SetValue(Entry, Cell.State.Meta.Stability);
        AttrStress->SetValue(Entry, Cell.HarvestStress);
        AttrShrine->SetValue(Entry, HerbalistCore::Shrine::GetInfluenceAt(FIntPoint(Cell.X, Cell.Y), Shrines, ShrineRadius));
        AttrBiome->SetValue(Entry, FBiomeDefaults::BiomeTypeToName(Cell.Biome).ToString());
        AttrIsWater->SetValue(Entry, Cell.bIsWater);
        AttrEntity->SetValue(Entry, Cell.ManifestedEntityID.IsNone() ? FString() : Cell.ManifestedEntityID.ToString());
    }

    UE_LOG(LogHerbalistWorld, Verbose, TEXT("[PCG] Отдано %d клеток из %d"), Selected.Num(), TotalCells);
    return true;
}

#undef LOCTEXT_NAMESPACE
