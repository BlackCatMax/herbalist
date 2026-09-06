// DomovoiMilkOfferingPatchCommandlet.cpp
#include "Commandlets/DomovoiMilkOfferingPatchCommandlet.h"
#include "Core/Dialogue/HerbalistDialogueTypes.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

int32 UDomovoiMilkOfferingPatchCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_Dialogue");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("DomovoiMilkOfferingPatch: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    FDialogueDefinition* Row = Table->FindRow<FDialogueDefinition>(
        FName(TEXT("Домовой")), TEXT("DomovoiMilkOfferingPatch"), /*bWarnIfRowMissing=*/false);
    if (!Row)
    {
        UE_LOG(LogTemp, Error, TEXT("DomovoiMilkOfferingPatch: ряд 'Домовой' не найден в живой таблице"));
        return 1;
    }

    FDialogueNode* HomeNode = nullptr;
    for (FDialogueNode& N : Row->Nodes)
    {
        if (N.NodeID == FName(TEXT("Home"))) { HomeNode = &N; break; }
    }
    if (!HomeNode)
    {
        UE_LOG(LogTemp, Error, TEXT("DomovoiMilkOfferingPatch: узел 'Home' не найден"));
        return 1;
    }

    FDialogueBranch* OfferBranch = nullptr;
    for (FDialogueBranch& B : HomeNode->Branches)
    {
        if (B.ActionText.ToString() == TEXT("Оставить у печи блюдце молока")) { OfferBranch = &B; break; }
    }
    if (!OfferBranch)
    {
        UE_LOG(LogTemp, Error, TEXT("DomovoiMilkOfferingPatch: ветка 'Оставить у печи блюдце молока' не найдена"));
        return 1;
    }

    if (OfferBranch->bIsSymbolicOffering)
    {
        UE_LOG(LogTemp, Display, TEXT("DomovoiMilkOfferingPatch: bIsSymbolicOffering уже true, пакет не пересохранён"));
        return 0;
    }

    OfferBranch->bIsSymbolicOffering = true;
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
        UE_LOG(LogTemp, Error, TEXT("DomovoiMilkOfferingPatch: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("DomovoiMilkOfferingPatch: bIsSymbolicOffering=true проставлен, остальные ряды не тронуты"));
    return 0;
}
