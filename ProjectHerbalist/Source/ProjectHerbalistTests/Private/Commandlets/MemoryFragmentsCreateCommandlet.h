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
// -sync (2026-09-19, 23_Journey_Order §23.6-23.7): в существующем ассете
// выравнивает ClarityGain по рядам ниже и дописывает недостающие ряды
// (BROD); тексты и класс актора не трогает.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=MemoryFragmentsCreate [-sync]
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

    // Выравнивает вес в якоре и дописывает недостающие ряды. Число
    // изменённых рядов (тест -- на копии таблицы).
    static int32 SyncExistingTable(class UDataTable* Table);
};
