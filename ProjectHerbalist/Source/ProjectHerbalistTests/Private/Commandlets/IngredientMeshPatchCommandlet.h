// IngredientMeshPatchCommandlet.h
//
// Проставляет FIngredientTableRow::ResourceMesh точечным рядам живого
// DT_IngredientClass (2026-09-28, «скачал в проект остальное»): серпы, нож,
// корзина, грибы, камни, фонарь -- модели из паков вместо шарика в руке и
// пестере; у ряда «Water» меш снимается (там стояли стрелки осей LiveLink,
// вода показывается мешем из Herbalist Settings, WaterItemMesh). Пустая строка
// в JSON -- меш снять.
//
// Точечно через FindRow, БЕЗ прохода GetTableAsJSON()/CreateTableFromJSON
// String() по всей таблице (тот же довод, что у IngredientDryingDurationPatch:
// полный круг через JSON переименовывает ряды с пробелами).
//
// Источник данных -- herbalist_docs/CSV_tabs/ingredient_mesh_patch.json.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientMeshPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientMeshPatchCommandlet.generated.h"

UCLASS()
class UIngredientMeshPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
