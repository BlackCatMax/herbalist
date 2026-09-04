// IngredientBiomeRangePatchCommandlet.h
//
// Проставляет пересмотренные AllowedBiomes (ботаническая сверка 76 карточек,
// 2026-09-04: см. CHANGELOG.md) на уже существующих рядах живого
// DT_IngredientClass -- точечно через FindRow, БЕЗ прохода
// GetTableAsJSON()/CreateTableFromJSONString() по всей таблице (тот же
// найденный баг, что уже задокументирован у
// IngredientGatheringAndGardenPatchCommandlet.cpp: полный JSON-роундтрип
// молча теряет ряды, чьё имя содержит пробел -- "Молодильное яблоко" и все
// 4 "Перо *"). FindRow трогает РОВНО те 70 рядов, что названы в патче,
// остальные 6 (сознательно оставленные ботанически узкими -- Сфагнум,
// Плакун-трава, Стрелолист, Катран, Тюльпан степной, Полярный мак) и Icon/
// ResourceMesh/BaseState/Tags всех рядов остаются побайтово теми же.
//
// Источник данных -- herbalist_docs/CSV_tabs/ingredient_biome_range_patch.json
// (70 из 76 не-водных карточек; сгенерирован из того же решения, что уже
// применено к herbalist_docs/CSV_tabs/ingredients.json).
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientBiomeRangePatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientBiomeRangePatchCommandlet.generated.h"

UCLASS()
class UIngredientBiomeRangePatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
