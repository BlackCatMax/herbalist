// IngredientGatheringAndGardenPatchCommandlet.h
//
// Проставляет bIronAverse/bDelicate/GardenNiche (DESIGN_Community_And_
// Homestead.md §2.3/§2.4, см. FIngredientTableRow) на уже существующих
// рядах DT_IngredientClass — тот же приём GetTableAsJSON/
// CreateTableFromJSONString, что уже применён IngredientHarvestWindowPatch
// Commandlet (не добавляет/не удаляет ряды, мержит только присланные ключи
// поверх живого JSON, остальные поля не трогает).
//
// Источник данных — herbalist_docs/CSV_tabs/ingredient_gathering_and_garden_
// flags.json. bIronAverse/bDelicate — три карточки (Плакун-трава/Чистотел/
// Медуница), дописанные реальным фольклором в этой же сессии 2026-08-31.
// GardenNiche — первая вертикальная выборка (2 ингредиента на каждую из 5
// пристроек, 10 из ~61 подтверждённых проходом по компендиуму) — не полный
// охват, тот же принцип, что бестиарий 4/62 или реестр ритуалов 1 рецепт.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientGatheringAndGardenPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientGatheringAndGardenPatchCommandlet.generated.h"

UCLASS()
class UIngredientGatheringAndGardenPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
