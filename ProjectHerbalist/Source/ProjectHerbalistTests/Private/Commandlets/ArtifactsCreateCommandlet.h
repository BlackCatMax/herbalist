// ArtifactsCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_Artifacts с нуля (2026-09-02, Unit 4/6
// миграции контента проекта на data-driven архитектуру) и заполняет
// 8 карточками, построчно перенесёнными без изменения значений из
// прежнего литерального массива ArtifactTypes.h::GetArtifactDefinitions().
// Тот же паттерн, что LegendaryEntitiesCreateCommandlet.cpp и др.
//
// Идемпотентен: если ассет уже существует, ничего не делает, успех.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=ArtifactsCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ArtifactsCreateCommandlet.generated.h"

UCLASS()
class UArtifactsCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
