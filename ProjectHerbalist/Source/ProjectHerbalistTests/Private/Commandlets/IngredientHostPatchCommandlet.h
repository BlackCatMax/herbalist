// IngredientHostPatchCommandlet.h
//
// Проставляет FIngredientTableRow::HostEntityID («хозяин» травы, решение
// пользователя 2026-09-20: Основной её биома; в Смешанном лесу грибы и
// деревья -- Боровику, цветы и травы -- Луговому; в Тайге ягоды -- Духу
// Медведя, остальное -- Ауке; Широколиственный лес -- Гуменнику) на уже
// существующих рядах DT_IngredientClass точечно, через FindRow, тем же
// способом, что IngredientHarvestWindowPatch.
//
// Источник -- herbalist_docs/CSV_tabs/ingredient_hosts.json.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=IngredientHostPatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IngredientHostPatchCommandlet.generated.h"

UCLASS()
class UIngredientHostPatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
