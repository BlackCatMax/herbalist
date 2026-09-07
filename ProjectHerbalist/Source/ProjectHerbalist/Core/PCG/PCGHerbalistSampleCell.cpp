// PCGHerbalistSampleCell.cpp

#include "Core/PCG/PCGHerbalistSampleCell.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Helpers/PCGHelpers.h"

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "HerbalistLogChannels.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "PCGHerbalistSampleCell"

namespace PCGHerbalistSampleCellAttributes
{
    // Имена совпадают с теми, что отдаёт «Get Herbalist Grid» — намеренно:
    // граф, собранный на одном узле, читается теми же Attribute Filter, что
    // и собранный на другом. MeshKey добавлен сверх того набора, его у
    // источника нет и быть не может (он про выбор меша, а не про мир).
    const FName Distortion(TEXT("Distortion"));
    const FName Corruption(TEXT("Corruption"));
    const FName HarvestStress(TEXT("HarvestStress"));
    const FName Biome(TEXT("Biome"));
    const FName Degrading(TEXT("bDegrading"));
    const FName MeshKey(TEXT("MeshKey"));
}

TArray<FPCGPinProperties> UPCGHerbalistSampleCellSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> Properties;
    FPCGPinProperties& InPin = Properties.Emplace_GetRef(
        PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
    InPin.SetRequiredPin();
    return Properties;
}

