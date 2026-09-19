// OrdersCreateCommandlet.cpp

#include "OrdersCreateCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    const TCHAR* OrdersAssetPath = TEXT("/Game/Herbalist/Data/DT_Orders");

    FOrderPayment Pay(const TCHAR* ItemID, int32 Count)
    {
        FOrderPayment Payment;
        Payment.ItemID = FName(ItemID);
        Payment.Count = Count;
        return Payment;
    }

    struct FOrderRowBuilder
    {
        TArray<FOrderDefinition>& Out;
        int32 Order = 0;

        FOrderDefinition& Add(const TCHAR* ID, EOrderCircle Circle, const TCHAR* Note, TArray<EOrderAxis> Leading, int32 DeadlineDays,
            FOrderPayment Deposit, FOrderPayment Payment, FOrderPayment Bonus)
        {
            FOrderDefinition& Row = Out.AddDefaulted_GetRef();
            Row.ID = FName(ID);
            Row.SortOrder = Order++;
            Row.Circle = Circle;
            Row.NoteText = FText::FromString(Note);
            Row.LeadingAxes = MoveTemp(Leading);
            Row.DeadlineDays = DeadlineDays;
            Row.Deposit = Deposit;
            Row.Payment = Payment;
            Row.ExactBonus = Bonus;
            return Row;
        }
    };
}

TArray<FOrderDefinition> UOrdersCreateCommandlet::BuildOrderRows()
{
    TArray<FOrderDefinition> Out;
    FOrderRowBuilder B{ Out };

    // ---- Селяне ----
    {
        FOrderDefinition& R = B.Add(TEXT("VIL_HEAL"), EOrderCircle::Villagers,
            TEXT("Мужик мой с покоса пришёл, руку косой рассёк, а к ночи огневица поднялась. Оставь снадобье у порога, заберу затемно. -- Ульяна"),
            { EOrderAxis::Body }, 2, Pay(TEXT("broad_04"), 1), Pay(TEXT("Туёс"), 1), Pay(TEXT("riv_04"), 2));
        R.MetaMin.Purity = 0.6f;
        R.MetaMax.Corruption = 0.2f;
    }
    {
        FOrderDefinition& R = B.Add(TEXT("VIL_CATTLE"), EOrderCircle::Villagers,
            TEXT("Корова третий день не встаёт, в боках огонь. Оставь что-нибудь у плетня до субботы. -- Фёкла с выселок"),
            { EOrderAxis::Nature, EOrderAxis::Body }, 3, Pay(TEXT("les_04"), 1), Pay(TEXT("Мешок"), 1), Pay(TEXT("les_03"), 2));
        R.MetaMin.Purity = 0.5f;
    }
    {
        FOrderDefinition& R = B.Add(TEXT("VIL_UNCURSE"), EOrderCircle::Villagers,
            TEXT("Дитё с Покрова сохнет, по ночам кричит. Бабки говорят -- сглазили на ярмарке. Помоги, чем знаешь. Отблагодарим."),
            { EOrderAxis::Spirit }, 4, Pay(TEXT("mix_04"), 1), Pay(TEXT("Корзина"), 1), Pay(TEXT("riv_11"), 1));
        R.MetaMin.Purity = 0.7f;
        R.MetaMax.Corruption = 0.1f;
    }
    {
        FOrderDefinition& R = B.Add(TEXT("VIL_HEARTH"), EOrderCircle::Villagers,
            TEXT("Сын с невесткой грызутся, как чужие, изба не держит лада. Не приворот прошу -- чтобы мир в доме был."),
            { EOrderAxis::Spirit, EOrderAxis::Mind }, 5, Pay(TEXT("broad_05"), 1), Pay(TEXT("broad_09"), 2), Pay(TEXT("tai_01"), 2));
        R.MetaMin.Stability = 0.5f;
    }

    // ---- Ратные и ловчие люди ----
    {
        FOrderDefinition& R = B.Add(TEXT("WAR_BATTLE"), EOrderCircle::Warriors,
            TEXT("Дружина уходит к броду стеречь рубеж. Нужно снадобье, чтоб рука не дрогнула и кровь не лилась попусту. Заберём через два дня."),
            { EOrderAxis::Body }, 2, Pay(TEXT("ste_05"), 1), Pay(TEXT("Медный серп"), 1), Pay(TEXT("Серебряный оберег"), 1));
        R.MetaMin.Potency = 0.6f;
    }
    {
        FOrderDefinition& R = B.Add(TEXT("WAR_ROAD"), EOrderCircle::Warriors,
            TEXT("Обоз идёт на полночь, за морошкой и мехами. Дай в дорогу, чтоб ноги несли и холод не брал. Расплатимся северным."),
            { EOrderAxis::Body, EOrderAxis::Nature }, 3, Pay(TEXT("tai_03"), 1), Pay(TEXT("tun_03"), 2), Pay(TEXT("tun_01"), 1));
        R.MetaMin.Stability = 0.5f;
    }
    {
        FOrderDefinition& R = B.Add(TEXT("WAR_HUNT"), EOrderCircle::Warriors,
            TEXT("Ловчие идут на лося. Дай такого, чтоб зверя чуять раньше, чем он нас. Нож костяной -- за труды."),
            { EOrderAxis::Nature, EOrderAxis::Mind }, 3, Pay(TEXT("tai_05"), 1), Pay(TEXT("Костяной нож"), 1), Pay(TEXT("tai_09"), 1));
        R.MetaMin.Potency = 0.4f;
    }

    // ---- Лихие люди ----
    {
        FOrderDefinition& R = B.Add(TEXT("OUT_POISON"), EOrderCircle::Outlaws,
            TEXT("Нужно такое, чтоб скотина у соседа легла и не встала. Оставь в дупле у старой вербы. Задаток под камнем. Серебром заплачу."),
            { EOrderAxis::Body }, 3, Pay(TEXT("ste_08"), 1), Pay(TEXT("Серебряный оберег"), 1), FOrderPayment());
        R.MetaMin.Corruption = 0.6f;
        R.Consequence = EOrderConsequence::Poison;
        R.ConsequenceRumor = FText::FromString(TEXT("На выселках пал скот, у двоих хворь с животом. Говорят -- отравили. Кто -- не говорят."));
    }
    {
        FOrderDefinition& R = B.Add(TEXT("OUT_SLEEP"), EOrderCircle::Outlaws,
            TEXT("Нужно, чтобы сторож у купеческих амбаров спал крепко. Одну ночь. Оставь в дупле у старой вербы. Задаток под камнем."),
            { EOrderAxis::Mind }, 2, Pay(TEXT("les_06"), 1), Pay(TEXT("Мешок"), 2), FOrderPayment());
        R.MetaMax.Stability = 0.3f;
        R.Consequence = EOrderConsequence::SleepTheft;
        R.ConsequenceRumor = FText::FromString(TEXT("У купца обнесли амбар, сторож проспал до полудня и ничего не помнит."));
    }
    {
        FOrderDefinition& R = B.Add(TEXT("OUT_CHARM"), EOrderCircle::Outlaws,
            TEXT("Есть девка за рекой, на меня не глядит. Сделай, чтоб глядела. Чтоб без меня ей свет был не мил."),
            { EOrderAxis::Spirit, EOrderAxis::Mind }, 4, Pay(TEXT("ste_03"), 1), Pay(TEXT("tai_10"), 1), FOrderPayment());
        R.MetaMin.Corruption = 0.4f;
        R.Consequence = EOrderConsequence::Charm;
        R.ConsequenceRumor = FText::FromString(TEXT("За рекой девка сама не своя: мать не узнаёт, ходит за одним по пятам. Люди шепчутся -- присушили."));
    }
    {
        FOrderDefinition& R = B.Add(TEXT("OUT_CURSE"), EOrderCircle::Outlaws,
            TEXT("Сосед межу украл и смеётся. Сделай, чтоб на его поле ничего не взошло. Подложу сам, ты только свари."),
            { EOrderAxis::Spirit }, 4, Pay(TEXT("mix_07"), 1), Pay(TEXT("mix_10"), 1), FOrderPayment());
        R.MetaMin.Corruption = 0.6f;
        R.Consequence = EOrderConsequence::CurseNeighbour;
        R.ConsequenceRumor = FText::FromString(TEXT("У одного на поле всё почернело за ночь. Говорят -- порча, и что соседа его в ту ночь видели у межи."));
    }
    return Out;
}

