// MemoryFragmentsCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_MemoryFragments с нуля (2026-09-02,
// Unit 6/6 миграции контента проекта на data-driven архитектуру) и
// заполняет 12 карточками, построчно перенесёнными без изменения значений
// из прежнего литерального массива MemoryFragmentDefinitions.h::
// GetAllMemoryFragmentDefinitions(). Тот же паттерн, что
// LegendaryEntitiesCreateCommandlet.cpp и др.
//
// Идемпотентен: если ассет уже существует, ничего не делает, успех.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=MemoryFragmentsCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MemoryFragmentsCreateCommandlet.generated.h"

UCLASS()
class UMemoryFragmentsCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
