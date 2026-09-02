// LegendaryEntitiesCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_LegendaryEntities с нуля (2026-09-02,
// юнит 3/3, последний, миграции бестиария на data-driven архитектуру) и
// заполняет 16 карточками, построчно перенесёнными без изменения чисел
// из прежнего литерального массива LegendaryEntityTypes.h::
// GetLegendaryEntityDefinitions(). Берегиня НЕ входит (свой per-клеточный
// путь, вне миграции). Тот же паттерн, что AmbientEntitiesCreateCommandlet.cpp
// (юнит 1/3) и LandmarksCreateCommandlet.cpp (юнит 2/3).
//
// Идемпотентен: если ассет уже существует, ничего не делает, успех.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=LegendaryEntitiesCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "LegendaryEntitiesCreateCommandlet.generated.h"

UCLASS()
class ULegendaryEntitiesCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
