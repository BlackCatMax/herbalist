// DryingStatePatchCommandlet.h
//
// Проставляет FIngredientTableRow::DriedStateDelta на 6 уже существующих
// рядах живого DT_IngredientClass (сушка, DESIGN_Community_And_Homestead.md
// §2.2 "Хранилища" пункт 3, 2026-09-04: проход по всем 76 карточкам
// Plant/Fungus, см. CHANGELOG.md) -- точечно через FindRow, БЕЗ прохода
// GetTableAsJSON()/CreateTableFromJSONString() по всей таблице (тот же
// найденный баг full-roundtrip, что уже задокументирован у
// IngredientBiomeRangePatchCommandlet.h). FindRow трогает РОВНО 6 названных
// в патче рядов (Мухомор/Аконит/Чистотел/Крапива/Белый гриб/Чёрная смородина),
// остальные 93 ряда таблицы остаются побайтово теми же.
//
// Источник данных -- herbalist_docs/CSV_tabs/ingredient_drying_state_patch.json.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=DryingStatePatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DryingStatePatchCommandlet.generated.h"

UCLASS()
class UDryingStatePatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
