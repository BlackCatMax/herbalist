// KalinovMostDialogueAppendCommandlet.h
//
// Калинов мост / Трёхглавый Змей (DESIGN_Brewing_Situations_And_Lore.md
// §4.4, 2026-09-06, решение пользователя: "диалоговый выбор" вместо боевой
// системы, которой в проекте нет). DialogueCreateCommandlet идемпотентен
// только в сторону "таблица уже есть — ничего не делаю", он не умеет
// добавлять НОВЫЙ ряд к уже существующей живой таблице — та же ситуация,
// что уже решалась отдельными Append-коммандлетами для DT_IngredientClass/
// DT_TieredWardCrystal. Добавляет ряд "ЗмейГорыныч" с одним узлом и двумя
// ветками ("Бой" — bIsKalinovMostFight=true, "Сделка" — без предмета,
// честный пробел, см. довод у FDialogueBranch::bIsKalinovMostFight).
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=KalinovMostDialogueAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "KalinovMostDialogueAppendCommandlet.generated.h"

UCLASS()
class UKalinovMostDialogueAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
