// KalinovMostDialogueAppendCommandlet.cpp
#include "Commandlets/KalinovMostDialogueAppendCommandlet.h"
#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    FDialogueDefinition BuildZmeyGorynychRow(int32 SortOrder)
    {
        FDialogueDefinition D;
        D.DialogueID = FName(TEXT("ЗмейГорыныч"));
        D.SortOrder = SortOrder;
        D.StartNodeID = FName(TEXT("Bridge"));

        FDialogueNode Bridge;
        Bridge.NodeID = FName(TEXT("Bridge"));
        Bridge.SpeakerLine = FText::FromString(TEXT("Огненная река шипит под Калиновым мостом — Трёхглавый Змей поднимает головы, дорога дальше закрыта."));

        FDialogueBranch Fight;
        Fight.ActionText = FText::FromString(TEXT("Вступить в бой со Змеем"));
        Fight.MinGate = -1.0f; Fight.MaxGate = 1.0f;
        Fight.NextNodeID = NAME_None;
        Fight.bIsKalinovMostFight = true;
        Bridge.Branches.Add(Fight);

        FDialogueBranch Deal;
        Deal.ActionText = FText::FromString(TEXT("Откупиться подношением, пройти без боя"));
        Deal.MinGate = -1.0f; Deal.MaxGate = 1.0f;
        Deal.NextNodeID = NAME_None;
        // Сделка (2026-09-06, DESIGN_POI_Art_And_LevelDesign.md) -- см.
        // довод у FDialogueBranch::bIsKalinovMostDeal.
        Deal.bIsKalinovMostDeal = true;
        Bridge.Branches.Add(Deal);

        D.Nodes.Add(Bridge);
        return D;
    }
}

int32 UKalinovMostDialogueAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_Dialogue");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDialogueAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    const FName RowName(TEXT("ЗмейГорыныч"));
    if (Table->FindRow<FDialogueDefinition>(RowName, TEXT("KalinovMostDialogueAppend"), /*bWarnIfRowMissing=*/false))
    {
        UE_LOG(LogTemp, Display, TEXT("KalinovMostDialogueAppend: ряд '%s' уже существует, ничего не делаю"), *RowName.ToString());
        return 0;
    }

    const FDialogueDefinition NewRow = BuildZmeyGorynychRow(Table->GetRowMap().Num());
    Table->AddRow(RowName, NewRow);
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
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDialogueAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("KalinovMostDialogueAppend: ряд '%s' добавлен, остальные ряды не тронуты"), *RowName.ToString());
    return 0;
}
