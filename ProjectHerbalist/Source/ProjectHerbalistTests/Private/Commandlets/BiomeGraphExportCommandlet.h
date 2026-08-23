// BiomeGraphExportCommandlet.h
//
// ROADMAP.md §4 "DA_BiomeGraph -> JSON": MorokAffinity/ZaryanaAffinity/Stability
// решают, срабатывает ли Bifurcation, но живут только в бинарнике DA_BiomeGraph
// (UBiomeGraphAsset, обычный UDataAsset — не UDataTable, у которого есть штатный
// Import/Export CSV/JSON, тут его нет). Этот коммандлет — та же роль, что у
// extract_ingredients.py/extract_biomes.py: сделать значения ревьюируемыми в git.
// В отличие от тех скриптов, читает не markdown, а сам живой asset через
// LoadObject — иначе бинарник в принципе не прочитать снаружи движка.
//
// Импорт (JSON -> asset, с записью пакета) сознательно не сделан в этом же
// проходе: экспорт закрывает названную в ROADMAP боль ("не ревьюится"), а
// запись пакета через UPackage::SavePackage — отдельная, более рискованная
// операция, которую не стоило приплетать не глядя.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=BiomeGraphExport
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BiomeGraphExportCommandlet.generated.h"

UCLASS()
class UBiomeGraphExportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
