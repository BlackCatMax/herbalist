// TieredWardCrystalAppendCommandlet.cpp
#include "Commandlets/TieredWardCrystalAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Оси/Meta -- тот же принцип, что уже устоялся у WardCrystalAppendCommandlet
    // (и у всех 76+3 карточек компендиума до этого): числа переводят
    // КАЧЕСТВЕННЫЙ фольклорный характер в существующую шкалу осей, не
    // изобретают баланс с нуля. Полное текстовое обоснование -- в
    // компендиумных карточках (herbalist_docs/Herbalist_Vault/04_Compendium/
    // Минералы/). Все три чуть сильнее (Magnitude/Potency) своих
    // тематических аналогов среди исходной тройки -- награда за прохождение
    // яруса опаснее любой находки в Пещере, не декоративное совпадение.

    // Алатырь (Ярус 1->2, EntityConceal) -- "всем камням отец", бел-горюч
    // камень посреди Окияна-моря на острове Буяне, откуда "заря занимается"
    // в заговорах: источник всякой силы и света, самый чистый и мощный
    // камень фольклора. Тот же апотропейный класс, что и у Плакун-камня
    // (EntityConceal), но Purity/Magnitude выше -- это ПЕРВАЯ, не третья
    // находка игрока, но самый "первозданный", легендарный образ во всём
    // своде поверий.
    FRealState MakeAlatyrBaseState()
    {
        FRealState S;
        S.Magnitude = 0.7f;
        S.Direction.Body = 0.2f;
        S.Direction.Mind = 0.3f;
        S.Direction.Spirit = 0.9f;   // доминанта -- источник всей силы
        S.Direction.Nature = 0.2f;
        S.Meta.Distortion = 0.03f;   // почти нулевое -- "камень всем камням отец", предельная ясность
        S.Meta.Stability = 0.97f;
        S.Meta.Purity = 0.95f;       // выше любой карточки компендиума -- первозданная чистота
        S.Meta.Potency = 0.7f;
        S.Meta.Resonance = 0.55f;
        S.Meta.Corruption = 0.01f;
        return S;
    }

    // Синь-камень (Ярус 2->3, MorokReduction) -- реальный этнографический
    // мотив (почитаемые "синие камни" на границах угодий/у воды и леса,
    // например на Плещеевом озере), цвет закатных сумерек -- грань дня и
    // ночи, опушки и чащи. Тот же класс, что Куриный бог (MorokReduction),
    // но Mind ещё выше -- пограничный камень видит ОБЕ стороны грани,
    // сильнее гасит морок восприятия на этой грани.
    FRealState MakeSinKamenBaseState()
    {
        FRealState S;
        S.Magnitude = 0.55f;
        S.Direction.Body = 0.2f;
        S.Direction.Mind = 0.6f;     // доминанта -- морок/восприятие грани
        S.Direction.Spirit = 0.75f;
        S.Direction.Nature = 0.25f;
        S.Meta.Distortion = 0.04f;   // суть эффекта MorokReduction -- гасит искажение
        S.Meta.Stability = 0.92f;
        S.Meta.Purity = 0.88f;
        S.Meta.Potency = 0.55f;
        S.Meta.Resonance = 0.55f;
        S.Meta.Corruption = 0.02f;
        return S;
    }

    // Гагат (Ярус 3->4, BrewBoost) -- чёрный камень (окаменевшая
    // болотная древесина), в народной традиции носили как оберег от
    // сглаза и порчи, "траурный" камень, рождённый в полночной топи-омуте
    // -- самый глубокий, самый опасный из трёх новых ярусов. Тот же класс,
    // что Громовая стрела (BrewBoost), но Potency -- максимум среди ВСЕХ
    // шести кристаллов: последний, самый дальний рубеж игры.
    FRealState MakeGagatBaseState()
    {
        FRealState S;
        S.Magnitude = 0.75f;
        S.Direction.Body = 0.5f;
        S.Direction.Mind = 0.15f;
        S.Direction.Spirit = 0.65f;
        S.Direction.Nature = 0.3f;
        S.Meta.Distortion = 0.08f;
        S.Meta.Stability = 0.88f;
        S.Meta.Purity = 0.7f;
        S.Meta.Potency = 0.9f;       // максимум среди всех шести -- сама суть BrewBoost, самый дальний рубеж
        S.Meta.Resonance = 0.65f;
        S.Meta.Corruption = 0.04f;
        return S;
    }
}

