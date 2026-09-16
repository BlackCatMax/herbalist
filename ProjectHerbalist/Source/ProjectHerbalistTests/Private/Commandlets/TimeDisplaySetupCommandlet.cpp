// TimeDisplaySetupCommandlet.cpp

#include "TimeDisplaySetupCommandlet.h"

#include "Core/Config/HerbalistSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Имена отличаются от соседних коммандлетов: unity-сборка склеивает
    // Commandlets/*.cpp, одинаковые функции в анонимных namespace дают C2084.
    bool SaveTimeDisplayCollectionPackage(UMaterialParameterCollection* Collection)
    {
        UPackage* Package = Collection->GetOutermost();
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Collection, *FileName, Args);
    }

    // Default -- значение в редакторе и на картах без менеджера сетки:
    // полдень середины лета, без листопада, луна не полная.
    bool AddTimeDisplayScalar(UMaterialParameterCollection* Collection, FName ParameterName, float DefaultValue)
    {
        for (const FCollectionScalarParameter& Existing : Collection->ScalarParameters)
        {
            if (Existing.ParameterName == ParameterName)
            {
                return false;
            }
        }

        FCollectionScalarParameter NewParameter;
        NewParameter.ParameterName = ParameterName;
        NewParameter.DefaultValue = DefaultValue;
        NewParameter.Id = FGuid::NewGuid();
        Collection->ScalarParameters.Add(NewParameter);
        UE_LOG(LogTemp, Display, TEXT("  + скаляр MPC: %s = %.3f"), *ParameterName.ToString(), DefaultValue);
        return true;
    }

    bool AddTimeDisplayVector(UMaterialParameterCollection* Collection, FName ParameterName, const FLinearColor& DefaultValue)
    {
        for (const FCollectionVectorParameter& Existing : Collection->VectorParameters)
        {
            if (Existing.ParameterName == ParameterName)
            {
                return false;
            }
        }

        FCollectionVectorParameter NewParameter;
        NewParameter.ParameterName = ParameterName;
        NewParameter.DefaultValue = DefaultValue;
        NewParameter.Id = FGuid::NewGuid();
        Collection->VectorParameters.Add(NewParameter);
        UE_LOG(LogTemp, Display, TEXT("  + вектор MPC: %s = %s"), *ParameterName.ToString(), *DefaultValue.ToString());
        return true;
    }
}

int32 UTimeDisplaySetupCommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Display, TEXT("=== TimeDisplaySetup ==="));

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    UMaterialParameterCollection* Collection = Settings ? Settings->TimeDisplayCollection.LoadSynchronous() : nullptr;
    if (!Collection)
    {
        UE_LOG(LogTemp, Error, TEXT("TimeDisplayCollection не назначен в Herbalist Settings или не загружается"));
        return 1;
    }

    bool bChanged = false;
    bChanged |= AddTimeDisplayScalar(Collection, TEXT("TimeOfDay01"), 13.0f / 32.0f);
    bChanged |= AddTimeDisplayScalar(Collection, TEXT("SeasonUDW"), 1.0f);
    bChanged |= AddTimeDisplayScalar(Collection, TEXT("LeafDrop01"), 0.0f);
    bChanged |= AddTimeDisplayScalar(Collection, TEXT("MoonFull01"), 0.0f);
    bChanged |= AddTimeDisplayVector(Collection, TEXT("DayPhaseWeights"), FLinearColor(0.0f, 1.0f, 0.0f, 0.0f));
    bChanged |= AddTimeDisplayVector(Collection, TEXT("SeasonWeights"), FLinearColor(0.0f, 1.0f, 0.0f, 0.0f));

    if (!bChanged)
    {
        UE_LOG(LogTemp, Display, TEXT("Параметры времени в %s уже есть -- не трогаю."), *Collection->GetPathName());
        return 0;
    }

    if (!SaveTimeDisplayCollectionPackage(Collection))
    {
        UE_LOG(LogTemp, Error, TEXT("Не удалось сохранить %s"), *Collection->GetPathName());
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("=== TimeDisplaySetup: сохранено %s ==="), *Collection->GetPathName());
    return 0;
}
