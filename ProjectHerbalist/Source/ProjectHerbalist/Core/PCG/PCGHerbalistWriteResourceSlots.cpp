// PCGHerbalistWriteResourceSlots.cpp

#include "Core/PCG/PCGHerbalistWriteResourceSlots.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "HerbalistLogChannels.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

#define LOCTEXT_NAMESPACE "PCGHerbalistWriteResourceSlots"

TArray<FPCGPinProperties> UPCGHerbalistWriteResourceSlotsSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> Properties;
    FPCGPinProperties& InPin = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
    InPin.SetRequiredPin();
    return Properties;
}

TArray<FPCGPinProperties> UPCGHerbalistWriteResourceSlotsSettings::OutputPinProperties() const
{
    return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UPCGHerbalistWriteResourceSlotsSettings::CreateElement() const
{
    return MakeShared<FPCGHerbalistWriteResourceSlotsElement>();
}

UHerbalistResourceSlots* FPCGHerbalistWriteResourceSlotsElement::FindOrCreateSlotsAsset(const FString& MapPackageName, bool bCreateIfMissing)
{
    const FString PackagePath = HerbalistResourceSlots::AssetPackagePathForMap(MapPackageName);
    const FString AssetName = FPackageName::GetShortName(PackagePath);
    if (UHerbalistResourceSlots* Existing = LoadObject<UHerbalistResourceSlots>(nullptr, *(PackagePath + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        return Existing;
    }
#if WITH_EDITOR
    if (bCreateIfMissing)
    {
        if (UPackage* Package = CreatePackage(*PackagePath))
        {
            UHerbalistResourceSlots* Created = NewObject<UHerbalistResourceSlots>(Package, *AssetName, RF_Public | RF_Standalone);
            FAssetRegistryModule::AssetCreated(Created);
            return Created;
        }
    }
#endif
    return nullptr;
}

bool FPCGHerbalistWriteResourceSlotsElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPCGHerbalistWriteResourceSlotsElement::Execute);
    check(Context);

    const UPCGHerbalistWriteResourceSlotsSettings* Settings = Context->GetInputSettings<UPCGHerbalistWriteResourceSlotsSettings>();
    check(Settings);

    // Точки проходят насквозь в любом случае.
    const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
    Context->OutputData.TaggedData = Inputs;

#if WITH_EDITOR
    if (PCGHelpers::IsRuntimeOrPIE())
    {
        return true;
    }

    const UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.Get());
    const AActor* Owner = Component ? Component->GetOwner() : nullptr;
    const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (!Owner || !World)
    {
        PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoOwner", "Нет актора-источника или мира -- слоты не записаны."));
        return true;
    }

    TArray<FHerbalistResourceSlot> Slots;
    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGBasePointData* Points = Cast<UPCGBasePointData>(Input.Data);
        if (!Points)
        {
            continue;
        }
        const FPCGMetadataAttribute<FString>* KindString = Points->Metadata
            ? Points->Metadata->GetConstTypedAttribute<FString>(Settings->KindAttribute) : nullptr;
        const FPCGMetadataAttribute<FName>* KindName = (!KindString && Points->Metadata)
            ? Points->Metadata->GetConstTypedAttribute<FName>(Settings->KindAttribute) : nullptr;
        const FConstPCGPointValueRanges Ranges(Points);
        for (int32 Index = 0; Index < Points->GetNumPoints(); ++Index)
        {
            FHerbalistResourceSlot& Slot = Slots.AddDefaulted_GetRef();
            Slot.Location = Ranges.TransformRange[Index].GetLocation();
            Slot.Kind = Settings->DefaultKind;
            const PCGMetadataEntryKey Entry = Ranges.MetadataEntryRange[Index];
            if (KindString)
            {
                Slot.Kind = HerbalistResourceSlots::ParseKind(KindString->GetValueFromItemKey(Entry));
            }
            else if (KindName)
            {
                Slot.Kind = HerbalistResourceSlots::ParseKind(KindName->GetValueFromItemKey(Entry).ToString());
            }
        }
    }

    UHerbalistResourceSlots* Asset = FindOrCreateSlotsAsset(World->GetOutermost()->GetName(), /*bCreateIfMissing=*/true);
    if (!Asset)
    {
        PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoAsset", "Ассет слотов карты не создан -- слоты не записаны."));
        return true;
    }

    const int32 Count = Slots.Num();
    Asset->Modify();
    Asset->ReplaceSet(FSoftObjectPath(Owner).ToString(), MoveTemp(Slots));
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    if (IsRunningCommandlet())
    {
        const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *FileName, Args))
        {
            PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("SaveFailed", "Ассет слотов {0} не сохранён."), FText::FromString(Package->GetName())));
        }
    }
    UE_LOG(LogHerbalistWorld, Display, TEXT("[Slots] %s: записано %d слотов, всего в %s -- %d"),
        *Owner->GetName(), Count, *Package->GetName(), Asset->CountSlots());
#endif
    return true;
}

#undef LOCTEXT_NAMESPACE
