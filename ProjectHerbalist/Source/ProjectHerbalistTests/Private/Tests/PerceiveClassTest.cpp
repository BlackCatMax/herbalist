// Source/ProjectHerbalistTests/Private/Tests/PerceiveClassTest.cpp
//
// PerceiveClass (DECISIONS_LOG.md решение №2, 2026-09-19): Морок подменяет
// не только числа, но и вид предмета -- при высоком местном искажении травник
// видит двойника. Двойник -- LookalikeID карточки или похожий вид (тот же
// класс, общий биом, ближайший по осям). Шанс -- от порога к максимуму,
// гасится ясностью; бросок устойчив для предмета. Подменяется только имя.

#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Types/HerbalistNameUtils.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    FIngredientTableRow MakePerceiveRow(const TCHAR* Name, EIngredientClass Class, TArray<EBiomeType> Biomes, float Body, float Spirit)
    {
        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(Name);
        Row.Class = Class;
        Row.AllowedBiomes = MoveTemp(Biomes);
        Row.BaseState.Direction.Body = Body;
        Row.BaseState.Direction.Spirit = Spirit;
        Row.BaseState.Meta.Purity = 0.5f;
        return Row;
    }

    UIngredientRegistrySubsystem* MakePerceiveRegistry()
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = FIngredientTableRow::StaticStruct();
        // Болотные травы: Near ближе к Real, чем Far; гриб и таёжная трава --
        // не двойники Real (другой класс, другой биом).
        Table->AddRow(FName(TEXT("Real")), MakePerceiveRow(TEXT("Настоящая"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.8f, 0.2f));
        Table->AddRow(FName(TEXT("Near")), MakePerceiveRow(TEXT("Похожая"), EIngredientClass::Plant, { EBiomeType::Bog, EBiomeType::Floodplain }, 0.75f, 0.25f));
        Table->AddRow(FName(TEXT("Far")), MakePerceiveRow(TEXT("Непохожая"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.1f, 0.9f));
        Table->AddRow(FName(TEXT("Mushroom")), MakePerceiveRow(TEXT("Гриб"), EIngredientClass::Fungus, { EBiomeType::Bog }, 0.8f, 0.2f));
        Table->AddRow(FName(TEXT("TaigaTwin")), MakePerceiveRow(TEXT("Таёжная"), EIngredientClass::Plant, { EBiomeType::Taiga }, 0.8f, 0.2f));
        // Ручной двойник перебивает похожего: сам по осям ближе к Far, а в
        // карточке назван Near.
        FIngredientTableRow WithTwin = MakePerceiveRow(TEXT("С двойником"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.4f, 0.6f);
        WithTwin.LookalikeID = FName(TEXT("Near"));
        Table->AddRow(FName(TEXT("WithTwin")), WithTwin);
        // Двойник из карточки -- не собираемый вид: как без него.
        FIngredientTableRow BadTwin = MakePerceiveRow(TEXT("С негодным двойником"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.15f, 0.85f);
        BadTwin.LookalikeID = FName(TEXT("Tool"));
        Table->AddRow(FName(TEXT("BadTwin")), BadTwin);
        // Не двойники: инструмент, вода, оберег -- даже в том же биоме.
        FIngredientTableRow Tool = MakePerceiveRow(TEXT("Серп"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.8f, 0.2f);
        Tool.bIsGatheringTool = true;
        Table->AddRow(FName(TEXT("Tool")), Tool);
        FIngredientTableRow Water = MakePerceiveRow(TEXT("Вода"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.8f, 0.2f);
        Water.bIsWater = true;
        Table->AddRow(FName(TEXT("WaterRow")), Water);

        UIngredientRegistrySubsystem* Registry = NewObject<UIngredientRegistrySubsystem>(NewObject<UGameInstance>(GEngine));
        Registry->LoadFromDataTable(Table);
        return Registry;
    }

    FInventoryItem MakePerceiveItem(FName ID, float CreationTime)
    {
        FInventoryItem Item;
        Item.IngredientID = ID;
        Item.CreationTime = CreationTime;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerceiveClass_LookalikeIsTheSameKindFromTheSameBiome,
    "Herbalist.PerceiveClass.LookalikeIsTheSameKindFromTheSameBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerceiveClass_LookalikeIsTheSameKindFromTheSameBiome::RunTest(const FString& Parameters)
{
    UIngredientRegistrySubsystem* Registry = MakePerceiveRegistry();
    TestEqual(TEXT("Двойник -- ближайшая трава общего биома"), Registry->FindLookalike(FName(TEXT("Real"))), FName(TEXT("Near")));
    TestEqual(TEXT("Ручной двойник карточки перебивает похожего"), Registry->FindLookalike(FName(TEXT("WithTwin"))), FName(TEXT("Near")));
    TestEqual(TEXT("Негодный двойник из карточки -- как без него"), Registry->FindLookalike(FName(TEXT("BadTwin"))), FName(TEXT("Far")));
    TestEqual(TEXT("У инструмента двойника нет"), Registry->FindLookalike(FName(TEXT("Tool"))), FName(NAME_None));
    TestEqual(TEXT("У воды двойника нет"), Registry->FindLookalike(FName(TEXT("WaterRow"))), FName(NAME_None));
    TestEqual(TEXT("У гриба среди болотных грибов двойника нет"), Registry->FindLookalike(FName(TEXT("Mushroom"))), FName(NAME_None));
    TestEqual(TEXT("Таёжной не с кем путаться"), Registry->FindLookalike(FName(TEXT("TaigaTwin"))), FName(NAME_None));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerceiveClass_ChanceFollowsDistortionAndClarity,
    "Herbalist.PerceiveClass.ChanceFollowsDistortionAndClarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerceiveClass_ChanceFollowsDistortionAndClarity::RunTest(const FString& Parameters)
{
    UIngredientRegistrySubsystem* Registry = MakePerceiveRegistry();
    // Бросок -- по виду и биому сбора: по долям смотрим на много видов.
    UDataTable* Many = NewObject<UDataTable>();
    Many->RowStruct = FIngredientTableRow::StaticStruct();
    for (int32 Index = 0; Index < 400; ++Index)
    {
        Many->AddRow(FName(*FString::Printf(TEXT("Herb%d"), Index)),
            MakePerceiveRow(TEXT("Трава"), EIngredientClass::Plant, { EBiomeType::Bog }, 0.5f + 0.001f * Index, 0.5f));
    }
    UIngredientRegistrySubsystem* ManyRegistry = NewObject<UIngredientRegistrySubsystem>(NewObject<UGameInstance>(GEngine));
    ManyRegistry->LoadFromDataTable(Many);
    auto CountSwapped = [ManyRegistry](float Distortion, float Clarity)
    {
        int32 Swapped = 0;
        for (int32 Index = 0; Index < 400; ++Index)
        {
            const FName ID(*FString::Printf(TEXT("Herb%d"), Index));
            Swapped += ManyRegistry->PerceiveIngredientID(MakePerceiveItem(ID, 1.0f), Distortion, Clarity) != ID ? 1 : 0;
        }
        return Swapped;
    };

    TestEqual(TEXT("Ниже порога 0.5 -- никогда"), CountSwapped(0.45f, 0.0f), 0);
    const int32 AtMax = CountSwapped(1.0f, 0.0f);
    TestTrue(*FString::Printf(TEXT("Искажение 1, ясности нет -- около половины (%d из 400)"), AtMax), AtMax > 150 && AtMax < 250);
    const int32 Bog = CountSwapped(0.7f, 0.0f);
    TestTrue(*FString::Printf(TEXT("Болото (0.7) -- около 20%% (%d из 400)"), Bog), Bog > 45 && Bog < 115);
    TestEqual(TEXT("Полная ясность -- никогда"), CountSwapped(1.0f, 1.0f), 0);

    // Устойчиво: тот же вид из того же биома -- тот же ответ, какое бы ни
    // было время создания (слияние стопок его усредняет).
    const FName First = Registry->PerceiveIngredientID(MakePerceiveItem(FName(TEXT("Real")), 12.5f), 1.0f, 0.0f);
    for (int32 Repeat = 0; Repeat < 10; ++Repeat)
    {
        TestEqual(TEXT("Бросок устойчив к слиянию стопок"),
            Registry->PerceiveIngredientID(MakePerceiveItem(FName(TEXT("Real")), 12.5f + Repeat * 3.7f), 1.0f, 0.0f), First);
    }

    // Зелье, вода и то, чего нет в реестре, -- всегда настоящие.
    TestEqual(TEXT("Зелье не подменяется"), Registry->PerceiveIngredientID(MakePerceiveItem(FName(TEXT("Potion")), 1.0f), 1.0f, 0.0f), FName(TEXT("Potion")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPerceiveClass_OnlyTheNameIsSwapped,
    "Herbalist.PerceiveClass.OnlyTheNameIsSwapped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPerceiveClass_OnlyTheNameIsSwapped::RunTest(const FString& Parameters)
{
    UIngredientRegistrySubsystem* Registry = MakePerceiveRegistry();
    // Найти биом сбора, из которого Real при искажении 1 подменяется.
    for (int32 Biome = 0; Biome < 8; ++Biome)
    {
        FInventoryItem Item = MakePerceiveItem(FName(TEXT("Real")), 1.0f);
        Item.SourceBiome = static_cast<EBiomeType>(Biome);
        if (Registry->PerceiveIngredientID(Item, 1.0f, 0.0f) == FName(TEXT("Real")))
        {
            continue;
        }
        TestEqual(TEXT("Видно имя двойника"), GetPerceivedItemDisplayName(Item, Registry, 1.0f, 0.0f), FString(TEXT("Похожая")));
        TestEqual(TEXT("В тихом месте -- настоящее"), GetPerceivedItemDisplayName(Item, Registry, 0.2f, 0.0f), FString(TEXT("Настоящая")));
        TestEqual(TEXT("Сам предмет не тронут"), Item.IngredientID, FName(TEXT("Real")));
        return true;
    }
    AddError(TEXT("Ни из одного биома Real не подменился при искажении 1"));
    return false;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