int32 UOrdersCreateCommandlet::Main(const FString& Params)
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, OrdersAssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    const bool bSync = FParse::Param(*Params, TEXT("sync"));
    if (Table && !bSync)
    {
        UE_LOG(LogTemp, Display, TEXT("OrdersCreate: %s уже есть (%d рядов), ничего не делаю (-sync -- переписать)"),
            OrdersAssetPath, Table->GetRowMap().Num());
        return 0;
    }
    if (!Table)
    {
        UPackage* Package = CreatePackage(OrdersAssetPath);
        Table = NewObject<UDataTable>(Package, TEXT("DT_Orders"), RF_Public | RF_Standalone);
        Table->RowStruct = FOrderDefinition::StaticStruct();
        FAssetRegistryModule::AssetCreated(Table);
    }
    Table->EmptyTable();
    for (const FOrderDefinition& Row : BuildOrderRows())
    {
        Table->AddRow(Row.ID, Row);
    }

    UPackage* Package = Table->GetOutermost();
    Package->MarkPackageDirty();
    const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Table, *FileName, Args))
    {
        UE_LOG(LogTemp, Error, TEXT("OrdersCreate: не удалось сохранить %s"), *FileName);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("OrdersCreate: %s -- %d заказов"), OrdersAssetPath, Table->GetRowMap().Num());
    return 0;
}
