#include "IngredientRegistry.h"
#include "Engine/DataTable.h"

TMap<FName, EIngredientClass> FIngredientRegistry::IngredientMap;
bool FIngredientRegistry::bIsInitialized = false;

void FIngredientRegistry::Initialize(UDataTable* IngredientTable)
{
    if (bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Herbalist] FIngredientRegistry already initialized, skipping"));
        return;
    }

    if (!IngredientTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[Herbalist] FIngredientRegistry: IngredientTable is null. Registry will be empty. All Classify calls will return Unknown."));
        bIsInitialized = true;
        return;
    }

    if (IngredientTable->GetRowStruct() != FIngredientTableRow::StaticStruct())
    {
        UE_LOG(LogTemp, Error, TEXT("[Herbalist] FIngredientRegistry: DataTable row structure mismatch. Expected FIngredientTableRow, got %s. Registry will be empty."),
            *IngredientTable->GetRowStruct()->GetName());
        bIsInitialized = true;
        return;
    }

    const FString ContextStr(TEXT("FIngredientRegistry::Initialize"));
    TArray<FName> RowNames = IngredientTable->GetRowNames();

    for (const FName& RowName : RowNames)
    {
        if (const FIngredientTableRow* Row = IngredientTable->FindRow<FIngredientTableRow>(RowName, ContextStr))
        {
            IngredientMap.Add(RowName, Row->Class);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Herbalist] FIngredientRegistry: Failed to read row '%s', skipping"), *RowName.ToString());
        }
    }

    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("[Herbalist] FIngredientRegistry initialized with %d ingredients"), IngredientMap.Num());
}

EIngredientClass FIngredientRegistry::Classify(FName IngredientName)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("[Herbalist] FIngredientRegistry::Classify called before Initialize! Returning Unknown."));
        return EIngredientClass::Unknown;
    }

    const EIngredientClass* Found = IngredientMap.Find(IngredientName);
    return Found ? *Found : EIngredientClass::Unknown;
}

bool FIngredientRegistry::IsWater(FName IngredientName)
{
    return Classify(IngredientName) == EIngredientClass::Water;
}

bool FIngredientRegistry::IsKnown(FName IngredientName)
{
    return bIsInitialized && IngredientMap.Contains(IngredientName);
}

int32 FIngredientRegistry::GetIngredientCount()
{
    return IngredientMap.Num();
}

void FIngredientRegistry::Reset()
{
    IngredientMap.Empty();
    bIsInitialized = false;
    UE_LOG(LogTemp, Log, TEXT("[Herbalist] FIngredientRegistry reset"));
}