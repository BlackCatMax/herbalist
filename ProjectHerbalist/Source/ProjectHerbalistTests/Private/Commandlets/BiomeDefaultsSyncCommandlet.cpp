// BiomeDefaultsSyncCommandlet.cpp
#include "Commandlets/BiomeDefaultsSyncCommandlet.h"

#include "Commandlets/DocumentedBiomeValues.h"
#include "Core/Types/BiomeRow.h"
#include "Core/Types/BiomeTypes.h"

#include "Engine/DataTable.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    // Имя уникальное, не SaveTable: модуль собирается unity-сборкой, все
    // Commandlets/*.cpp склеиваются в одну единицу трансляции, и одноимённый
    // помощник в анонимном namespace соседнего файла дал бы ODR-редефиницию
    // (MSVC C2084 -- урок из шапки Tests/TestWorldHelpers.h).
    bool SaveBiomeDefaultsTable(UDataTable* Table)
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

    // Пишет значение, если оно расходится с документацией, и печатает
    // «было -> стало». Возвращает true, если что-то изменилось.
    bool SyncField(const TCHAR* BiomeName, const TCHAR* FieldName, float& Current, float Documented)
    {
        if (FMath::IsNearlyEqual(Current, Documented, KINDA_SMALL_NUMBER))
        {
            return false;
        }
        UE_LOG(LogTemp, Display, TEXT("  ~ %s.%s: %.3f -> %.3f"), BiomeName, FieldName, Current, Documented);
        Current = Documented;
        return true;
    }
}

int32 UBiomeDefaultsSyncCommandlet::Main(const FString& Params)
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_BiomeDefaults"));
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeDefaultsSync: DT_BiomeDefaults не загрузилась"));
        return 1;
    }

    int32 Changed = 0, Missing = 0;

    for (const FDocumentedBiome& Doc : HerbalistDocumentedBiomes::Get())
    {
        const FName RowName = FBiomeDefaults::BiomeTypeToName(Doc.Biome);
        FBiomeRow* Row = Table->FindRow<FBiomeRow>(RowName, TEXT("BiomeDefaultsSync"));
        if (!Row)
        {
            UE_LOG(LogTemp, Error, TEXT("  ! строки '%s' нет в таблице"), *RowName.ToString());
            ++Missing;
            continue;
        }

        const FString NameStr = RowName.ToString();
        const TCHAR* N = *NameStr;

        int32 Before = Changed;
        Changed += SyncField(N, TEXT("Direction.Body"), Row->Direction.Body, Doc.Body) ? 1 : 0;
        Changed += SyncField(N, TEXT("Direction.Mind"), Row->Direction.Mind, Doc.Mind) ? 1 : 0;
        Changed += SyncField(N, TEXT("Direction.Spirit"), Row->Direction.Spirit, Doc.Spirit) ? 1 : 0;
        Changed += SyncField(N, TEXT("Direction.Nature"), Row->Direction.Nature, Doc.Nature) ? 1 : 0;
        Changed += SyncField(N, TEXT("Magnitude"), Row->Magnitude, Doc.Magnitude) ? 1 : 0;
        Changed += SyncField(N, TEXT("Meta.Distortion"), Row->Meta.Distortion, Doc.Distortion) ? 1 : 0;
        Changed += SyncField(N, TEXT("Meta.Stability"), Row->Meta.Stability, Doc.Stability) ? 1 : 0;
        Changed += SyncField(N, TEXT("Meta.Purity"), Row->Meta.Purity, Doc.Purity) ? 1 : 0;
        Changed += SyncField(N, TEXT("Meta.Potency"), Row->Meta.Potency, Doc.Potency) ? 1 : 0;
        Changed += SyncField(N, TEXT("Meta.Resonance"), Row->Meta.Resonance, Doc.Resonance) ? 1 : 0;
        Changed += SyncField(N, TEXT("Meta.Corruption"), Row->Meta.Corruption, Doc.Corruption) ? 1 : 0;
        Changed += SyncField(N, TEXT("Environment.Toxicity"), Row->Environment.Toxicity, Doc.Toxicity) ? 1 : 0;
        Changed += SyncField(N, TEXT("Environment.Fertility"), Row->Environment.Fertility, Doc.Fertility) ? 1 : 0;
        Changed += SyncField(N, TEXT("Environment.Moisture"), Row->Environment.Moisture, Doc.Moisture) ? 1 : 0;

        if (Changed == Before)
        {
            UE_LOG(LogTemp, Display, TEXT("  = %s: уже по документации"), N);
        }
    }

    if (Missing > 0)
    {
        return 1;
    }

    if (Changed > 0 && !SaveBiomeDefaultsTable(Table))
    {
        UE_LOG(LogTemp, Error, TEXT("BiomeDefaultsSync: не удалось сохранить DT_BiomeDefaults"));
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("BiomeDefaultsSync: приведено к документации значений: %d"), Changed);
    UE_LOG(LogTemp, Display, TEXT("НЕ трогались (документация их не задаёт): EntityActivityBase, DefaultWaterState, StressRecoveryMultiplier."));
    return 0;
}
