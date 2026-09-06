// GatheringToolAppendCommandlet.cpp
#include "Commandlets/GatheringToolAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Оси/Meta — утварь/артефакт, не алхимическое сырьё: тот же приём
    // перевода КАЧЕСТВЕННОГО характера предмета в существующую шкалу, что
    // уже устоялся у контейнеров/кристаллов Пещеры (числа не изобретают
    // баланс с нуля, только переводят "обыденный железный инструмент" vs
    // "апотропейная находка из кургана" в Direction/Meta).

    // Железный серп — самый обыденный инструмент из четырёх, тот же порядок
    // величины, что у Корзины (стартовая утварь).
    FRealState MakeIronBladeBaseState()
    {
        FRealState S;
        S.Magnitude = 0.05f;
        S.Direction.Body = 0.6f;    // рабочий инструмент, не природный материал
        S.Direction.Mind = 0.05f;
        S.Direction.Spirit = 0.05f;
        S.Direction.Nature = 0.3f;
        S.Meta.Distortion = 0.05f;
        S.Meta.Stability = 0.85f;   // кованый металл — крепче плетёной утвари
        S.Meta.Purity = 0.4f;
        S.Meta.Potency = 0.05f;
        S.Meta.Resonance = 0.1f;
        S.Meta.Corruption = 0.03f;
        return S;
    }

    // Медный серп — общинный товар, чуть выше Железного по Purity (медь не
    // куёт из руды так грубо, как обиходное железо), тот же класс величины.
    FRealState MakeCopperBladeBaseState()
    {
        FRealState S;
        S.Magnitude = 0.06f;
        S.Direction.Body = 0.55f;
        S.Direction.Mind = 0.05f;
        S.Direction.Spirit = 0.1f;
        S.Direction.Nature = 0.3f;
        S.Meta.Distortion = 0.04f;
        S.Meta.Stability = 0.85f;
        S.Meta.Purity = 0.55f;
        S.Meta.Potency = 0.06f;
        S.Meta.Resonance = 0.15f;
        S.Meta.Corruption = 0.02f;
        return S;
    }

    // Костяной нож (болин) — курганная находка, не обиходный товар: заметно
    // выше Purity/Resonance/Spirit обоих серпов, тот же довод, что уже
    // отличает Туёс (общинная награда) от Корзины/Мешка.
    FRealState MakeBoneKnifeBaseState()
    {
        FRealState S;
        S.Magnitude = 0.1f;
        S.Direction.Body = 0.4f;
        S.Direction.Mind = 0.05f;
        S.Direction.Spirit = 0.4f;   // курганная находка — ближе к оберегу, чем к обиходной утвари
        S.Direction.Nature = 0.15f;
        S.Meta.Distortion = 0.03f;
        S.Meta.Stability = 0.9f;
        S.Meta.Purity = 0.7f;
        S.Meta.Potency = 0.1f;
        S.Meta.Resonance = 0.35f;
        S.Meta.Corruption = 0.01f;
        return S;
    }

    // Серебряный оберег — не резак вовсе, чистый апотропей: тот же порядок
    // Purity/Resonance/Spirit, что у кристаллов Пещеры (Плакун-камень и
    // др.), не у утвари — предмет того же назначения (защита), просто
    // другого источника (курган, не ритуал перехода яруса).
    FRealState MakeSilverWardBaseState()
    {
        FRealState S;
        S.Magnitude = 0.4f;
        S.Direction.Body = 0.1f;
        S.Direction.Mind = 0.1f;
        S.Direction.Spirit = 0.7f;
        S.Direction.Nature = 0.1f;
        S.Meta.Distortion = 0.03f;
        S.Meta.Stability = 0.9f;
        S.Meta.Purity = 0.85f;
        S.Meta.Potency = 0.4f;
        S.Meta.Resonance = 0.55f;
        S.Meta.Corruption = 0.01f;
        return S;
    }
}