TArray<FPCGPinProperties> UPCGHerbalistSampleCellSettings::OutputPinProperties() const
{
    return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UPCGHerbalistSampleCellSettings::CreateElement() const
{
    return MakeShared<FPCGHerbalistSampleCellElement>();
}

bool FPCGHerbalistSampleCellElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPCGHerbalistSampleCellElement::Execute);
    check(Context);

    const UPCGHerbalistSampleCellSettings* Settings =
        Context->GetInputSettings<UPCGHerbalistSampleCellSettings>();
    check(Settings);

    const TArray<FPCGTaggedData> Inputs =
        Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

    UWorld* World = Context->ExecutionSource.IsValid()
        ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;

    AGridWorldManager* Manager = nullptr;
    if (World)
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            Manager = *It;
            break;
        }
    }

    // Ни мира, ни менеджера, ни клеток -- пропускаем точки НЕТРОНУТЫМИ и
    // говорим об этом прямо. Молча пометить их «здоровыми» значило бы
    // показать нетронутый мир там, где состояние просто неизвестно: ошибка
    // выглядела бы нормальной картинкой. Это тот же случай, что описан в
    // шапке «Get Herbalist Grid» -- в редакторе до BeginPlay клеток нет.
    // Признак «клетки уже есть» -- наличие самой первой. Отдельного
    // счётчика менеджер наружу не отдаёт, и правильно делает: массив клеток
    // приватный.
    if (!Manager || Manager->GetCellConst(0, 0) == nullptr)
    {
        PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoGrid",
            "Сетка недоступна (клетки появляются в BeginPlay) -- точки пропущены без изменений. "
            "Для работы по живому состоянию мира включи у PCG-компонента генерацию в рантайме."));
        Outputs = Inputs;
        return true;
    }

    int32 TotalPoints = 0;
    int32 OutsideGrid = 0;
    int32 DegradingPoints = 0;

    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGBasePointData* InPointData = Cast<UPCGBasePointData>(Input.Data);
        if (!InPointData)
        {
            continue;
        }

        const int32 NumPoints = InPointData->GetNumPoints();
        const FConstPCGPointValueRanges InRanges(InPointData);

        // Считаем, какие точки доживут до выхода: при bDropPointsOutsideGrid
        // число точек на выходе меньше входного, и знать его надо до
        // аллокации.
        TArray<int32> KeptIndices;
        KeptIndices.Reserve(NumPoints);
        for (int32 Index = 0; Index < NumPoints; ++Index)
        {
            const FVector Location = InRanges.TransformRange[Index].GetLocation();
            int32 CellX = -1, CellY = -1;
            const bool bInside = Manager->WorldPositionToCell(Location, CellX, CellY);
            if (!bInside)
            {
                ++OutsideGrid;
                if (Settings->bDropPointsOutsideGrid) continue;
            }
            KeptIndices.Add(Index);
        }

        FPCGTaggedData& Output = Outputs.Emplace_GetRef(Input);
        UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(Context);
        check(OutPointData && OutPointData->Metadata);
        Output.Data = OutPointData;

        FPCGMetadataAttribute<float>* AttrDistortion = OutPointData->Metadata->CreateAttribute<float>(
            PCGHerbalistSampleCellAttributes::Distortion, 0.0f, true, false);
        FPCGMetadataAttribute<float>* AttrCorruption = OutPointData->Metadata->CreateAttribute<float>(
            PCGHerbalistSampleCellAttributes::Corruption, 0.0f, true, false);
        FPCGMetadataAttribute<float>* AttrStress = OutPointData->Metadata->CreateAttribute<float>(
            PCGHerbalistSampleCellAttributes::HarvestStress, 0.0f, true, false);
        FPCGMetadataAttribute<FString>* AttrBiome = OutPointData->Metadata->CreateAttribute<FString>(
            PCGHerbalistSampleCellAttributes::Biome, FString(), false, false);
        FPCGMetadataAttribute<bool>* AttrDegrading = OutPointData->Metadata->CreateAttribute<bool>(
            PCGHerbalistSampleCellAttributes::Degrading, false, false, false);
        FPCGMetadataAttribute<FString>* AttrMeshKey = OutPointData->Metadata->CreateAttribute<FString>(
            PCGHerbalistSampleCellAttributes::MeshKey, Settings->HealthyMeshKey, false, false);

        OutPointData->SetNumPoints(KeptIndices.Num(), /*bInitializeValues=*/false);
        OutPointData->AllocateProperties(EPCGPointNativeProperties::All);

        FPCGPointValueRanges OutRanges(OutPointData, /*bAllocate=*/false);

        for (int32 OutIndex = 0; OutIndex < KeptIndices.Num(); ++OutIndex)
        {
            const int32 InIndex = KeptIndices[OutIndex];

            // Геометрия точки не трогается вовсе: узел добавляет знание о
            // мире, а не переставляет растительность. Всё, что решил граф
            // выше по течению (положение, поворот, масштаб, сид), едет
            // дальше без изменений.
            OutRanges.TransformRange[OutIndex]     = InRanges.TransformRange[InIndex];
            OutRanges.BoundsMinRange[OutIndex]     = InRanges.BoundsMinRange[InIndex];
            OutRanges.BoundsMaxRange[OutIndex]     = InRanges.BoundsMaxRange[InIndex];
            OutRanges.DensityRange[OutIndex]       = InRanges.DensityRange[InIndex];
            OutRanges.SteepnessRange[OutIndex]     = InRanges.SteepnessRange[InIndex];
            OutRanges.ColorRange[OutIndex]         = InRanges.ColorRange[InIndex];
            OutRanges.SeedRange[OutIndex]          = InRanges.SeedRange[InIndex];

            const PCGMetadataEntryKey Entry = OutPointData->Metadata->AddEntry();
            OutRanges.MetadataEntryRange[OutIndex] = Entry;

            const FVector Location = InRanges.TransformRange[InIndex].GetLocation();
            int32 CellX = -1, CellY = -1;
            const FGridCell* Cell = Manager->WorldPositionToCell(Location, CellX, CellY)
                ? Manager->GetCellConst(CellX, CellY)
                : nullptr;

            ++TotalPoints;

            if (!Cell)
            {
                // Вне сетки и не выброшена: оси нулевые, ключ здоровый.
                AttrMeshKey->SetValue(Entry, Settings->HealthyMeshKey);
                continue;
            }

            AttrDistortion->SetValue(Entry, Cell->State.Meta.Distortion);
            AttrCorruption->SetValue(Entry, Cell->State.Meta.Corruption);
            AttrStress->SetValue(Entry, Cell->HarvestStress);
            AttrBiome->SetValue(Entry, FBiomeDefaults::BiomeTypeToName(Cell->Biome).ToString());
            AttrDegrading->SetValue(Entry, Cell->Memory.bDegrading);

            // Довод за липкий флаг вместо порога -- в шапке заголовка.
            if (Cell->Memory.bDegrading)
            {
                ++DegradingPoints;
                AttrMeshKey->SetValue(Entry, Settings->DegradingMeshKey);
            }
            else
            {
                AttrMeshKey->SetValue(Entry, Settings->HealthyMeshKey);
            }
        }
    }

    UE_LOG(LogHerbalistWorld, Verbose,
        TEXT("[PCG] SampleHerbalistCell: точек %d, вне сетки %d, в испорченном полюсе %d"),
        TotalPoints, OutsideGrid, DegradingPoints);

    return true;
}

#undef LOCTEXT_NAMESPACE
