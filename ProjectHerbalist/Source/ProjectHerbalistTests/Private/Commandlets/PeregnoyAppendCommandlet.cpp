// PeregnoyAppendCommandlet.cpp
#include "Commandlets/PeregnoyAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Перегной -- не "испорченная трава" по своим осям, а новая, честная
    // сущность: то, во что превращается растение, отдав всё гниению. Nature
    // доминирует (тело вернулось в землю), Magnitude низкий (сила потрачена
    // на само гниение, не осталась в предмете), Purity/Stability скромные,
    // но НЕ на пределе, как были у сгнившего исходника, -- это уже новое,
    // определившееся состояние, не застывший момент разложения.
    FRealState MakePeregnoyBaseState()
    {
        FRealState S;
        S.Magnitude = 0.1f;
        S.Direction.Body = 0.1f;
        S.Direction.Mind = 0.05f;
        S.Direction.Spirit = 0.1f;
        S.Direction.Nature = 0.75f;   // доминанта -- вернулось в землю
        S.Meta.Distortion = 0.2f;
        S.Meta.Stability = 0.3f;
        S.Meta.Purity = 0.1f;
        S.Meta.Potency = 0.2f;
        S.Meta.Resonance = 0.3f;
        S.Meta.Corruption = 0.1f;
        return S;
    }
}

int32 UPeregnoyAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("PeregnoyAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    const FName ID = UHerbalistInventoryComponent::PeregnoyIngredientID;
    if (Table->GetRowMap().Contains(ID))
    {
        UE_LOG(LogTemp, Display, TEXT("PeregnoyAppend: ряд '%s' уже существует, нечего добавлять, пакет не сохранён"), *ID.ToString());
        return 0;
    }

    FIngredientTableRow Row;
    Row.DisplayName = FText::FromString(TEXT("Перегной"));
    Row.Description = FText::FromString(TEXT("Тёмная, рассыпчатая земля — то, во что превращается собранная трава, если её не довести до дела вовремя. Не мусор: старики говорили, такой землёй жирнее всего кормить грядку, откуда сама трава и родом. Внести в клетку (ApplyFertilizer) — поднимает её плодородие."));
    Row.BaseState = MakePeregnoyBaseState();
    Row.Class = EIngredientClass::Plant;
    Row.bIsWater = false;
    Row.RarityWeight = 1;
    Row.DecayRate = 0.0f;    // терминальное состояние -- дальше портиться некуда
    Row.Resilience = 1.0f;   // сад его не касается, никогда не занимает клетку
    Row.Element = FName(TEXT("Земля"));
    Row.Tags = { FName(TEXT("перегной")), FName(TEXT("компост")), FName(TEXT("гниль")), FName(TEXT("удобрение")) };
    Row.GardenNiche = EGardenNiche::None;
    Row.bIsWard = false;

    Table->AddRow(ID, Row);

    Table->MarkPackageDirty();

    UPackage* Package = Table->GetOutermost();
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("PeregnoyAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("PeregnoyAppend: %s теперь содержит %d рядов (добавлен 1)"),
        AssetPath, Table->GetRowMap().Num());
    return 0;
}
