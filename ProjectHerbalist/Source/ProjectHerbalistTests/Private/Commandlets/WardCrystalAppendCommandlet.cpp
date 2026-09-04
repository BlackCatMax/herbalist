// WardCrystalAppendCommandlet.cpp
#include "Commandlets/WardCrystalAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Оси/Meta не взяты из фольклора буквально (заговоры не называют
    // чисел) -- та же практика, что уже устоялась для всех 76 карточек
    // компендиума: числа переводят КАЧЕСТВЕННЫЙ фольклорный характер
    // (апотропей/сила грозы, чистота, устойчивость камня) в существующую
    // шкалу осей, не изобретают баланс с нуля. Полное текстовое обоснование
    // -- в компендиумных карточках (herbalist_docs/Herbalist_Vault/
    // 04_Compendium/Минералы/).
    FRealState MakePlakunKamenBaseState()
    {
        FRealState S;
        S.Magnitude = 0.5f;
        S.Direction.Body = 0.3f;
        S.Direction.Mind = 0.2f;
        S.Direction.Spirit = 0.8f;
        S.Direction.Nature = 0.2f;
        S.Meta.Distortion = 0.05f;
        S.Meta.Stability = 0.95f;   // камень -- устойчивее любой травы компендиума
        S.Meta.Purity = 0.85f;
        S.Meta.Potency = 0.6f;
        S.Meta.Resonance = 0.5f;
        S.Meta.Corruption = 0.02f;
        return S;
    }

    FRealState MakeGromovayaStrelaBaseState()
    {
        FRealState S;
        S.Magnitude = 0.65f;
        S.Direction.Body = 0.6f;    // окаменевший наконечник -- вещь, удар
        S.Direction.Mind = 0.1f;
        S.Direction.Spirit = 0.7f;  // сила громовника Перуна
        S.Direction.Nature = 0.1f;
        S.Meta.Distortion = 0.1f;
        S.Meta.Stability = 0.9f;
        S.Meta.Purity = 0.75f;
        S.Meta.Potency = 0.85f;     // сама суть эффекта BrewBoost -- сила в варке
        S.Meta.Resonance = 0.6f;
        S.Meta.Corruption = 0.03f;
        return S;
    }

    // Куриный бог (второй заход, 2026-09-04) -- камень с естественной
    // сквозной дырой, точенной водой/ветром (см. компендиумную карточку):
    // тот же принцип перевода КАЧЕСТВЕННОГО фольклорного характера в
    // существующую шкалу, что и у двух кристаллов выше, не выдуманные
    // числа. Дырка-проход и защита именно сна/дурных снов -- заметный
    // Mind выше, чем у обоих соседей (те про тело/варку и слёзное изгнание
    // нечисти, этот про морок восприятия во сне), Magnitude ниже всех --
    // это самый обыденный, бытовой из трёх оберегов (вешался в курятнике),
    // не купальский заговор и не громовой удар.
    FRealState MakeKurinyiBogBaseState()
    {
        FRealState S;
        S.Magnitude = 0.45f;
        S.Direction.Body = 0.15f;
        S.Direction.Mind = 0.5f;    // дыра-проход -- морок/сон/восприятие
        S.Direction.Spirit = 0.7f;  // тот же апотропейный класс, что и плакун-камень
        S.Direction.Nature = 0.15f;
        S.Meta.Distortion = 0.05f;  // сама суть эффекта MorokReduction -- гасит искажение
        S.Meta.Stability = 0.9f;
        S.Meta.Purity = 0.85f;
        S.Meta.Potency = 0.5f;      // слабее двух других -- самый бытовой оберег
        S.Meta.Resonance = 0.5f;
        S.Meta.Corruption = 0.02f;
        return S;
    }
}

