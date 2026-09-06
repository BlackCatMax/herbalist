// KalinovMostDealPatchCommandlet.cpp
#include "Commandlets/KalinovMostDealPatchCommandlet.h"
#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

int32 UKalinovMostDealPatchCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_Dialogue");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDealPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    FDialogueDefinition* Row = Table->FindRow<FDialogueDefinition>(
        FName(TEXT("ЗмейГорыныч")), TEXT("KalinovMostDealPatch"), /*bWarnIfRowMissing=*/false);
    if (!Row)
    {
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDealPatch: ряд 'ЗмейГорыныч' не найден в живой таблице"));
        return 1;
    }

    FDialogueNode* BridgeNode = nullptr;
    for (FDialogueNode& N : Row->Nodes)
    {
        if (N.NodeID == FName(TEXT("Bridge"))) { BridgeNode = &N; break; }
    }
    if (!BridgeNode)
    {
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDealPatch: узел 'Bridge' не найден"));
        return 1;
    }

    FDialogueBranch* DealBranch = nullptr;
    for (FDialogueBranch& B : BridgeNode->Branches)
    {
        if (B.ActionText.ToString() == TEXT("Откупиться подношением, пройти без боя")) { DealBranch = &B; break; }
    }
    if (!DealBranch)
    {
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDealPatch: ветка 'Откупиться подношением, пройти без боя' не найдена"));
        return 1;
    }

    if (DealBranch->bIsKalinovMostDeal)
    {
        UE_LOG(LogTemp, Display, TEXT("KalinovMostDealPatch: bIsKalinovMostDeal уже true, пакет не пересохранён"));
        return 0;
    }

    DealBranch->bIsKalinovMostDeal = true;
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
        UE_LOG(LogTemp, Error, TEXT("KalinovMostDealPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("KalinovMostDealPatch: bIsKalinovMostDeal=true проставлен, остальные ряды не тронуты"));
    return 0;
}
