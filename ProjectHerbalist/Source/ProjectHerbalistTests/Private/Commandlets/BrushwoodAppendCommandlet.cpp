// BrushwoodAppendCommandlet.cpp
#include "Commandlets/BrushwoodAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Types/BiomeTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

int32 UBrushwoodAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("BrushwoodAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    const FName ID(TEXT("Хворост"));
    if (Table->GetRowMap().Contains(ID))
    {
        UE_LOG(LogTemp, Warning, TEXT("BrushwoodAppend: ряд '%s' уже существует, пакет не сохранён"), *ID.ToString());
        return 0;
    }
    const int32 RowsBefore = Table->GetRowMap().Num();

    // Числа -- карточки «Хворост» (те же, что у корзины: сухое дерево и лыко,
    // ContainerAppendCommandlet). Не портится, как вся утварь; редкость 1, как
    // у всех собираемых трав.
    FIngredientTableRow Row;
    Row.DisplayName = FText::FromString(TEXT("Хворост"));
    Row.Description = FText::FromString(TEXT("Сухие ветки, сучья и валежник, собранные в вязанку и перетянутые лыком. Не трава и не снадобье — топливо: без него в лесу не развести огня, а без огня не переждать ночь вдали от дома."));
    Row.BaseState.Magnitude = 0.05f;
    Row.BaseState.Direction.Body = 0.3f;
    Row.BaseState.Direction.Mind = 0.05f;
    Row.BaseState.Direction.Spirit = 0.05f;
    Row.BaseState.Direction.Nature = 0.6f;
    Row.BaseState.Meta.Distortion = 0.05f;
    Row.BaseState.Meta.Stability = 0.8f;
    Row.BaseState.Meta.Purity = 0.5f;
    Row.BaseState.Meta.Potency = 0.05f;
    Row.BaseState.Meta.Resonance = 0.2f;
    Row.BaseState.Meta.Corruption = 0.02f;
    Row.Class = EIngredientClass::Plant;
    Row.bIsWater = false;
    Row.AllowedBiomes = { EBiomeType::MixedForest, EBiomeType::BroadleafForest, EBiomeType::Taiga };
    Row.RarityWeight = 1;
    Row.DecayRate = 0.0f;
    Row.Resilience = 1.0f;
    Row.Element = FName(TEXT("Огонь"));
    Row.Tags = { FName(TEXT("хворост")), FName(TEXT("сушняк")), FName(TEXT("костёр")), FName(TEXT("лагерь")), FName(TEXT("утварь")) };

    Table->AddRow(ID, Row);
    Table->MarkPackageDirty();

    UPackage* Package = Table->GetOutermost();
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("BrushwoodAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("BrushwoodAppend: %s теперь содержит %d рядов (было %d)"),
        AssetPath, Table->GetRowMap().Num(), RowsBefore);
    return 0;
}
