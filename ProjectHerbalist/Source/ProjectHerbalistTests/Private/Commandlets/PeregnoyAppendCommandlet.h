// PeregnoyAppendCommandlet.h
//
// Добавляет ряд для "Перегноя" -- терминального продукта гниения
// (UHerbalistInventoryComponent::TickComponent, ShouldConvertToPeregnoy,
// 2026-09-04, прямой запрос: "с биологической точки зрения гнилая трава не
// зола... либо выкидывать, либо придумать применение") -- в живой
// DT_IngredientClass через UDataTable::AddRow, тот же безопасный точечный
// приём, что и WardCrystalAppendCommandlet/TieredWardCrystalAppendCommandlet
// (никогда GetTableAsJSON()/CreateTableFromJSONString()).
//
// В отличие от карточек трав/оберегов -- этот ряд НЕ собирается в мире
// вовсе (AllowedBiomes пуст, GardenNiche::None) и не выдаётся ритуалом --
// единственный путь получить его -- довести собранную траву до предельного
// гниения (Purity/Distortion пороги, HerbalistSettings.h). DecayRate=0.0 --
// терминальное состояние, дальше портиться некуда, тот же довод, что уже у
// каменных оберегов ("камень не портится"), просто по другой причине (уже
// прогнило до конца, не "никогда не портится по природе материала").
//
// Идемпотентен: ряд, уже присутствующий в таблице, пропускается с
// предупреждением, не перезаписывается.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=PeregnoyAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PeregnoyAppendCommandlet.generated.h"

UCLASS()
class UPeregnoyAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