int32 UWardCrystalAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("WardCrystalAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 AddedCount = 0;
    int32 SkippedCount = 0;

    auto AddCrystal = [&](FName ID, const TCHAR* DisplayName, const TCHAR* Description,
        const FRealState& BaseState, EWardEffectType WardEffect, TArray<FName> Tags, FName ElementName = FName(TEXT("Огонь")))
    {
        if (Table->GetRowMap().Contains(ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("WardCrystalAppend: ряд '%s' уже существует, пропущен"), *ID.ToString());
            ++SkippedCount;
            return;
        }

        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(DisplayName);
        Row.Description = FText::FromString(Description);
        Row.BaseState = BaseState;
        Row.Class = EIngredientClass::Mineral;
        Row.bIsWater = false;
        // AllowedBiomes пуст -- никогда не выпадает случайным сбором, только
        // через нишу сада (см. .h выше). GardenNiche=Cave -- единственный
        // путь получения.
        Row.RarityWeight = 1;
        Row.DecayRate = 0.0f;    // камень не портится (факт материала)
        Row.Resilience = 0.0f;   // намеренно НЕ 1.0 -- сад должен уметь тянуть State к BaseState
        Row.Element = ElementName;
        Row.Tags = MoveTemp(Tags);
        Row.GardenNiche = EGardenNiche::Cave;
        Row.bIsWard = true;
        Row.WardEffectType = WardEffect;

        Table->AddRow(ID, Row);
        ++AddedCount;
    };

    AddCrystal(
        FName(TEXT("Плакун-камень")),
        TEXT("Плакун-камень"),
        TEXT("Серый ноздреватый камень со дна речной излучины, весь в мелких слёзных вмятинах -- тот же корень имени, что у плакун-травы, тот же слёзный, отгоняющий нечисть смысл. В заговорной традиции поминается рядом с бел-горюч Алатырём: сидит заговаривающий на плакун-камне посреди Окияна-моря и шлёт оттуда слово против всякой порчи."),
        MakePlakunKamenBaseState(),
        EWardEffectType::EntityConceal,
        { FName(TEXT("оберег")), FName(TEXT("плакун")), FName(TEXT("купала")), FName(TEXT("апотропей")), FName(TEXT("кристалл")) });

    AddCrystal(
        FName(TEXT("Громовая стрела")),
        TEXT("Громовая стрела"),
        TEXT("Тёмный гладкий стержень, сужающийся к одному концу, будто наконечник копья, вросший в камень (окаменелость белемнита). По поверью -- та самая молния Перуна, что семь лет идёт от места удара вглубь земли, пока не выйдет наружу, отвердев в камень. Хранили за иконой от пожара и порчи, тёрли ею вымя и рога скотине перед первым выгоном -- сила самого громового удара, только уже не разящая, а охраняющая."),
        MakeGromovayaStrelaBaseState(),
        EWardEffectType::BrewBoost,
        { FName(TEXT("оберег")), FName(TEXT("перун")), FName(TEXT("гроза")), FName(TEXT("кристалл")) });

    AddCrystal(
        FName(TEXT("Куриный бог")),
        TEXT("Куриный бог"),
        TEXT("Небольшой серый камешек с одной сквозной дырочкой, проточенной в нём водой и ветром, не рукой -- оттого и сила его не человеческая. Другое имя -- Божье око. Находят такой камень случайно, в поле или на дороге, никогда не ищут нарочно: кто найдёт -- тому удача. Вешали над постелью, в хлеву, в курятнике -- от кикиморы, от порчи, от дурного глаза и от дурных снов."),
        MakeKurinyiBogBaseState(),
        EWardEffectType::MorokReduction,
        { FName(TEXT("оберег")), FName(TEXT("куриный бог")), FName(TEXT("апотропей")), FName(TEXT("кристалл")) },
        // Вода, не Огонь (в отличие от двух соседей выше) -- дырка в камне
        // проточена именно водой/ветром по тексту поверья, не связана с
        // громом/Купалой.
        FName(TEXT("Вода")));

    if (AddedCount == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("WardCrystalAppend: нечего добавлять (%d уже существует), пакет не сохранён"), SkippedCount);
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
        UE_LOG(LogTemp, Error, TEXT("WardCrystalAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("WardCrystalAppend: %s теперь содержит %d рядов (добавлено %d, пропущено %d)"),
        AssetPath, Table->GetRowMap().Num(), AddedCount, SkippedCount);
    return 0;
}