int32 UTieredWardCrystalAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("TieredWardCrystalAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 AddedCount = 0;
    int32 SkippedCount = 0;

    auto AddCrystal = [&](FName ID, const TCHAR* DisplayName, const TCHAR* Description,
        const FRealState& BaseState, EWardEffectType WardEffect, const TArray<EBiomeType>& HomeBiomes,
        TArray<FName> Tags, FName ElementName)
    {
        if (Table->GetRowMap().Contains(ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("TieredWardCrystalAppend: ряд '%s' уже существует, пропущен"), *ID.ToString());
            ++SkippedCount;
            return;
        }

        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(DisplayName);
        Row.Description = FText::FromString(Description);
        Row.BaseState = BaseState;
        Row.Class = EIngredientClass::Mineral;
        Row.bIsWater = false;
        // AllowedBiomes пуст И GardenNiche=None (в ОТЛИЧИЕ от исходной тройки,
        // GardenNiche::Cave) -- см. довод в .h: единственный путь получения --
        // завершить ритуал перехода яруса, не сбор и не сад.
        Row.RarityWeight = 1;
        Row.DecayRate = 0.0f;    // камень не портится (факт материала, тот же довод, что у исходных трёх)
        Row.Resilience = 1.0f;   // намеренно НЕ 0.0 -- см. довод в .h (сад этих кристаллов не касается)
        Row.Element = ElementName;
        Row.Tags = MoveTemp(Tags);
        Row.GardenNiche = EGardenNiche::None;
        Row.bIsWard = true;
        Row.WardEffectType = WardEffect;
        Row.bIsTieredWard = true;
        Row.WardHomeBiomes = HomeBiomes;

        Table->AddRow(ID, Row);
        ++AddedCount;
    };

    AddCrystal(
        FName(TEXT("Алатырь")),
        TEXT("Алатырь"),
        TEXT("Бел-горюч камень посреди Окияна-моря, на острове Буяне -- \"всем камням отец\", как называют его заговоры. Из-под него, по преданию, вытекает целебная река, а над ним занимается заря: порог, с которого начинается любая сила, любой день. Награда за первый пройденный рубеж -- Тундру и Степь: кто дошёл до края первого яруса живым, тому камень открывает дорогу дальше, в Тайгу и Лесостепь."),
        MakeAlatyrBaseState(),
        EWardEffectType::EntityConceal,
        { EBiomeType::Taiga, EBiomeType::ForestSteppe },
        { FName(TEXT("оберег")), FName(TEXT("алатырь")), FName(TEXT("буян")), FName(TEXT("апотропей")), FName(TEXT("кристалл")), FName(TEXT("рассвет")) },
        FName(TEXT("Огонь")));

    AddCrystal(
        FName(TEXT("Синь-камень")),
        TEXT("Синь-камень"),
        TEXT("Сизо-синий валун, каким его застаёт закатный час -- издавна чтимый на опушках и у воды, там, где земля одного урочища встречается с другим. Пограничный камень: не принадлежит целиком ни лесу, ни полю, ни дню, ни ночи, оттого и видит по обе стороны грани. Награда за пройденную Тайгу и Лесостепь -- пропуск в глушь Смешанного и Широколиственного леса."),
        MakeSinKamenBaseState(),
        EWardEffectType::MorokReduction,
        { EBiomeType::MixedForest, EBiomeType::BroadleafForest },
        { FName(TEXT("оберег")), FName(TEXT("синь-камень")), FName(TEXT("грань")), FName(TEXT("апотропей")), FName(TEXT("кристалл")), FName(TEXT("закат")) },
        FName(TEXT("Вода")));

    AddCrystal(
        FName(TEXT("Гагат")),
        TEXT("Гагат"),
        TEXT("Чёрный, гладкий, будто вобравший в себя всю темноту камень -- окаменевшая древесина, что веками пролежала на дне полночного омута. В народе носили от сглаза и порчи, как самый строгий, самый мрачный из оберегов -- траурный камень, отпугивающий не мельче нечисти, чем сама тьма, из которой он вышел. Награда за пройденный Смешанный и Широколиственный лес -- последний рубеж, Пойму и Болото."),
        MakeGagatBaseState(),
        EWardEffectType::BrewBoost,
        { EBiomeType::Floodplain, EBiomeType::Bog },
        { FName(TEXT("оберег")), FName(TEXT("гагат")), FName(TEXT("омут")), FName(TEXT("апотропей")), FName(TEXT("кристалл")), FName(TEXT("ночь")) },
        FName(TEXT("Земля")));

    if (AddedCount == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("TieredWardCrystalAppend: нечего добавлять (%d уже существует), пакет не сохранён"), SkippedCount);
        return 0;
    }

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
        UE_LOG(LogTemp, Error, TEXT("TieredWardCrystalAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("TieredWardCrystalAppend: %s теперь содержит %d рядов (добавлено %d, пропущено %d)"),
        AssetPath, Table->GetRowMap().Num(), AddedCount, SkippedCount);
    return 0;
}
