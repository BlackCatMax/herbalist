// BiomeGraphImportCommandlet.h
//
// Обратное направление к BiomeGraphExportCommandlet: читает
// herbalist_docs/CSV_tabs/DA_BiomeGraph.json и записывает значения обратно в
// живой DA_BiomeGraph (LoadObject + UPackage::SavePackage — у UDataAsset,
// в отличие от UDataTable, нет штатной кнопки реимпорта). Осознанно отдельный
// коммандлет от экспорта, не флаг того же: запись пакета — необратимая
// операция другого класса риска, чем чтение, лучше не покрывать одним именем.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=BiomeGraphImport
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BiomeGraphImportCommandlet.generated.h"

UCLASS()
class UBiomeGraphImportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
