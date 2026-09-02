// AmbientEntitiesCreateCommandlet.cpp
#include "Commandlets/AmbientEntitiesCreateCommandlet.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    // Построчная транскрипция прежнего литерального массива
    // AmbientEntityTypes.h::GetAmbientEntityDefinitions() (28 карточек) --
    // числа/биомы/гейты не менялись, только способ хранения. SortOrder ==
    // порядковый номер в исходном массиве, задокументированные тай-брейки
    // ("Метельники ДО Ледяных духов/Шептунов" и т.д.) держатся на этом
    // порядке, не на порядке итерации DataTable.
    TArray<FAmbientEntityDefinition> BuildAmbientRows()
    {
        TArray<FAmbientEntityDefinition> Defs;
        int32 Order = 0;

        // Гнильники (Болото, земля): Corruption > 0.6 -> Corruption++, Purity--.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Гнильники"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Corruption;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            D.CorruptionRate = 0.01f;
            D.PurityRate = -0.005f;
            Defs.Add(D);
        }
        // Моховые духи (Тайга, земля): Purity высокая -> Stability++, Purity++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Моховые духи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Purity;
            D.TriggerThreshold = 0.75f;
            D.bTriggerAbove = true;
            D.PurityRate = 0.01f;
            D.StabilityRate = 0.01f;
            Defs.Add(D);
        }
        // Степные огни (Степь, земля): сумерки/ночь -> Distortion++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Степные огни"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.DistortionRate = 0.008f;
            Defs.Add(D);
        }
        // Кувшинкины духи (Речная пойма, земля): ночь -> Resonance++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Кувшинкины духи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.ResonanceRate = 0.01f;
            Defs.Add(D);
        }
        // Метельники (Тундра, земля): активная метель -> Distortion++.
        // Зарегистрировано ДО Ледяных духов и Шептунов (тот же биом Tundra) --
        // их условия строго шире, без этого порядка Метельники не получили
        // бы свою редкую метель.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Метельники"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresWeather = true;
            D.RequiredWeather = EWeatherCondition::Blizzard;
            D.DistortionRate = 0.015f;
            Defs.Add(D);
        }
        // Ледяные духи (Тундра, земля): Зима -> Magnitude--.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Ледяные духи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresSeason = true;
            D.RequiredSeason = ESeason::Winter;
            D.MagnitudeRate = -0.01f;
            Defs.Add(D);
        }
        // Суховейки (Степь, земля): Лето -> Magnitude--.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Суховейки"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresSeason = true;
            D.RequiredSeason = ESeason::Summer;
            D.MagnitudeRate = -0.008f;
            Defs.Add(D);
        }
        // Вихри (Степь, земля): ветер -> Distortion++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Вихри"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Steppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresWeather = true;
            D.RequiredWeather = EWeatherCondition::Wind;
            D.DistortionRate = 0.01f;
            Defs.Add(D);
        }
        // Омутные огни (Речная пойма, ВОДА): безлунная ночь -> Distortion++.
        // Зарегистрировано ДО Русалок (тот же биом+вода+ночь) -- условие
        // строго уже, без этого порядка Русалки забирали бы клетку каждую
        // ночь раньше них.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Омутные огни"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.bRequiresMoonPhase = true;
            D.RequiredMoonPhase = EMoonPhase::NewMoon;
            D.DistortionRate = 0.02f;
            Defs.Add(D);
        }
        // Русалки (Речная пойма, ВОДА): ночь -> Distortion++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Русалки"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            D.DistortionRate = 0.012f;
            Defs.Add(D);
        }
        // Ржавые духи (Болото, земля): Stability низкая -> проявление (эффект-заглушка).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Ржавые духи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Stability;
            D.TriggerThreshold = 0.3f;
            D.bTriggerAbove = false;
            Defs.Add(D);
        }
        // Водяные бесы (Речная пойма, ВОДА): Distortion высокий -> проявление (эффект-заглушка).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Водяные бесы"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Distortion;
            D.TriggerThreshold = 0.5f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Злыдни (Широколиств. лес, земля): HarvestStress высокий -> проявление (эффект-заглушка).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Злыдни"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::HarvestStress;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Трясинные духи (Болото, земля): Nature доминирует -> проявление (эффект-заглушка).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Трясинные духи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Nature;
            D.TriggerThreshold = 0.4f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Болотные огни (Болото, земля): Distortion высокий + ночь -> Resonance++, Distortion++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Болотные огни"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Bog;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Distortion;
            D.TriggerThreshold = 0.5f;
            D.bTriggerAbove = true;
            D.bRequiresNight = true;
            D.ResonanceRate = 0.01f;
            D.DistortionRate = 0.006f;
            Defs.Add(D);
        }
        // Купальские (Смешанный лес, земля): ночь на Купалу -> Resonance++.
        // Зарегистрировано ДО Древесных огней (тот же биом MixedForest) --
        // Купальская ночь это подмножество любой ночи, без этого порядка
        // Древесные огни забирали бы клетку каждую ночь раньше редкого окна.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Купальские"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresKupalaNight = true;
            D.ResonanceRate = 0.015f;
            Defs.Add(D);
        }
        // Шишиги (Смешанный лес, земля): сумерки -> проявление (эффект-заглушка).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Шишиги"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresDusk = true;
            Defs.Add(D);
        }
        // Древесные огни (Смешанный лес, земля): ночь -> декоративный, без эффекта.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Древесные огни"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            Defs.Add(D);
        }
        // Листовики (Смешанный лес, земля): "осень" (IsLateSummer) -> Nature++.
        // Зарегистрировано ПОСЛЕ Древесных огней/Шишиг -- в отличие от них,
        // IsLateSummer() не требует ночи/сумерек, без этого порядка забирало
        // бы клетку даже ночью весь конец Лета.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Листовики"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::MixedForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresLateSummer = true;
            D.NatureRate = 0.01f;
            Defs.Add(D);
        }
        // Чащобные духи (Тайга, земля): Nature экстремум -> проявление (эффект-заглушка).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Чащобные духи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Nature;
            D.TriggerThreshold = 0.45f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Снежные огни (Тундра, земля): ночь -> декоративный, без эффекта.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Снежные огни"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresNight = true;
            Defs.Add(D);
        }
        // Плескуны (Речная пойма, ВОДА): тонкий гейт Purity>=0 (почти всегда истинно) -- декоративный.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Плескуны"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Floodplain;
            D.bWaterOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Purity;
            D.TriggerThreshold = 0.0f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Шептуны (Тундра, земля): тонкий гейт Stability>=0 -- искажение тултипа (не подключено).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Шептуны"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Tundra;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Stability;
            D.TriggerThreshold = 0.0f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Подпольники (Широколиств. лес, земля): HarvestStress>0.4 -- предупреждающий сигнал (не подключен).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Подпольники"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::HarvestStress;
            D.TriggerThreshold = 0.4f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Стукачи (Широколиств. лес, земля): Distortion>0.6 -- предвестник (не подключен).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Стукачи"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::BroadleafForest;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::Distortion;
            D.TriggerThreshold = 0.6f;
            D.bTriggerAbove = true;
            Defs.Add(D);
        }
        // Пеньковые (Тайга, земля): HarvestStress<0.1 (нетронутый участок) -- маскировка (не подключена).
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Пеньковые"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::Taiga;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::HarvestStress;
            D.TriggerThreshold = 0.1f;
            D.bTriggerAbove = false;
            Defs.Add(D);
        }
        // Межевые (Лесостепь, земля): граница биомов графа -> Nature++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Межевые"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::ForestSteppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresBiomeBorder = true;
            D.NatureRate = 0.01f;
            Defs.Add(D);
        }
        // Ветряные бесы (Лесостепь, земля): ветер -> Distortion++.
        {
            FAmbientEntityDefinition D;
            D.EntityID = FName(TEXT("Ветряные бесы"));
            D.SortOrder = Order++;
            D.Biome = EBiomeType::ForestSteppe;
            D.bLandOnly = true;
            D.TriggerAxis = EAmbientTriggerAxis::None;
            D.bRequiresWeather = true;
            D.RequiredWeather = EWeatherCondition::Wind;
            D.DistortionRate = 0.01f;
            Defs.Add(D);
        }

        return Defs;
    }
}

int32 UAmbientEntitiesCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_AmbientEntities");

    // Идемпотентность -- тот же признак "уже существует", что уже
    // ArtifactIngredientAppendCommandlet.cpp использует на уровне ряда,
    // здесь на уровне всего ассета (создаём с нуля, не дополняем).
    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("AmbientEntitiesCreate: %s уже существует (%d рядов), ничего не делаю"),
            AssetPath, Existing->GetRowMap().Num());
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientEntitiesCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }

    UDataTable* Table = NewObject<UDataTable>(Package, FName(TEXT("DT_AmbientEntities")), RF_Public | RF_Standalone);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientEntitiesCreate: не удалось создать UDataTable"));
        return 1;
    }
    Table->RowStruct = FAmbientEntityDefinition::StaticStruct();

    FAssetRegistryModule::AssetCreated(Table);

    int32 AddedCount = 0;
    for (const FAmbientEntityDefinition& Row : BuildAmbientRows())
    {
        Table->AddRow(Row.EntityID, Row);
        ++AddedCount;
    }

    Table->MarkPackageDirty();

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientEntitiesCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("AmbientEntitiesCreate: создан %s, %d рядов добавлено"), AssetPath, AddedCount);
    return 0;
}
