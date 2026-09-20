// RitualRewardAppendCommandlet.h
//
// Три награды новых ритуалов (решение пользователя 2026-09-20, «нужны
// реальные составы»): Полынный пояс, Одолень-корень, Перунов цвет.
// Дописываются в /Game/Herbalist/Data/DT_IngredientClass тем же способом,
// что кристаллы оберегов (WardCrystalAppendCommandlet): точечный AddRow,
// существующие ряды не трогаются, повторный прогон пропускает уже
// добавленное.
//
// Два первых -- обереги (`bIsWard`), то есть уже существующий класс
// предметов с уже работающим эффектом; третий -- исключительный ингредиент
// без своего эффекта, как награды Легендарных (§16.4): его ценность в
// BaseState, а не в новой механике. `AllowedBiomes` у всех трёх пуст --
// случайным сбором не выпадают, единственный путь получения -- ритуал
// (`RitualTypes.h`, `GrantsIngredientID`).
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=RitualRewardAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RitualRewardAppendCommandlet.generated.h"

UCLASS()
class URitualRewardAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
