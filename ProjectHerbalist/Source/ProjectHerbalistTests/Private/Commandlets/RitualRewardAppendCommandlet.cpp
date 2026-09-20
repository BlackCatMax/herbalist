// RitualRewardAppendCommandlet.cpp
#include "Commandlets/RitualRewardAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    // Числа -- перевод качественного характера карточки в уже существующие
    // шкалы, тот же приём, что у кристаллов оберегов: «горькая, сухая,
    // отгоняющая» и «взятая на заре у воды» читаются по осям, не
    // выдумываются.

    // Полынный пояс: полынь с крапивой, свитые на закате. «Кто опояшется
    // полынным поясом, того ни одна русалка не защекочет» (карточка Полыни).
    FRealState MakePolynnyPoyasBaseState()
    {
        FRealState S;
        S.Magnitude = 0.55f;
        S.Direction.Body = 0.35f;    // жгучая крапива -- телесная, колкая защита
        S.Direction.Mind = 0.1f;
        S.Direction.Spirit = 0.65f;  // тот же апотропейный класс, что Плакун-камень
        S.Direction.Nature = 0.3f;
        S.Meta.Distortion = 0.1f;
        S.Meta.Stability = 0.75f;
        S.Meta.Purity = 0.7f;
        S.Meta.Potency = 0.6f;
        S.Meta.Resonance = 0.45f;
        S.Meta.Corruption = 0.03f;
        return S;
    }

    // Одолень-корень: кувшинка с ирным корнем, взятые на заре. «Её корень в
    // путь с собой берут -- одолеет все напасти дорожные» (карточка
    // Кувшинки). Гасит Морок вокруг -- та же защита пути, что у Куриного
    // бога защита сна.
    FRealState MakeOdolenKorenBaseState()
    {
        FRealState S;
        S.Magnitude = 0.5f;
        S.Direction.Body = 0.2f;
        S.Direction.Mind = 0.45f;    // «одолевает» морок пути, а не врага
        S.Direction.Spirit = 0.55f;
        S.Direction.Nature = 0.5f;   // речной корень, живая вода
        S.Meta.Distortion = 0.05f;   // суть эффекта MorokReduction
        S.Meta.Stability = 0.8f;
        S.Meta.Purity = 0.85f;       // «берут на заре, с поклоном реке»
        S.Meta.Potency = 0.55f;
        S.Meta.Resonance = 0.5f;
        S.Meta.Corruption = 0.02f;
        return S;
    }

    // Перунов цвет: папоротник с плакун-травой в Купальскую ночь. «Кто его в
    // ночь на Купалу найдёт, тому все тайны откроются, да не всяк выдержит»
    // (карточка Папоротника). Единственный раз в году -- отсюда предельные
    // числа, сравнимые с наградами Легендарных, и заметная Порча: «не всяк
    // выдержит» -- не бесплатная сила.
    FRealState MakePerunovTsvetBaseState()
    {
        FRealState S;
        S.Magnitude = 0.95f;
        S.Direction.Body = 0.1f;
        S.Direction.Mind = 0.85f;    // «все тайны откроются» -- знание, не тело
        S.Direction.Spirit = 0.9f;
        S.Direction.Nature = 0.4f;
        S.Meta.Distortion = 0.25f;   // «да не всяк выдержит»
        S.Meta.Stability = 0.5f;
        S.Meta.Purity = 0.8f;
        S.Meta.Potency = 0.95f;
        S.Meta.Resonance = 0.9f;
        S.Meta.Corruption = 0.1f;
        return S;
    }
}

int32 URitualRewardAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("RitualRewardAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 AddedCount = 0;
    int32 SkippedCount = 0;

    auto AddReward = [&](FName ID, const TCHAR* DisplayName, const TCHAR* Description,
        const FRealState& BaseState, bool bIsWard, EWardEffectType WardEffect,
        EIngredientClass Class, FName ElementName, TArray<FName> Tags)
    {
        if (Table->GetRowMap().Contains(ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("RitualRewardAppend: ряд '%s' уже существует, пропущен"), *ID.ToString());
            ++SkippedCount;
            return;
        }

        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(DisplayName);
        Row.Description = FText::FromString(Description);
        Row.BaseState = BaseState;
        Row.Class = Class;
        Row.bIsWater = false;
        // AllowedBiomes пуст -- случайным сбором не выпадает: путь один,
        // через ритуал (RitualTypes.h, GrantsIngredientID).
        Row.RarityWeight = 1;
        // Не портятся и не тянутся к BaseState: это не собранная трава, а
        // готовая вещь ритуала -- свитый пояс, высушенный корень, добытый
        // цвет (тот же довод, что у кристаллов оберегов, только материал
        // другой). Ревью 2026-09-21: объяснить, почему трава не гниёт.
        Row.DecayRate = 0.0f;
        Row.Resilience = 0.0f;
        Row.Element = ElementName;
        Row.Tags = MoveTemp(Tags);
        Row.bIsWard = bIsWard;
        Row.WardEffectType = WardEffect;

        Table->AddRow(ID, Row);
        ++AddedCount;
    };

    AddReward(
        FName(TEXT("Полынный пояс")),
        TEXT("Полынный пояс"),
        TEXT("Свитый на закате жгут полыни с крапивой, горький и колкий. Опоясанного им не тронет ни русалка, ни иная нечисть: они горечи не переносят, как не переносят её ни хворь, ни дурной глаз."),
        MakePolynnyPoyasBaseState(),
        /*bIsWard=*/true,
        EWardEffectType::EntityConceal,
        EIngredientClass::Plant,
        FName(TEXT("Огонь")),
        { FName(TEXT("оберег")), FName(TEXT("полынь")), FName(TEXT("апотропей")), FName(TEXT("ритуал")) });

    AddReward(
        FName(TEXT("Одолень-корень")),
        TEXT("Одолень-корень"),
        TEXT("Корень кувшинки, взятый на заре с поклоном реке и высушенный с аиром. Его берут в дорогу: одолевает напасти пути -- и хворь, и порчу, и наведённый морок, оттого и зовётся одолень."),
        MakeOdolenKorenBaseState(),
        /*bIsWard=*/true,
        EWardEffectType::MorokReduction,
        EIngredientClass::Plant,
        FName(TEXT("Вода")),
        { FName(TEXT("оберег")), FName(TEXT("одолень")), FName(TEXT("дорога")), FName(TEXT("ритуал")) });

    AddReward(
        FName(TEXT("Перунов цвет")),
        TEXT("Перунов цвет"),
        TEXT("То, что цветёт на папоротнике одну ночь в году, в ночь на Купалу, и гаснет к рассвету. Кто его добудет, тому открыто скрытое -- да не всяк выдерживает открытое: взятая разом ясность и мутит, и жжёт."),
        MakePerunovTsvetBaseState(),
        /*bIsWard=*/false,
        EWardEffectType::None,
        EIngredientClass::Plant,
        FName(TEXT("Огонь")),
        { FName(TEXT("купала")), FName(TEXT("перун")), FName(TEXT("папоротник")), FName(TEXT("ритуал")) });

    if (AddedCount > 0)
    {
        Table->MarkPackageDirty();

        UPackage* Package = Table->GetOutermost();
        const FString PackageFileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;

        if (!UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs))
        {
            UE_LOG(LogTemp, Error, TEXT("RitualRewardAppend: не удалось сохранить пакет %s"), *PackageFileName);
            return 1;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("RitualRewardAppend: %s -- добавлено %d, пропущено %d"),
        AssetPath, AddedCount, SkippedCount);
    return 0;
}
