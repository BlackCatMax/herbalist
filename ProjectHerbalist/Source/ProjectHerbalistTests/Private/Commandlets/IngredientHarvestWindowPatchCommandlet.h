// IngredientHarvestWindowPatchCommandlet.h
//
// Проставляет 5 новых полей окна сбора (AllowedSeasons/bAutumnOnly/
// HarvestTimeWindow/bRequiresMoonPhase+RequiredMoonPhase/bRequiresDryWeather,
// см. FIngredientTableRow, IngredientTableRow.h) на уже существующих рядах
// DT_IngredientClass — не добавляет и не удаляет ряды (в отличие от
// IngredientAppendCommandlet), только точечно мержит эти 5 ключей поверх
// живого JSON таблицы (тот же приём GetTableAsJSON/CreateTableFromJSONString,
// что и там), не трогая остальные поля (BaseState/Icon/ResourceMesh и т.д.),
// даже если те были правлены в редакторе после исходного импорта.
//
// Источник данных — herbalist_docs/CSV_tabs/ingredient_harvest_windows.json,
// вручную составлен по отчёту агента, прочитавшего все 76 карточек компендиума
// (04_Compendium/Растительность/*/*.md, раздел "## Где искать").
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientHarvestWindowPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientHarvestWindowPatchCommandlet.generated.h"

UCLASS()
class UIngredientHarvestWindowPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
