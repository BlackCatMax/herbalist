// DialogueCreateCommandlet.h
//
// Создаёт /Game/Herbalist/Data/DT_Dialogue с нуля (2026-09-02, Unit 5/6
// миграции контента проекта на data-driven архитектуру) и заполняет
// одной карточкой (Домовой), построчно перенесённой без изменения
// значений из прежнего литерального массива HerbalistDialogueTypes.h::
// GetDialogueDefinitions(). Тот же паттерн, что LegendaryEntitiesCreateCommandlet.cpp
// и др.
//
// Идемпотентен: если ассет уже существует, ничего не делает, успех.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=DialogueCreate
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DialogueCreateCommandlet.generated.h"

UCLASS()
class UDialogueCreateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
