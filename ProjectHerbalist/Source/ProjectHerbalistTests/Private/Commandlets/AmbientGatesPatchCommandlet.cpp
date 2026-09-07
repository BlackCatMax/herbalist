// AmbientGatesPatchCommandlet.cpp
#include "Commandlets/AmbientGatesPatchCommandlet.h"

#include "Core/Entities/AmbientEntityTypes.h"

#include "Engine/DataTable.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    struct FGatePatch
    {
        const TCHAR* EntityID;
        EAmbientTriggerAxis Axis;
        float Threshold;
        bool bAbove;
        const TCHAR* Reason;
    };

    // Обоснование каждой строки -- в шапке заголовка (карточки компендиума
    // плюс природное значение оси у биома). Здесь только сами числа.
    const FGatePatch Patches[] = {
        { TEXT("Шептуны"),  EAmbientTriggerAxis::Spirit, 0.4f, true,
          TEXT("Spirit-доминанта карточки (0.70); природный Spirit тундры 0.30") },
        { TEXT("Плескуны"), EAmbientTriggerAxis::Nature, 0.4f, true,
          TEXT("Nature-доминанта карточки (0.80); природная Nature поймы 0.275") },
    };

    // Имя уникальное, не SaveTable: модуль собирается unity-сборкой, все
    // Commandlets/*.cpp склеиваются в одну единицу трансляции, и такой же
    // помощник в анонимном namespace у BestiaryStubsAppendCommandlet.cpp
    // дал бы ODR-редефиницию (MSVC C2084 -- тот же урок, что записан в
    // шапке Tests/TestWorldHelpers.h). Словили ровно один раз, до коммита.
    bool SaveAmbientGatesTable(UDataTable* Table)
    {
        Table->MarkPackageDirty();
        UPackage* Package = Table->GetOutermost();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Table, *FileName, Args);
    }
}

int32 UAmbientGatesPatchCommandlet::Main(const FString& Params)
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_AmbientEntities"));
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientGatesPatch: DT_AmbientEntities не загрузилась"));
        return 1;
    }

    int32 Patched = 0, AlreadyOk = 0, Missing = 0;

    for (const FGatePatch& Patch : Patches)
    {
        const FName Id(Patch.EntityID);
        FAmbientEntityDefinition* Row = Table->FindRow<FAmbientEntityDefinition>(Id, TEXT("AmbientGatesPatch"));
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("  ! строки '%s' нет в таблице -- пропущена"), Patch.EntityID);
            ++Missing;
            continue;
        }

        if (Row->TriggerAxis == Patch.Axis
            && Row->bTriggerAbove == Patch.bAbove
            && FMath::IsNearlyEqual(Row->TriggerThreshold, Patch.Threshold, KINDA_SMALL_NUMBER))
        {
            ++AlreadyOk;
            continue;
        }

        UE_LOG(LogTemp, Display, TEXT("  ~ %s: ось %d порог %.2f (above=%d) -> ось %d порог %.2f (above=%d). %s"),
            Patch.EntityID,
            static_cast<int32>(Row->TriggerAxis), Row->TriggerThreshold, Row->bTriggerAbove ? 1 : 0,
            static_cast<int32>(Patch.Axis), Patch.Threshold, Patch.bAbove ? 1 : 0,
            Patch.Reason);

        Row->TriggerAxis = Patch.Axis;
        Row->TriggerThreshold = Patch.Threshold;
        Row->bTriggerAbove = Patch.bAbove;
        ++Patched;
    }

    if (Missing > 0)
    {
        return 1;
    }

    if (Patched > 0 && !SaveAmbientGatesTable(Table))
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientGatesPatch: не удалось сохранить DT_AmbientEntities"));
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("AmbientGatesPatch: исправлено %d, уже верных %d"), Patched, AlreadyOk);
    return 0;
}
