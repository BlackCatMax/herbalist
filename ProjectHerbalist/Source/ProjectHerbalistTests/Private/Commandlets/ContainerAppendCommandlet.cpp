// ContainerAppendCommandlet.cpp
#include "Commandlets/ContainerAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Оси/Meta -- утварь, не алхимическое сырьё: числа скромные, нейтральные,
    // тот же порядок величины, что уже у Перегноя (Magnitude/Potency низкие,
    // не сильный ингредиент для варки и не должен им быть). Direction.Nature
    // доминирует у всех трёх -- растительный/природный материал (лыко,
    // береста, дерюга), не рукотворный металл/камень.

    // Корзина (лукошко) -- плетёная из лыка/бересты, открытая, дышащая
    // (см. довод у EStorageContainerType::Basket). Самая скромная утварь из
    // трёх: наименьшие Purity/Stability среди контейнеров.
    FRealState MakeBasketBaseState()
    {
        FRealState S;
        S.Magnitude = 0.05f;
        S.Direction.Body = 0.3f;
        S.Direction.Mind = 0.05f;
        S.Direction.Spirit = 0.05f;
        S.Direction.Nature = 0.6f;
        S.Meta.Distortion = 0.05f;
        S.Meta.Stability = 0.8f;    // плетёная, но крепкая утварь
        S.Meta.Purity = 0.5f;
        S.Meta.Potency = 0.05f;
        S.Meta.Resonance = 0.2f;
        S.Meta.Corruption = 0.02f;
        return S;
    }

    // Мешок (дерюга/рогожа) -- грубая тканина, хуже корзины: держит влагу
    // вместо проветривания (тот же довод, что уже у SackDecayMultiplier).
    // Purity ниже Корзины -- грубее выделка.
    FRealState MakeSackBaseState()
    {
        FRealState S;
        S.Magnitude = 0.05f;
        S.Direction.Body = 0.3f;
        S.Direction.Mind = 0.05f;
        S.Direction.Spirit = 0.05f;
        S.Direction.Nature = 0.6f;
        S.Meta.Distortion = 0.06f;
        S.Meta.Stability = 0.75f;
        S.Meta.Purity = 0.4f;
        S.Meta.Potency = 0.05f;
        S.Meta.Resonance = 0.15f;
        S.Meta.Corruption = 0.03f;
        return S;
    }

    // Туёс (берестяной короб) -- лучшая утварь из трёх: реальное природное
    // антисептическое/влагостойкое свойство бересты (см. довод у
    // EStorageContainerType::Tues), выше Purity/Stability обоих
    // остальных -- качественная общинная награда, не примитивная находка.
    FRealState MakeTuesBaseState()
    {
        FRealState S;
        S.Magnitude = 0.08f;
        S.Direction.Body = 0.3f;
        S.Direction.Mind = 0.1f;
        S.Direction.Spirit = 0.15f;
        S.Direction.Nature = 0.45f;
        S.Meta.Distortion = 0.03f;
        S.Meta.Stability = 0.85f;
        S.Meta.Purity = 0.65f;   // антисептическая береста -- чище обоих остальных
        S.Meta.Potency = 0.08f;
        S.Meta.Resonance = 0.25f;
        S.Meta.Corruption = 0.01f;
        return S;
    }
}

int32 UContainerAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("ContainerAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 AddedCount = 0;
    int32 SkippedCount = 0;

    auto AddContainer = [&](FName ID, const TCHAR* DisplayName, const TCHAR* Description,
        const FRealState& BaseState, EStorageContainerType GrantsType, int32 RarityWeight,
        TArray<FName> Tags, FName ElementName)
    {
        if (Table->GetRowMap().Contains(ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("ContainerAppend: ряд '%s' уже существует, пропущен"), *ID.ToString());
            ++SkippedCount;
            return;
        }

        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(DisplayName);
        Row.Description = FText::FromString(Description);
        Row.BaseState = BaseState;
        Row.Class = EIngredientClass::Catalyst;   // утварь/инструмент, не расходуемое сырьё варки
        Row.bIsWater = false;
        // AllowedBiomes пуст -- не собирается в мире. GardenNiche::None --
        // сад контейнеров не касается (тот же приём, что уже у тиражных
        // оберегов/Перегноя).
        Row.RarityWeight = RarityWeight;
        Row.DecayRate = 0.0f;    // сам контейнер не портится (факт материала, тот же довод, что у каменных оберегов)
        Row.Resilience = 1.0f;   // сад его не касается, никогда не занимает клетку
        Row.Element = ElementName;
        Row.Tags = MoveTemp(Tags);
        Row.GardenNiche = EGardenNiche::None;
        Row.bIsWard = false;
        Row.GrantsContainerType = GrantsType;

        Table->AddRow(ID, Row);
        ++AddedCount;
    };

    AddContainer(
        FName(TEXT("Корзина")),
        TEXT("Корзина"),
        TEXT("Плетёное лукошко из лыка или бересты — самая простая и самая привычная утварь любого крестьянского дома, с ней сподручно и по грибы-ягоды, и мимо погреба. Открытая, дышащая работа: трава в такой корзине не преет, но и от сырости почти не защищена. С такой корзиной за плечами Травник и выходит в первый раз за порог — примитивный, но честный переносной запас."),
        MakeBasketBaseState(),
        EStorageContainerType::Basket,
        5,
        { FName(TEXT("утварь")), FName(TEXT("корзина")), FName(TEXT("лукошко")), FName(TEXT("лыко")), FName(TEXT("контейнер")) },
        FName(TEXT("Воздух")));

    AddContainer(
        FName(TEXT("Мешок")),
        TEXT("Мешок"),
        TEXT("Грубая дерюга или рогожа, сшитая из толстой пряжи лыка или мочала — тканина попроще корзины, зато вместительнее. Держит форму, но и влагу держит не хуже — сырость из мешка так просто не выйдет, а моль да вредители до содержимого добираются охотнее, чем до плетёного лукошка. Получить такой можно у общины, за настоящую услугу или обмен, не с самого первого дня."),
        MakeSackBaseState(),
        EStorageContainerType::Sack,
        3,
        { FName(TEXT("утварь")), FName(TEXT("мешок")), FName(TEXT("дерюга")), FName(TEXT("рогожа")), FName(TEXT("контейнер")) },
        FName(TEXT("Вода")));

    AddContainer(
        FName(TEXT("Туёс")),
        TEXT("Туёс"),
        TEXT("Берестяной короб с плотно пригнанной крышкой — на Руси в него клали то, что дорого сохранить: мёд, масло, сушёные травы. Береста обладает настоящим природным антисептическим и влагостойким свойством, оттого туёс издавна превосходил многие другие сосуды для хранения — не мебель дома, а то, что берут с собой в дорогу, но такое, что дому не уступит. Заслужить его можно только у общины — качественная переносная вещь не даётся даром."),
        MakeTuesBaseState(),
        EStorageContainerType::Tues,
        1,
        { FName(TEXT("утварь")), FName(TEXT("туёс")), FName(TEXT("береста")), FName(TEXT("контейнер")) },
        FName(TEXT("Земля")));

    if (AddedCount == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("ContainerAppend: нечего добавлять (%d уже существует), пакет не сохранён"), SkippedCount);
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
        UE_LOG(LogTemp, Error, TEXT("ContainerAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("ContainerAppend: %s теперь содержит %d рядов (добавлено %d, пропущено %d)"),
        AssetPath, Table->GetRowMap().Num(), AddedCount, SkippedCount);
    return 0;
}
