// LandmarksCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_Landmarks с нуля (2026-09-02, юнит 2/3
// миграции бестиария на data-driven архитектуру) и заполняет 15
// карточками, построчно перенесёнными без изменения чисел из прежнего
// литерального массива LandmarkTypes.h::GetLandmarkDefinitions(). Тот же
// паттерн, что AmbientEntitiesCreateCommandlet.cpp (юнит 1/3) -- см.
// подробное обоснование там.
//
// Идемпотентен: если ассет уже существует, ничего не делает, успех.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=LandmarksCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "LandmarksCreateCommandlet.generated.h"

UCLASS()
class ULandmarksCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
