// IngredientDryingDurationPatchCommandlet.h
//
// Проставляет FIngredientTableRow::DryingDurationSeconds на ВСЕХ 76 рядах
// Plant/Fungus живого DT_IngredientClass (сушка, "процесс сушки у разных
// растений разный (длительность)", прямой запрос пользователя 2026-09-05) --
// ОТДЕЛЬНЫЙ коммандлет/JSON от DryingStatePatchCommandlet намеренно: тот
// патчит DriedStateDelta только 15 карточкам с реальным ботаническим
// основанием для алхимической дельты, а здесь нужно назначить длительность
// ВСЕМ 76 карточкам разом (каждая либо тонкая, либо ягода, либо гриб, либо
// плотный/деревянистый материал -- ни одна не остаётся без решения) -- два
// разных по охвату патча честнее держать раздельно, чем натягивать узкий
// JSON/командлет на широкую задачу.
//
// Точечно через FindRow, БЕЗ прохода GetTableAsJSON()/CreateTableFromJSON
// String() по всей таблице (тот же найденный full-roundtrip баг, что уже
// задокументирован у IngredientBiomeRangePatchCommandlet.h/DryingStatePatchCommandlet.h).
//
// Источник данных -- herbalist_docs/CSV_tabs/ingredient_drying_duration_patch.json.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientDryingDurationPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientDryingDurationPatchCommandlet.generated.h"

UCLASS()
class UIngredientDryingDurationPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
