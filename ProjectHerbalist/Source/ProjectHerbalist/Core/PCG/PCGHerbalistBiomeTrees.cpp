// PCGHerbalistBiomeTrees.cpp

#include "Core/PCG/PCGHerbalistBiomeTrees.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Helpers/PCGHelpers.h"
#include "Engine/StaticMesh.h"

#include "Core/World/BiomeRegionVolume.h"
#include "HerbalistLogChannels.h"

#define LOCTEXT_NAMESPACE "PCGHerbalistBiomeTrees"

float UPCGHerbalistBiomeTreesSettings::KeepFraction(float BiomeTreesPer100SquareMeters, float SamplerTreesPer100SquareMeters, float Falloff)
{
    if (SamplerTreesPer100SquareMeters <= 0.0f || BiomeTreesPer100SquareMeters <= 0.0f)
    {
        return 0.0f;
    }
    return FMath::Clamp(BiomeTreesPer100SquareMeters / SamplerTreesPer100SquareMeters * FMath::Clamp(Falloff, 0.0f, 1.0f), 0.0f, 1.0f);
}

int32 UPCGHerbalistBiomeTreesSettings::PickRow(const TArray<const FHerbalistBiomeTreeRow*>& Rows, float Pick)
{
    float Total = 0.0f;
    for (const FHerbalistBiomeTreeRow* Row : Rows)
    {
        Total += Row ? FMath::Max(Row->TreesPer100SquareMeters, 0.0f) : 0.0f;
    }
    if (Total <= 0.0f)
    {
        return INDEX_NONE;
    }
    float Cursor = FMath::Clamp(Pick, 0.0f, 1.0f) * Total;
    int32 LastWithWeight = INDEX_NONE;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const float Weight = Rows[Index] ? FMath::Max(Rows[Index]->TreesPer100SquareMeters, 0.0f) : 0.0f;
        if (Weight <= 0.0f)
        {
            continue;
        }
        LastWithWeight = Index;
        if (Cursor < Weight)
        {
            return Index;
        }
        Cursor -= Weight;
    }
    // Pick == 1 -- последняя строка с весом.
    return LastWithWeight;
}

TArray<FPCGPinProperties> UPCGHerbalistBiomeTreesSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> Properties;
    FPCGPinProperties& InPin = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
    InPin.SetRequiredPin();
    return Properties;
}

