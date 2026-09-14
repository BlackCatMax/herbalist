// IngredientHarvestWindowPatchCommandlet.h
//
// Проставляет 5 новых полей окна сбора (AllowedSeasons/bAutumnOnly/
// HarvestTimeWindow/bRequiresMoonPhase+RequiredMoonPhase/bRequiresDryWeather,
// см. FIngredientTableRow, IngredientTableRow.h) на уже существующих рядах
// DT_IngredientClass — не добавляет и не удаляет ряды (в отличие от
// IngredientAppendCommandlet), только точечно правит эти 5 полей через
// FindRow, не трогая остальные поля и ряды. Полный JSON-проход
// (GetTableAsJSON/CreateTableFromJSONString) убран 2026-09-04: он молча терял
// ряды с пробелом в имени (см. .cpp).
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
