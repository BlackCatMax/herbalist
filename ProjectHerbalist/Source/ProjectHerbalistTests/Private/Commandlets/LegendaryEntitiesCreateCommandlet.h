// LegendaryEntitiesCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_LegendaryEntities с нуля и заполняет 17
// карточками. Изначально (2026-09-02, юнит 3/3 миграции бестиария) — 16
// якорных карточек, Берегиня была вне миграции (свой per-клеточный путь,
// хардкод в GridWorldManagerEntities.cpp). 2026-09-02, унификация Берегини
// (отдельный юнит, ответ на "все сущности должны быть равны... без
// исключений"): добавлена 17-й строкой, bUsesCellHistoryPurity=true (см.
// LegendaryEntityTypes.h) — тот же реестр, второй механизм триггера, не
// исключение. Тот же паттерн, что AmbientEntitiesCreateCommandlet.cpp.
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