int32 UGatheringToolAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("GatheringToolAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 AddedCount = 0;
    int32 SkippedCount = 0;

    auto AddRow = [&Table, &AddedCount, &SkippedCount](FName ID, const TCHAR* DisplayName, const TCHAR* Description,
        const FRealState& BaseState, int32 RarityWeight, FName ElementName, TArray<FName> Tags,
        bool bGatheringTool, EGatheringTool ToolType, bool bSilverWard)
    {
        if (Table->GetRowMap().Contains(ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("GatheringToolAppend: ряд '%s' уже существует, пропущен"), *ID.ToString());
            ++SkippedCount;
            return;
        }

        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(DisplayName);
        Row.Description = FText::FromString(Description);
        Row.BaseState = BaseState;
        Row.Class = EIngredientClass::Catalyst;   // утварь/артефакт, не расходуемое сырьё варки
        Row.bIsWater = false;
        // AllowedBiomes пуст -- не собирается в мире. GardenNiche::None --
        // сад инструментов не касается (тот же приём, что уже у контейнеров).
        Row.RarityWeight = RarityWeight;
        Row.DecayRate = 0.0f;    // сам предмет не портится (факт материала)
        Row.Resilience = 1.0f;   // сад его не касается, никогда не занимает клетку
        Row.Element = ElementName;
        Row.Tags = MoveTemp(Tags);
        Row.GardenNiche = EGardenNiche::None;
        Row.bIsWard = false;
        Row.bIsGatheringTool = bGatheringTool;
        Row.GatheringToolType = ToolType;
        Row.bIsSilverWard = bSilverWard;

        Table->AddRow(ID, Row);
        ++AddedCount;
    };

    AddRow(
        FName(TEXT("Железный серп")),
        TEXT("Железный серп"),
        TEXT("Обиходный кованый серп, самый быстрый резак из четырёх — но железо чует и не прощает плакун-траве, чистотелу и прочей траве, не терпящей металла: тронешь их железом, вся сила уйдёт в землю. Достаётся ремеслом или уже лежит в котомке с самого первого дня."),
        MakeIronBladeBaseState(),
        5,
        FName(TEXT("Огонь")),
        { FName(TEXT("инструмент")), FName(TEXT("серп")), FName(TEXT("железо")) },
        /*bGatheringTool=*/true, EGatheringTool::IronBlade, /*bSilverWard=*/false);

    AddRow(
        FName(TEXT("Медный серп")),
        TEXT("Медный серп"),
        TEXT("Серп из красной меди — чуть медленнее железного, зато не-железный металл снимает табу с трав, что чуют сталь. Обычный товар общины, не редкость и не находка — выменять его можно за настоящую услугу или обмен, как мешок или любую другую утварь."),
        MakeCopperBladeBaseState(),
        3,
        FName(TEXT("Земля")),
        { FName(TEXT("инструмент")), FName(TEXT("серп")), FName(TEXT("медь")) },
        /*bGatheringTool=*/true, EGatheringTool::CopperBlade, /*bSilverWard=*/false);

    AddRow(
        FName(TEXT("Костяной нож")),
        TEXT("Костяной нож"),
        TEXT("Нож-болин с серповидным клинком на светлой, деревянной или костяной рукояти — необходим для сбора тонких целебных трав: срезанные им, они сохраняют всю силу, которую даже медь слегка «травмирует» при срезе. Кость — тоже не железо, снимает и табу отдельных трав заодно. Такую вещь не купишь у общины — только находка в кургане или дар хозяина места."),
        MakeBoneKnifeBaseState(),
        1,
        FName(TEXT("Земля")),
        { FName(TEXT("инструмент")), FName(TEXT("нож")), FName(TEXT("болин")), FName(TEXT("кость")), FName(TEXT("курган")) },
        /*bGatheringTool=*/true, EGatheringTool::BoneKnife, /*bSilverWard=*/false);

    AddRow(
        FName(TEXT("Серебряный оберег")),
        TEXT("Серебряный оберег"),
        TEXT("Серебро в поверье — не про остроту среза, а про защиту от нечисти: оно лежит в основе большинства оберегов, потому что нечисть его боится и сторонится. Экипированный, не расходуется и не режет — постоянно, пусть и слабо, отгоняет враждебные проявления, пока Травник его носит. Как и Костяной нож, не рыночный товар — только находка в кургане или дар хозяина места."),
        MakeSilverWardBaseState(),
        1,
        FName(TEXT("Вода")),
        { FName(TEXT("оберег")), FName(TEXT("серебро")), FName(TEXT("апотропей")), FName(TEXT("курган")) },
        /*bGatheringTool=*/false, EGatheringTool::BareHands, /*bSilverWard=*/true);

    if (AddedCount == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("GatheringToolAppend: нечего добавлять (%d уже существует), пакет не сохранён"), SkippedCount);
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
        UE_LOG(LogTemp, Error, TEXT("GatheringToolAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("GatheringToolAppend: %s теперь содержит %d рядов (добавлено %d, пропущено %d)"),
        AssetPath, Table->GetRowMap().Num(), AddedCount, SkippedCount);
    return 0;
}