TArray<FPCGPinProperties> UPCGHerbalistBiomeTreesSettings::OutputPinProperties() const
{
    return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UPCGHerbalistBiomeTreesSettings::CreateElement() const
{
    return MakeShared<FPCGHerbalistBiomeTreesElement>();
}

bool FPCGHerbalistBiomeTreesElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPCGHerbalistBiomeTreesElement::Execute);
    check(Context);

    const UPCGHerbalistBiomeTreesSettings* Settings = Context->GetInputSettings<UPCGHerbalistBiomeTreesSettings>();
    check(Settings);

    const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

    // Владелец -- у исходного компонента: с разбиением граф исполняется на
    // актерах разделов.
    const UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.Get());
    const UPCGComponent* Original = Component ? Component->GetOriginalComponent() : nullptr;
    const ABiomeRegionVolume* Region = Cast<ABiomeRegionVolume>(Original ? Original->GetOwner() : (Component ? Component->GetOwner() : nullptr));
    const UDataTable* Table = Settings->TreeTable.LoadSynchronous();
    if (!Region || !Table)
    {
        PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoRegionOrTable",
            "Нет объёма биома-владельца или таблицы деревьев -- деревьев нет."));
        return true;
    }

    TArray<const FHerbalistBiomeTreeRow*> Rows;
    float BiomeDensity = 0.0f;
    Table->ForeachRow<FHerbalistBiomeTreeRow>(TEXT("HerbalistBiomeTrees"),
        [&Rows, &BiomeDensity, Region](const FName&, const FHerbalistBiomeTreeRow& Row)
        {
            if (Row.Biome == Region->Biome && !Row.Mesh.IsNull() && Row.TreesPer100SquareMeters > 0.0f)
            {
                Rows.Add(&Row);
                BiomeDensity += Row.TreesPer100SquareMeters;
            }
        });
    if (Rows.Num() == 0)
    {
        UE_LOG(LogHerbalistWorld, Verbose, TEXT("[PCG] BiomeTrees %s: у биома нет деревьев в таблице"), *Region->GetName());
        return true;
    }
    if (BiomeDensity > Settings->SamplerTreesPer100SquareMeters)
    {
        PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("SamplerTooSparse",
            "Деревьев биома {0} на 100 м², а сэмплер даёт {1} -- лес будет реже задуманного."),
            FText::AsNumber(BiomeDensity), FText::AsNumber(Settings->SamplerTreesPer100SquareMeters)));
    }

    int32 Planted = 0;
    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGBasePointData* InPointData = Cast<UPCGBasePointData>(Input.Data);
        if (!InPointData)
        {
            continue;
        }
        const int32 NumPoints = InPointData->GetNumPoints();
        const FConstPCGPointValueRanges InRanges(InPointData);

        struct FChosen
        {
            int32 InIndex;
            int32 Row;
            float Scale;
            float Yaw;
        };
        TArray<FChosen> Chosen;
        Chosen.Reserve(NumPoints);
        for (int32 Index = 0; Index < NumPoints; ++Index)
        {
            const FVector Location = InRanges.TransformRange[Index].GetLocation();
            const int32 PointSeed = InRanges.SeedRange[Index] != 0
                ? InRanges.SeedRange[Index]
                : PCGHelpers::ComputeSeedFromPosition(Location);
            // Своя соль -- сид точки уже потрачен сэмплером.
            FRandomStream Stream(static_cast<int32>(HashCombine(static_cast<uint32>(PointSeed), 0x7EE5u)));
            const float Falloff = Region->DensityFalloffStrength > 0.0f
                ? FMath::Lerp(1.0f, 1.0f - Region->GetNormalizedDistanceFromCenter(Location), Region->DensityFalloffStrength)
                : 1.0f;
            if (Stream.GetFraction() >= UPCGHerbalistBiomeTreesSettings::KeepFraction(BiomeDensity, Settings->SamplerTreesPer100SquareMeters, Falloff))
            {
                continue;
            }
            const int32 Row = UPCGHerbalistBiomeTreesSettings::PickRow(Rows, Stream.GetFraction());
            if (Row == INDEX_NONE)
            {
                continue;
            }
            const float Scale = FMath::Lerp(Rows[Row]->ScaleMin, FMath::Max(Rows[Row]->ScaleMax, Rows[Row]->ScaleMin), Stream.GetFraction());
            Chosen.Add({ Index, Row, FMath::Max(Scale, 0.01f), static_cast<float>(Stream.FRandRange(0.0f, 360.0f)) });
        }

        FPCGTaggedData& Output = Outputs.Emplace_GetRef(Input);
        UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(Context);
        check(OutPointData && OutPointData->Metadata);
        Output.Data = OutPointData;
        FPCGMetadataAttribute<FSoftObjectPath>* AttrMesh = OutPointData->Metadata->CreateAttribute<FSoftObjectPath>(
            Settings->MeshAttribute, FSoftObjectPath(), false, false);

        OutPointData->SetNumPoints(Chosen.Num(), /*bInitializeValues=*/false);
        OutPointData->AllocateProperties(EPCGPointNativeProperties::All);
        FPCGPointValueRanges OutRanges(OutPointData, /*bAllocate=*/false);
        for (int32 OutIndex = 0; OutIndex < Chosen.Num(); ++OutIndex)
        {
            const FChosen& Pick = Chosen[OutIndex];
            const FHerbalistBiomeTreeRow& Row = *Rows[Pick.Row];
            const FVector Location = InRanges.TransformRange[Pick.InIndex].GetLocation();
            OutRanges.TransformRange[OutIndex] = FTransform(FRotator(0.0f, Pick.Yaw, 0.0f), Location, FVector(Pick.Scale));
            // Границы в своих единицах точки: мировой радиус -- половина SpacingMeters.
            const double LocalRadius = FMath::Max(Row.SpacingMeters, 0.0f) * 50.0 / Pick.Scale;
            OutRanges.BoundsMinRange[OutIndex] = FVector(-LocalRadius, -LocalRadius, 0.0);
            OutRanges.BoundsMaxRange[OutIndex] = FVector(LocalRadius, LocalRadius, 1.0);
            OutRanges.DensityRange[OutIndex] = 1.0f;
            OutRanges.SteepnessRange[OutIndex] = InRanges.SteepnessRange[Pick.InIndex];
            OutRanges.ColorRange[OutIndex] = InRanges.ColorRange[Pick.InIndex];
            OutRanges.SeedRange[OutIndex] = InRanges.SeedRange[Pick.InIndex];
            const PCGMetadataEntryKey Entry = OutPointData->Metadata->AddEntry();
            OutRanges.MetadataEntryRange[OutIndex] = Entry;
            AttrMesh->SetValue(Entry, Row.Mesh.ToSoftObjectPath());
        }
        Planted += Chosen.Num();
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[PCG] BiomeTrees %s (%s): деревьев %d, видов %d, плотность %.2f на 100 м²"),
        *Region->GetName(), *UEnum::GetValueAsString(Region->Biome), Planted, Rows.Num(), BiomeDensity);
    return true;
}

#undef LOCTEXT_NAMESPACE
