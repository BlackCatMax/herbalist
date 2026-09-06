// DialogueCreateCommandlet.cpp
#include "Commandlets/DialogueCreateCommandlet.h"
#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    // Построчная транскрипция прежнего литерального массива
    // HerbalistDialogueTypes.h::GetDialogueDefinitions() (1 карточка,
    // Домовой) -- значения не менялись, только способ хранения.
    TArray<FDialogueDefinition> BuildDialogueRows()
    {
        TArray<FDialogueDefinition> Defs;
        int32 Order = 0;

        {
            FDialogueDefinition D;
            D.DialogueID = FName(TEXT("Домовой"));
            D.SortOrder = Order++;
            D.StartNodeID = FName(TEXT("Home"));

            FDialogueNode Home;
            Home.NodeID = FName(TEXT("Home"));
            Home.SpeakerLine = FText::FromString(TEXT("Домовой молчит, но чувствуется его взгляд из-за печи."));

            FDialogueBranch Offer;
            Offer.ActionText = FText::FromString(TEXT("Оставить у печи блюдце молока"));
            Offer.MinGate = -1.0f; Offer.MaxGate = 1.0f;
            Offer.NextNodeID = NAME_None;
            // Символическое подношение (2026-09-06, решение пользователя:
            // бесплатный жест, без предмета) -- см. довод у
            // FDialogueBranch::bIsSymbolicOffering.
            Offer.bIsSymbolicOffering = true;
            Home.Branches.Add(Offer);

            FDialogueBranch Good;
            Good.ActionText = FText::FromString(TEXT("Прислушаться — как он расположен к дому?"));
            Good.MinGate = 0.5f; Good.MaxGate = 1.0f;
            Good.NextNodeID = FName(TEXT("GoodStanding"));
            Home.Branches.Add(Good);

            FDialogueBranch Bad;
            Bad.ActionText = FText::FromString(TEXT("Заметить неладное в углу"));
            Bad.MinGate = -1.0f; Bad.MaxGate = -0.3f;
            Bad.NextNodeID = FName(TEXT("BadStanding"));
            Home.Branches.Add(Bad);

            D.Nodes.Add(Home);

            FDialogueNode Good2;
            Good2.NodeID = FName(TEXT("GoodStanding"));
            Good2.SpeakerLine = FText::FromString(TEXT("Дом тих и ладен — Домовой оберегает его от порчи."));
            D.Nodes.Add(Good2);

            FDialogueNode Bad2;
            Bad2.NodeID = FName(TEXT("BadStanding"));
            Bad2.SpeakerLine = FText::FromString(TEXT("Пряжа спутана, миска молока опрокинута — Домовой недоволен домом."));
            D.Nodes.Add(Bad2);

            Defs.Add(D);
        }

        return Defs;
    }
}

int32 UDialogueCreateCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_Dialogue");

    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, AssetPath))
    {
        UE_LOG(LogTemp, Display, TEXT("DialogueCreate: %s уже существует (%d рядов), ничего не делаю"),
            AssetPath, Existing->GetRowMap().Num());
        return 0;
    }

    UPackage* Package = CreatePackage(AssetPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueCreate: не удалось создать пакет %s"), AssetPath);
        return 1;
    }

    UDataTable* Table = NewObject<UDataTable>(Package, FName(TEXT("DT_Dialogue")), RF_Public | RF_Standalone);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueCreate: не удалось создать UDataTable"));
        return 1;
    }
    Table->RowStruct = FDialogueDefinition::StaticStruct();

    FAssetRegistryModule::AssetCreated(Table);

    int32 AddedCount = 0;
    for (const FDialogueDefinition& Row : BuildDialogueRows())
    {
        Table->AddRow(Row.DialogueID, Row);
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
        UE_LOG(LogTemp, Error, TEXT("DialogueCreate: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("DialogueCreate: создан %s, %d рядов добавлено"), AssetPath, AddedCount);
    return 0;
}
