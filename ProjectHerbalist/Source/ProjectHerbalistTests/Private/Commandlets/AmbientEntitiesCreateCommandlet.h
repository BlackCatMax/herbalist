// AmbientEntitiesCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_AmbientEntities с нуля (2026-09-02,
// миграция бестиария Низшего ранга на data-driven архитектуру, прямой
// запрос пользователя) и заполняет 28 карточками, построчно перенесёнными
// без изменения чисел из прежнего литерального массива
// AmbientEntityTypes.h::GetAmbientEntityDefinitions() (см. историю
// коммита, где массив ещё стоял в заголовке). В отличие от
// ArtifactIngredientAppendCommandlet.cpp (дополняет уже существующий
// ассет), этот коммандлет ПЕРВЫМ в проекте создаёт DataTable-пакет с нуля
// (CreatePackage + NewObject<UDataTable> + RowStruct + AddRow +
// FAssetRegistryModule::AssetCreated + SavePackage).
//
// Идемпотентен: если ассет уже существует (LoadObject успешен), команда
// ничего не делает и завершается успехом — тот же признак, что уже
// использует ArtifactIngredientAppendCommandlet.cpp для "ряд уже есть".
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=AmbientEntitiesCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AmbientEntitiesCreateCommandlet.generated.h"

UCLASS()
class UAmbientEntitiesCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
