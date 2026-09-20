// AmbientPackSizePatchCommandlet.h
//
// Проставляет FAmbientEntityDefinition::PackSize (сколько особей выпускает
// один спавнер, DESIGN_Entity_Spawners.md) на уже существующих рядах
// DT_AmbientEntities -- точечно, через FindRow, тем же способом, что
// AmbientEntitySpacingPatch. Решение пользователя 2026-09-20: огни и
// Купальские ходят стайкой по три, остальные поодиночке (у них остаётся
// дефолт 1, и в патче их нет).
//
// Источник -- herbalist_docs/CSV_tabs/ambient_pack_size.json.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=AmbientPackSizePatch
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AmbientPackSizePatchCommandlet.generated.h"

UCLASS()
class UAmbientPackSizePatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
