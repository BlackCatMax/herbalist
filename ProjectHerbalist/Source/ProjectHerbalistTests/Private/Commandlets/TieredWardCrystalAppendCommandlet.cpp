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

    // Зорин-камень (Ярус 1->2, EntityConceal) -- маленький светлый камень,
    // какие в поверьях подбирают на зорьке, на распутье между полем и
    // лесом: скромный, рядовой оберег на первый выход за порог. Тот же
    // апотропейный класс, что и у Плакун-камня (EntityConceal), никакой
    // особой избранности сверх него -- обе находки одного ряда. Раньше на
    // этом месте по ошибке стоял Алатырь ("всем камням отец", остров
    // Буян) -- несоразмерно значимое, вселенского масштаба имя для рядовой
    // первой находки; исправлено 2026-09-04 (см. довод в RitualTypes.h).
    FRealState MakeZorinKamenBaseState()
    {
        FRealState S;
        S.Magnitude = 0.6f;
        S.Direction.Body = 0.2f;
        S.Direction.Mind = 0.25f;
        S.Direction.Spirit = 0.6f;   // доминанта -- скрытность, тише воды
        S.Direction.Nature = 0.35f;
        S.Meta.Distortion = 0.1f;
        S.Meta.Stability = 0.75f;
        S.Meta.Purity = 0.7f;
        S.Meta.Potency = 0.5f;
        S.Meta.Resonance = 0.45f;
        S.Meta.Corruption = 0.05f;
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
    bool bRemovedOldRow = false;

    // Разовая чистка (2026-09-04): первая версия этого коммандлета создала
    // ряд "Алатырь" -- ошибочное имя (см. довод в RitualTypes.h/выше в этом
    // файле). Точечное удаление через RemoveRow, не JSON-роундтрип -- тот
    // же принцип безопасности, что и у AddRow ниже. Идемпотентно: если ряда
    // уже нет (чистая база или повторный запуск), просто ничего не делает.
    if (Table->GetRowMap().Contains(FName(TEXT("Алатырь"))))
    {
        Table->RemoveRow(FName(TEXT("Алатырь")));
        bRemovedOldRow = true;
        UE_LOG(LogTemp, Warning, TEXT("TieredWardCrystalAppend: удалён ошибочный ряд 'Алатырь' (заменён на 'Зорин-камень')"));
    }

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
        FName(TEXT("Зорин-камень")),
        TEXT("Зорин-камень"),
        TEXT("Небольшой светлый голыш, какие подбирают на зорьке у степной или тундровой межи -- там, где кончается открытое поле и начинается чужая, лесная сторона. Ничего особого на вид, разве что чуть тёплый, будто ещё держит утренний свет. Народ верил: такой камень в кармане отводит взгляд лихого, пока идёшь по незнакомой земле. Награда за первый пройденный рубеж -- Тундру и Степь: скромный, рядовой оберег в дорогу, не более."),
        MakeZorinKamenBaseState(),
        EWardEffectType::EntityConceal,
        { EBiomeType::Taiga, EBiomeType::ForestSteppe },
        { FName(TEXT("оберег")), FName(TEXT("зорин-камень")), FName(TEXT("зорька")), FName(TEXT("апотропей")), FName(TEXT("кристалл")), FName(TEXT("рассвет")) },
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
